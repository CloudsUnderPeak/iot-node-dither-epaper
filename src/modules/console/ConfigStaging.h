#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "modules/config/model/DeviceConfig.h"

enum class ConsoleConfigGroup : uint8_t {
  Wifi,
  System,
  Auth,
  Unknown,
};

enum class ConsoleConfigKey : uint8_t {
  SystemHostname,
  AuthUsername,
  AuthPassword,
  AuthPasswordSet,
  WifiMode,
  WifiFallbackToAp,
  WifiStaSsid,
  WifiStaSecurity,
  WifiStaPassword,
  WifiStaPasswordSet,
  WifiStaIpMode,
  WifiStaIpAddress,
  WifiStaIpGateway,
  WifiStaIpNetmask,
  WifiStaIpDns,
  WifiApSsid,
  WifiApPasswordEnabled,
  WifiApIpMode,
  WifiApIpAddress,
  WifiApIpNetmask,
  Count,
  Unknown = 255,
};

struct ConsoleConfigKeyInfo {
  ConsoleConfigKey key = ConsoleConfigKey::Unknown;
  const char *name = "";
  ConsoleConfigGroup group = ConsoleConfigGroup::Unknown;
  bool readable = false;
  bool writable = false;
  bool secret = false;
};

struct ConsoleConfigSetResult {
  bool success = false;
  const char *message = "invalid value";
};

class ConfigStaging {
 public:
  ~ConfigStaging();

  static size_t keyCount();
  static const ConsoleConfigKeyInfo &keyInfo(size_t index);
  static ConsoleConfigKey findKey(const char *name);
  static ConsoleConfigGroup groupFromString(const char *value);
  static const char *groupName(ConsoleConfigGroup group);
  static bool keyMatchesPrefix(ConsoleConfigKey key, const char *prefix);

  ConsoleConfigSetResult set(ConsoleConfigKey key, JsonVariantConst value);
  bool dirty(ConsoleConfigKey key) const;
  bool dirty(ConsoleConfigGroup group) const;
  bool anyDirty() const;
  void apply(DeviceConfig &config) const;
  void applyGroup(ConsoleConfigGroup group, DeviceConfig &config) const;
  void revert(ConsoleConfigKey key);
  void revert(ConsoleConfigGroup group);
  void clear();

 private:
  DeviceConfig values_{};
  uint32_t dirtyBits_ = 0;

  static uint32_t keyMask(ConsoleConfigKey key);
  static void secureZero(char *value, size_t length);
  void applyKey(ConsoleConfigKey key, DeviceConfig &config) const;
  void wipeKey(ConsoleConfigKey key);
};
