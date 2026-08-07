#include "ConfigStaging.h"

#include <cstring>

namespace {
constexpr ConsoleConfigKeyInfo kKeys[] = {
    {ConsoleConfigKey::SystemHostname, "system.hostname", ConsoleConfigGroup::System, true, true, false},
    {ConsoleConfigKey::AuthUsername, "auth.username", ConsoleConfigGroup::Auth, true, false, false},
    {ConsoleConfigKey::AuthPassword, "auth.password", ConsoleConfigGroup::Auth, false, true, true},
    {ConsoleConfigKey::AuthPasswordSet, "auth.password_set", ConsoleConfigGroup::Auth, true, false, false},
    {ConsoleConfigKey::WifiMode, "wifi.mode", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiFallbackToAp, "wifi.fallback_to_ap", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiStaSsid, "wifi.sta.ssid", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiStaSecurity, "wifi.sta.security", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiStaPassword, "wifi.sta.password", ConsoleConfigGroup::Wifi, false, true, true},
    {ConsoleConfigKey::WifiStaPasswordSet, "wifi.sta.password_set", ConsoleConfigGroup::Wifi, true, false, false},
    {ConsoleConfigKey::WifiStaIpMode, "wifi.sta.ip_config.mode", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiStaIpAddress, "wifi.sta.ip_config.address", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiStaIpGateway, "wifi.sta.ip_config.gateway", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiStaIpNetmask, "wifi.sta.ip_config.netmask", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiStaIpDns, "wifi.sta.ip_config.dns", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiApSsid, "wifi.ap.ssid", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiApPasswordEnabled, "wifi.ap.password_enabled", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiApIpMode, "wifi.ap.ip_config.mode", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiApIpAddress, "wifi.ap.ip_config.address", ConsoleConfigGroup::Wifi, true, true, false},
    {ConsoleConfigKey::WifiApIpNetmask, "wifi.ap.ip_config.netmask", ConsoleConfigGroup::Wifi, true, true, false},
};

bool printableAscii(const char *value) {
  if (value == nullptr) return false;
  for (const unsigned char *cursor = reinterpret_cast<const unsigned char *>(value);
       *cursor != '\0'; ++cursor) {
    if (*cursor < 0x20 || *cursor > 0x7e) return false;
  }
  return true;
}

ConsoleConfigSetResult copyString(const char *value,
                                  char *target,
                                  size_t targetSize,
                                  Result (*validator)(const char *)) {
  if (value == nullptr || !printableAscii(value) || strlen(value) >= targetSize) {
    return {false, "value is too long or contains unsupported characters"};
  }
  if (value[0] != '\0') {
    const Result validation = validator(value);
    if (!validation.ok()) return {false, validation.message};
  }
  strlcpy(target, value, targetSize);
  return {true, "ok"};
}

}  // namespace

ConfigStaging::~ConfigStaging() {
  clear();
}

size_t ConfigStaging::keyCount() {
  return sizeof(kKeys) / sizeof(kKeys[0]);
}

const ConsoleConfigKeyInfo &ConfigStaging::keyInfo(size_t index) {
  static const ConsoleConfigKeyInfo kUnknown{};
  return index < keyCount() ? kKeys[index] : kUnknown;
}

ConsoleConfigKey ConfigStaging::findKey(const char *name) {
  if (name == nullptr) return ConsoleConfigKey::Unknown;
  for (const ConsoleConfigKeyInfo &info : kKeys) {
    if (strcmp(name, info.name) == 0) return info.key;
  }
  return ConsoleConfigKey::Unknown;
}

ConsoleConfigGroup ConfigStaging::groupFromString(const char *value) {
  if (value == nullptr) return ConsoleConfigGroup::Unknown;
  if (strcmp(value, "wifi") == 0) return ConsoleConfigGroup::Wifi;
  if (strcmp(value, "system") == 0) return ConsoleConfigGroup::System;
  if (strcmp(value, "auth") == 0) return ConsoleConfigGroup::Auth;
  return ConsoleConfigGroup::Unknown;
}

