#include "CaptivePortalDnsService.h"

namespace {
constexpr uint16_t kDnsPort = 53;
}

Result CaptivePortalDnsService::begin(const WifiStatus &wifiStatus) {
  return restart(wifiStatus);
}

Result CaptivePortalDnsService::restart(const WifiStatus &wifiStatus) {
  stop();

  if (!shouldRun(wifiStatus)) {
    return okResult();
  }

  if (!dnsServer_.start(kDnsPort, "*", wifiStatus.apIp)) {
    return networkError("failed to start captive DNS");
  }

  captiveIp_ = wifiStatus.apIp;
  running_ = true;
  return okResult();
}

void CaptivePortalDnsService::poll() {
  if (running_) {
    dnsServer_.processNextRequest();
  }
}

void CaptivePortalDnsService::stop() {
  if (running_) {
    dnsServer_.stop();
  }
  running_ = false;
  captiveIp_ = IPAddress();
}

bool CaptivePortalDnsService::running() const {
  return running_;
}

IPAddress CaptivePortalDnsService::captiveIp() const {
  return captiveIp_;
}

bool CaptivePortalDnsService::shouldRun(const WifiStatus &wifiStatus) const {
  return (wifiStatus.mode == WifiMode::Ap || wifiStatus.mode == WifiMode::ApSta) &&
         wifiStatus.apIp != IPAddress(0, 0, 0, 0);
}
