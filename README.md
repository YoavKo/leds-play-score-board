# leds-play-score-board

Arduino/PlatformIO library for chained MAX7219 8x8 LED modules (the 4-in-1
32x8 boards) on an ESP32. Pixels, 1-bit bitmaps, text and scrolling text.

## Wiring (ESP32 DevKit, VSPI)

| MAX7219 | ESP32 |
|---------|-------|
| VCC     | 5V (3V3 often works, dimmer) |
| GND     | GND   |
| DIN     | GPIO23 |
| CLK     | GPIO18 |
| CS      | GPIO5 |

Use the **input** (DIN) side of the module chain.

## Usage

```cpp
Max7219Matrix matrix(/*cs=*/5, /*modules=*/4);
SPI.begin();
matrix.begin(SPI, /*brightness 0..15=*/4);
matrix.clear();
matrix.drawBitmap(0, 0, kSmiley, 8, 8);
matrix.drawText(10, 0, "Hi!");
matrix.show();                       // nothing appears until show()
matrix.startScroll("hello", 50);     // then call matrix.update() in loop()
```

Images: `uv run --with pillow tools/png2bitmap.py img.png --name kLogo > logo.h`,
then `matrix.drawBitmap(x, y, kLogo, kLogoW, kLogoH)`.

## Numbers (7-segment style)

```cpp
matrix.showNumber(1234);                          // right-aligned, shown immediately
matrix.showNumber(42, Max7219Matrix::kCenter);
matrix.showSegmentText("12.5");                   // digits, '-', ' ' and '.'
```

Digits are 5x8 px with a 1px gap, so the 32 columns fit 5 digits. For
regular letters/numbers use `drawText` instead.

## Orientation

Cheap boards differ in how the chips are mounted. Set `kCalibrate = true` in
`src/main.cpp`; a dot should appear at the display's top-left with an upright
"F" next to it. Otherwise OR flags into the layout argument:
`kReverseModules` (modules in the wrong order), `kSwapXY`, `kFlipX`, `kFlipY`.

## Status

Compiles against stub headers and the framebuffer/SPI byte count was checked
on a PC, but it has **not** been built with PlatformIO or run on hardware yet:
`pio run -t upload`.