const char *ConfigStaging::groupName(ConsoleConfigGroup group) {
  switch (group) {
    case ConsoleConfigGroup::Wifi: return "wifi";
    case ConsoleConfigGroup::System: return "system";
    case ConsoleConfigGroup::Auth: return "auth";
    case ConsoleConfigGroup::Unknown: return "unknown";
  }
  return "unknown";
}

bool ConfigStaging::keyMatchesPrefix(ConsoleConfigKey key, const char *prefix) {
  if (prefix == nullptr || prefix[0] == '\0') return true;
  const ConsoleConfigKeyInfo &info = keyInfo(static_cast<size_t>(key));
  const size_t prefixLength = strlen(prefix);
  return strncmp(info.name, prefix, prefixLength) == 0 &&
         (info.name[prefixLength] == '\0' || info.name[prefixLength] == '.');
}

ConsoleConfigSetResult ConfigStaging::set(ConsoleConfigKey key,
                                          JsonVariantConst value) {
  if (key == ConsoleConfigKey::Unknown ||
      !keyInfo(static_cast<size_t>(key)).writable) {
    return {false, key == ConsoleConfigKey::Unknown ? "unknown key" : "key is read-only"};
  }

  ConsoleConfigSetResult result{false, "value has the wrong JSON type"};
  switch (key) {
    case ConsoleConfigKey::SystemHostname:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.hostname,
                            sizeof(values_.hostname), validateHostnameValue);
      }
      break;
    case ConsoleConfigKey::AuthPassword:
      if (value.is<const char *>()) {
        char candidate[sizeof(values_.adminPassword)]{};
        result = copyString(value.as<const char *>(), candidate,
                            sizeof(candidate), validateAdminPasswordValue);
        if (result.success) {
          wipeKey(key);
          strlcpy(values_.adminPassword, candidate, sizeof(values_.adminPassword));
        }
        secureZero(candidate, sizeof(candidate));
      }
      break;
    case ConsoleConfigKey::WifiMode:
      if (value.is<const char *>()) {
        const char *mode = value.as<const char *>();
        if (strcmp(mode, "sta") == 0) {
          values_.wifiMode = WifiMode::Sta;
          result = {true, "ok"};
        } else if (strcmp(mode, "ap") == 0) {
          values_.wifiMode = WifiMode::Ap;
          result = {true, "ok"};
        } else if (strcmp(mode, "ap_sta") == 0) {
          values_.wifiMode = WifiMode::ApSta;
          result = {true, "ok"};
        } else {
          result = {false, "value must be sta, ap, or ap_sta"};
        }
      }
      break;
    case ConsoleConfigKey::WifiFallbackToAp:
      if (value.is<bool>()) {
        values_.fallbackToAp = value.as<bool>();
        result = {true, "ok"};
      }
      break;
    case ConsoleConfigKey::WifiStaSsid:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.staSsid,
                            sizeof(values_.staSsid),
                            [](const char *text) { return validateStaSsidValue(text, false); });
      }
      break;
    case ConsoleConfigKey::WifiStaSecurity:
      if (value.is<const char *>() &&
          staSecurityFromString(value.as<const char *>(), values_.staSecurity)) {
        result = {true, "ok"};
      } else if (value.is<const char *>()) {
        result = {false, "value must be wpa or open"};
      }
      break;
    case ConsoleConfigKey::WifiStaPassword:
      if (value.is<const char *>()) {
        char candidate[sizeof(values_.staPassword)]{};
        result = copyString(value.as<const char *>(), candidate,
                            sizeof(candidate), validateStaPasswordValue);
        if (result.success) {
          wipeKey(key);
          strlcpy(values_.staPassword, candidate, sizeof(values_.staPassword));
        }
        secureZero(candidate, sizeof(candidate));
      }
      break;
    case ConsoleConfigKey::WifiStaIpMode:
      if (value.is<const char *>() &&
          staIpModeFromString(value.as<const char *>(), values_.staIpMode)) {
        result = {true, "ok"};
      } else if (value.is<const char *>()) {
        result = {false, "value must be dhcp or static"};
      }
      break;
    case ConsoleConfigKey::WifiStaIpAddress:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.staIpAddress,
                            sizeof(values_.staIpAddress), validateIpv4AddressOptionalValue);
      }
      break;
    case ConsoleConfigKey::WifiStaIpGateway:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.staIpGateway,
                            sizeof(values_.staIpGateway), validateIpv4AddressOptionalValue);
      }
      break;
    case ConsoleConfigKey::WifiStaIpNetmask:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.staIpNetmask,
                            sizeof(values_.staIpNetmask), validateIpv4NetmaskOptionalValue);
      }
      break;
    case ConsoleConfigKey::WifiStaIpDns:
      if (value.is<JsonArrayConst>()) {
        JsonArrayConst dns = value.as<JsonArrayConst>();
        if (dns.size() > 2) {
          result = {false, "DNS accepts at most two addresses"};
          break;
        }
        char dns1[DEVICE_CONFIG_IPV4_SIZE]{};
        char dns2[DEVICE_CONFIG_IPV4_SIZE]{};
        size_t index = 0;
        result = {true, "ok"};
        for (JsonVariantConst item : dns) {
          if (!item.is<const char *>()) {
            result = {false, "DNS values must be strings"};
            break;
          }
          char *target = index++ == 0 ? dns1 : dns2;
          result = copyString(item.as<const char *>(), target,
                              DEVICE_CONFIG_IPV4_SIZE, validateIpv4AddressOptionalValue);
          if (!result.success) break;
        }
        if (result.success) {
          strlcpy(values_.staDns1, dns1, sizeof(values_.staDns1));
          strlcpy(values_.staDns2, dns2, sizeof(values_.staDns2));
        }
      }
      break;
    case ConsoleConfigKey::WifiApSsid:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.apSsid,
                            sizeof(values_.apSsid),
                            [](const char *text) { return validateApSsidValue(text, false); });
      }
      break;
    case ConsoleConfigKey::WifiApPasswordEnabled:
      if (value.is<bool>()) {
        values_.apPasswordEnabled = value.as<bool>();
        result = {true, "ok"};
      }
      break;
    case ConsoleConfigKey::WifiApIpMode:
      if (value.is<const char *>() &&
          apIpModeFromString(value.as<const char *>(), values_.apIpMode)) {
        result = {true, "ok"};
      } else if (value.is<const char *>()) {
        result = {false, "value must be default or static"};
      }
      break;
    case ConsoleConfigKey::WifiApIpAddress:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.apIpAddress,
                            sizeof(values_.apIpAddress), validateIpv4AddressOptionalValue);
      }
      break;
    case ConsoleConfigKey::WifiApIpNetmask:
      if (value.is<const char *>()) {
        result = copyString(value.as<const char *>(), values_.apIpNetmask,
                            sizeof(values_.apIpNetmask), validateIpv4NetmaskOptionalValue);
      }
      break;
    case ConsoleConfigKey::AuthUsername:
    case ConsoleConfigKey::AuthPasswordSet:
    case ConsoleConfigKey::WifiStaPasswordSet:
    case ConsoleConfigKey::Count:
    case ConsoleConfigKey::Unknown:
      break;
  }

  if (result.success) dirtyBits_ |= keyMask(key);
  return result;
}

