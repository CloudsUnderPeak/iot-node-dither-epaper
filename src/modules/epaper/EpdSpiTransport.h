#pragma once

#include "EpdTransport.h"
#include "../hardware/EpaperHardware.h"
#include "../hardware/SpiBus.h"

class EpdSpiTransport final : public EpdTransport {
 public:
  Result begin(SpiBus *bus, PinRegistry *pins);

  bool ready() const override;
  bool setReset(bool high) override;
  bool busyHigh() const override;
  bool writeCommand(uint8_t command) override;
  bool writeData(const uint8_t *data, size_t length) override;
  void logicalQuiesce() override;
  void delayMs(uint32_t durationMs) override;
  void yieldCpu() override;
  uint32_t nowMs() const override;

 private:
  SpiBus *bus_ = nullptr;
  SpiDeviceConfig device_{};
};
