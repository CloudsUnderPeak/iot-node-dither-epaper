#include <Arduino.h>

#include "api/ApiRouter.h"
#include "core/SubsystemRegistry.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/auth/AuthService.h"
#include "modules/captive/CaptivePortalDnsService.h"
#include "modules/config/ConfigService.h"
#include "modules/config/storage/ArduinoPreferencesBackend.h"
#include "modules/config/storage/PreferencesConfigStore.h"
#include "modules/console/ConsoleShell.h"
#include "modules/http/ApiServer.h"
#include "modules/mdns/MdnsService.h"
#include "modules/storage/EmbeddedWebAssets.h"
#include "modules/storage/FlashStorage.h"
#include "modules/storage/StorageLifecycle.h"
#include "modules/storage/UserDataStorage.h"
#include "modules/wifi/WifiManager.h"
#include "modules/wifi/ArduinoWifiDriver.h"
#include "modules/wifi/WifiRadio.h"
#include "modules/wifi/WifiScanner.h"
#include "selftest/FirmwareSelfTest.h"

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN -1
#endif

namespace {
ArduinoPreferencesBackend configBackend;
PreferencesConfigStore configStore(configBackend);
ConfigService configService(configStore);
WifiRadio wifiRadio;
ArduinoWifiDriver wifiDriver;
ArduinoMonotonicClock monotonicClock;
WifiManager wifiManager;
WifiScanner wifiScanner;
EmbeddedWebAssets embeddedWebAssets;
FlashStorage flashStorage;
UserDataStorage userDataStorage;
StorageLifecycle storageLifecycle;
MdnsService mdnsService;
CaptivePortalDnsService captiveDnsService;
AuthService authService;
RuntimeActionScheduler runtimeActions;
ApiRouter apiRouter;
ApiServer apiServer;
ConsoleShell consoleShell;

uint32_t tick = 0;
uint32_t lastHeartbeatMs = 0;

const char *readyLabel(bool ready) {
  return ready ? "READY" : "FAIL";
}

bool wifiHealthy(const WifiStatus &status) {
  if (status.apState == WifiApState::Failed) return false;
  if (status.staState == WifiLinkState::Failed) {
    return status.apState == WifiApState::Active;
  }
  return true;
}

void printWifiStatus(const WifiStatus &status) {
  Serial.printf("wifi: mode=%s, sta_state=%s, sta_ip=%s, ap_ip=%s\n",
                wifiModeToString(status.mode),
                wifiLinkStateToString(status.staState),
                status.staIp.toString().c_str(),
                status.apIp.toString().c_str());
}

Result startUserdata() {
  return storageLifecycle.begin(&userDataStorage);
}

bool userdataHealthy() {
  return userDataStorage.mounted();
}

void reportUserdata(const Result &) {
  const UploadCapacity capacity = userDataStorage.uploadCapacity();
  Serial.printf(
      "userdata: capacity=%u, used=%u, available=%u, max_upload=%u\n",
      static_cast<unsigned>(capacity.totalBytes),
      static_cast<unsigned>(capacity.usedBytes),
      static_cast<unsigned>(capacity.availableBytes),
      static_cast<unsigned>(capacity.maxUploadBytes));
}

Result startConfig() {
  const Result result = configService.begin();
  if (!result.ok() || !configService.ready()) return result;
#if ENABLE_CONFIG_STORE_SELF_TEST
  DeviceConfig config = configService.snapshot();
  if (!FirmwareSelfTest::runConfigStore(config)) {
    return storageError("config store self-test failed");
  }
  return configService.commit(config);
#else
  return result;
#endif
}

bool configHealthy() {
  return configService.ready();
}

void reportConfig(const Result &) {
  const DeviceConfig config = configService.snapshot();
  Serial.printf(
      "config: state=%s, recovery=%s, schema=%u, wifi_mode=%s, hostname=%s\n",
      configStartupStateToString(configService.startupState()),
      configRecoveryReasonToString(configService.recoveryReason()),
      config.schemaVersion,
      wifiModeToString(config.wifiMode),
      config.hostname);
}

Result startWifi() {
  Result result = wifiRadio.begin();
  if (!result.ok()) return result;
  result = wifiManager.begin(&wifiRadio, &wifiDriver, &monotonicClock);
  if (!result.ok()) return result;

  const DeviceConfig config = configService.snapshot();
  WifiStatus status{};
#if ENABLE_WIFI_MODE_SELF_TEST
  return FirmwareSelfTest::runWifiModes(config, wifiManager, status)
             ? okResult()
             : networkError("Wi-Fi mode self-test failed");
#else
  return configService.ready()
             ? wifiManager.apply(config, status)
             : invalidInput("config unavailable");
#endif
}

bool wifiSubsystemHealthy() {
  return wifiHealthy(wifiManager.status());
}

void reportWifi(const Result &) {
  printWifiStatus(wifiManager.status());
}

Result startAssets() {
  return embeddedWebAssets.begin();
}

void reportAssets(const Result &) {
  Serial.printf("assets: files=%u, payload=%u\n",
                static_cast<unsigned>(embeddedWebAssets.count()),
                static_cast<unsigned>(embeddedWebAssets.payloadBytes()));
}

Result startFlashLayout() {
  return flashStorage.begin(
      embeddedWebAssets.payloadBytes(), embeddedWebAssets.bundled());
}

void reportFlashLayout(const Result &) {
  const FlashStorageSnapshot snapshot = flashStorage.snapshot();
  Serial.printf(
      "flash-layout: total=%u, app_capacity=%u, firmware_image=%u, "
      "frontend=%u, app_available=%u\n",
      static_cast<unsigned>(snapshot.totalBytes),
      static_cast<unsigned>(snapshot.app.totalBytes),
      static_cast<unsigned>(snapshot.app.imageBytes),
      static_cast<unsigned>(snapshot.app.frontendBytes),
      static_cast<unsigned>(snapshot.app.availableBytes));
}

Result startMdns() {
  return mdnsService.begin(configService.snapshot(), wifiManager.status());
}

void reportMdns(const Result &) {
  Serial.printf("mdns: host=%s.local\n",
                mdnsService.running() ? mdnsService.hostName() : "disabled");
}

Result startCaptiveDns() {
  return captiveDnsService.begin(wifiManager.status());
}

void reportCaptiveDns(const Result &) {
  Serial.printf(
      "captive-dns: ip=%s\n",
      captiveDnsService.running()
          ? captiveDnsService.captiveIp().toString().c_str()
          : "disabled");
}

Result startAuth() {
  return authService.begin(&configService);
}

Result startWifiScanner() {
  return wifiRadio.ready()
             ? wifiScanner.begin(&wifiRadio)
             : networkError("Wi-Fi radio unavailable");
}

Result startRuntime() {
  return runtimeActions.begin(
      &configService, &wifiManager, &mdnsService, &captiveDnsService);
}

bool runtimeHealthy() {
  return runtimeActions.ready();
}

Result startApiRouter() {
  const ApiRouterDeps deps{
      configService,
      wifiManager,
      wifiScanner,
      embeddedWebAssets,
      flashStorage,
      userDataStorage,
      storageLifecycle,
      authService,
      runtimeActions,
  };
  return apiRouter.begin(deps);
}

Result startHttp() {
  return apiServer.begin(&wifiManager, &embeddedWebAssets, &apiRouter);
}

bool httpHealthy() {
  return apiServer.started();
}

Result startConsole() {
  return consoleShell.begin(
      &configService, &wifiManager, &wifiScanner,
      &flashStorage, &userDataStorage, &authService, &apiRouter);
}

void reportConsole(const Result &) {
  Serial.println("console: type help");
}

enum SubsystemIndex : size_t {
  kUserdataSubsystem,
  kConfigSubsystem,
  kWifiSubsystem,
  kAssetsSubsystem,
  kFlashLayoutSubsystem,
  kMdnsSubsystem,
  kCaptiveDnsSubsystem,
  kAuthSubsystem,
  kWifiScanSubsystem,
  kRuntimeSubsystem,
  kApiSubsystem,
  kHttpSubsystem,
  kConsoleSubsystem,
  kSubsystemCount,
};

Subsystem subsystems[] = {
    {"userdata", startUserdata, userdataHealthy, reportUserdata},
    {"config", startConfig, configHealthy, reportConfig},
    {"wifi", startWifi, wifiSubsystemHealthy, reportWifi},
    {"assets", startAssets, nullptr, reportAssets},
    {"flash_layout", startFlashLayout, nullptr, reportFlashLayout},
    {"mdns_service", startMdns, nullptr, reportMdns},
    {"captive_dns_service", startCaptiveDns, nullptr, reportCaptiveDns},
    {"auth", startAuth},
    {"wifi_scan", startWifiScanner},
    {"runtime", startRuntime, runtimeHealthy},
    {"api", startApiRouter},
    {"http", startHttp, httpHealthy},
    {"console", startConsole, nullptr, reportConsole},
};

static_assert(
    sizeof(subsystems) / sizeof(subsystems[0]) == kSubsystemCount,
    "subsystem index and registry must stay aligned");

void printHeartbeatField(const char *name, const char *value) {
  Serial.print(", ");
  Serial.print(name);
  Serial.print('=');
  Serial.print(value);
}

void printHeartbeatField(const char *name, uint32_t value) {
  Serial.print(", ");
  Serial.print(name);
  Serial.print('=');
  Serial.print(value);
}

void printHeartbeat() {
  const WifiStatus status = wifiManager.status();
  const String captiveIp = captiveDnsService.running()
                               ? captiveDnsService.captiveIp().toString()
                               : String("disabled");
  const String staIp = status.staIp.toString();
  const String apIp = status.apIp.toString();

  Serial.print("alive tick=");
  Serial.print(tick++);
  printHeartbeatField("free_heap", ESP.getFreeHeap());
  for (const Subsystem &subsystem : subsystems) {
    printHeartbeatField(
        subsystem.name,
        readyLabel(subsystemHealthy(subsystem)));
  }
  printHeartbeatField(
      "config_state",
      configStartupStateToString(configService.startupState()));
  printHeartbeatField(
      "config_recovery",
      configRecoveryReasonToString(configService.recoveryReason()));
  printHeartbeatField("captive_dns", captiveIp.c_str());
  printHeartbeatField(
      "mdns",
      mdnsService.running() ? mdnsService.hostName() : "disabled");
  printHeartbeatField("wifi_mode", wifiModeToString(status.mode));
  printHeartbeatField("sta_state", wifiLinkStateToString(status.staState));
  printHeartbeatField("sta_ip", staIp.c_str());
  printHeartbeatField("ap_ip", apIp.c_str());
  Serial.println();
}
}  // namespace

