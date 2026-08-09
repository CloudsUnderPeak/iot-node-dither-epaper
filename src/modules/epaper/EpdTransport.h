#pragma once

#include <cstddef>
#include <cstdint>

class EpdTransport {
 public:
  virtual ~EpdTransport() = default;

  virtual bool ready() const = 0;
  virtual bool setReset(bool high) = 0;
  virtual bool busyHigh() const = 0;
  virtual bool writeCommand(uint8_t command) = 0;
  virtual bool writeData(const uint8_t *data, size_t length) = 0;
  virtual void logicalQuiesce() = 0;
  virtual void delayMs(uint32_t durationMs) = 0;
  virtual void yieldCpu() = 0;
  virtual uint32_t nowMs() const = 0;
};
