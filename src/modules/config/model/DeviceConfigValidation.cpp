#include "DeviceConfig.h"

#include <cstring>

#include "modules/config/storage/ConfigSchema.h"
#include "StrictIpv4.h"

namespace {
constexpr const char *kDefaultAdminUsername = "admin";
constexpr const char *kDefaultApIpAddress = "192.168.4.1";
constexpr const char *kDefaultApIpNetmask = "255.255.255.0";

bool isAlphaNumeric(char value) {
  return (value >= '0' && value <= '9') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z');
}

bool isPrintableAscii(const char *value) {
  for (const char *cursor = value; *cursor != '\0'; ++cursor) {
    const unsigned char current = static_cast<unsigned char>(*cursor);
    if (current < 0x20 || current > 0x7e) return false;
  }
  return true;
}

Result validateSsidValue(const char *value,
                         bool required,
                         const char *emptyMessage,
                         const char *tooLongMessage,
                         const char *unsupportedMessage) {
  const size_t length = strlen(value);
  if (length == 0) return required ? invalidInput(emptyMessage) : okResult();
  if (length > 32) return invalidInput(tooLongMessage);
  if (!isPrintableAscii(value)) return invalidInput(unsupportedMessage);
  return okResult();
}

Result validatePasswordValue(const char *value,
                             const char *tooShortMessage,
                             const char *tooLongMessage,
                             const char *unsupportedMessage) {
  const size_t length = strlen(value);
  if (length == 0) return okResult();
  if (length < 8) return invalidInput(tooShortMessage);
  if (length > 63) return invalidInput(tooLongMessage);
  if (!isPrintableAscii(value)) return invalidInput(unsupportedMessage);
  return okResult();
}

bool parseIpv4(const char *value, StrictIpv4Address &parsed) {
  return parseStrictIpv4(value, parsed);
}

uint32_t ipv4ToUint32(const StrictIpv4Address &address) {
  return (static_cast<uint32_t>(address.octets[0]) << 24) |
         (static_cast<uint32_t>(address.octets[1]) << 16) |
         (static_cast<uint32_t>(address.octets[2]) << 8) |
         static_cast<uint32_t>(address.octets[3]);
}

bool isUsableNetmask(const StrictIpv4Address &netmask) {
  const uint32_t mask = ipv4ToUint32(netmask);
  const uint32_t hostMask = ~mask;
  return mask != 0 && mask != 0xffffffffU && hostMask >= 3 &&
         (hostMask & (hostMask + 1U)) == 0;
}

uint8_t netmaskPrefixLength(const StrictIpv4Address &netmask) {
  uint32_t mask = ipv4ToUint32(netmask);
  uint8_t prefixLength = 0;
  while ((mask & 0x80000000U) != 0) {
    ++prefixLength;
    mask <<= 1;
  }
  return prefixLength;
}

bool isUsableHost(const StrictIpv4Address &address, const StrictIpv4Address &netmask) {
  const uint32_t ip = ipv4ToUint32(address);
  const uint32_t mask = ipv4ToUint32(netmask);
  const uint32_t network = ip & mask;
  const uint32_t broadcast = network | ~mask;
  return ip != 0 && ip != 0xffffffffU && ip != network && ip != broadcast;
}

DeviceConfigValidation validateStaticStaIpv4(const DeviceConfig &config) {
  StrictIpv4Address address;
  StrictIpv4Address gateway;
  StrictIpv4Address netmask;
  if (!parseIpv4(config.staIpAddress, address)) {
    return {invalidInput("STA static IP address is required"), DeviceConfigField::StaIpAddress};
  }
  if (!parseIpv4(config.staIpGateway, gateway)) {
    return {invalidInput("STA static gateway is required"), DeviceConfigField::StaIpGateway};
  }
  if (!parseIpv4(config.staIpNetmask, netmask) || !isUsableNetmask(netmask)) {
    return {invalidInput("invalid STA static netmask"), DeviceConfigField::StaIpNetmask};
  }
  if (!isUsableHost(address, netmask)) {
    return {invalidInput("invalid STA static IP address"), DeviceConfigField::StaIpAddress};
  }
  if (!isUsableHost(gateway, netmask) ||
      (ipv4ToUint32(address) & ipv4ToUint32(netmask)) !=
          (ipv4ToUint32(gateway) & ipv4ToUint32(netmask))) {
    return {invalidInput("STA gateway must be a host in the STA subnet"), DeviceConfigField::StaIpConfig};
  }
  if (ipv4ToUint32(address) == ipv4ToUint32(gateway)) {
    return {invalidInput("STA IP address and gateway must differ"), DeviceConfigField::StaIpConfig};
  }

  StrictIpv4Address dns;
  if (config.staDns1[0] != '\0' && (!parseIpv4(config.staDns1, dns) || ipv4ToUint32(dns) == 0)) {
    return {invalidInput("invalid STA primary DNS"), DeviceConfigField::StaDns};
  }
  if (config.staDns2[0] != '\0' && (!parseIpv4(config.staDns2, dns) || ipv4ToUint32(dns) == 0)) {
    return {invalidInput("invalid STA secondary DNS"), DeviceConfigField::StaDns};
  }
  return {okResult(), DeviceConfigField::None};
}

DeviceConfigValidation validateApIpv4(const DeviceConfig &config) {
  StrictIpv4Address address;
  StrictIpv4Address netmask;
  if (!parseIpv4(config.apIpAddress, address)) {
    return {invalidInput("AP IP address is required"), DeviceConfigField::ApIpAddress};
  }
  if (!parseIpv4(config.apIpNetmask, netmask) || !isUsableNetmask(netmask)) {
    return {invalidInput("invalid AP netmask"), DeviceConfigField::ApIpNetmask};
  }
  const uint8_t prefixLength = netmaskPrefixLength(netmask);
  if (prefixLength < 24 || prefixLength > 28) {
    return {invalidInput("AP netmask must be between /24 and /28"), DeviceConfigField::ApIpNetmask};
  }
  if (!isUsableHost(address, netmask)) {
    return {invalidInput("invalid AP IP address"), DeviceConfigField::ApIpAddress};
  }
  return {okResult(), DeviceConfigField::None};
}
}  // namespace

