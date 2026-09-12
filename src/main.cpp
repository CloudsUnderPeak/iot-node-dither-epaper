#include <Arduino.h>
#include <SPI.h>
#include <esp_system.h>

#include "api/ApiRouter.h"
#include "board/BoardProfile.h"
#include "core/SubsystemRegistry.h"
#include "modules/epaper/Epd7In3E.h"
#include "modules/epaper/EpdSpiTransport.h"
#include "modules/epaper/ArduinoCpuFrequencyDriver.h"
#include "modules/epaper/ArduinoEpaperSafetyStorage.h"
#include "modules/epaper/ArduinoRestartDriver.h"
#include "modules/epaper/EpaperSafetyStore.h"
#include "modules/epaper/EpaperShutdownCoordinator.h"
#include "modules/epaper/EpaperCooldown.h"
#include "modules/epaper/EpaperPowerProbe.h"
#include "modules/epaper/EpaperRefreshProbe.h"
#include "modules/epaper/EpaperService.h"
#include "modules/epaper/calibration/EpaperCalibrationService.h"
#include "modules/epaper/calibration/storage/PreferencesEpaperCalibrationStore.h"
#include "modules/hardware/EpaperHardware.h"
#include "modules/hardware/PinRegistry.h"
#include "modules/hardware/SpiBus.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/auth/AuthService.h"
#include "modules/captive/CaptivePortalDnsService.h"
#include "modules/config/ConfigService.h"
#include "modules/config/storage/ArduinoPreferencesBackend.h"
#include "modules/config/storage/PreferencesConfigStore.h"
#include "modules/console/ConsoleShell.h"
#include "modules/http/ApiServer.h"
#include "modules/mdns/MdnsService.h"
#include "modules/power/ArduinoBatteryAdc.h"
#include "modules/power/BatteryMonitor.h"
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

#ifndef ENABLE_EPAPER_PANEL_SELF_TEST
#define ENABLE_EPAPER_PANEL_SELF_TEST 0
#endif

#ifndef ENABLE_EPAPER_REFRESH_SELF_TEST
#define ENABLE_EPAPER_REFRESH_SELF_TEST 0
#endif

#ifndef ENABLE_EPAPER_CONFIRMED_POWER_CYCLE_RECOVERY
#define ENABLE_EPAPER_CONFIRMED_POWER_CYCLE_RECOVERY 0
#endif

#if ENABLE_EPAPER_PANEL_SELF_TEST && ENABLE_EPAPER_REFRESH_SELF_TEST
#error "Only one e-paper hardware self-test may be enabled"
#endif

