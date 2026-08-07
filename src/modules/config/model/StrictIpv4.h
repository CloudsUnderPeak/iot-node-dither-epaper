#pragma once

#include <cstdint>

struct StrictIpv4Address {
  uint8_t octets[4] = {};
};

bool parseStrictIpv4(const char *value, StrictIpv4Address &parsed);