Result validateHostnameValue(const char *value) {
  const size_t length = strlen(value);
  if (length == 0) return invalidInput("hostname is empty");
  if (length > 31) return invalidInput("hostname too long");
  if (value[0] == '-' || value[length - 1] == '-') {
    return invalidInput("hostname cannot start or end with hyphen");
  }
  for (const char *cursor = value; *cursor != '\0'; ++cursor) {
    if (!isAlphaNumeric(*cursor) && *cursor != '-') {
      return invalidInput("hostname has unsupported characters");
    }
  }
  return okResult();
}

Result validateWifiTxDbmValue(uint8_t value) {
  if (value < kMinWifiTxDbm || value > kMaxWifiTxDbm) {
    return invalidInput("Wi-Fi TX power must be between 2 and 20 dBm");
  }
  return okResult();
}

Result validateStaSsidValue(const char *value, bool required) {
  return validateSsidValue(value, required,
                           "STA SSID is required for selected Wi-Fi mode",
                           "STA SSID too long",
                           "STA SSID has unsupported characters");
}

Result validateApSsidValue(const char *value, bool required) {
  return validateSsidValue(value, required,
                           "AP SSID is required for selected Wi-Fi mode",
                           "AP SSID too long",
                           "AP SSID has unsupported characters");
}

Result validateStaPasswordValue(const char *value) {
  return validatePasswordValue(value,
                               "STA password too short",
                               "STA password too long",
                               "STA password must contain only printable ASCII characters");
}

Result validateIpv4AddressOptionalValue(const char *value) {
  if (value[0] == '\0') return okResult();
  StrictIpv4Address parsed;
  return parseIpv4(value, parsed) ? okResult() : invalidInput("invalid IPv4 address");
}

Result validateIpv4NetmaskOptionalValue(const char *value) {
  if (value[0] == '\0') return okResult();
  StrictIpv4Address parsed;
  return parseIpv4(value, parsed) && isUsableNetmask(parsed)
             ? okResult()
             : invalidInput("invalid IPv4 netmask");
}

bool ipv4SubnetsOverlap(const char *leftAddress,
                        const char *leftNetmask,
                        const char *rightAddress,
                        const char *rightNetmask) {
  StrictIpv4Address leftIp;
  StrictIpv4Address leftMask;
  StrictIpv4Address rightIp;
  StrictIpv4Address rightMask;
  if (!parseIpv4(leftAddress, leftIp) || !parseIpv4(leftNetmask, leftMask) ||
      !parseIpv4(rightAddress, rightIp) || !parseIpv4(rightNetmask, rightMask)) {
    return false;
  }
  const uint32_t leftMaskValue = ipv4ToUint32(leftMask);
  const uint32_t rightMaskValue = ipv4ToUint32(rightMask);
  const uint32_t leftStart = ipv4ToUint32(leftIp) & leftMaskValue;
  const uint32_t leftEnd = leftStart | ~leftMaskValue;
  const uint32_t rightStart = ipv4ToUint32(rightIp) & rightMaskValue;
  const uint32_t rightEnd = rightStart | ~rightMaskValue;
  return leftStart <= rightEnd && rightStart <= leftEnd;
}

Result validateAdminUsernameValue(const char *value) {
  return strcmp(value, kDefaultAdminUsername) == 0
             ? okResult()
             : invalidInput("admin username must be admin");
}

Result validateAdminPasswordValue(const char *value) {
  if (value[0] == '\0') return invalidInput("admin password is required");
  return validatePasswordValue(value,
                               "admin password too short",
                               "admin password too long",
                               "admin password must contain only printable ASCII characters");
}

