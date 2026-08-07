#include "ArduinoWifiDriver.h"

#include <WiFi.h>

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
