#include "FirmwareSelfTest.h"

#if ENABLE_CONFIG_STORE_SELF_TEST || ENABLE_WIFI_MODE_SELF_TEST
#include "../modules/config/storage/ArduinoPreferencesBackend.h"
#include "../modules/config/storage/PreferencesConfigStore.h"
#endif

#if ENABLE_WIFI_MODE_SELF_TEST
#if __has_include("../secrets.local.h")
#include "../secrets.local.h"
#endif
#endif

namespace FirmwareSelfTest {

#if ENABLE_CONFIG_STORE_SELF_TEST || ENABLE_WIFI_MODE_SELF_TEST
namespace {
void printResult(const char *scope, const char *label, const Result &result) {
  Serial.printf("self-test %s: %s: %s (%u)\n",
                scope,
                label,
                result.message,
                static_cast<unsigned>(result.code));
}
}
#endif

#if ENABLE_CONFIG_STORE_SELF_TEST
bool runConfigStore(DeviceConfig &activeConfig) {
  // Config persistence: load current settings or create defaults.
  ArduinoPreferencesBackend backend;
  PreferencesConfigStore store(backend);
  DeviceConfig config{};

  Result loadResult = store.load(config);
  if (!loadResult.ok()) {
    printResult("config-store", "load existing", loadResult);
    config = defaultDeviceConfig();
    Result saveDefaultResult = store.save(config);
    printResult("config-store", "save defaults", saveDefaultResult);
    if (!saveDefaultResult.ok()) {
      return false;
    }
  }

  // Config persistence: prove a validated config survives a save/reload cycle.
  Result saveResult = store.save(config);
  printResult("config-store", "save current config", saveResult);
  if (!saveResult.ok()) {
    return false;
  }

  DeviceConfig reloaded{};
  Result reloadResult = store.load(reloaded);
  printResult("config-store", "reload saved config", reloadResult);
  if (!reloadResult.ok()) {
    return false;
  }

  // Config validation: unknown Wi-Fi modes must be rejected before persistence.
  DeviceConfig invalid = reloaded;
  invalid.wifiMode = static_cast<WifiMode>(99);
  Result invalidResult = validateDeviceConfig(invalid);
  printResult("config-store", "reject invalid wifi mode", invalidResult);
  if (invalidResult.ok()) {
    return false;
  }

  activeConfig = reloaded;
  Serial.printf("self-test config-store: schema=%u, wifi_mode=%s, hostname=%s\n",
                activeConfig.schemaVersion,
                wifiModeToString(activeConfig.wifiMode),
                activeConfig.hostname);
  return true;
}
#endif

#if ENABLE_WIFI_MODE_SELF_TEST
namespace {
bool provisionLocalStaCredentials(DeviceConfig &config) {
#if defined(LOCAL_WIFI_STA_SSID) && defined(LOCAL_WIFI_STA_PASSWORD)
  config.wifiMode = WifiMode::Sta;
  strlcpy(config.staSsid, LOCAL_WIFI_STA_SSID, sizeof(config.staSsid));
  strlcpy(config.staPassword, LOCAL_WIFI_STA_PASSWORD, sizeof(config.staPassword));

  ArduinoPreferencesBackend backend;
  PreferencesConfigStore store(backend);
  Result saveResult = store.save(config);
  printResult("wifi-modes", "provision local STA credentials", saveResult);
  return saveResult.ok();
#else
  Serial.println("self-test wifi-modes: no local STA credentials configured");
  return false;
#endif
}

void printWifiStatus(const WifiStatus &status) {
  Serial.printf("self-test wifi-modes: mode=%s, sta_state=%s, sta_ip=%s, ap_ip=%s\n",
                wifiModeToString(status.mode),
                wifiLinkStateToString(status.staState),
                status.staIp.toString().c_str(),
                status.apIp.toString().c_str());
}

bool isNonZeroIp(const IPAddress &ip) {
  return ip != IPAddress(0, 0, 0, 0);
}

bool applyAndCheckWifiMode(WifiManager &wifiManager,
                           const DeviceConfig &config,
                           bool expectStaConnected,
                           bool expectApStarted) {
  WifiStatus status{};
  Result applyResult = wifiManager.apply(config, status);
  Serial.printf("self-test wifi-modes: apply %s: %s (%u)\n",
                wifiModeToString(config.wifiMode),
                applyResult.message,
                static_cast<unsigned>(applyResult.code));
  printWifiStatus(status);

  const bool staOk = expectStaConnected
                         ? status.staState == WifiLinkState::Connected && isNonZeroIp(status.staIp)
                         : status.staState == WifiLinkState::Disabled;
  const bool apOk = expectApStarted ? isNonZeroIp(status.apIp) : true;
  return applyResult.ok() && staOk && apOk;
}
}

bool runWifiModes(DeviceConfig &activeConfig, WifiManager &wifiManager, WifiStatus &wifiStatus) {
  // Wi-Fi credential setup: require local credentials because STA validation
  // cannot be meaningfully exercised without a real network.
  DeviceConfig wifiConfig = activeConfig;
  if (!provisionLocalStaCredentials(wifiConfig)) {
    return false;
  }

  ArduinoPreferencesBackend backend;
  PreferencesConfigStore store(backend);
  DeviceConfig persisted{};
  Result loadResult = store.load(persisted);
  printResult("wifi-modes", "reload provisioned config", loadResult);
  if (!loadResult.ok()) {
    return false;
  }

  activeConfig = persisted;
  bool allModesPassed = true;

  // Wi-Fi mode matrix: verify the observable contract for each supported mode.
  DeviceConfig offConfig = persisted;
  offConfig.wifiMode = WifiMode::Off;
  allModesPassed = applyAndCheckWifiMode(wifiManager, offConfig, false, false) && allModesPassed;

  DeviceConfig staConfig = persisted;
  staConfig.wifiMode = WifiMode::Sta;
  allModesPassed = applyAndCheckWifiMode(wifiManager, staConfig, true, false) && allModesPassed;

  DeviceConfig apConfig = persisted;
  apConfig.wifiMode = WifiMode::Ap;
  allModesPassed = applyAndCheckWifiMode(wifiManager, apConfig, false, true) && allModesPassed;

  DeviceConfig apStaConfig = persisted;
  apStaConfig.wifiMode = WifiMode::ApSta;
  allModesPassed = applyAndCheckWifiMode(wifiManager, apStaConfig, true, true) && allModesPassed;

  // Wi-Fi recovery: restore the persisted config after destructive mode cycling.
  Result saveFinalResult = store.save(persisted);
  printResult("wifi-modes", "restore final config", saveFinalResult);
  if (!saveFinalResult.ok()) {
    return false;
  }

  Result finalApplyResult = wifiManager.apply(persisted, wifiStatus);
  Serial.printf("self-test wifi-modes: final apply %s: %s (%u)\n",
                wifiModeToString(persisted.wifiMode),
                finalApplyResult.message,
                static_cast<unsigned>(finalApplyResult.code));
  printWifiStatus(wifiStatus);

  return allModesPassed && finalApplyResult.ok() && wifiStatus.staState == WifiLinkState::Connected;
}
#endif

}  // namespace FirmwareSelfTest
