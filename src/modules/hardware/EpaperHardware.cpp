#include "EpaperHardware.h"

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <driver/gpio.h>
#endif

namespace EpaperHardware {

namespace {

bool presetOutputLevel(int pin, bool high) {
#if defined(ARDUINO_ARCH_ESP32)
  // Arduino-ESP32 rejects digitalWrite() until pinMode(OUTPUT), while the IDF
  // GPIO driver can preload the output latch before enabling the output
  // driver. This preserves the inactive levels without a CS/RST glitch.
  return gpio_set_level(static_cast<gpio_num_t>(pin), high ? 1U : 0U) == ESP_OK;
#else
  digitalWrite(pin, high ? HIGH : LOW);
  return true;
#endif
}

}  // namespace

Result claimAndQuiescePins(PinRegistry *pins) {
  if (pins == nullptr) return invalidInput("pin registry is required");
  constexpr Board::EpaperPins epaper = Board::ActiveProfile::kEpaper;
  const PinClaim claims[] = {
      {epaper.cs, PinClaimKind::Exclusive, kOwner, "cs"},
      {epaper.dc, PinClaimKind::Exclusive, kOwner, "dc"},
      {epaper.reset, PinClaimKind::Exclusive, kOwner, "reset"},
      {epaper.busy, PinClaimKind::Exclusive, kOwner, "busy"},
  };
  if (pins->claimAll(claims, sizeof(claims) / sizeof(claims[0])) !=
      PinClaimStatus::Ok) {
    return invalidInput("e-paper pin claim failed");
  }

  // Set output latches before enabling output mode to avoid a select/reset
  // glitch while the GPIO matrix changes ownership.
  if (!presetOutputLevel(epaper.cs, Board::ActiveProfile::kSafeCsHigh) ||
      !presetOutputLevel(epaper.dc, Board::ActiveProfile::kSafeDcHigh) ||
      !presetOutputLevel(epaper.reset, Board::ActiveProfile::kSafeResetHigh)) {
    return storageError("e-paper safe output latch failed");
  }
  pinMode(epaper.cs, OUTPUT);
  pinMode(epaper.dc, OUTPUT);
  pinMode(epaper.reset, OUTPUT);
  // BUSY is active-low. Keep its fail-safe idle level high when the panel
  // controller is held in reset and may leave the output high-impedance.
  pinMode(epaper.busy, INPUT_PULLUP);
  return okResult();
}

}  // namespace EpaperHardware
