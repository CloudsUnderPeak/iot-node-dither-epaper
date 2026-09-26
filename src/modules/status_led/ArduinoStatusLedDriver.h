#pragma once
#include <cstdint>
#include "StatusLed.h"
#if IOT_FEATURE_STATUS_LED
class ArduinoStatusLedDriver final : public StatusLedDriver {
 public:
  bool write(bool on) override;
  bool holdOff() override;
  bool releaseHold() override;
  uint32_t duty() const;
  uint32_t frequencyHz() const;
  bool pwmAttached() const { return pwmAttached_; }

 private:
  bool stopPwm();
  bool pwmAttached_ = false;
};
#endif
