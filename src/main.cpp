// Demo for Max7219Matrix. Wiring (ESP32 DevKit, VSPI):
//   MAX7219 VCC -> 5V (or 3V3), GND -> GND
//   DIN -> GPIO23, CLK -> GPIO18, CS -> GPIO5
#include <Arduino.h>
#include <Max7219Matrix.h>

constexpr uint8_t kCsPin = 5;
constexpr uint8_t kModules = 4;

// Set true to show a test pattern instead of the demo, then tweak kLayout
// (OR flags together) until the dot is at the display's top-left and the
// "F" in the first module is upright and unmirrored.
constexpr bool kCalibrate = false;
constexpr uint8_t kLayout = Max7219Matrix::kReverseModules;

Max7219Matrix matrix(kCsPin, kModules, kLayout);

void setup() {
  Serial.begin(115200);
  SPI.begin();  // default VSPI pins: SCK 18, MOSI 23
  matrix.begin(SPI, /*brightness=*/4);

  matrix.clear();
  if (kCalibrate) {
    matrix.setPixel(0, 0);
    matrix.drawText(2, 0, "F");
    matrix.show();
    return;
  }

  matrix.setFont(Max7219Matrix::kFont3x7);  // 3px-wide digits, 1px gap
}

// 8 digits * 4 - 1 = 31 columns, fits the 32-column display.
// Alternate 0-7 and 2-9, 3 seconds each.
void loop() {
  matrix.clear();
  matrix.drawTextCentered(0, "01234567");
  matrix.show();
  delay(3000);

  matrix.clear();
  matrix.drawTextCentered(0, "23456789");
  matrix.show();
  delay(3000);
}
