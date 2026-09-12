#include "ArduinoWifiScanDriver.h"
#include <WiFi.h>
#include <esp_wifi.h>

namespace {
const char *encryptionTypeToString(wifi_auth_mode_t authMode) {
  switch (authMode) {
    case WIFI_AUTH_OPEN: return "open";
    case WIFI_AUTH_WEP: return "wep";
    case WIFI_AUTH_WPA_PSK: return "wpa";
    case WIFI_AUTH_WPA2_PSK: return "wpa2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "wpa_wpa2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "wpa2_enterprise";
    case WIFI_AUTH_WPA3_PSK: return "wpa3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "wpa2_wpa3";
    default: return "unknown";
  }
}
}  // namespace


int ArduinoWifiScanDriver::start() {
  if (!listening_) {
    WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t) { done_.store(true); }, ARDUINO_EVENT_WIFI_SCAN_DONE);
    listening_ = true;
  }
  done_.store(false);
  // Application timeout/stop ACK occurs first; Arduino must not discard its
  // scanning bit before the owner receives the actual completion event.
  WiFi.setScanTimeout(20000);
  return WiFi.scanNetworks(true, true);
}
int ArduinoWifiScanDriver::completion() { return WiFi.scanComplete(); }
bool ArduinoWifiScanDriver::requestStop() { return esp_wifi_scan_stop() == ESP_OK; }
void ArduinoWifiScanDriver::clearResults() { WiFi.scanDelete(); }
WifiScanNetwork ArduinoWifiScanDriver::network(size_t index) {
  WifiScanNetwork value;
  value.ssid = WiFi.SSID(index);
  value.rssi = WiFi.RSSI(index);
  value.channel = WiFi.channel(index);
  const auto encryption = WiFi.encryptionType(index);
  value.encryptionType = static_cast<int>(encryption);
  value.encryption = encryptionTypeToString(encryption);
  value.hidden = value.ssid.length() == 0;
  return value;
}
