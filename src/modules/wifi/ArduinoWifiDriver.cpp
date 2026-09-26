#include "ArduinoWifiDriver.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include "modules/config/model/DeviceConfig.h"

namespace {
wifi_mode_t toArduinoMode(WifiDriverMode mode) {
  switch (mode) {
    case WifiDriverMode::Off: return WIFI_OFF;
    case WifiDriverMode::Sta: return WIFI_STA;
    case WifiDriverMode::Ap: return WIFI_AP;
    case WifiDriverMode::ApSta: return WIFI_AP_STA;
  }
  return WIFI_OFF;
}
}

void ArduinoWifiDriver::setPersistent(bool enabled) {
  WiFi.persistent(enabled);
}

bool ArduinoWifiDriver::setMode(WifiDriverMode mode) {
  return WiFi.mode(toArduinoMode(mode));
}

bool ArduinoWifiDriver::setTxPower(uint8_t configuredDbm) {
  // ESP32-C6 accepts quarter-dBm input in [8,84]. The top input maps to the
  // current platform/country maximum (20 dBm in this SDK's mapping table).
  constexpr int8_t kPlatformMaxQuarterDbm = 84;
  const int8_t sdkPower = configuredDbm == kMaxWifiTxDbm
                              ? kPlatformMaxQuarterDbm
                              : static_cast<int8_t>(configuredDbm * 4U);
  const esp_err_t result = esp_wifi_set_max_tx_power(sdkPower);
  Serial.printf("wifi tx power: configured_dbm=%u, sdk_quarter_dbm=%d, result=%d\n",
                static_cast<unsigned>(configuredDbm),
                static_cast<int>(sdkPower),
                static_cast<int>(result));
  return result == ESP_OK;
}

bool ArduinoWifiDriver::setPowerSave(bool enabled) {
  if (enabled && !normalPowerSaveCaptured_) {
    wifi_ps_type_t current = WIFI_PS_MIN_MODEM;
    const esp_err_t readResult = esp_wifi_get_ps(&current);
    if (readResult != ESP_OK) {
      Serial.printf("wifi power save: read current failed, result=%d\n",
                    static_cast<int>(readResult));
      return false;
    }
    normalPowerSaveMode_ = static_cast<uint8_t>(current);
    normalPowerSaveCaptured_ = true;
  }
  if (!enabled && !normalPowerSaveCaptured_) {
    // No wake-cycle override was applied in this boot. Preserve the Arduino
    // framework's configured default instead of forcing a different mode.
    return true;
  }
  const wifi_ps_type_t target = enabled
      ? WIFI_PS_MIN_MODEM
      : static_cast<wifi_ps_type_t>(normalPowerSaveMode_);
  const esp_err_t result = esp_wifi_set_ps(target);
  if (result == ESP_OK && !enabled) normalPowerSaveCaptured_ = false;
  Serial.printf("wifi power save: mode=%s, sdk_mode=%u, result=%d\n",
                enabled ? "wake_cycle" : "normal_default",
                static_cast<unsigned>(target),
                static_cast<int>(result));
  return result == ESP_OK;
}

bool ArduinoWifiDriver::setHostname(const char *hostname) {
  return WiFi.setHostname(hostname);
}

bool ArduinoWifiDriver::configureStation(
    const IPAddress &localIp,
    const IPAddress &gateway,
    const IPAddress &netmask,
    const IPAddress &dns1,
    const IPAddress &dns2) {
  return WiFi.config(localIp, gateway, netmask, dns1, dns2);
}

void ArduinoWifiDriver::beginStation(
    const char *ssid, const char *password) {
  if (password == nullptr) {
    WiFi.begin(ssid);
  } else {
    WiFi.begin(ssid, password);
  }
}

bool ArduinoWifiDriver::stationConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress ArduinoWifiDriver::stationIp() const {
  return WiFi.localIP();
}

IPAddress ArduinoWifiDriver::stationNetmask() const {
  return WiFi.subnetMask();
}

void ArduinoWifiDriver::disconnectStation(
    bool turnOffRadio, bool eraseCredentials) {
  WiFi.disconnect(turnOffRadio, eraseCredentials);
}

void ArduinoWifiDriver::disconnectStationAsync(
    bool turnOffRadio, bool eraseCredentials) {
  WiFi.disconnectAsync(turnOffRadio, eraseCredentials);
}

bool ArduinoWifiDriver::configureAp(
    const IPAddress &localIp,
    const IPAddress &gateway,
    const IPAddress &netmask,
    const IPAddress &dns) {
  return WiFi.softAPConfig(localIp, gateway, netmask, dns, localIp);
}

bool ArduinoWifiDriver::startAp(
    const char *ssid, const char *password) {
  return WiFi.softAP(ssid, password);
}

bool ArduinoWifiDriver::stopAp(bool turnOffRadio) {
  return WiFi.softAPdisconnect(turnOffRadio);
}

IPAddress ArduinoWifiDriver::apIp() const {
  return WiFi.softAPIP();
}

bool ArduinoWifiDriver::enableDhcpCaptivePortal() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
  return WiFi.AP.enableDhcpCaptivePortal();
#else
  return true;
#endif
}

uint32_t ArduinoMonotonicClock::nowMs() const {
  return millis();
}
