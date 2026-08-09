#pragma once

#include <cstddef>
#include <cstdint>

namespace Board {

struct FireBeetle2Esp32C6Profile {
  static constexpr const char *kBoardId = "dfrobot-firebeetle2-esp32c6";
  static constexpr const char *kChip = "ESP32-C6FH4";
  static constexpr uint32_t kFlashBytes = 4U * 1024U * 1024U;
  static constexpr bool kHasPsram = false;

  // DFRobot GDI routes: SCK=23, MOSI=22, MISO=21. The e-paper bus is
  // deliberately write-only so GPIO21 remains exclusively available for BUSY.
  static constexpr SpiRoute kSpi{23, 22, kNoPin};
  static constexpr I2cRoute kI2c{19, 20};
  // DFRobot's DFR1075 battery example reads GPIO0 and doubles the calibrated
  // ADC pin voltage to account for the onboard divider.
  static constexpr BatterySense kBatterySense{0, 2, 1};
  static constexpr EpaperPins kEpaper{18, 1, 14, 21};

  static constexpr uint32_t kEpaperSpiHz = 4000000;
  static constexpr uint8_t kEpaperSpiMode = 0;
  static constexpr bool kSafeCsHigh = true;
  static constexpr bool kSafeDcHigh = false;
  static constexpr bool kSafeResetHigh = false;

  static constexpr PinDisposition pinDisposition(int pin) {
    if (pin < 0 || pin > 30) return PinDisposition::NotExposed;
    if (pin >= 24) return PinDisposition::Flash;
    if (pin == 4 || pin == 5 || pin == 8 || pin == 9 || pin == 15) {
      return PinDisposition::Strapping;
    }
    if (pin == 12 || pin == 13) return PinDisposition::UsbJtag;
    // GPIO0 is hard-wired to battery-voltage measurement on DFR1075.
    if (pin == kBatterySense.pin) return PinDisposition::BoardReserved;
    // ESP32-C6FH4 has in-package flash; GPIO10/11 are not bonded out.
    if (pin == 10 || pin == 11) return PinDisposition::NotExposed;
    return PinDisposition::Available;
  }

  static constexpr bool usable(int pin) {
    return pinDisposition(pin) == PinDisposition::Available;
  }
};

constexpr int8_t kFireBeetleEpaperPins[] = {
    FireBeetle2Esp32C6Profile::kSpi.sck,
    FireBeetle2Esp32C6Profile::kSpi.mosi,
    FireBeetle2Esp32C6Profile::kEpaper.cs,
    FireBeetle2Esp32C6Profile::kEpaper.dc,
    FireBeetle2Esp32C6Profile::kEpaper.reset,
    FireBeetle2Esp32C6Profile::kEpaper.busy,
};

static_assert(uniquePins(kFireBeetleEpaperPins,
                         sizeof(kFireBeetleEpaperPins) /
                             sizeof(kFireBeetleEpaperPins[0])),
              "e-paper signal pins must be unique");
static_assert(FireBeetle2Esp32C6Profile::kSpi.miso == kNoPin,
              "e-paper SPI must remain write-only; GPIO21 is BUSY");
static_assert(FireBeetle2Esp32C6Profile::kEpaper.busy == 21,
              "GPIO21 is reserved for the e-paper BUSY input");
static_assert(FireBeetle2Esp32C6Profile::kBatterySense.pin == 0 &&
                  FireBeetle2Esp32C6Profile::kBatterySense.dividerNumerator == 2 &&
                  FireBeetle2Esp32C6Profile::kBatterySense.dividerDenominator == 1 &&
                  FireBeetle2Esp32C6Profile::pinDisposition(
                      FireBeetle2Esp32C6Profile::kBatterySense.pin) ==
                      PinDisposition::BoardReserved,
              "DFR1075 battery ADC must remain a dedicated board function");
static_assert(FireBeetle2Esp32C6Profile::usable(
                  FireBeetle2Esp32C6Profile::kSpi.sck) &&
                  FireBeetle2Esp32C6Profile::usable(
                      FireBeetle2Esp32C6Profile::kSpi.mosi) &&
                  FireBeetle2Esp32C6Profile::usable(
                      FireBeetle2Esp32C6Profile::kEpaper.cs) &&
                  FireBeetle2Esp32C6Profile::usable(
                      FireBeetle2Esp32C6Profile::kEpaper.dc) &&
                  FireBeetle2Esp32C6Profile::usable(
                      FireBeetle2Esp32C6Profile::kEpaper.reset) &&
                  FireBeetle2Esp32C6Profile::usable(
                      FireBeetle2Esp32C6Profile::kEpaper.busy),
              "e-paper pins must avoid board and ESP32-C6 restricted GPIOs");

}  // namespace Board
