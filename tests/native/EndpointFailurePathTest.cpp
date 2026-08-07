#include <ArduinoJson.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "api/auth/AuthEndpoints.h"
#include "api/storage/StorageEndpoints.h"
#include "api/system/SystemEndpoints.h"
#include "api/web/WebEndpoints.h"
#include "api/wifi/WifiEndpoints.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

Api::Request jsonRequest(JsonDocument &document, const char *json) {
  deserializeJson(document, json);
  Api::Request request;
  request.hasJsonBody = true;
  request.body = document.as<JsonVariantConst>();
  return request;
}

void testWebIdentityReportsOnlySourceAndSha256() {
  EmbeddedWebAssets assets;
  Api::Response response = WebEndpoints::get(assets);
  expect(response.success && response.statusCode == 200 &&
             response.data.c_str() == std::string(
                 "{\"source\":\"builtin\",\"sha256\":"
                 "\"0123456789abcdef0123456789abcdef0123456789abcdef"
                 "0123456789abcdef\"}"),
         "web identity must expose only the selected source and embedded tree SHA-256");

  assets.sourceValue = "none";
  assets.sha256Value = nullptr;
  response = WebEndpoints::get(assets);
  expect(response.success && response.statusCode == 200 &&
             response.data.c_str() ==
                 std::string("{\"source\":\"none\",\"sha256\":null}"),
         "firmware without a frontend must expose source none and a null SHA-256");
}

