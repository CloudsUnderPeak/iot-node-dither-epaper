#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../core/Result.h"
#include "WifiRadio.h"

constexpr size_t kWifiScanMaxResults = 20;
constexpr int32_t kWifiScanMinRssi = -75;

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

// Single owner for scan serialization, cooldown, result selection and
// encryption mapping shared by REST and the human console.
class WifiScanner {
 public:
  Result begin(WifiRadio *radio);
  WifiScanResult scan();

 private:
  static constexpr uint32_t kMinIntervalMs = 10000;

  SemaphoreHandle_t mutex_ = nullptr;
  WifiRadio *radio_ = nullptr;
  bool hasRun_ = false;
  uint32_t lastCompletedMs_ = 0;
};
