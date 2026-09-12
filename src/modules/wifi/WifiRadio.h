#pragma once

#include <freertos/FreeRTOS.h>
#include <atomic>
#include <freertos/semphr.h>

#include "../../core/Result.h"

// Serializes access to the Arduino WiFi driver across the HTTP and loop tasks.
class WifiRadio {
 public:
  Result begin();
  bool ready() const;
  bool lock(TickType_t timeoutTicks);
  void unlock();
  bool scanReserved() const { return scanReserved_.load(); }
  // Only scanner owner calls these, while holding the physical radio mutex.
  void reserveScanLocked() { scanReserved_.store(true); }
  void releaseScanLocked() { scanReserved_.store(false); }
  bool lockScan(TickType_t timeoutTicks);

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  std::atomic<bool> scanReserved_{false};
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