void testStorageReportsFlashAppAndUploadCapacity() {
  FlashStorage flashStorage;
  UserDataStorage userData;
  Api::Response response = StorageEndpoints::get(flashStorage, userData);
  expect(response.success && response.statusCode == 200 &&
             response.data.c_str() == std::string(
                 "{\"flash\":{\"total_bytes\":4194304,\"fixed_regions\":{"
                 "\"bootloader_reserved_bytes\":32768,\"partition_table_bytes\":4096},\"partitions\":["
                 "{\"id\":\"nvs\",\"type\":\"data\",\"subtype\":\"nvs\",\"offset_bytes\":36864,\"size_bytes\":20480},"
                 "{\"id\":\"otadata\",\"type\":\"data\",\"subtype\":\"ota\",\"offset_bytes\":57344,\"size_bytes\":8192},"
                 "{\"id\":\"app0\",\"type\":\"app\",\"subtype\":\"ota_0\",\"offset_bytes\":65536,\"size_bytes\":2031616},"
                 "{\"id\":\"userdata\",\"type\":\"data\",\"subtype\":\"spiffs\",\"offset_bytes\":2097152,\"size_bytes\":1998848},"
                 "{\"id\":\"user_nvs\",\"type\":\"data\",\"subtype\":\"nvs\",\"offset_bytes\":4096000,\"size_bytes\":32768},"
                 "{\"id\":\"coredump\",\"type\":\"data\",\"subtype\":\"coredump\",\"offset_bytes\":4128768,\"size_bytes\":65536}]},"
                 "\"app\":{\"partition_id\":\"app0\",\"frontend_bundled\":true,\"capacity\":{"
                 "\"total_bytes\":2031616,\"firmware_image_bytes\":1261568,"
                 "\"frontend_payload_bytes\":40000,\"available_bytes\":770048}},"
                 "\"user\":{\"partition_id\":\"userdata\",\"filesystem\":\"littlefs\","
                 "\"mounted\":true,\"capabilities\":{\"file_upload\":true,"
                 "\"file_list\":true,\"file_download\":true,\"file_delete\":true},"
                 "\"capacity\":{\"total_bytes\":1933312,\"used_bytes\":327680,"
                 "\"available_bytes\":1605632},\"limits\":{\"max_upload_bytes\":1605632,"
                 "\"reserved_bytes\":65536,\"allocation_unit_bytes\":4096,"
                 "\"max_filename_bytes\":64}}}"),
         "storage must report partitions, the bundled app image, and usable upload capacity");

  userData.mountedValue = false;
  response = StorageEndpoints::get(flashStorage, userData);
  expect(response.success &&
             response.data.c_str() == std::string(
                 "{\"flash\":{\"total_bytes\":4194304,\"fixed_regions\":{"
                 "\"bootloader_reserved_bytes\":32768,\"partition_table_bytes\":4096},\"partitions\":["
                 "{\"id\":\"nvs\",\"type\":\"data\",\"subtype\":\"nvs\",\"offset_bytes\":36864,\"size_bytes\":20480},"
                 "{\"id\":\"otadata\",\"type\":\"data\",\"subtype\":\"ota\",\"offset_bytes\":57344,\"size_bytes\":8192},"
                 "{\"id\":\"app0\",\"type\":\"app\",\"subtype\":\"ota_0\",\"offset_bytes\":65536,\"size_bytes\":2031616},"
                 "{\"id\":\"userdata\",\"type\":\"data\",\"subtype\":\"spiffs\",\"offset_bytes\":2097152,\"size_bytes\":1998848},"
                 "{\"id\":\"user_nvs\",\"type\":\"data\",\"subtype\":\"nvs\",\"offset_bytes\":4096000,\"size_bytes\":32768},"
                 "{\"id\":\"coredump\",\"type\":\"data\",\"subtype\":\"coredump\",\"offset_bytes\":4128768,\"size_bytes\":65536}]},"
                 "\"app\":{\"partition_id\":\"app0\",\"frontend_bundled\":true,\"capacity\":{"
                 "\"total_bytes\":2031616,\"firmware_image_bytes\":1261568,"
                 "\"frontend_payload_bytes\":40000,\"available_bytes\":770048}},"
                 "\"user\":{\"partition_id\":\"userdata\",\"filesystem\":\"littlefs\","
                 "\"mounted\":false,\"capabilities\":{\"file_upload\":true,"
                 "\"file_list\":true,\"file_download\":true,\"file_delete\":true},"
                 "\"capacity\":{\"total_bytes\":0,\"used_bytes\":0,\"available_bytes\":0},"
                 "\"limits\":{\"max_upload_bytes\":0,\"reserved_bytes\":65536,"
                 "\"allocation_unit_bytes\":4096,\"max_filename_bytes\":64}}}"),
         "an unavailable user partition must expose a zero upload limit without hiding flash layout");

  flashStorage.frontendBundledValue = false;
  flashStorage.frontendBytesValue = 0;
  response = StorageEndpoints::get(flashStorage, userData);
  expect(response.success &&
             std::strstr(response.data.c_str(), "\"frontend_bundled\":false") != nullptr &&
             std::strstr(response.data.c_str(), "\"frontend_payload_bytes\":0") != nullptr,
         "firmware without a frontend must report no bundled frontend and zero payload");
}

void testUploadCapacityKeepsReserveAndAlignment() {
  const UploadCapacity nearFull = calculateUploadCapacity(
      {1703936, 1642496, 61440}, 65536, 4096);
  expect(nearFull.availableBytes == 0 && nearFull.maxUploadBytes == 0 &&
             nearFull.reservedBytes == 65536,
         "free bytes inside the reserve must not be offered for upload");

  const UploadCapacity unaligned = calculateUploadCapacity(
      {1703936, 1627936, 76000}, 65536, 4096);
  expect(unaligned.availableBytes == 8192 &&
             unaligned.maxUploadBytes == unaligned.availableBytes &&
             unaligned.availableBytes % unaligned.allocationUnitBytes == 0,
         "upload capacity must round down to a complete allocation unit");
}

