#pragma once

#include "CpuFrequencyGuard.h"

class ArduinoCpuFrequencyDriver final : public CpuFrequencyDriver {
 public:
  uint32_t currentMhz() const override;
  bool setMhz(uint32_t mhz) override;
};
