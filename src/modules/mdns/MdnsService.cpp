#include "MdnsService.h"

#include <ESPmDNS.h>

Result MdnsService::begin(const DeviceConfig &config, const WifiStatus &wifiStatus) {
  return restart(config, wifiStatus);
}

Result MdnsService::restart(const DeviceConfig &config, const WifiStatus &wifiStatus) {
  stop();

  if (!shouldRun(wifiStatus)) {
    return okResult();
  }

  if (!isValidHostName(config.hostname)) {
    return invalidInput("invalid mDNS hostname");
  }

  if (!MDNS.begin(config.hostname)) {
    return networkError("failed to start mDNS");
  }

  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "path", "/");
  strlcpy(hostName_, config.hostname, sizeof(hostName_));
  running_ = true;
  return okResult();
}

void MdnsService::stop() {
  if (running_) {
    MDNS.end();
  }
  running_ = false;
  hostName_[0] = '\0';
}

bool MdnsService::running() const {
  return running_;
}

const char *MdnsService::hostName() const {
  return hostName_;
}

bool MdnsService::shouldRun(const WifiStatus &wifiStatus) const {
  return (wifiStatus.mode == WifiMode::Sta || wifiStatus.mode == WifiMode::ApSta) &&
         wifiStatus.staState == WifiLinkState::Connected;
}

bool MdnsService::isValidHostName(const char *hostName) const {
  if (hostName == nullptr || hostName[0] == '\0') {
    return false;
  }

  for (const char *cursor = hostName; *cursor != '\0'; ++cursor) {
    const char c = *cursor;
    const bool allowed = isalnum(static_cast<unsigned char>(c)) || c == '-';
    if (!allowed) {
      return false;
    }
  }

  return hostName[0] != '-' && hostName[strlen(hostName) - 1] != '-';
}