void testSystemUpdatePreflightsRuntime() {
  ConfigService config;
  StorageLifecycle storage;
  RuntimeActionScheduler runtime;
  SystemEndpoints endpoints;
  expect(endpoints.begin(&config, &storage, &runtime).ok(), "system endpoints should initialize");

  JsonDocument document;
  Api::Request request = jsonRequest(document, R"({"hostname":"new-device"})");
  Api::Response response = endpoints.update(request);
  expect(response.statusCode == 503 && config.commitCount == 0 && runtime.wifiApplyCount == 0,
         "unavailable runtime must reject hostname before persistence");

  runtime.available = true;
  response = endpoints.update(request);
  expect(response.success && config.commitCount == 1 && runtime.wifiApplyCount == 1 &&
             strcmp(config.value.hostname, "new-device") == 0,
         "hostname success must persist and guarantee Wi-Fi apply");
}

void testFactoryResetPreflightsRuntime() {
  ConfigService config;
  StorageLifecycle storage;
  RuntimeActionScheduler runtime;
  SystemEndpoints endpoints;
  endpoints.begin(&config, &storage, &runtime);

  Api::Response response = endpoints.reset(StorageResetScope::All);
  expect(response.statusCode == 503 && storage.requestCount == 0 && runtime.resetCount == 0,
         "unavailable runtime must reject reset before recording reset intent");

  runtime.available = true;
  response = endpoints.reset(StorageResetScope::All);
  expect(response.success && storage.requestCount == 1 &&
             storage.lastScope == StorageResetScope::All && runtime.resetCount == 1,
         "factory reset success must persist the all scope and guarantee restart");

  response = endpoints.reset(StorageResetScope::Settings);
  expect(response.success && storage.requestCount == 2 &&
             storage.lastScope == StorageResetScope::Settings && runtime.resetCount == 2,
         "settings reset must persist only the settings scope");

  response = endpoints.reset(StorageResetScope::Data);
  expect(response.success && storage.requestCount == 3 &&
             storage.lastScope == StorageResetScope::Data && runtime.resetCount == 3,
         "data reset must persist only the data scope");

  storage.requestResult = storageError("write failed");
  response = endpoints.reset(StorageResetScope::All);
  expect(response.statusCode == 500 && storage.requestCount == 4 && runtime.resetCount == 3,
         "failed reset intent persistence must not schedule restart");
}

void testPasswordUpdatePreflightsRequiredApRestart() {
  ConfigService config;
  config.value.apPasswordEnabled = true;
  AuthService auth;
  RuntimeActionScheduler runtime;
  AuthEndpoints endpoints;
  endpoints.begin(&config, &auth, &runtime);

  JsonDocument document;
  Api::Request request = jsonRequest(document, R"({"password":"New pass!"})");
  Api::Response response = endpoints.updatePassword(request);
  expect(response.statusCode == 503 && config.commitCount == 0 && !auth.invalidated &&
             runtime.resetCount == 0,
         "unavailable AP restart must reject password before persistence or session invalidation");

  runtime.available = true;
  response = endpoints.updatePassword(request);
  expect(response.success && config.commitCount == 1 && auth.invalidated &&
             runtime.wifiApplyCount == 0 && runtime.resetCount == 1 &&
             runtime.lastResetDelayMs == 300 &&
             strcmp(config.value.adminPassword, "New pass!") == 0,
         "protected AP password success must persist, invalidate the session, and guarantee restart");
}

void testPasswordUpdateWithoutApProtectionDoesNotRestart() {
  ConfigService config;
  config.value.apPasswordEnabled = false;
  AuthService auth;
  RuntimeActionScheduler runtime;
  AuthEndpoints endpoints;
  endpoints.begin(&config, &auth, &runtime);

  JsonDocument document;
  Api::Request request = jsonRequest(document, R"({"password":"New pass!"})");
  const Api::Response response = endpoints.updatePassword(request);
  expect(response.success && config.commitCount == 1 && auth.invalidated &&
             runtime.wifiApplyCount == 0 && runtime.resetCount == 0 &&
             strcmp(config.value.adminPassword, "New pass!") == 0,
         "admin password change must not restart when AP protection is disabled");
}

