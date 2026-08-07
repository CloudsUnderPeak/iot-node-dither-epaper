#pragma once

#include <ArduinoJson.h>

#include "core/Result.h"
#include "api/shared/JsonReader.h"
#include "modules/config/model/DeviceConfig.h"

namespace WifiPayload {

Result validateJsonStringTree(JsonVariantConst value, uint8_t depth = 0);
bool wifiModeFromApiString(const char *value, WifiMode &mode);
const char *wifiModeToApiString(WifiMode mode);
Result validateStaSsidOptional(const char *value);
Result validateApSsidOptional(const char *value);

struct PasswordUpdate {
  bool provided = false;
  bool keepExisting = false;
  const char *value = "";
};

struct StaIpRequest {
  StaIpMode mode = StaIpMode::Dhcp;
  const char *address = "";
  const char *gateway = "";
  const char *netmask = "";
  const char *dns[2] = {};
  size_t dnsCount = 0;
};

struct StaRequest {
  const char *ssid = "";
  StaSecurity security = StaSecurity::Wpa;
  PasswordUpdate password;
  StaIpRequest ip;
};

struct ApIpRequest {
  ApIpMode mode = ApIpMode::Default;
  bool addressProvided = false;
  bool netmaskProvided = false;
  const char *address = "";
  const char *netmask = "";
};

struct ApRequest {
  const char *ssid = "";
  bool passwordEnabled = false;
  ApIpRequest ip;
};

struct Request {
  // String members are views into the input JsonDocument and are valid only
  // for the duration of the router call.
  WifiMode mode = WifiMode::Off;
  bool fallbackToAp = false;
  StaRequest sta;
  ApRequest ap;
};

bool decode(JsonObjectConst root, Request &request, JsonDecodeError &error);
bool apply(const Request &request,
           const DeviceConfig &current,
           DeviceConfig &updated,
           JsonDecodeError &error);

}  // namespace WifiPayload
