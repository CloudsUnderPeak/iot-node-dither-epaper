#pragma once

#include <Arduino.h>

#include "core/Result.h"

constexpr size_t kWifiScanMaxResults = 20;

struct WifiScanNetwork {
  String ssid;
  int32_t rssi = 0;
  int32_t channel = 0;
  int encryptionType = 0;
  const char *encryption = "unknown";
  bool hidden = false;
};

struct WifiScanResult {
  Result result = okResult();
  WifiScanNetwork networks[kWifiScanMaxResults];
  size_t count = 0;
  uint32_t retryAfterSeconds = 0;
};

class WifiScanner {
 public:
  WifiScanResult value;
  unsigned scanCount = 0;
  WifiScanResult scan() {
    ++scanCount;
    return value;
  }
};
