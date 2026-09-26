#include "ArduinoStatusLedDriver.h"
#if IOT_FEATURE_STATUS_LED
#include <Arduino.h>
#include <esp32-hal-ledc.h>
#include "modules/hardware/StatusLedHardware.h"
#include "board/BoardProfile.h"

namespace {
constexpr uint8_t kPin = Board::ActiveProfile::kStatusLedPin;
constexpr uint32_t kCycle = 1UL << Board::ActiveProfile::kStatusLedPwmBits;
constexpr uint32_t kOnDuty =
    (kCycle * Board::ActiveProfile::kStatusLedBrightnessPercent + 50) / 100;
constexpr uint32_t kDuty = Board::ActiveProfile::kStatusLedActiveHigh
                              ? kOnDuty : kCycle - kOnDuty;
static_assert(Board::ActiveProfile::kStatusLedBrightnessPercent > 0 &&
                  Board::ActiveProfile::kStatusLedBrightnessPercent < 100,
              "status LED PWM brightness must be between 1 and 99 percent");
}

bool ArduinoStatusLedDriver::stopPwm() {
  if (pwmAttached_ && ledcDetach(kPin)) pwmAttached_ = false;
  // Restore the GPIO mux and preload off even when peripheral detach fails.
  // An unresolved peripheral owner still prevents committing deep sleep.
  const bool off = StatusLedHardware::bootOff();
  return !pwmAttached_ && off;
}

bool ArduinoStatusLedDriver::write(bool on) {
  bool ok = false;
  if (on) {
    // A previous failed detach may retain peripheral ownership after GPIO off.
    // Resolve that owner before attaching a fresh output route.
    const bool routeReady = !pwmAttached_ || stopPwm();
    if (routeReady) {
      // XTAL remains stable while the e-paper owner changes CPU frequency.
      const bool clockReady = ledcGetClockSource() == LEDC_USE_XTAL_CLK ||
                              ledcSetClockSource(LEDC_USE_XTAL_CLK);
      pwmAttached_ = clockReady &&
          ledcAttach(kPin, Board::ActiveProfile::kStatusLedPwmHz,
                     Board::ActiveProfile::kStatusLedPwmBits);
    }
    ok = routeReady && pwmAttached_ && ledcWrite(kPin, kDuty);
    if (ok) {
      // LEDC latches the new duty on the next PWM cycle. Yield for two
      // cycles before reading the active duty; this runs only on transition.
      delay((2000U + Board::ActiveProfile::kStatusLedPwmHz - 1) /
            Board::ActiveProfile::kStatusLedPwmHz);
      ok = duty() == kDuty && frequencyHz() == Board::ActiveProfile::kStatusLedPwmHz;
    }
    if (!ok) stopPwm();
  } else {
    ok = stopPwm();
  }
  Serial.printf("status-led: state=%s, gpio=%d, brightness_percent=%u, "
                "duty=%lu, pwm_hz=%lu, attached=%d, level=%d, verified=%s\n",
                on ? "on" : "off", kPin,
                on && ok ? Board::ActiveProfile::kStatusLedBrightnessPercent : 0,
                static_cast<unsigned long>(duty()),
                static_cast<unsigned long>(frequencyHz()), pwmAttached_,
                StatusLedHardware::level(), ok ? "yes" : "no");
  return ok;
}
bool ArduinoStatusLedDriver::holdOff() {
  const bool ok = stopPwm() && StatusLedHardware::holdOff();
  Serial.printf("status-led: sleep_hold=off, level=%d, verified=%s\n",
                StatusLedHardware::level(), ok ? "yes" : "no");
  return ok;
}
bool ArduinoStatusLedDriver::releaseHold() {
  return StatusLedHardware::releaseHold();
}
uint32_t ArduinoStatusLedDriver::duty() const {
  return pwmAttached_ ? ledcRead(kPin) : 0;
}
uint32_t ArduinoStatusLedDriver::frequencyHz() const {
  return pwmAttached_ ? ledcReadFreq(kPin) : 0;
}
#endif