void setup() {
#if STATUS_LED_PIN >= 0
  pinMode(STATUS_LED_PIN, OUTPUT);
#endif

  // The default USB CDC receive queue is smaller than a valid serial API
  // request. Keep the transport queue aligned with ConsoleShell's parser.
  Serial.setRxBufferSize(ConsoleShell::kInputCapacity);
  Serial.begin(115200);
  delay(2000);
  Serial.println("ESP32 Wi-Fi setup firmware");
  Serial.printf("Chip model: %s, revision: %u, cores: %u\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  Serial.printf("Flash: %u MB, free heap: %u bytes\n",
                ESP.getFlashChipSize() / (1024 * 1024), ESP.getFreeHeap());

  for (Subsystem &subsystem : subsystems) {
    const Result result = startSubsystem(subsystem);
    Serial.printf("%s: start: %s (%u), state=%s\n",
                  subsystem.name,
                  result.message,
                  static_cast<unsigned>(result.code),
                  readyLabel(subsystem.ready));
    if (subsystem.report != nullptr) {
      subsystem.report(result);
    }
  }
}

void loop() {
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, tick & 1U);
#endif

  if (subsystemHealthy(subsystems[kRuntimeSubsystem])) {
    runtimeActions.poll();
  }

  const DeviceConfig config = configService.snapshot();
  if (subsystemHealthy(subsystems[kConfigSubsystem]) &&
      wifiManager.poll(config)) {
    const WifiStatus status = wifiManager.status();
    mdnsService.restart(config, status);
    captiveDnsService.restart(status);
  }
  captiveDnsService.poll();
  if (subsystemHealthy(subsystems[kConsoleSubsystem])) {
    consoleShell.poll();
  }

  const uint32_t now = millis();
  if (now - lastHeartbeatMs >= 1000U) {
    lastHeartbeatMs = now;
    printHeartbeat();
  }
  delay(10);
}
