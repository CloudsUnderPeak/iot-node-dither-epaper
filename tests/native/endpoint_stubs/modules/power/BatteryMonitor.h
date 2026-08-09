#pragma once

#include <cstdint>

struct BatteryEstimate {
  bool available = false;
  uint8_t percent = 0;
};

struct BatterySnapshot {
  bool sampleValid = false;
  uint32_t voltageMilliVolts = 0;
  BatteryEstimate estimate;
  uint32_t sampleAgeMs = 0;
};

class BatteryMonitor {
 public:
  BatterySnapshot current;

  BatterySnapshot snapshot(uint32_t) const {
    return current;
  }
};
