#include <ArduinoJson.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "api/shared/ApiResponse.h"
#include "api/wifi/WifiPayload.h"
#include "modules/config/model/StrictIpv4.h"
#include "modules/http/HttpJsonBody.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

bool decodeAndApply(const char *json,
                    const DeviceConfig &current,
                    DeviceConfig &updated,
                    JsonDecodeError &error) {
  JsonDocument document;
  if (deserializeJson(document, json)) return false;
  WifiPayload::Request request;
  JsonObjectConst root = document.as<JsonObjectConst>();
  return WifiPayload::decode(root, request, error) &&
         WifiPayload::apply(request, current, updated, error);
}

void testStrictIpv4Parser() {
  StrictIpv4Address address;
  expect(parseStrictIpv4("192.168.4.1", address), "valid IPv4 should parse");
  expect(address.octets[0] == 192 && address.octets[3] == 1, "IPv4 octets should be retained");
  expect(!parseStrictIpv4("256.1.1.1", address), "octets above 255 should fail");
  expect(!parseStrictIpv4("1.2.3", address), "IPv4 must contain four octets");
  expect(!parseStrictIpv4("1..2.3", address), "empty IPv4 octets should fail");
  expect(!parseStrictIpv4("0000.1.1.1", address), "IPv4 octets longer than three digits should fail");
  expect(!parseStrictIpv4("1.2.3.4x", address), "IPv4 suffixes should fail");
  expect(!parseStrictIpv4("::1", address), "IPv6 loopback must not pass IPv4 validation");
  expect(!parseStrictIpv4("c0a8:0401::", address), "short IPv6 must not pass IPv4 validation");
}

void testEnumsAndScalarLimits() {
  WifiMode wifiMode;
  expect(WifiPayload::wifiModeFromApiString("sta", wifiMode) && wifiMode == WifiMode::Sta,
         "sta mode should parse");
  expect(WifiPayload::wifiModeFromApiString("ap", wifiMode) && wifiMode == WifiMode::Ap,
         "ap mode should parse");
  expect(WifiPayload::wifiModeFromApiString("ap_sta", wifiMode) && wifiMode == WifiMode::ApSta,
         "ap_sta mode should parse");
  expect(!WifiPayload::wifiModeFromApiString("STA", wifiMode), "mode parsing should be exact");

  StaSecurity security;
  expect(staSecurityFromString("wpa", security), "wpa security should parse");
  expect(staSecurityFromString("open", security), "open security should parse");
  expect(!staSecurityFromString("wpa2", security), "unsupported STA security should fail");

  StaIpMode staIpMode;
  expect(staIpModeFromString("dhcp", staIpMode), "dhcp should parse");
  expect(staIpModeFromString("static", staIpMode), "static STA IP mode should parse");
  expect(!staIpModeFromString("auto", staIpMode), "unsupported STA IP mode should fail");

  ApIpMode apIpMode;
  expect(apIpModeFromString("default", apIpMode), "default AP IP mode should parse");
  expect(apIpModeFromString("static", apIpMode), "static AP IP mode should parse");
  expect(!apIpModeFromString("dhcp", apIpMode), "AP DHCP mode should fail");

  char longSsid[34];
  memset(longSsid, 'A', 33);
  longSsid[33] = '\0';
  expect(!validateStaSsidValue(longSsid, true).ok(), "SSID longer than 32 bytes should fail");
  expect(!validateStaSsidValue("", true).ok(), "required SSID should not be empty");
  expect(validateStaSsidValue("My WiFi", true).ok(), "printable SSID should pass");
  const char nonPrintableSsid[] = {'A', '\n', 'B', '\0'};
  expect(!validateStaSsidValue(nonPrintableSsid, true).ok(), "non-printable SSID should fail");

  expect(validateStaPasswordValue("WiFi pass!").ok(), "printable ASCII STA password should pass");
  expect(!validateStaPasswordValue("short").ok(), "short STA password should fail");
  const char nonPrintablePassword[] = {'p', 'a', 's', 's', 'w', 'o', 'r', 'd', '\n', '\0'};
  expect(!validateStaPasswordValue(nonPrintablePassword).ok(),
         "non-printable STA password should fail");
  expect(validateAdminPasswordValue("password").ok(), "printable ASCII admin password should pass");
  expect(!validateAdminPasswordValue("").ok(), "empty admin password should fail");
  char maxPassword[64];
  memset(maxPassword, 'A', 63);
  maxPassword[63] = '\0';
  expect(validateAdminPasswordValue(maxPassword).ok(), "63-character password should pass");
  char longPassword[65];
  memset(longPassword, 'A', 64);
  longPassword[64] = '\0';
  expect(!validateAdminPasswordValue(longPassword).ok(), "64-character password should fail");
  expect(!validateAdminPasswordValue(nonPrintablePassword).ok(),
         "non-printable admin password should fail");
  expect(defaultDeviceConfig().wifiTxDbm == 15,
         "factory Wi-Fi TX power should default to 15 dBm");
  expect(validateWifiTxDbmValue(2).ok() && validateWifiTxDbmValue(20).ok(),
         "Wi-Fi TX power bounds should validate");
  expect(!validateWifiTxDbmValue(1).ok() && !validateWifiTxDbmValue(21).ok() &&
             !validateWifiTxDbmValue(0xff).ok(),
         "Wi-Fi TX power outside 2-20 must fail");
}