namespace {
PinRegistry pinRegistry;
SpiBus spiBus;
EpdSpiTransport epdTransport;
Epd7In3E epdDriver;
ArduinoCpuFrequencyDriver epaperCpuFrequency;
ArduinoEpaperSafetyStorage epaperSafetyStorage;
EpaperSafetyStore epaperSafetyStore;
ArduinoRestartDriver restartDriver;
EpaperShutdownCoordinator epaperShutdownCoordinator;
EpaperCooldown epaperCooldown;
EpaperPowerProbe epaperPowerProbe;
EpaperRefreshProbe epaperRefreshProbe;
EpaperPaletteFrameSource epaperPaletteFrame;
EpaperService epaperService;
ArduinoPreferencesBackend epaperCalibrationBackend;
PreferencesEpaperCalibrationStore epaperCalibrationStore(epaperCalibrationBackend);
EpaperCalibrationService epaperCalibrationService(epaperCalibrationStore);
ArduinoBatteryAdc batteryAdc;
BatteryMonitor batteryMonitor;
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
bool epaperPowerCycleRecovered = false;
const char *epaperRecoveryEvidence = "none";

const char *readyLabel(bool ready) {
  return ready ? "READY" : "FAIL";
}

const char *epaperBusyLabel() {
  if (!epdTransport.ready()) return "unavailable";
  return epdTransport.busyHigh() ? "high_idle" : "low_busy";
}

bool wifiHealthy(const WifiStatus &status) {
  if (status.apState == WifiApState::Failed) return false;
  if (status.staState == WifiLinkState::Failed) {
    return status.apState == WifiApState::Active;
  }
  return true;
}

Result startEpaperHardware() {
  Result result = EpaperHardware::claimAndQuiescePins(&pinRegistry);
  if (!result.ok()) return result;
  result = spiBus.begin(&SPI, &pinRegistry, Board::ActiveProfile::kSpi);
  if (!result.ok()) return result;
  result = epdTransport.begin(&spiBus, &pinRegistry);
  if (!result.ok()) return result;
  if (!epdDriver.begin(&epdTransport)) {
    return invalidInput("e-paper driver logical quiesce failed");
  }
  if (!epaperSafetyStore.begin(&epaperSafetyStorage)) {
    return storageError("e-paper safety marker unavailable");
  }
  if (epaperSafetyStore.stage() == EpaperProtectionStage::Active) {
    const bool powerOnReset = esp_reset_reason() == ESP_RST_POWERON;
    const bool confirmedRecovery =
        powerOnReset || ENABLE_EPAPER_CONFIRMED_POWER_CYCLE_RECOVERY;
    if (confirmedRecovery) {
      if (!epaperSafetyStore.recoverActiveAfterConfirmedPowerCycle()) {
        return storageError("e-paper power-cycle marker recovery failed");
      }
      epaperPowerCycleRecovered = true;
      epaperRecoveryEvidence = powerOnReset ? "power_on_reset" : "build_confirmed";
    }
  }
  if (!epaperShutdownCoordinator.begin(
          &epdDriver, &epaperSafetyStore, &restartDriver)) {
    return invalidInput("e-paper shutdown coordinator unavailable");
  }
  return epaperShutdownCoordinator.unavailable()
             ? storageError("e-paper active marker requires power-cycle recovery")
             : okResult();
}

bool epaperHardwareHealthy() {
  const Epd7In3E::State driverState = epdDriver.state();
  return spiBus.ready() && epdTransport.ready() &&
         (driverState == Epd7In3E::State::Quiesced ||
          driverState == Epd7In3E::State::Sleeping) &&
         epaperSafetyStore.ready() && epaperShutdownCoordinator.ready() &&
         !epaperShutdownCoordinator.unavailable();
}

void reportEpaperHardware(const Result &) {
  Serial.printf(
      "epaper-hardware: board=%s, spi=%d/%d/%d, cs=%d, dc=%d, rst=%d, "
      "busy=%d, busy_level=%s, marker=%s, cpu_mhz=%u, "
      "recovered=%s, recovery_evidence=%s, "
      "state=logical_quiesce, refresh=disabled\n",
      Board::ActiveProfile::kBoardId,
      Board::ActiveProfile::kSpi.sck,
      Board::ActiveProfile::kSpi.mosi,
      Board::ActiveProfile::kSpi.miso,
      Board::ActiveProfile::kEpaper.cs,
      Board::ActiveProfile::kEpaper.dc,
      Board::ActiveProfile::kEpaper.reset,
      Board::ActiveProfile::kEpaper.busy,
      epaperBusyLabel(),
      epaperProtectionStageToString(epaperSafetyStore.stage()),
      static_cast<unsigned>(epaperCpuFrequency.currentMhz()),
      epaperPowerCycleRecovered ? "yes" : "no",
      epaperRecoveryEvidence);
}

void runEpaperPanelSelfTest() {
#if ENABLE_EPAPER_PANEL_SELF_TEST || ENABLE_EPAPER_REFRESH_SELF_TEST
  if (epaperPowerCycleRecovered) {
    Serial.println(
        "epaper-test: skipped after power-cycle recovery, automatic retry=disabled");
    return;
  }
#endif
#if ENABLE_EPAPER_PANEL_SELF_TEST
  Serial.println(
      "epaper-self-test: armed in 5000 ms, power-only, refresh=disabled");
  delay(5000);
  Serial.printf(
      "epaper-self-test: start marker=%s, cpu_mhz=%u, busy=%s, refresh=disabled\n",
      epaperProtectionStageToString(epaperSafetyStore.stage()),
      static_cast<unsigned>(epaperCpuFrequency.currentMhz()),
      epaperBusyLabel());
  const bool success = epaperPowerProbe.run(
      &epdDriver, &epdTransport, &epaperSafetyStore, &epaperCpuFrequency,
      &epaperShutdownCoordinator);
  if (success) epaperCooldown.begin(millis());
  Serial.printf(
      "epaper-self-test: result=%s, driver_error=%u, shutdown=%s, marker=%s, "
      "cpu_mhz=%u, busy=%s, refresh=disabled\n",
      epaperPowerProbeResultToString(epaperPowerProbe.result()),
      static_cast<unsigned>(epaperPowerProbe.driverError()),
      epaperShutdownOutcomeToString(epaperShutdownCoordinator.lastOutcome()),
      epaperProtectionStageToString(epaperSafetyStore.stage()),
      static_cast<unsigned>(epaperCpuFrequency.currentMhz()),
      epaperBusyLabel());
#elif ENABLE_EPAPER_REFRESH_SELF_TEST
  Serial.println(
      "epaper-refresh-test: armed in 5000 ms, pattern=palette, refresh=once");
  delay(5000);
  Serial.printf(
      "epaper-refresh-test: start marker=%s, cpu_mhz=%u, busy=%s, "
      "frame_bytes=%u\n",
      epaperProtectionStageToString(epaperSafetyStore.stage()),
      static_cast<unsigned>(epaperCpuFrequency.currentMhz()),
      epaperBusyLabel(),
      static_cast<unsigned>(EpaperImageFormat::kFrameBytes));
  const bool success = epaperRefreshProbe.run(
      &epdDriver, &epdTransport, &epaperPaletteFrame, &epaperSafetyStore,
      &epaperCpuFrequency, &epaperShutdownCoordinator);
  if (success) epaperCooldown.begin(millis());
  Serial.printf(
      "epaper-refresh-test: result=%s, driver_error=%u, transferred=%u, "
      "shutdown=%s, marker=%s, cpu_mhz=%u, busy=%s\n",
      epaperRefreshProbeResultToString(epaperRefreshProbe.result()),
      static_cast<unsigned>(epaperRefreshProbe.driverError()),
      static_cast<unsigned>(epaperRefreshProbe.transferredBytes()),
      epaperShutdownOutcomeToString(epaperShutdownCoordinator.lastOutcome()),
      epaperProtectionStageToString(epaperSafetyStore.stage()),
      static_cast<unsigned>(epaperCpuFrequency.currentMhz()),
      epaperBusyLabel());
#endif
}

void printWifiStatus(const WifiStatus &status) {
  const DeviceConfig config = configService.snapshot();
  Serial.printf(
      "wifi: mode=%s, sta_state=%s, sta_ip=%s, ap_ip=%s, "
      "wifi_tx_dbm=%u\n",
                wifiModeToString(status.mode),
                wifiLinkStateToString(status.staState),
                status.staIp.toString().c_str(),
                status.apIp.toString().c_str(),
                static_cast<unsigned>(config.wifiTxDbm));
}

Result startUserdata() {
  return storageLifecycle.begin(&userDataStorage);
}

Result startEpaperService() {
  return epaperService.begin(
      &userDataStorage, &epdDriver, &epdTransport, &epaperSafetyStore,
      &epaperCpuFrequency, &epaperShutdownCoordinator);
}

Result startEpaperCalibration() {
  return epaperCalibrationService.begin();
}

bool epaperCalibrationHealthy() {
  return epaperCalibrationService.ready();
}

bool epaperServiceHealthy() {
  return epaperService.ready() &&
         epaperService.snapshot(millis()).state != EpaperServiceState::Unavailable;
}

bool userdataHealthy() {
  return userDataStorage.mounted();
}

Result startBatteryMonitor() {
  return batteryMonitor.begin(&batteryAdc, millis());
}

bool batteryMonitorHealthy() {
  return batteryMonitor.ready();
}

void reportBatteryMonitor(const Result &) {
  const BatterySnapshot snapshot = batteryMonitor.snapshot(millis());
  Serial.printf("battery: pin=%d, voltage_mv=%u, estimated_percent=%d\n",
                Board::ActiveProfile::kBatterySense.pin,
                static_cast<unsigned>(snapshot.voltageMilliVolts),
                snapshot.estimate.available
                    ? static_cast<int>(snapshot.estimate.percent)
                    : -1);
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
      &configService, &wifiManager, &mdnsService, &captiveDnsService,
      &epaperShutdownCoordinator);
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
      epaperService,
      epaperCalibrationService,
      batteryMonitor,
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
  kEpaperHardwareSubsystem,
  kUserdataSubsystem,
  kEpaperServiceSubsystem,
  kEpaperCalibrationSubsystem,
  kBatterySubsystem,
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
    {"epaper_hardware", startEpaperHardware, epaperHardwareHealthy,
     reportEpaperHardware},
    {"userdata", startUserdata, userdataHealthy, reportUserdata},
    {"epaper", startEpaperService, epaperServiceHealthy},
    {"epaper_calibration", startEpaperCalibration,
     epaperCalibrationHealthy},
    {"battery", startBatteryMonitor, batteryMonitorHealthy,
     reportBatteryMonitor},
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
  printHeartbeatField("epaper_busy", epaperBusyLabel());
  printHeartbeatField(
      "epaper_marker",
      epaperProtectionStageToString(epaperSafetyStore.stage()));
  printHeartbeatField("cpu_mhz", epaperCpuFrequency.currentMhz());
  printHeartbeatField(
      "epaper_cooldown_seconds",
      epaperService.ready()
          ? epaperService.snapshot(millis()).retryAfterSeconds
          : epaperCooldown.retryAfterSeconds(millis()));
  const BatterySnapshot battery = batteryMonitor.snapshot(millis());
  printHeartbeatField(
      "battery_mv", battery.sampleValid ? battery.voltageMilliVolts : 0);
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
  // Establish CS high, DC low, and inactive-high RST before Serial startup
  // delays or any network/storage subsystem. This is logical quiesce only; it
  // never sends a panel command and must not be reported as Power OFF or Deep
  // Sleep.
  const Result epaperHardwareResult =
      startSubsystem(subsystems[kEpaperHardwareSubsystem]);

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

