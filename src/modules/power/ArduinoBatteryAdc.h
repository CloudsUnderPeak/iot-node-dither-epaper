#pragma once

#include "BatteryMonitor.h"

class ArduinoBatteryAdc final : public BatteryAdc {
 public:
  bool begin(uint8_t pin) override;
  bool readMilliVolts(uint8_t pin, uint32_t &milliVolts) override;
};