void testConfigCrossFieldValidation() {
  DeviceConfig config = defaultDeviceConfig();
  expect(strcmp(config.adminPassword, "password") == 0,
         "factory admin password should match the documented default");
  expect(validateDeviceConfig(config).ok(), "factory config should validate");

  config.wifiMode = static_cast<WifiMode>(99);
  expect(!validateDeviceConfig(config).ok(), "unknown persisted Wi-Fi mode should fail");

  config = defaultDeviceConfig();
  config.wifiMode = WifiMode::Sta;
  strlcpy(config.staSsid, "MyWiFi", sizeof(config.staSsid));
  expect(!validateDeviceConfig(config).ok(), "WPA STA mode should require a password");
  strlcpy(config.staPassword, "WiFi pass!", sizeof(config.staPassword));
  expect(validateDeviceConfig(config).ok(), "valid WPA station config should pass");

  config.staIpMode = StaIpMode::Static;
  strlcpy(config.staIpAddress, "::1", sizeof(config.staIpAddress));
  strlcpy(config.staIpGateway, "192.168.1.1", sizeof(config.staIpGateway));
  strlcpy(config.staIpNetmask, "255.255.255.0", sizeof(config.staIpNetmask));
  expect(!validateDeviceConfig(config).ok(), "IPv6 text should fail STA IPv4 validation");

  strlcpy(config.staIpAddress, "192.168.1.0", sizeof(config.staIpAddress));
  expect(!validateDeviceConfig(config).ok(), "network address should not be a STA host");
  strlcpy(config.staIpAddress, "192.168.1.10", sizeof(config.staIpAddress));
  strlcpy(config.staIpGateway, "192.168.2.1", sizeof(config.staIpGateway));
  expect(!validateDeviceConfig(config).ok(), "gateway outside the STA subnet should fail");

  config = defaultDeviceConfig();
  config.apIpMode = ApIpMode::Static;
  strlcpy(config.apIpAddress, "192.168.4.1", sizeof(config.apIpAddress));
  strlcpy(config.apIpNetmask, "255.255.0.0", sizeof(config.apIpNetmask));
  expect(!validateDeviceConfig(config).ok(), "AP netmask outside /24-/28 should fail");
}

