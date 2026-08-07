#include <ArduinoJson.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "modules/config/model/DeviceConfig.h"
#include "modules/console/ConfigStaging.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

ConsoleConfigSetResult setJson(ConfigStaging &staging,
                               ConsoleConfigKey key,
                               const char *json) {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, json);
  if (error) return {false, "test JSON failed to parse"};
  return staging.set(key, document.as<JsonVariantConst>());
}

void testRegistry() {
  expect(ConfigStaging::keyCount() == 20, "config registry should expose all planned keys");
  expect(ConfigStaging::findKey("system.hostname") == ConsoleConfigKey::SystemHostname,
         "key lookup should find exact names");
  expect(ConfigStaging::findKey("system") == ConsoleConfigKey::Unknown,
         "key lookup should not infer groups");
  expect(ConfigStaging::keyMatchesPrefix(ConsoleConfigKey::WifiStaSsid, "wifi.sta"),
         "prefix lookup should honor component boundaries");
  expect(!ConfigStaging::keyMatchesPrefix(ConsoleConfigKey::WifiStaSsid, "wifi.st"),
         "partial component prefix should fail");

  const ConsoleConfigKeyInfo &password =
      ConfigStaging::keyInfo(static_cast<size_t>(ConsoleConfigKey::AuthPassword));
  expect(password.secret && password.writable && !password.readable,
         "secret key must be writable but never readable");
  const ConsoleConfigKeyInfo &passwordSet =
      ConfigStaging::keyInfo(static_cast<size_t>(ConsoleConfigKey::AuthPasswordSet));
  expect(passwordSet.readable && !passwordSet.writable && !passwordSet.secret,
         "derived secret state must be read-only");
}

void testTypedOverlayAndRevert() {
  ConfigStaging staging;
  DeviceConfig base = defaultDeviceConfig();
  DeviceConfig applied = base;

  expect(setJson(staging, ConsoleConfigKey::SystemHostname, "\"stage-host\"").success,
         "valid hostname should stage");
  expect(setJson(staging, ConsoleConfigKey::WifiFallbackToAp, "false").success,
         "boolean should stage with its JSON type");
  expect(setJson(staging, ConsoleConfigKey::WifiMode, "\"ap_sta\"").success,
         "public Wi-Fi mode spelling should stage");
  expect(!setJson(staging, ConsoleConfigKey::WifiFallbackToAp, "\"false\"").success,
         "string should not stage into a boolean key");
  expect(!setJson(staging, ConsoleConfigKey::WifiMode, "\"off\"").success,
         "unsupported Wi-Fi mode should fail");
  expect(!setJson(staging, ConsoleConfigKey::AuthUsername, "\"admin\"").success,
         "read-only key should fail staging");

  staging.apply(applied);
  expect(strcmp(applied.hostname, "stage-host") == 0,
         "overlay should apply the staged hostname");
  expect(!applied.fallbackToAp && applied.wifiMode == WifiMode::ApSta,
         "overlay should apply staged scalar values");
  expect(strcmp(applied.apSsid, base.apSsid) == 0,
         "unstaged fields should remain from the latest base snapshot");

  staging.revert(ConsoleConfigKey::SystemHostname);
  applied = base;
  staging.apply(applied);
  expect(strcmp(applied.hostname, base.hostname) == 0,
         "key revert should remove only that overlay entry");
  expect(staging.dirty(ConsoleConfigGroup::Wifi) && !staging.dirty(ConsoleConfigGroup::System),
         "group dirty state should follow staged keys");
  staging.revert(ConsoleConfigGroup::Wifi);
  expect(!staging.anyDirty(), "group revert should clear that group");
}

void testArraysAndSecretReplacement() {
  ConfigStaging staging;
  expect(setJson(staging, ConsoleConfigKey::WifiStaIpDns,
                 "[\"1.1.1.1\",\"8.8.8.8\"]").success,
         "two DNS addresses should stage");
  expect(!setJson(staging, ConsoleConfigKey::WifiStaIpDns,
                  "[\"1.1.1.1\",\"8.8.8.8\",\"9.9.9.9\"]").success,
         "more than two DNS addresses should fail");

  expect(setJson(staging, ConsoleConfigKey::AuthPassword, "\"FirstPass1\"").success,
         "valid secret should stage");
  expect(!setJson(staging, ConsoleConfigKey::AuthPassword, "\"short\"").success,
         "invalid replacement secret should fail");
  DeviceConfig applied = defaultDeviceConfig();
  staging.apply(applied);
  expect(strcmp(applied.adminPassword, "FirstPass1") == 0,
         "failed replacement must preserve the previous staged secret");
  expect(setJson(staging, ConsoleConfigKey::AuthPassword, "\"SecondPass2\"").success,
         "valid replacement secret should stage");
  applied = defaultDeviceConfig();
  staging.apply(applied);
  expect(strcmp(applied.adminPassword, "SecondPass2") == 0,
         "latest valid secret should win");

  staging.clear();
  applied = defaultDeviceConfig();
  const DeviceConfig clean = applied;
  staging.apply(applied);
  expect(memcmp(&applied, &clean, sizeof(applied)) == 0 && !staging.anyDirty(),
         "clear should wipe and remove the full overlay");
}
}  // namespace

int main() {
  testRegistry();
  testTypedOverlayAndRevert();
  testArraysAndSecretReplacement();
  if (failures != 0) {
    std::cerr << failures << " config staging test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All config staging tests passed\n";
  return EXIT_SUCCESS;
}