void testWifiUpdatePreflightsRuntime() {
  ConfigService config;
  WifiManager manager;
  WifiScanner scanner;
  RuntimeActionScheduler runtime;
  WifiEndpoints endpoints;
  endpoints.begin(&config, &manager, &scanner, &runtime);

  JsonDocument document;
  Api::Request request = jsonRequest(document, R"({
    "mode":"ap","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })");
  Api::Response response = endpoints.update(request);
  expect(response.statusCode == 503 && config.commitCount == 0 && runtime.wifiApplyCount == 0,
         "unavailable runtime must reject Wi-Fi update before persistence");

  runtime.available = true;
  response = endpoints.update(request);
  expect(response.success && config.commitCount == 1 && runtime.wifiApplyCount == 1 &&
             config.value.wifiMode == WifiMode::Ap && strcmp(config.value.apSsid, "TestAP") == 0,
         "Wi-Fi success must persist and guarantee apply");
}

void testWifiApProtectionChangeSchedulesRestart() {
  ConfigService config;
  WifiManager manager;
  WifiScanner scanner;
  RuntimeActionScheduler runtime;
  runtime.available = true;
  WifiEndpoints endpoints;
  endpoints.begin(&config, &manager, &scanner, &runtime);

  JsonDocument document;
  Api::Request request = jsonRequest(document, R"({
    "mode":"ap","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"TestAP","password_enabled":true,"ip_config":{"mode":"default"}}
    }
  })");
  const Api::Response response = endpoints.update(request);
  expect(response.success && config.commitCount == 1 &&
             config.value.apPasswordEnabled && runtime.wifiApplyCount == 0 &&
             runtime.resetCount == 1 && runtime.lastResetDelayMs == 300,
         "changing AP protection must persist once and guarantee a prompt device restart");
}

void testWifiApToStaUpdateQueuesSafeTransition() {
  ConfigService config;
  config.value.wifiMode = WifiMode::Ap;
  strlcpy(config.value.apSsid, "ManagementAP", sizeof(config.value.apSsid));
  WifiManager manager;
  manager.value.mode = WifiMode::Ap;
  manager.value.apEnabled = true;
  manager.value.apState = WifiApState::Active;
  WifiScanner scanner;
  RuntimeActionScheduler runtime;
  runtime.available = true;
  WifiEndpoints endpoints;
  endpoints.begin(&config, &manager, &scanner, &runtime);

  JsonDocument document;
  Api::Request request = jsonRequest(document, R"({
    "mode":"sta","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"TestSTA","security":"wpa","password":"A1b2C3d4","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"FutureAP","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })");
  Api::Response response = endpoints.update(request);
  expect(response.statusCode == 202 && response.success && config.commitCount == 0 &&
             runtime.wifiApplyCount == 0 && manager.queueCount == 1 &&
             response.data.c_str() == std::string("{\"state\":\"connecting\"}") &&
             manager.previous.wifiMode == WifiMode::Ap &&
             strcmp(manager.previous.apSsid, "ManagementAP") == 0 &&
             manager.candidate.wifiMode == WifiMode::Sta &&
             strcmp(manager.candidate.staSsid, "TestSTA") == 0 &&
             strcmp(manager.candidate.apSsid, "FutureAP") == 0,
         "AP to STA update must queue the complete candidate without persisting or dropping the AP");

  manager.blocksScan = true;
  JsonDocument busyDocument;
  Api::Request busyRequest = jsonRequest(busyDocument, R"({
    "mode":"ap","fallback_to_ap":true,
    "interfaces":{
      "sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},
      "ap":{"ssid":"OtherAP","password_enabled":false,"ip_config":{"mode":"default"}}
    }
  })");
  response = endpoints.update(busyRequest);
  expect(response.statusCode == 409 && config.commitCount == 0 && runtime.wifiApplyCount == 0,
         "an active safe transition must reject another Wi-Fi update before persistence");

  response = endpoints.reconnect();
  expect(response.statusCode == 409 && runtime.wifiApplyCount == 0,
         "an active safe transition must reject reconnect without interrupting the transaction");
}

