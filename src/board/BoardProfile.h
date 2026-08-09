#pragma once

#include <cstddef>
#include <cstdint>

namespace Board {

constexpr int8_t kNoPin = -1;

enum class PinDisposition : uint8_t {
  Available,
  BoardReserved,
  Strapping,
  UsbJtag,
  Flash,
  NotExposed,
};

struct SpiRoute {
  int8_t sck = kNoPin;
  int8_t mosi = kNoPin;
  int8_t miso = kNoPin;
};

struct I2cRoute {
  int8_t sda = kNoPin;
  int8_t scl = kNoPin;
};

struct BatterySense {
  int8_t pin = kNoPin;
  uint8_t dividerNumerator = 1;
  uint8_t dividerDenominator = 1;
};

struct EpaperPins {
  int8_t cs = kNoPin;
  int8_t dc = kNoPin;
  int8_t reset = kNoPin;
  int8_t busy = kNoPin;
};

constexpr bool uniquePins(const int8_t *pins, size_t count) {
  for (size_t left = 0; left < count; ++left) {
    if (pins[left] == kNoPin) continue;
    for (size_t right = left + 1; right < count; ++right) {
      if (pins[left] == pins[right]) return false;
    }
  }
  return true;
}

}  // namespace Board

#include "profiles/FireBeetle2Esp32C6Profile.h"

namespace Board {
using ActiveProfile = FireBeetle2Esp32C6Profile;
}  // namespace Board
