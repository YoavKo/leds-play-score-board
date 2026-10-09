#pragma once
#include <Arduino.h>
#include <SPI.h>

// Driver + drawing helpers for a chain of MAX7219 8x8 LED modules (e.g. the
// 4-in-1 "FC-16" style boards). Drawing goes into an in-RAM framebuffer;
// call show() to push it to the display.
//
// Coordinates: (0,0) is the top-left pixel of the logical display, x grows
// right, y grows down. The display is (8 * numModules) wide and 8 tall.
class Max7219Matrix {
 public:
  // How the physical modules are oriented relative to the logical display.
  // Boards differ, so if the output is mirrored/rotated/reversed, OR some of
  // these together (see the `calibrate` mode of the demo).
  enum Layout : uint8_t {
    kDefault = 0,
    kReverseModules = 1 << 0,  // chain's first module is the rightmost one
    kSwapXY = 1 << 1,          // module registers are columns, not rows
    kFlipX = 1 << 2,           // mirror inside each module horizontally
    kFlipY = 1 << 3,           // mirror inside each module vertically
  };

  static constexpr uint8_t kMaxModules = 16;

  Max7219Matrix(uint8_t csPin, uint8_t numModules = 4, uint8_t layout = kDefault);

  // Call SPI.begin() (or SPI.begin(sck, -1, mosi, -1) for custom pins) first.
  void begin(SPIClass& spi = SPI, uint8_t brightness = 4);

  void setBrightness(uint8_t level);  // 0..15
  void setPower(bool on);             // false = shutdown (keeps framebuffer)

  uint16_t width() const { return _width; }
  uint16_t height() const { return 8; }

  // --- drawing (framebuffer only; nothing is visible until show()) ---
  void clear();
  void setPixel(int16_t x, int16_t y, bool on = true);
  bool getPixel(int16_t x, int16_t y) const;

  // 1-bit bitmap, row-major, each row padded to a whole byte, MSB = leftmost
  // pixel (same as Adafruit_GFX drawBitmap / `tools/png2bitmap.py` output).
  // Set bits turn pixels on; clear bits are left alone unless `opaque`.
  void drawBitmap(int16_t x, int16_t y, const uint8_t* bmp, uint16_t w,
                  uint16_t h, bool opaque = false);
  // Same layout but stored in flash (PROGMEM); identical on ESP32.
  void drawBitmapP(int16_t x, int16_t y, const uint8_t* bmp, uint16_t w,
                   uint16_t h, bool opaque = false) {
    drawBitmap(x, y, bmp, w, h, opaque);
  }

  // Font used by drawChar/drawText/textWidth/scrolling. kFont3x5, kFont3x7 and kFont3x8 are
  // compact 7-segment style digits-only fonts (0-9, fixed 3px wide, 5, 7 or 8
  // tall; other chars are blank).
  enum Font : uint8_t { kFont5x7, kFont3x5, kFont3x7, kFont3x8 };
  void setFont(Font f) { _font = f; }

  // Draws one glyph; returns its advance (width + 1px gap). Unknown chars
  // render as '?'.
  uint8_t drawChar(int16_t x, int16_t y, char c);
  // Draws a string with the built-in 5x7 proportional font; returns the x
  // just past the last glyph. Text is clipped to the display.
  int16_t drawText(int16_t x, int16_t y, const char* text);
  int16_t textWidth(const char* text) const;

  // Draws text horizontally centred on the display.
  void drawTextCentered(int16_t y, const char* text);

  // --- 7-segment style digits (5 wide x 8 tall, 1px gap) ---
  // Supports 0-9, '-', ' ' and '.'; anything else is drawn as blank.
  enum Align : uint8_t { kLeft, kCenter, kRight };
  int16_t drawSegmentText(int16_t x, const char* text);
  int16_t segmentTextWidth(const char* text) const;
  // Clears, draws `value` in segment digits aligned within the display, and
  // shows it. 32 columns fit 5 digits (e.g. "-9999".."99999").
  void showNumber(long value, Align align = kRight);
  // Same, for an arbitrary string, in segment digits.
  void showSegmentText(const char* text, Align align = kRight);

  // --- boards: independent groups of modules in one chain ---
  // A "board" is `modulesPerBoard` consecutive modules (default 4 = one 32x8
  // board), numbered from 1 starting at the left. These draw into one board
  // only, clipped to it, leave the others untouched, and call show().
  void setModulesPerBoard(uint8_t n) { _modulesPerBoard = n ? n : 1; }
  uint8_t boardCount() const { return _n / _modulesPerBoard; }
  // Shows `value` on `board` using the current font, zero-padded to
  // `minDigits` (0 = no padding). e.g. setBoardNumber(2, 42, 4) -> "0042".
  void setBoardNumber(uint8_t board, long value, uint8_t minDigits = 0,
                      Align align = kCenter);
  void setBoardText(uint8_t board, const char* text, Align align = kCenter);

  // --- scrolling text (non-blocking) ---
  // Call update() from loop(); it redraws + shows when the step interval has
  // elapsed. Returns true while the scroll is still running.
  // kScrollLeft: text moves right-to-left (usual marquee); kScrollRight: the
  // text travels left-to-right.
  enum ScrollDir : uint8_t { kScrollLeft, kScrollRight };
  void startScroll(const String& text, uint16_t stepMs = 60, bool loop = true,
                   ScrollDir dir = kScrollLeft);
  void stopScroll() { _scrolling = false; }
  bool update();

  // Pushes the framebuffer to the chips.
  void show();

 private:
  void sendAll(uint8_t reg, uint8_t value);
  int16_t fontTopY() const { return _font == kFont3x5 ? 1 : 0; }
  uint8_t nativeByte(uint8_t logicalModule, uint8_t reg) const;

  uint8_t _cs;
  uint8_t _n;
  uint8_t _layout;
  uint16_t _width;
  SPIClass* _spi = nullptr;
  // Column-major: _fb[x] bit y (0 = top).
  uint8_t _fb[kMaxModules * 8] = {};

  String _scrollText;
  int16_t _scrollX = 0;
  int16_t _scrollW = 0;
  uint16_t _scrollStepMs = 60;
  bool _scrollLoop = true;
  ScrollDir _scrollDir = kScrollLeft;
  bool _scrolling = false;
  Font _font = kFont5x7;
  uint8_t _modulesPerBoard = 4;
  // Drawing is clipped to columns [_clipL, _clipR); whole display by default.
  int16_t _clipL = 0;
  int16_t _clipR = 0;  // 0 = _width, set in the constructor
  uint32_t _lastStep = 0;
};
