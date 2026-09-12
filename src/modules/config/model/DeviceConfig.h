#pragma once

#include <Arduino.h>

#include "core/Result.h"

enum class WifiMode : uint8_t {
  Off = 0,
  Sta = 1,
  Ap = 2,
  ApSta = 3,
};

enum class StaSecurity : uint8_t {
  Wpa = 0,
  Open = 1,
};

enum class StaIpMode : uint8_t {
  Dhcp = 0,
  Static = 1,
};

enum class ApIpMode : uint8_t {
  Default = 0,
  Static = 1,
};

#define DEVICE_CONFIG_HOSTNAME_SIZE 32
#define DEVICE_CONFIG_SSID_SIZE 33
#define DEVICE_CONFIG_PASSWORD_SIZE 65
#define DEVICE_CONFIG_IPV4_SIZE 16
#define DEVICE_CONFIG_ADMIN_USERNAME_SIZE DEVICE_CONFIG_SSID_SIZE

constexpr uint8_t kDefaultWifiTxDbm = 15;
constexpr uint8_t kMinWifiTxDbm = 2;
constexpr uint8_t kMaxWifiTxDbm = 20;

// Persistent user-facing settings. Keep this structure compact because it is
// mirrored into NVS through PreferencesConfigStore.
struct DeviceConfig {
  uint16_t schemaVersion;
  WifiMode wifiMode;
  uint8_t wifiTxDbm;
  char hostname[DEVICE_CONFIG_HOSTNAME_SIZE];
  // Wi-Fi credentials are stored locally on the device and must not be emitted
  // through REST responses or reusable documentation.
  char staSsid[DEVICE_CONFIG_SSID_SIZE];
  char staPassword[DEVICE_CONFIG_PASSWORD_SIZE];
  StaSecurity staSecurity;
  StaIpMode staIpMode;
  char staIpAddress[DEVICE_CONFIG_IPV4_SIZE];
  char staIpGateway[DEVICE_CONFIG_IPV4_SIZE];
  char staIpNetmask[DEVICE_CONFIG_IPV4_SIZE];
  char staDns1[DEVICE_CONFIG_IPV4_SIZE];
  char staDns2[DEVICE_CONFIG_IPV4_SIZE];
  char apSsid[DEVICE_CONFIG_SSID_SIZE];
  bool apPasswordEnabled;
  ApIpMode apIpMode;
  char apIpAddress[DEVICE_CONFIG_IPV4_SIZE];
  char apIpNetmask[DEVICE_CONFIG_IPV4_SIZE];
  bool fallbackToAp;
  char adminUsername[DEVICE_CONFIG_ADMIN_USERNAME_SIZE];
  char adminPassword[DEVICE_CONFIG_PASSWORD_SIZE];
};

enum class DeviceConfigField : uint8_t {
  None,
  Schema,
  Hostname,
  WifiMode,
  WifiTxDbm,
  StaSsid,
  StaPassword,
  StaSecurity,
  StaIpMode,
  StaIpConfig,
  StaIpAddress,
  StaIpGateway,
  StaIpNetmask,
  StaDns,
  ApSsid,
  ApIpMode,
  ApIpAddress,
  ApIpNetmask,
  Interfaces,
  AdminUsername,
  AdminPassword,
};

struct DeviceConfigValidation {
  Result result;
  DeviceConfigField field = DeviceConfigField::None;

  bool ok() const { return result.ok(); }
};

const char *wifiModeToString(WifiMode mode);
bool wifiModeFromString(const char *value, WifiMode &mode);
const char *staSecurityToString(StaSecurity security);
bool staSecurityFromString(const char *value, StaSecurity &security);
const char *staIpModeToString(StaIpMode mode);
bool staIpModeFromString(const char *value, StaIpMode &mode);
const char *apIpModeToString(ApIpMode mode);
bool apIpModeFromString(const char *value, ApIpMode &mode);
DeviceConfig defaultDeviceConfig();
Result validateHostnameValue(const char *value);
Result validateWifiTxDbmValue(uint8_t value);
Result validateStaSsidValue(const char *value, bool required);
Result validateApSsidValue(const char *value, bool required);
Result validateStaPasswordValue(const char *value);
Result validateIpv4AddressOptionalValue(const char *value);
Result validateIpv4NetmaskOptionalValue(const char *value);
Result validateAdminUsernameValue(const char *value);
Result validateAdminPasswordValue(const char *value);
bool ipv4SubnetsOverlap(const char *leftAddress,
                        const char *leftNetmask,
                        const char *rightAddress,
                        const char *rightNetmask);
// Validates cross-field requirements before a config is persisted or applied.
Result validateDeviceConfig(const DeviceConfig &config);
DeviceConfigValidation validateDeviceConfigDetailed(const DeviceConfig &config);
