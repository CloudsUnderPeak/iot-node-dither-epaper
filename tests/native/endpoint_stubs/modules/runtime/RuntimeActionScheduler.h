#pragma once

#include <Arduino.h>

struct RuntimeActionSnapshot {
  bool restartPending = false;
  bool restartFailed = false;
};

class RuntimeActionScheduler {
 public:
  bool available = false;
  unsigned wifiApplyCount = 0;
  unsigned resetCount = 0;
  uint32_t lastWifiApplyDelayMs = 0;
  uint32_t lastResetDelayMs = 0;
  RuntimeActionSnapshot current;

  bool ready() const { return available; }
  void scheduleWifiApply(uint32_t delayMs) {
    ++wifiApplyCount;
    lastWifiApplyDelayMs = delayMs;
  }
  void scheduleSystemReset(uint32_t delayMs) {
    ++resetCount;
    lastResetDelayMs = delayMs;
  }
  RuntimeActionSnapshot snapshot() { return current; }
};