void testJsonSerialization() {
  const String quoted = Api::quote("line\nquote\"slash\\\x01");
  expect(quoted == "\"line\\nquote\\\"slash\\\\\\u0001\"", "JSON quote should escape control characters");

  const Api::Response response = Api::error(400, "{\"code\":\"invalid_field\"}", "bad\nfield");
  const String serialized = Api::serialize(response);
  JsonDocument document;
  expect(!deserializeJson(document, serialized.c_str()), "serialized response should be valid JSON");
  expect(!document["success"].as<bool>(), "serialized response should retain success=false");
  expect(document["message"].as<const char *>() == std::string("bad\nfield"),
         "serialized response should retain message");

  const Api::Response unauthorized = Api::unauthorized("invalid credentials");
  document.clear();
  expect(!deserializeJson(document, unauthorized.data.c_str()),
         "unauthorized response data should be valid JSON");
  expect(unauthorized.statusCode == 401 && !unauthorized.success,
         "unauthorized response should retain its status and failure state");
  expect(document["code"].as<const char *>() == std::string("unauthorized") &&
             !document["authenticated"].as<bool>(),
         "unauthorized response should use the shared structured payload");
  expect(unauthorized.message == "invalid credentials",
         "unauthorized response should preserve the caller message");
}

void testRequestMatching() {
  Api::Request request;
  request.method = Api::Method::Get;
  request.path = "/api/device";
  expect(request.matches(Api::Method::Get, "/api/device"),
         "request should match exact method and path");
  expect(!request.matches(Api::Method::Post, "/api/device"),
         "wrong method should not match");
  expect(!request.matches(Api::Method::Get, "/api/devices"),
         "wrong path should not match");
}

void testHttpJsonTransportContract() {
  JsonDocument document;
  const uint8_t valid[] = {'{', '"', 'x', '"', ':', '1', '}'};
  Api::Response response = HttpJsonBody::parse("application/json", valid, sizeof(valid), document);
  expect(response.success && document["x"].as<int>() == 1,
         "HTTP adapter should accept an application/json object");

  document.clear();
  response = HttpJsonBody::parse("Application/JSON; charset=utf-8", valid, sizeof(valid), document);
  expect(response.success, "HTTP adapter should accept JSON content type parameters case-insensitively");

  document.clear();
  const uint8_t malformed[] = {'{'};
  response = HttpJsonBody::parse("application/json", malformed, sizeof(malformed), document);
  expect(response.statusCode == 400 && !response.success &&
             response.data.c_str() == std::string("{\"code\":\"invalid_json\"}"),
         "malformed HTTP JSON should return invalid_json");

  document.clear();
  response = HttpJsonBody::parse("application/json", nullptr, 0, document);
  expect(response.statusCode == 400 &&
             response.data.c_str() == std::string("{\"code\":\"invalid_json\"}"),
         "empty HTTP JSON should return invalid_json");

  document.clear();
  response = HttpJsonBody::parse("text/plain", valid, sizeof(valid), document);
  expect(response.statusCode == 400 &&
             response.data.c_str() == std::string("{\"code\":\"invalid_json\"}"),
         "wrong HTTP content type should return invalid_json");

  document.clear();
  response = HttpJsonBody::parse(
      "application/json", valid, HttpJsonBody::kMaxBytes + 1, document);
  expect(response.statusCode == 413 &&
             response.data.c_str() == std::string("{\"code\":\"payload_too_large\"}"),
         "oversized HTTP JSON should return payload_too_large");
}

