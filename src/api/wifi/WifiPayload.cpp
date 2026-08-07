#include "WifiPayload.h"

#include <cstring>

namespace WifiPayload {
namespace {
constexpr const char *kPasswordMask = "********";

bool isPrintableAscii(const char *value) {
  for (const char *cursor = value; *cursor != '\0'; ++cursor) {
    const unsigned char current = static_cast<unsigned char>(*cursor);
    if (current < 0x20 || current > 0x7e) return false;
  }
  return true;
}

void fail(JsonDecodeError &error, const char *code, const char *field, const char *message) {
  if (!error.ok()) return;
  error.code = code;
  error.field = field;
  error.message = message;
}

const char *fieldForConfigValidation(DeviceConfigField field) {
  switch (field) {
    case DeviceConfigField::WifiMode: return "mode";
    case DeviceConfigField::StaSsid: return "interfaces.sta.ssid";
    case DeviceConfigField::StaPassword: return "interfaces.sta.password";
    case DeviceConfigField::StaSecurity: return "interfaces.sta.security";
    case DeviceConfigField::StaIpMode: return "interfaces.sta.ip_config.mode";
    case DeviceConfigField::StaIpConfig: return "interfaces.sta.ip_config";
    case DeviceConfigField::StaIpAddress: return "interfaces.sta.ip_config.address";
    case DeviceConfigField::StaIpGateway: return "interfaces.sta.ip_config.gateway";
    case DeviceConfigField::StaIpNetmask: return "interfaces.sta.ip_config.netmask";
    case DeviceConfigField::StaDns: return "interfaces.sta.ip_config.dns";
    case DeviceConfigField::ApSsid: return "interfaces.ap.ssid";
    case DeviceConfigField::ApIpMode: return "interfaces.ap.ip_config.mode";
    case DeviceConfigField::ApIpAddress: return "interfaces.ap.ip_config.address";
    case DeviceConfigField::ApIpNetmask: return "interfaces.ap.ip_config.netmask";
    case DeviceConfigField::Interfaces: return "interfaces";
    default: return "";
  }
}

bool copyString(const char *value,
                char *target,
                size_t targetSize,
                Result (*validator)(const char *),
                const char *field,
                JsonDecodeError &error) {
  const Result validation = validator(value);
  if (!validation.ok()) {
    fail(error, "invalid_field", field, validation.message);
    return false;
  }
  if (strlen(value) >= targetSize) {
    fail(error, "invalid_field", field, "string too long");
    return false;
  }
  strlcpy(target, value, targetSize);
  return true;
}

void decodeStaIp(JsonReader reader, StaIpRequest &request, JsonDecodeError &error) {
  const char *mode = reader.requiredString("mode", "STA IP mode is required");
  if (error.ok() && !staIpModeFromString(mode, request.mode)) {
    fail(error, "invalid_field", "interfaces.sta.ip_config.mode", "invalid STA IP mode");
  }

  request.address = reader.requiredString("address", "STA address is required");
  request.gateway = reader.requiredString("gateway", "STA gateway is required");
  request.netmask = reader.requiredString("netmask", "STA netmask is required");
  JsonArrayConst dns = reader.requiredArray("dns", "STA DNS is required");
  reader.finish({"mode", "address", "gateway", "netmask", "dns"});

  if (!error.ok()) return;
  if (dns.size() > 2) {
    fail(error, "invalid_field", "interfaces.sta.ip_config.dns", "STA DNS supports at most two addresses");
    return;
  }
  for (JsonVariantConst item : dns) {
    if (!item.is<const char *>()) {
      fail(error, "invalid_field", "interfaces.sta.ip_config.dns", "STA DNS item must be a string");
      return;
    }
    request.dns[request.dnsCount++] = item.as<const char *>();
  }
}

void decodeSta(JsonReader reader, StaRequest &request, JsonDecodeError &error) {
  request.ssid = reader.requiredString("ssid", "STA SSID is required");
  const char *security = reader.requiredString("security", "STA security is required");
  request.password.value = reader.optionalString("password", request.password.provided, "STA password must be a string");
  JsonReader ip = reader.requiredObject("ip_config", "STA IP settings are required");
  decodeStaIp(ip, request.ip, error);
  reader.finish({"ssid", "security", "password", "ip_config"});

  if (!error.ok()) return;
  if (!staSecurityFromString(security, request.security)) {
    fail(error, "invalid_field", "interfaces.sta.security", "invalid STA security");
  }
  if (!error.ok() || !request.password.provided) return;

  request.password.keepExisting = strcmp(request.password.value, kPasswordMask) == 0;
}

void decodeApIp(JsonReader reader, ApIpRequest &request, JsonDecodeError &error) {
  const char *mode = reader.requiredString("mode", "AP IP mode is required");
  request.address = reader.optionalString("address", request.addressProvided, "AP address must be a string");
  request.netmask = reader.optionalString("netmask", request.netmaskProvided, "AP netmask must be a string");
  reader.finish({"mode", "address", "netmask"});

  if (!error.ok()) return;
  if (!apIpModeFromString(mode, request.mode)) {
    fail(error, "invalid_field", "interfaces.ap.ip_config.mode", "invalid AP IP mode");
    return;
  }
}

void decodeAp(JsonReader reader, ApRequest &request, JsonDecodeError &error) {
  request.ssid = reader.requiredString("ssid", "AP SSID is required");
  request.passwordEnabled = reader.requiredBool("password_enabled", "AP password setting is required");
  JsonReader ip = reader.requiredObject("ip_config", "AP IP settings are required");
  decodeApIp(ip, request.ip, error);
  reader.finish({"ssid", "password_enabled", "ip_config"});
}

bool applyStaIp(const StaIpRequest &request, DeviceConfig &updated, JsonDecodeError &error) {
  updated.staIpMode = request.mode;
  if (request.mode == StaIpMode::Dhcp) {
    if (request.address[0] != '\0' || request.gateway[0] != '\0' ||
        request.netmask[0] != '\0' || request.dnsCount > 0) {
      fail(error, "invalid_field", "interfaces.sta.ip_config", "STA DHCP fields must be empty");
      return false;
    }
    updated.staIpAddress[0] = updated.staIpGateway[0] = updated.staIpNetmask[0] = '\0';
    updated.staDns1[0] = updated.staDns2[0] = '\0';
    return true;
  }

  if (!copyString(request.address, updated.staIpAddress, sizeof(updated.staIpAddress),
                  validateIpv4AddressOptionalValue, "interfaces.sta.ip_config.address", error) ||
      !copyString(request.gateway, updated.staIpGateway, sizeof(updated.staIpGateway),
                  validateIpv4AddressOptionalValue, "interfaces.sta.ip_config.gateway", error) ||
      !copyString(request.netmask, updated.staIpNetmask, sizeof(updated.staIpNetmask),
                  validateIpv4NetmaskOptionalValue, "interfaces.sta.ip_config.netmask", error)) {
    return false;
  }

  updated.staDns1[0] = updated.staDns2[0] = '\0';
  if (request.dnsCount > 0 &&
      !copyString(request.dns[0], updated.staDns1, sizeof(updated.staDns1),
                  validateIpv4AddressOptionalValue, "interfaces.sta.ip_config.dns", error)) {
    return false;
  }
  if (request.dnsCount > 1 &&
      !copyString(request.dns[1], updated.staDns2, sizeof(updated.staDns2),
                  validateIpv4AddressOptionalValue, "interfaces.sta.ip_config.dns", error)) {
    return false;
  }
  return true;
}

bool applyApIp(const ApIpRequest &request, DeviceConfig &updated, JsonDecodeError &error) {
  updated.apIpMode = request.mode;
  if (request.mode == ApIpMode::Default) {
    if (request.addressProvided || request.netmaskProvided) {
      fail(error, "invalid_field", "interfaces.ap.ip_config",
           "AP default mode does not accept address or netmask");
      return false;
    }
    const DeviceConfig defaults = defaultDeviceConfig();
    strlcpy(updated.apIpAddress, defaults.apIpAddress, sizeof(updated.apIpAddress));
    strlcpy(updated.apIpNetmask, defaults.apIpNetmask, sizeof(updated.apIpNetmask));
    return true;
  }

  if (!request.addressProvided || !request.netmaskProvided) {
    fail(error, "missing_field", "interfaces.ap.ip_config",
         "AP static address and netmask are required");
    return false;
  }
  return copyString(request.address, updated.apIpAddress, sizeof(updated.apIpAddress),
                    validateIpv4AddressOptionalValue, "interfaces.ap.ip_config.address", error) &&
         copyString(request.netmask, updated.apIpNetmask, sizeof(updated.apIpNetmask),
                    validateIpv4NetmaskOptionalValue, "interfaces.ap.ip_config.netmask", error);
}
}  // namespace

Result validateJsonStringTree(JsonVariantConst value, uint8_t depth) {
  if (depth > 8) return invalidInput("JSON body is too nested");
  if (value.is<const char *>()) {
    const char *text = value.as<const char *>();
    if (strlen(text) > 128) return invalidInput("string too long");
    return isPrintableAscii(text)
               ? okResult()
               : invalidInput("string has unsupported characters");
  }
  if (value.is<JsonObjectConst>()) {
    for (JsonPairConst pair : value.as<JsonObjectConst>()) {
      const Result result = validateJsonStringTree(pair.value(), depth + 1);
      if (!result.ok()) return result;
    }
  } else if (value.is<JsonArrayConst>()) {
    for (JsonVariantConst item : value.as<JsonArrayConst>()) {
      const Result result = validateJsonStringTree(item, depth + 1);
      if (!result.ok()) return result;
    }
  }
  return okResult();
}

bool wifiModeFromApiString(const char *value, WifiMode &mode) {
  if (strcmp(value, "off") == 0) mode = WifiMode::Off;
  else if (strcmp(value, "sta") == 0) mode = WifiMode::Sta;
  else if (strcmp(value, "ap") == 0) mode = WifiMode::Ap;
  else if (strcmp(value, "ap_sta") == 0) mode = WifiMode::ApSta;
  else return false;
  return true;
}

const char *wifiModeToApiString(WifiMode mode) {
  switch (mode) {
    case WifiMode::Off: return "off";
    case WifiMode::Sta: return "sta";
    case WifiMode::Ap: return "ap";
    case WifiMode::ApSta: return "ap_sta";
  }
  return "UNKNOWN";
}

Result validateStaSsidOptional(const char *value) {
  return validateStaSsidValue(value, false);
}

Result validateApSsidOptional(const char *value) {
  return validateApSsidValue(value, false);
}

bool decode(JsonObjectConst root, Request &request, JsonDecodeError &error) {
  JsonReader reader(root, "", error);
  const char *mode = reader.requiredString("mode", "mode is required");
  request.fallbackToAp = reader.requiredBool("fallback_to_ap", "fallback_to_ap is required");
  JsonReader interfaces = reader.requiredObject("interfaces", "interfaces is required");
  JsonReader sta = interfaces.requiredObject("sta", "STA interface is required");
  JsonReader ap = interfaces.requiredObject("ap", "AP interface is required");

  decodeSta(sta, request.sta, error);
  decodeAp(ap, request.ap, error);
  interfaces.finish({"sta", "ap"});
  reader.finish({"mode", "fallback_to_ap", "interfaces"});

  if (!error.ok()) return false;
  if (!wifiModeFromApiString(mode, request.mode) || request.mode == WifiMode::Off) {
    fail(error, "invalid_field", "mode", "mode must be sta, ap, or ap_sta");
    return false;
  }
  return true;
}

bool apply(const Request &request,
           const DeviceConfig &current,
           DeviceConfig &updated,
           JsonDecodeError &error) {
  updated = current;
  updated.wifiMode = request.mode;
  updated.fallbackToAp = request.fallbackToAp;
  if (!copyString(request.sta.ssid, updated.staSsid, sizeof(updated.staSsid),
                  validateStaSsidOptional, "interfaces.sta.ssid", error)) {
    return false;
  }
  updated.staSecurity = request.sta.security;
  if (request.sta.password.provided && !request.sta.password.keepExisting) {
    if (!copyString(request.sta.password.value, updated.staPassword, sizeof(updated.staPassword),
                    validateStaPasswordValue, "interfaces.sta.password", error)) {
      return false;
    }
  }
  if (updated.staSecurity == StaSecurity::Open) updated.staPassword[0] = '\0';
  if (!applyStaIp(request.sta.ip, updated, error)) return false;

  if (!copyString(request.ap.ssid, updated.apSsid, sizeof(updated.apSsid),
                  validateApSsidOptional, "interfaces.ap.ssid", error)) {
    return false;
  }
  updated.apPasswordEnabled = request.ap.passwordEnabled;
  if (!applyApIp(request.ap.ip, updated, error)) return false;

  const DeviceConfigValidation validation = validateDeviceConfigDetailed(updated);
  if (!validation.ok()) {
    const bool overlap = validation.field == DeviceConfigField::Interfaces;
    fail(error,
         overlap ? "subnet_overlap" : "invalid_field",
         fieldForConfigValidation(validation.field),
         validation.result.message);
    return false;
  }
  return true;
}

}  // namespace WifiPayload
