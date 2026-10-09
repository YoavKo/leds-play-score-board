#include "Max7219Matrix.h"

#include "Font3x5.h"
#include "Font3x7.h"
#include "Font3x8.h"
#include "Font5x7.h"

namespace {
constexpr uint8_t kRegDecode = 0x09;
constexpr uint8_t kRegIntensity = 0x0A;
constexpr uint8_t kRegScanLimit = 0x0B;
constexpr uint8_t kRegShutdown = 0x0C;
constexpr uint8_t kRegTest = 0x0F;
constexpr uint8_t kGap = 1;  // blank columns between glyphs
}  // namespace

Max7219Matrix::Max7219Matrix(uint8_t csPin, uint8_t numModules, uint8_t layout)
    : _cs(csPin),
      _n(min<uint8_t>(numModules, kMaxModules)),
      _layout(layout),
      _width(_n * 8) {}

void Max7219Matrix::begin(SPIClass& spi, uint8_t brightness) {
  _spi = &spi;
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  sendAll(kRegTest, 0);
  sendAll(kRegDecode, 0);
  sendAll(kRegScanLimit, 7);
  setBrightness(brightness);
  setPower(true);
  clear();
  show();
}

void Max7219Matrix::setBrightness(uint8_t level) {
  sendAll(kRegIntensity, min<uint8_t>(level, 15));
}

void Max7219Matrix::setPower(bool on) { sendAll(kRegShutdown, on ? 1 : 0); }

void Max7219Matrix::sendAll(uint8_t reg, uint8_t value) {
  if (!_spi) return;
  _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  for (uint8_t i = 0; i < _n; i++) {
    _spi->transfer(reg);
    _spi->transfer(value);
  }
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
}

void Max7219Matrix::clear() { memset(_fb, 0, sizeof(_fb)); }

void Max7219Matrix::setPixel(int16_t x, int16_t y, bool on) {
  if (x < 0 || x >= _width || y < 0 || y >= 8) return;
  if (on)
    _fb[x] |= (1 << y);
  else
    _fb[x] &= ~(1 << y);
}

bool Max7219Matrix::getPixel(int16_t x, int16_t y) const {
  if (x < 0 || x >= _width || y < 0 || y >= 8) return false;
  return _fb[x] & (1 << y);
}

void Max7219Matrix::drawBitmap(int16_t x, int16_t y, const uint8_t* bmp,
                               uint16_t w, uint16_t h, bool opaque) {
  const uint16_t stride = (w + 7) / 8;
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      const bool bit = bmp[row * stride + col / 8] & (0x80 >> (col % 8));
      if (bit || opaque) setPixel(x + col, y + row, bit);
    }
  }
}

namespace {
const uint8_t* glyph(char c) {
  if (c < max7219::kFontFirst || c > max7219::kFontLast) c = '?';
  return &max7219::kFont5x7[(c - max7219::kFontFirst) * max7219::kFontCols];
}

// Columns up to and including the last non-empty one; space gets 3.
uint8_t glyphWidth5x7(char c) {
  if (c == ' ') return 3;
  const uint8_t* g = glyph(c);
  uint8_t w = max7219::kFontCols;
  while (w > 1 && g[w - 1] == 0) w--;
  return w;
}

constexpr uint8_t kW3x5 = 3;
}  // namespace

uint8_t Max7219Matrix::drawChar(int16_t x, int16_t y, char c) {
  if (_font != kFont5x7) {
    if (c >= '0' && c <= '9') {
      const uint8_t* table = _font == kFont3x5   ? max7219::kFont3x5Digits
                             : _font == kFont3x7 ? max7219::kFont3x7Digits
                                                 : max7219::kFont3x8Digits;
      const uint8_t* g = &table[(c - '0') * kW3x5];
      for (uint8_t col = 0; col < kW3x5; col++)
        for (uint8_t row = 0; row < 8; row++)
          if (g[col] & (1 << row)) setPixel(x + col, y + row, true);
    }
    return kW3x5 + kGap;
  }
  const uint8_t* g = glyph(c);
  const uint8_t w = glyphWidth5x7(c);
  for (uint8_t col = 0; col < w; col++)
    for (uint8_t row = 0; row < 8; row++)
      if (g[col] & (1 << row)) setPixel(x + col, y + row, true);
  return w + kGap;
}

int16_t Max7219Matrix::drawText(int16_t x, int16_t y, const char* text) {
  for (; *text; text++) {
    if (x >= (int16_t)_width) break;  // fully off the right edge
    x += drawChar(x, y, *text);
  }
  return x;
}

int16_t Max7219Matrix::textWidth(const char* text) const {
  int16_t w = 0;
  for (; *text; text++)
    w += (_font != kFont5x7 ? kW3x5 : glyphWidth5x7(*text)) + kGap;
  return w > 0 ? w - kGap : 0;
}

void Max7219Matrix::drawTextCentered(int16_t y, const char* text) {
  drawText((int16_t)(_width - textWidth(text)) / 2, y, text);
}