void testWifiScanBusyAndSuccessPaths() {
  ConfigService config;
  WifiManager manager;
  WifiScanner scanner;
  RuntimeActionScheduler runtime;
  WifiEndpoints endpoints;
  endpoints.begin(&config, &manager, &scanner, &runtime);

  manager.blocksScan = true;
  Api::Response response = endpoints.scan();
  expect(response.statusCode == 409 &&
             response.data.c_str() == std::string("{\"code\":\"wifi_connect_busy\"}") &&
             scanner.scanCount == 0,
         "active STA connection must block scan before touching the scanner");

  manager.blocksScan = false;
  manager.staBlocksScan = true;
  response = endpoints.scan();
  expect(response.statusCode == 409 &&
             response.data.c_str() == std::string("{\"code\":\"wifi_scan_busy\",\"retry_after_seconds\":1}") &&
             scanner.scanCount == 0,
         "persisted STA connection attempt must return retryable scan busy");

  manager.staBlocksScan = false;
  scanner.value.count = 1;
  scanner.value.networks[0].ssid = "TestNetwork";
  scanner.value.networks[0].rssi = -48;
  scanner.value.networks[0].channel = 6;
  scanner.value.networks[0].encryptionType = 3;
  scanner.value.networks[0].encryption = "wpa2";
  response = endpoints.scan();
  expect(response.success && response.statusCode == 200 && scanner.scanCount == 1 &&
             response.data.c_str() == std::string("{\"networks\":[{\"ssid\":\"TestNetwork\",\"rssi\":-48,\"channel\":6,\"hidden\":false,\"encryption\":\"wpa2\",\"encryption_type\":3}]}"),
         "scan must resume and serialize results after radio blockers clear");
}

void testWifiConnectQueuesMinimalCredentials() {
  ConfigService config;
  WifiManager manager;
  WifiScanner scanner;
  RuntimeActionScheduler runtime;
  WifiEndpoints endpoints;
  endpoints.begin(&config, &manager, &scanner, &runtime);
  manager.value.mode = WifiMode::Ap;
  manager.value.apEnabled = true;
  manager.value.apState = WifiApState::Active;

  JsonDocument document;
  Api::Request request = jsonRequest(document, R"({"ssid":"TestSTA","password":"A1b2C3d4"})");

  Api::Response response = endpoints.connect(request);
  expect(response.statusCode == 503 && config.commitCount == 0 && manager.queueCount == 0,
         "unavailable runtime must reject STA connection before queueing");

  runtime.available = true;
  response = endpoints.connect(request);
  expect(response.statusCode == 202 && response.success && config.commitCount == 0 &&
             response.data.c_str() == std::string("{\"state\":\"connecting\"}") &&
             manager.queueCount == 1 && manager.candidate.wifiMode == WifiMode::ApSta &&
             strcmp(manager.candidate.staSsid, "TestSTA") == 0 &&
             strcmp(manager.candidate.staPassword, "A1b2C3d4") == 0,
         "STA connection must queue minimal credentials and retain the management AP");
}

