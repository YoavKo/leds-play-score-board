# leds-play-score-board

Arduino/PlatformIO library for chained MAX7219 8x8 LED modules (the 4-in-1
32x8 boards) on an ESP32. Pixels, 1-bit bitmaps, text, scrolling text,
7-segment style digits, and per-board numbers for several boards in one chain.

Build and flash: `uvx platformio run -t upload` (the apt `pio` is too old).

## Wiring (ESP32 DevKit, VSPI)

| MAX7219 | ESP32 |
|---------|-------|
| VCC     | 5V (3V3 often works, dimmer) |
| GND     | GND   |
| DIN     | GPIO23 |
| CLK     | GPIO18 |
| CS      | GPIO5 |

Use the **input** (DIN) side of the module chain.

### More than one board

To chain a second 4-module board, connect the first board's **output** side to
the second board's **input** side: DOUT -> DIN, **and also** CLK -> CLK,
CS -> CS, VCC and GND. (CLK and CS are not passed along inside the board;
without them the far board stays dark or never updates.) Then set the module
count, e.g. 8 modules for two boards.

Power: all LEDs lit can draw several hundred mA per board. Use a separate 5V
supply for more than one board, join the grounds, and keep brightness low.

## Usage

```cpp
Max7219Matrix matrix(/*cs=*/5, /*modules=*/4, /*layout=*/Max7219Matrix::kReverseModules);
SPI.begin();
matrix.begin(SPI, /*brightness 0..15=*/4);
matrix.clear();
matrix.drawBitmap(0, 0, kSmiley, 8, 8);
matrix.drawText(10, 0, "Hi!");
matrix.show();                       // nothing appears until show()
```

Coordinates: (0,0) is top-left, x grows right, y down. The display is
`8 * modules` wide and 8 tall.

Images: `uv run --with pillow tools/png2bitmap.py img.png --name kLogo > logo.h`,
then `matrix.drawBitmap(x, y, kLogo, kLogoW, kLogoH)`.

## Fonts

`setFont()` picks the font used by `drawChar`/`drawText`/`drawTextCentered`/
`textWidth`/scrolling:

| Font | Size | Chars |
|------|------|-------|
| `kFont5x7` (default) | proportional, ~5 wide | printable ASCII |
| `kFont3x5` | 3 wide x 5 tall | `0-9` only |
| `kFont3x7` | 3 wide x 7 tall | `0-9` only |
| `kFont3x8` | 3 wide x 8 tall | `0-9` only |

The 3-wide digit fonts are 7-segment style with a 1px gap, so 8 digits take
31 columns (one board) and 16 take 63. Other characters are drawn blank.

There is also a separate 5x8 segment-digit renderer:
`showNumber(1234)`, `showSegmentText("12.5")` (digits, `-`, ` `, `.`;
5 digits fit 32 columns), independent of `setFont`.

## Scrolling

```cpp
matrix.startScroll("hello", /*stepMs=*/60, /*loop=*/true, Max7219Matrix::kScrollLeft);
// kScrollLeft = right-to-left marquee (default), kScrollRight = left-to-right
void loop() { matrix.update(); }     // non-blocking
```

## Boards

A *board* is a group of modules (default 4) in the chain, numbered from 1 at
the left. These draw into one board only, clipped to it, leave the others
untouched, and update the display:

```cpp
Max7219Matrix matrix(5, 8, Max7219Matrix::kReverseModules);   // two boards
matrix.setFont(Max7219Matrix::kFont3x7);
matrix.setBoardNumber(1, 98765432, 8);   // minDigits=8 zero-pads: "98765432"
matrix.setBoardNumber(2, 1234567, 8);    // "01234567"
matrix.setBoardText(2, "1234");          // current font, centered by default
```

`setModulesPerBoard(n)` changes the board size, `boardCount()` returns how many
boards there are. Pass `kLeft`/`kCenter`/`kRight` as the last argument to align.

Careful: in C++ a literal with a leading zero (`01234567`) is **octal**. Pass
`1234567` and a `minDigits` instead.

## Orientation

Cheap boards differ in how the chips are mounted. Set `kCalibrate = true` in
`src/main.cpp`; a dot should appear at the display's top-left with an upright
"F" next to it. Otherwise OR flags into the layout argument:
`kReverseModules` (modules in the wrong order), `kSwapXY`, `kFlipX`, `kFlipY`.
The author's boards work with `kReverseModules` alone. The layout applies to
all modules the same way.

## Status

Builds with PlatformIO and runs on an ESP32 with a 4-module board. Chaining a
second board (8 modules) is wired up in the demo but the far board has not been
confirmed working yet (check DOUT/CLK/CS wiring as above).
