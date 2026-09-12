#pragma once
#include <Arduino.h>

struct WifiScanNetwork {
  String ssid;
  int32_t rssi = 0;
  int32_t channel = 0;
  int encryptionType = 0;
  const char *encryption = "unknown";
  bool hidden = false;
};

class WifiScanDriver {
 public:
  virtual ~WifiScanDriver() = default;
  // -1 running, -2 failure, >=0 completed result count.
  virtual int start() = 0;
  virtual int completion() = 0;
  virtual WifiScanNetwork network(size_t index) = 0;
  virtual bool requestStop() = 0;
  virtual bool stopConfirmed() const = 0;
  virtual void clearResults() = 0;
};
