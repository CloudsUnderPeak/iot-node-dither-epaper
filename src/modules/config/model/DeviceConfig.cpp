#include "DeviceConfig.h"

#include <cstring>
#include <esp_mac.h>

#include "modules/config/storage/ConfigSchema.h"

namespace {
constexpr const char *kDefaultHostname = "esp32-device";
constexpr const char *kDefaultAdminUsername = "admin";
constexpr const char *kDefaultAdminPassword = "password";
constexpr const char *kDefaultApIpAddress = "192.168.4.1";
constexpr const char *kDefaultApIpNetmask = "255.255.255.0";

void copyDefaultApSsid(char *target, size_t targetSize, const char *hostname) {
  uint8_t mac[6] = {};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    snprintf(target, targetSize, "%s", hostname);
    return;
  }
  snprintf(target, targetSize, "%s-%02X%02X", hostname, mac[4], mac[5]);
}
}  // namespace

const char *wifiModeToString(WifiMode mode) {
  switch (mode) {
    case WifiMode::Off:
      return "WIFI_OFF";
    case WifiMode::Sta:
      return "WIFI_STA";
    case WifiMode::Ap:
      return "WIFI_AP";
    case WifiMode::ApSta:
      return "WIFI_AP_STA";
  }
  return "UNKNOWN";
}

bool wifiModeFromString(const char *value, WifiMode &mode) {
  if (strcmp(value, "WIFI_OFF") == 0) {
    mode = WifiMode::Off;
    return true;
  }
  if (strcmp(value, "WIFI_STA") == 0) {
    mode = WifiMode::Sta;
    return true;
  }
  if (strcmp(value, "WIFI_AP") == 0) {
    mode = WifiMode::Ap;
    return true;
  }
  if (strcmp(value, "WIFI_AP_STA") == 0) {
    mode = WifiMode::ApSta;
    return true;
  }
  return false;
}

const char *staSecurityToString(StaSecurity security) {
  switch (security) {
    case StaSecurity::Wpa:
      return "wpa";
    case StaSecurity::Open:
      return "open";
  }
  return "unknown";
}

bool staSecurityFromString(const char *value, StaSecurity &security) {
  if (strcmp(value, "wpa") == 0) {
    security = StaSecurity::Wpa;
    return true;
  }
  if (strcmp(value, "open") == 0) {
    security = StaSecurity::Open;
    return true;
  }
  return false;
}

const char *staIpModeToString(StaIpMode mode) {
  switch (mode) {
    case StaIpMode::Dhcp:
      return "dhcp";
    case StaIpMode::Static:
      return "static";
  }
  return "unknown";
}

bool staIpModeFromString(const char *value, StaIpMode &mode) {
  if (strcmp(value, "dhcp") == 0) {
    mode = StaIpMode::Dhcp;
    return true;
  }
  if (strcmp(value, "static") == 0) {
    mode = StaIpMode::Static;
    return true;
  }
  return false;
}

const char *apIpModeToString(ApIpMode mode) {
  switch (mode) {
    case ApIpMode::Default:
      return "default";
    case ApIpMode::Static:
      return "static";
  }
  return "unknown";
}

bool apIpModeFromString(const char *value, ApIpMode &mode) {
  if (strcmp(value, "default") == 0) {
    mode = ApIpMode::Default;
    return true;
  }
  if (strcmp(value, "static") == 0) {
    mode = ApIpMode::Static;
    return true;
  }
  return false;
}

DeviceConfig defaultDeviceConfig() {
  DeviceConfig config{};
  config.schemaVersion = kDeviceConfigSchemaVersion;
  config.wifiMode = WifiMode::Ap;
  strlcpy(config.hostname, kDefaultHostname, sizeof(config.hostname));
  config.staSsid[0] = '\0';
  config.staPassword[0] = '\0';
  config.staSecurity = StaSecurity::Wpa;
  config.staIpMode = StaIpMode::Dhcp;
  config.staIpAddress[0] = '\0';
  config.staIpGateway[0] = '\0';
  config.staIpNetmask[0] = '\0';
  config.staDns1[0] = '\0';
  config.staDns2[0] = '\0';
  copyDefaultApSsid(config.apSsid, sizeof(config.apSsid), config.hostname);
  config.apPasswordEnabled = false;
  config.apIpMode = ApIpMode::Default;
  strlcpy(config.apIpAddress, kDefaultApIpAddress, sizeof(config.apIpAddress));
  strlcpy(config.apIpNetmask, kDefaultApIpNetmask, sizeof(config.apIpNetmask));
  config.fallbackToAp = true;
  strlcpy(config.adminUsername, kDefaultAdminUsername, sizeof(config.adminUsername));
  strlcpy(config.adminPassword, kDefaultAdminPassword, sizeof(config.adminPassword));
  return config;
}