DeviceConfigValidation validateDeviceConfigDetailed(const DeviceConfig &config) {
  const auto fail = [](DeviceConfigField field, const char *message) {
    return DeviceConfigValidation{invalidInput(message), field};
  };
  if (config.schemaVersion != kDeviceConfigSchemaVersion) {
    return fail(DeviceConfigField::Schema, "unsupported schema version");
  }

  Result validation = validateHostnameValue(config.hostname);
  if (!validation.ok()) return {validation, DeviceConfigField::Hostname};

  validation = validateWifiTxDbmValue(config.wifiTxDbm);
  if (!validation.ok()) return {validation, DeviceConfigField::WifiTxDbm};

  if (static_cast<uint8_t>(config.wifiMode) > static_cast<uint8_t>(WifiMode::ApSta)) {
    return fail(DeviceConfigField::WifiMode, "invalid Wi-Fi mode");
  }
  validation = validateStaSsidValue(
      config.staSsid, config.wifiMode == WifiMode::Sta || config.wifiMode == WifiMode::ApSta);
  if (!validation.ok()) return {validation, DeviceConfigField::StaSsid};

  const bool apRequired = config.wifiMode == WifiMode::Ap ||
                          config.wifiMode == WifiMode::ApSta ||
                          (config.wifiMode == WifiMode::Sta && config.fallbackToAp);
  validation = validateApSsidValue(config.apSsid, apRequired);
  if (!validation.ok()) return {validation, DeviceConfigField::ApSsid};

  validation = validateStaPasswordValue(config.staPassword);
  if (!validation.ok()) return {validation, DeviceConfigField::StaPassword};

  if (static_cast<uint8_t>(config.staSecurity) > static_cast<uint8_t>(StaSecurity::Open)) {
    return fail(DeviceConfigField::StaSecurity, "invalid STA security");
  }
  if (config.staSecurity == StaSecurity::Open && config.staPassword[0] != '\0') {
    return fail(DeviceConfigField::StaPassword, "STA password must be empty when security is open");
  }

  if (static_cast<uint8_t>(config.staIpMode) > static_cast<uint8_t>(StaIpMode::Static)) {
    return fail(DeviceConfigField::StaIpMode, "invalid STA IP mode");
  }
  if (config.staIpMode == StaIpMode::Static) {
    const DeviceConfigValidation staticIpValidation = validateStaticStaIpv4(config);
    if (!staticIpValidation.ok()) return staticIpValidation;
  }

  const DeviceConfigValidation apIpValidation = validateApIpv4(config);
  if (!apIpValidation.ok()) return apIpValidation;

  if (static_cast<uint8_t>(config.apIpMode) > static_cast<uint8_t>(ApIpMode::Static)) {
    return fail(DeviceConfigField::ApIpMode, "invalid AP IP mode");
  }
  if (config.apIpMode == ApIpMode::Default &&
      (strcmp(config.apIpAddress, kDefaultApIpAddress) != 0 ||
       strcmp(config.apIpNetmask, kDefaultApIpNetmask) != 0)) {
    return fail(DeviceConfigField::ApIpMode, "AP default IP mode must use factory IPv4 values");
  }

  const bool apCanRunWithSta = config.wifiMode == WifiMode::ApSta ||
                               (config.wifiMode == WifiMode::Sta && config.fallbackToAp);
  if (apCanRunWithSta && config.staIpMode == StaIpMode::Static &&
      ipv4SubnetsOverlap(config.staIpAddress, config.staIpNetmask,
                         config.apIpAddress, config.apIpNetmask)) {
    return fail(DeviceConfigField::Interfaces, "STA and AP subnets must not overlap");
  }

  validation = validateAdminUsernameValue(config.adminUsername);
  if (!validation.ok()) return {validation, DeviceConfigField::AdminUsername};
  validation = validateAdminPasswordValue(config.adminPassword);
  if (!validation.ok()) return {validation, DeviceConfigField::AdminPassword};

  if (config.apPasswordEnabled && config.adminPassword[0] == '\0') {
    return fail(DeviceConfigField::AdminPassword,
                "admin password is required when AP password is enabled");
  }
  if (config.wifiMode == WifiMode::Sta || config.wifiMode == WifiMode::ApSta) {
    if (config.staSsid[0] == '\0') {
      return fail(DeviceConfigField::StaSsid,
                  "STA SSID is required for selected Wi-Fi mode");
    }
    if (config.staSecurity == StaSecurity::Wpa && config.staPassword[0] == '\0') {
      return fail(DeviceConfigField::StaPassword,
                  "STA password is required when security is wpa");
    }
  }
  if (apRequired && config.apSsid[0] == '\0') {
    return fail(DeviceConfigField::ApSsid,
                "AP SSID is required for selected Wi-Fi mode");
  }
  return {okResult(), DeviceConfigField::None};
}

Result validateDeviceConfig(const DeviceConfig &config) {
  return validateDeviceConfigDetailed(config).result;
}
