#pragma once
#include <cstdint>
struct RuntimeActionSnapshot {
  bool restartPending = false;
  bool restartFailed = false;
  bool runtimeActionPending = false;
};
class RuntimeActionScheduler {
 public:
  RuntimeActionSnapshot current;
  unsigned wifiApplyCalls = 0;
  RuntimeActionSnapshot snapshot() { return current; }
  void scheduleWifiApply(uint32_t) { ++wifiApplyCalls; }
};
