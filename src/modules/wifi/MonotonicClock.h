#pragma once

#include <cstdint>

class MonotonicClock {
 public:
  virtual ~MonotonicClock() = default;
  virtual uint32_t nowMs() const = 0;
};