  const Subsystem &epaperHardware = subsystems[kEpaperHardwareSubsystem];
  Serial.printf("%s: start: %s (%u), state=%s\n",
                epaperHardware.name,
                epaperHardwareResult.message,
                static_cast<unsigned>(epaperHardwareResult.code),
                readyLabel(epaperHardware.ready));
  if (epaperHardware.report != nullptr) {
    epaperHardware.report(epaperHardwareResult);
  }

  // Diagnostic probes run before storage and network startup so their load is
  // isolated from Wi-Fi. A production build compiles this call to a no-op.
  runEpaperPanelSelfTest();

  for (size_t index = kUserdataSubsystem; index < kSubsystemCount; ++index) {
    Subsystem &subsystem = subsystems[index];
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

  const uint32_t now = millis();
  if (epaperService.ready()) epaperService.poll(now);
  const bool epaperDrawing =
      epaperService.ready() &&
      epaperService.snapshot(now).state == EpaperServiceState::Drawing;
  if (!epaperDrawing &&
      subsystemHealthy(subsystems[kBatterySubsystem])) {
    batteryMonitor.poll(now);
  }
  if (epaperCooldown.elapsed(now)) {
    const bool markerCleared = epaperSafetyStore.clear();
    epaperCooldown.releaseIfElapsed(now, markerCleared);
  }

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

  if (Serial && now - lastHeartbeatMs >= 1000U) {
    lastHeartbeatMs = now;
    printHeartbeat();
  }
  delay(10);
}