bool ConfigStaging::dirty(ConsoleConfigKey key) const {
  return key != ConsoleConfigKey::Unknown && (dirtyBits_ & keyMask(key)) != 0;
}

bool ConfigStaging::dirty(ConsoleConfigGroup group) const {
  for (const ConsoleConfigKeyInfo &info : kKeys) {
    if (info.group == group && dirty(info.key)) return true;
  }
  return false;
}

bool ConfigStaging::anyDirty() const {
  return dirtyBits_ != 0;
}

void ConfigStaging::apply(DeviceConfig &config) const {
  for (const ConsoleConfigKeyInfo &info : kKeys) {
    if (dirty(info.key)) applyKey(info.key, config);
  }
}

void ConfigStaging::applyGroup(ConsoleConfigGroup group,
                               DeviceConfig &config) const {
  for (const ConsoleConfigKeyInfo &info : kKeys) {
    if (info.group == group && dirty(info.key)) applyKey(info.key, config);
  }
}

void ConfigStaging::revert(ConsoleConfigKey key) {
  if (!dirty(key)) return;
  wipeKey(key);
  dirtyBits_ &= ~keyMask(key);
}

void ConfigStaging::revert(ConsoleConfigGroup group) {
  for (const ConsoleConfigKeyInfo &info : kKeys) {
    if (info.group == group) revert(info.key);
  }
}