void testWifiRequestValidationAndPaths() {
  const DeviceConfig current = defaultDeviceConfig();
  DeviceConfig updated;
  JsonDecodeError error;

  const char *validAp = R"({
    "mode":"ap","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(decodeAndApply(validAp, current, updated, error), "valid AP request should pass");

  const char *invalidMode = R"({
    "mode":"STA","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(!decodeAndApply(invalidMode, current, updated, error) && error.field.c_str() == std::string("mode"),
         "invalid mode should report mode");

  error = JsonDecodeError();
  const char *offMode = R"({
    "mode":"off","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(!decodeAndApply(offMode, current, updated, error) && error.field.c_str() == std::string("mode"),
         "PUT Wi-Fi should reject off mode");

  error = JsonDecodeError();
  const char *ipv6Address = R"({
    "mode":"sta","fallback_to_ap":false,
    "interfaces":{
      "sta":{"ssid":"MyWiFi","security":"wpa","password":"A1b2C3d4","ip_config":{"mode":"static","address":"c0a8:010a::","gateway":"192.168.1.1","netmask":"255.255.255.0","dns":[]}},
      "ap":{"ssid":"","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(!decodeAndApply(ipv6Address, current, updated, error) &&
             error.field.c_str() == std::string("interfaces.sta.ip_config.address"),
         "IPv6 STA address should fail with the address path");

  error = JsonDecodeError();
  const char *invalidDns = R"({
    "mode":"sta","fallback_to_ap":false,
    "interfaces":{
      "sta":{"ssid":"MyWiFi","security":"wpa","password":"A1b2C3d4","ip_config":{"mode":"static","address":"192.168.1.10","gateway":"192.168.1.1","netmask":"255.255.255.0","dns":["::1"]}},
      "ap":{"ssid":"","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(!decodeAndApply(invalidDns, current, updated, error) &&
             error.field.c_str() == std::string("interfaces.sta.ip_config.dns"),
         "IPv6 DNS should fail with the DNS path");

  error = JsonDecodeError();
  const char *dhcpWithAddress = R"({
    "mode":"sta","fallback_to_ap":false,
    "interfaces":{
      "sta":{"ssid":"MyWiFi","security":"wpa","password":"A1b2C3d4","ip_config":{"mode":"dhcp","address":"192.168.1.10","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(!decodeAndApply(dhcpWithAddress, current, updated, error) &&
             error.field.c_str() == std::string("interfaces.sta.ip_config"),
         "DHCP should reject static fields");

  error = JsonDecodeError();
  const char *missingPassword = R"({
    "mode":"sta","fallback_to_ap":false,
    "interfaces":{
      "sta":{"ssid":"MyWiFi","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(!decodeAndApply(missingPassword, current, updated, error) &&
             error.field.c_str() == std::string("interfaces.sta.password"),
         "missing stored WPA password should report the password path");

  error = JsonDecodeError();
  const char *invalidApMask = R"({
    "mode":"ap","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":false,"ip_config":{"mode":"static","address":"192.168.4.1","netmask":"255.255.0.0"}}
    }
  })";
  expect(!decodeAndApply(invalidApMask, current, updated, error) &&
             error.field.c_str() == std::string("interfaces.ap.ip_config.netmask"),
         "invalid AP prefix should report the netmask path");

  error = JsonDecodeError();
  const char *overlap = R"({
    "mode":"ap_sta","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"MyWiFi","security":"wpa","password":"A1b2C3d4","ip_config":{"mode":"static","address":"192.168.4.10","gateway":"192.168.4.2","netmask":"255.255.255.0","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":false,"ip_config":{"mode":"static","address":"192.168.4.1","netmask":"255.255.255.0"}}
    }
  })";
  expect(!decodeAndApply(overlap, current, updated, error) &&
             strcmp(error.code, "subnet_overlap") == 0 && error.field.c_str() == std::string("interfaces"),
         "overlapping subnets should return a path and stable code");

  error = JsonDecodeError();
  const char *unknownField = R"({
    "mode":"ap","fallback_to_ap":true,"extra":1,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })";
  expect(!decodeAndApply(unknownField, current, updated, error) &&
             strcmp(error.code, "unsupported_field") == 0 && error.field.c_str() == std::string("extra"),
         "unknown fields should be rejected with their path");
}
}  // namespace

int main() {
  testStrictIpv4Parser();
  testEnumsAndScalarLimits();
  testConfigCrossFieldValidation();
  testJsonSerialization();
  testRequestMatching();
  testHttpJsonTransportContract();
  testWifiRequestValidationAndPaths();

  if (failures != 0) {
    std::cerr << failures << " validation test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All API validation tests passed\n";
  return EXIT_SUCCESS;
}
