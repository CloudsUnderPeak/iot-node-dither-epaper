#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>

#include "core/Result.h"

class BatteryAdc {
 public:
  virtual ~BatteryAdc() = default;
  virtual bool begin(uint8_t pin) = 0;
  virtual bool readMilliVolts(uint8_t pin, uint32_t &milliVolts) = 0;
};

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
  static constexpr uint32_t kSampleIntervalMs = 10000;
  static constexpr size_t kSamplesPerReading = 7;

  Result begin(BatteryAdc *adc, uint32_t nowMs);
  void poll(uint32_t nowMs);
  bool ready() const;
  BatterySnapshot snapshot(uint32_t nowMs) const;

  static BatteryEstimate estimatePercent(uint32_t batteryMilliVolts);
  static uint32_t median(uint32_t *values, size_t count);

 private:
  bool sample(uint32_t nowMs);

  BatteryAdc *adc_ = nullptr;
  mutable portMUX_TYPE snapshotMux_ = portMUX_INITIALIZER_UNLOCKED;
  bool ready_ = false;
  bool sampleValid_ = false;
  uint32_t voltageMilliVolts_ = 0;
  uint32_t sampledAtMs_ = 0;
  uint32_t lastAttemptMs_ = 0;
};
