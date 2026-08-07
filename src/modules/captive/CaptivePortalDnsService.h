#pragma once

#include <Arduino.h>
#include <DNSServer.h>

#include "../../core/Result.h"
#include "../wifi/WifiManager.h"

// Answers DNS queries from SoftAP clients with the SoftAP IP. This is only
// active while a setup AP is actually running, so normal STA LAN DNS is never
// hijacked.
class CaptivePortalDnsService {
 public:
  Result begin(const WifiStatus &wifiStatus);
  Result restart(const WifiStatus &wifiStatus);
  void poll();
  void stop();
  bool running() const;
  IPAddress captiveIp() const;

 private:
  DNSServer dnsServer_;
  bool running_ = false;
  IPAddress captiveIp_;

  bool shouldRun(const WifiStatus &wifiStatus) const;
};
