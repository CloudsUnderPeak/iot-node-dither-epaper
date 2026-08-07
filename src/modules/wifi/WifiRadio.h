#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../core/Result.h"

// Serializes access to the Arduino WiFi driver across the HTTP and loop tasks.
class WifiRadio {
 public:
  Result begin();
  bool ready() const;
  bool lock(TickType_t timeoutTicks);
  void unlock();

 private:
  SemaphoreHandle_t mutex_ = nullptr;
};

class WifiRadioGuard {
 public:
  WifiRadioGuard(WifiRadio *radio, TickType_t timeoutTicks);
  ~WifiRadioGuard();

  bool locked() const { return locked_; }

 private:
  WifiRadio *radio_;
  bool locked_;
};