namespace {
// Segment bits: a=top, b=upper right, c=lower right, d=bottom,
// e=lower left, f=upper left, g=middle.
enum : uint8_t { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };
constexpr uint8_t kDigitSegs[10] = {
    A | B | C | D | E | F, B | C,         A | B | D | E | G, A | B | C | D | G,
    B | C | F | G,         A | C | D | F | G, A | C | D | E | F | G, A | B | C,
    A | B | C | D | E | F | G, A | B | C | D | F | G};
constexpr uint8_t kSegW = 5;

uint8_t segmentsFor(char c) {
  if (c >= '0' && c <= '9') return kDigitSegs[c - '0'];
  if (c == '-') return G;
  return 0;
}

uint8_t segmentAdvance(char c) { return (c == '.' ? 1 : c == ' ' ? 3 : kSegW) + kGap; }
}  // namespace

int16_t Max7219Matrix::drawSegmentText(int16_t x, const char* text) {
  for (; *text; text++) {
    const char ch = *text;
    if (ch == '.') {
      setPixel(x, 7);
    } else {
      const uint8_t s = segmentsFor(ch);
      for (uint8_t i = 1; i < kSegW - 1; i++) {
        if (s & A) setPixel(x + i, 0);
        if (s & G) setPixel(x + i, 3);
        if (s & D) setPixel(x + i, 7);
      }
      for (uint8_t r = 1; r <= 2; r++) {
        if (s & F) setPixel(x, r);
        if (s & B) setPixel(x + kSegW - 1, r);
      }
      for (uint8_t r = 4; r <= 6; r++) {
        if (s & E) setPixel(x, r);
        if (s & C) setPixel(x + kSegW - 1, r);
      }
      // corners belong to the horizontal segments' neighbours
      if (s & (A | F)) setPixel(x, 0);
      if (s & (A | B)) setPixel(x + kSegW - 1, 0);
      if (s & (G | F | E)) setPixel(x, 3);
      if (s & (G | B | C)) setPixel(x + kSegW - 1, 3);
      if (s & (D | E)) setPixel(x, 7);
      if (s & (D | C)) setPixel(x + kSegW - 1, 7);
    }
    x += segmentAdvance(ch);
  }
  return x;
}

int16_t Max7219Matrix::segmentTextWidth(const char* text) const {
  int16_t w = 0;
  for (; *text; text++) w += segmentAdvance(*text);
  return w > 0 ? w - kGap : 0;
}

void Max7219Matrix::showSegmentText(const char* text, Align align) {
  const int16_t tw = segmentTextWidth(text);
  int16_t x = 0;
  if (align == kRight) x = (int16_t)_width - tw;
  else if (align == kCenter) x = ((int16_t)_width - tw) / 2;
  clear();
  drawSegmentText(x, text);
  show();
}

void Max7219Matrix::showNumber(long value, Align align) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", value);
  showSegmentText(buf, align);
}

void Max7219Matrix::startScroll(const String& text, uint16_t stepMs, bool loop,
                                ScrollDir dir) {
  _scrollText = text;
  _scrollW = textWidth(text.c_str());
  _scrollDir = dir;
  _scrollX = (dir == kScrollLeft) ? (int16_t)_width : -_scrollW;
  _scrollStepMs = stepMs;
  _scrollLoop = loop;
  _scrolling = true;
  _lastStep = millis() - stepMs;  // draw the first frame immediately
}

bool Max7219Matrix::update() {
  if (!_scrolling) return false;
  const uint32_t now = millis();
  if (now - _lastStep < _scrollStepMs) return true;
  _lastStep = now;

  clear();
  drawText(_scrollX, _font == kFont3x5 ? 1 : 0, _scrollText.c_str());
  show();

  const bool done = (_scrollDir == kScrollLeft) ? (--_scrollX < -_scrollW)
                                                : (++_scrollX > (int16_t)_width);
  if (done) {
    if (_scrollLoop)
      _scrollX = (_scrollDir == kScrollLeft) ? (int16_t)_width : -_scrollW;
    else
      _scrolling = false;
  }
  return _scrolling;
}

// What the chip register `reg` (0..7) of the module at logical position
// `logicalModule` (0 = leftmost) must hold, given the layout flags.
uint8_t Max7219Matrix::nativeByte(uint8_t logicalModule, uint8_t reg) const {
  uint8_t out = 0;
  for (uint8_t bit = 0; bit < 8; bit++) {
    uint8_t lx, ly;
    if (_layout & kSwapXY) {
      lx = reg;
      ly = bit;
    } else {
      ly = reg;
      lx = 7 - bit;  // bit 7 = leftmost pixel
    }
    if (_layout & kFlipX) lx = 7 - lx;
    if (_layout & kFlipY) ly = 7 - ly;
    if (getPixel(logicalModule * 8 + lx, ly)) out |= (1 << bit);
  }
  return out;
}

void Max7219Matrix::show() {
  if (!_spi) return;
  for (uint8_t reg = 0; reg < 8; reg++) {
    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_cs, LOW);
    // Data shifts through the chain, so the module farthest from the ESP32
    // must be sent first. Chain position 0 is the one wired to DIN.
    for (int8_t pos = _n - 1; pos >= 0; pos--) {
      const uint8_t logical = (_layout & kReverseModules) ? (_n - 1 - pos) : pos;
      _spi->transfer(reg + 1);
      _spi->transfer(nativeByte(logical, reg));
    }
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
  }
}