void ConfigStaging::clear() {
  for (const ConsoleConfigKeyInfo &info : kKeys) wipeKey(info.key);
  dirtyBits_ = 0;
}

uint32_t ConfigStaging::keyMask(ConsoleConfigKey key) {
  return 1UL << static_cast<uint8_t>(key);
}

void ConfigStaging::secureZero(char *value, size_t length) {
  volatile char *cursor = value;
  while (length-- > 0) *cursor++ = '\0';
}

void ConfigStaging::applyKey(ConsoleConfigKey key, DeviceConfig &config) const {
  switch (key) {
    case ConsoleConfigKey::SystemHostname: strlcpy(config.hostname, values_.hostname, sizeof(config.hostname)); break;
    case ConsoleConfigKey::AuthPassword: strlcpy(config.adminPassword, values_.adminPassword, sizeof(config.adminPassword)); break;
    case ConsoleConfigKey::WifiMode: config.wifiMode = values_.wifiMode; break;
    case ConsoleConfigKey::WifiFallbackToAp: config.fallbackToAp = values_.fallbackToAp; break;
    case ConsoleConfigKey::WifiStaSsid: strlcpy(config.staSsid, values_.staSsid, sizeof(config.staSsid)); break;
    case ConsoleConfigKey::WifiStaSecurity: config.staSecurity = values_.staSecurity; break;
    case ConsoleConfigKey::WifiStaPassword: strlcpy(config.staPassword, values_.staPassword, sizeof(config.staPassword)); break;
    case ConsoleConfigKey::WifiStaIpMode: config.staIpMode = values_.staIpMode; break;
    case ConsoleConfigKey::WifiStaIpAddress: strlcpy(config.staIpAddress, values_.staIpAddress, sizeof(config.staIpAddress)); break;
    case ConsoleConfigKey::WifiStaIpGateway: strlcpy(config.staIpGateway, values_.staIpGateway, sizeof(config.staIpGateway)); break;
    case ConsoleConfigKey::WifiStaIpNetmask: strlcpy(config.staIpNetmask, values_.staIpNetmask, sizeof(config.staIpNetmask)); break;
    case ConsoleConfigKey::WifiStaIpDns:
      strlcpy(config.staDns1, values_.staDns1, sizeof(config.staDns1));
      strlcpy(config.staDns2, values_.staDns2, sizeof(config.staDns2));
      break;
    case ConsoleConfigKey::WifiApSsid: strlcpy(config.apSsid, values_.apSsid, sizeof(config.apSsid)); break;
    case ConsoleConfigKey::WifiApPasswordEnabled: config.apPasswordEnabled = values_.apPasswordEnabled; break;
    case ConsoleConfigKey::WifiApIpMode: config.apIpMode = values_.apIpMode; break;
    case ConsoleConfigKey::WifiApIpAddress: strlcpy(config.apIpAddress, values_.apIpAddress, sizeof(config.apIpAddress)); break;
    case ConsoleConfigKey::WifiApIpNetmask: strlcpy(config.apIpNetmask, values_.apIpNetmask, sizeof(config.apIpNetmask)); break;
    case ConsoleConfigKey::AuthUsername:
    case ConsoleConfigKey::AuthPasswordSet:
    case ConsoleConfigKey::WifiStaPasswordSet:
    case ConsoleConfigKey::Count:
    case ConsoleConfigKey::Unknown:
      break;
  }
}

void ConfigStaging::wipeKey(ConsoleConfigKey key) {
  if (key == ConsoleConfigKey::AuthPassword) {
    secureZero(values_.adminPassword, sizeof(values_.adminPassword));
  } else if (key == ConsoleConfigKey::WifiStaPassword) {
    secureZero(values_.staPassword, sizeof(values_.staPassword));
  }
}