void testWifiConnectPasswordSemantics() {
  ConfigService config;
  config.value.wifiMode = WifiMode::ApSta;
  config.value.staSecurity = StaSecurity::Wpa;
  strlcpy(config.value.staSsid, "SavedSTA", sizeof(config.value.staSsid));
  strlcpy(config.value.staPassword, "SavedPass1", sizeof(config.value.staPassword));
  WifiManager manager;
  WifiScanner scanner;
  RuntimeActionScheduler runtime;
  runtime.available = true;
  WifiEndpoints endpoints;
  endpoints.begin(&config, &manager, &scanner, &runtime);

  JsonDocument reuseDocument;
  Api::Request reuseRequest = jsonRequest(reuseDocument, R"({"ssid":"SavedSTA"})");
  Api::Response response = endpoints.connect(reuseRequest);
  expect(response.statusCode == 202 &&
             manager.candidate.staSecurity == StaSecurity::Wpa &&
             strcmp(manager.candidate.staPassword, "SavedPass1") == 0,
         "omitted password must reuse the saved WPA credential only for the same SSID");

  JsonDocument openDocument;
  Api::Request openRequest = jsonRequest(openDocument, R"({"ssid":"Guest","password":""})");
  response = endpoints.connect(openRequest);
  expect(response.statusCode == 202 && manager.candidate.staSecurity == StaSecurity::Open &&
             manager.candidate.staPassword[0] == '\0',
         "an explicit empty password must select an open network");

  const unsigned queueCount = manager.queueCount;
  JsonDocument invalidDocument;
  Api::Request invalidRequest = jsonRequest(invalidDocument, R"({"ssid":"Office","password":"short"})");
  response = endpoints.connect(invalidRequest);
  expect(response.statusCode == 400 && manager.queueCount == queueCount,
         "an invalid WPA password must be rejected before queueing");
}

void testWifiConnectionStatusHidesInternalTransaction() {
  ConfigService config;
  WifiManager manager;
  WifiScanner scanner;
  RuntimeActionScheduler runtime;
  runtime.available = true;
  WifiEndpoints endpoints;
  endpoints.begin(&config, &manager, &scanner, &runtime);
  manager.testValue.state = WifiTestState::Testing;
  Api::Response response = endpoints.connectionStatus();
  expect(response.success &&
             response.data.c_str() == std::string("{\"state\":\"connecting\",\"failure_code\":\"none\",\"ip\":\"0.0.0.0\",\"ap_shutdown_in_seconds\":0}"),
         "connection status must expose only the simple connecting state");

  manager.testValue.state = WifiTestState::Finalizing;
  manager.testValue.persisted = true;
  manager.testValue.apShutdownInSeconds = 5;
  response = endpoints.connectionStatus();
  expect(response.success &&
             response.data.c_str() == std::string("{\"state\":\"connected\",\"failure_code\":\"none\",\"ip\":\"0.0.0.0\",\"ap_shutdown_in_seconds\":5}"),
         "connection status must expose the committed STA AP-shutdown grace period");

  manager.testValue.apShutdownInSeconds = 0;
  response = endpoints.connectionStatus();
  expect(response.success &&
             response.data.c_str() == std::string("{\"state\":\"connecting\",\"failure_code\":\"none\",\"ip\":\"0.0.0.0\",\"ap_shutdown_in_seconds\":0}"),
         "AP + STA finalization must not report connected before the final AP apply succeeds");

  manager.testValue.state = WifiTestState::Succeeded;
  response = endpoints.connectionStatus();
  expect(response.success &&
             response.data.c_str() == std::string("{\"state\":\"connected\",\"failure_code\":\"none\",\"ip\":\"0.0.0.0\",\"ap_shutdown_in_seconds\":0}"),
         "connection status must report terminal connected after finalization succeeds");
}
}  // namespace

int main() {
  testWebIdentityReportsOnlySourceAndSha256();
  testStorageReportsFlashAppAndUploadCapacity();
  testUploadCapacityKeepsReserveAndAlignment();
  testSystemUpdatePreflightsRuntime();
  testFactoryResetPreflightsRuntime();
  testPasswordUpdatePreflightsRequiredApRestart();
  testPasswordUpdateWithoutApProtectionDoesNotRestart();
  testWifiUpdatePreflightsRuntime();
  testWifiApProtectionChangeSchedulesRestart();
  testWifiApToStaUpdateQueuesSafeTransition();
  testWifiScanBusyAndSuccessPaths();
  testWifiConnectQueuesMinimalCredentials();
  testWifiConnectPasswordSemantics();
  testWifiConnectionStatusHidesInternalTransaction();
  if (failures != 0) {
    std::cerr << failures << " endpoint failure-path test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Endpoint failure-path tests passed\n";
  return EXIT_SUCCESS;
}
