#pragma once

#include <cstdint>
#include <cstdio>

#include "Arduino.h"

class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : value_((static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(c) << 8) |
               static_cast<uint32_t>(d)) {}

  bool fromString(const char *value) {
    unsigned a = 0;
    unsigned b = 0;
    unsigned c = 0;
    unsigned d = 0;
    char trailing = '\0';
    if (value == nullptr ||
        std::sscanf(value, "%u.%u.%u.%u%c", &a, &b, &c, &d, &trailing) != 4 ||
        a > 255 || b > 255 || c > 255 || d > 255) {
      value_ = 0;
      return false;
    }
    value_ = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
  }

  String toString() const {
    char text[16]{};
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u",
                  static_cast<unsigned>((value_ >> 24) & 0xff),
                  static_cast<unsigned>((value_ >> 16) & 0xff),
                  static_cast<unsigned>((value_ >> 8) & 0xff),
                  static_cast<unsigned>(value_ & 0xff));
    return String(text);
  }

  bool operator==(const IPAddress &other) const {
    return value_ == other.value_;
  }

  bool operator!=(const IPAddress &other) const {
    return !(*this == other);
  }

 private:
  uint32_t value_ = 0;
};
