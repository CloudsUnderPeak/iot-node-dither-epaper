#pragma once

#include <Arduino.h>

class RuntimeActionScheduler {
 public:
  bool available = false;
  unsigned wifiApplyCount = 0;
  unsigned resetCount = 0;
  uint32_t lastWifiApplyDelayMs = 0;
  uint32_t lastResetDelayMs = 0;

  bool ready() const { return available; }
  void scheduleWifiApply(uint32_t delayMs) {
    ++wifiApplyCount;
    lastWifiApplyDelayMs = delayMs;
  }
  void scheduleSystemReset(uint32_t delayMs) {
    ++resetCount;
    lastResetDelayMs = delayMs;
  }
};
