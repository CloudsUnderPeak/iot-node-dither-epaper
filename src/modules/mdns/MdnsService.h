#pragma once

#include <Arduino.h>

#include "../../core/Result.h"
#include "../config/model/DeviceConfig.h"
#include "../wifi/WifiManager.h"

// Owns the Arduino ESPmDNS responder lifecycle. mDNS is only started when the
// station interface is connected because .local names are useful on the LAN.
class MdnsService {
 public:
  Result begin(const DeviceConfig &config, const WifiStatus &wifiStatus);
  Result restart(const DeviceConfig &config, const WifiStatus &wifiStatus);
  void stop();
  bool running() const;
  const char *hostName() const;

 private:
  bool running_ = false;
  char hostName_[32] = "";

  bool shouldRun(const WifiStatus &wifiStatus) const;
  bool isValidHostName(const char *hostName) const;
};
