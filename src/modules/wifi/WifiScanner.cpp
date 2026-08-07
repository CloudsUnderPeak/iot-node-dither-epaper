#include "WifiScanner.h"

#include <WiFi.h>

#include "core/SemaphoreGuard.h"

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

Result WifiScanner::begin(WifiRadio *radio) {
  if (radio == nullptr) return invalidInput("missing Wi-Fi radio");
  radio_ = radio;
  mutex_ = xSemaphoreCreateMutex();
  return mutex_ == nullptr ? outOfSpace("failed to create Wi-Fi scan mutex") : okResult();
}

WifiScanResult WifiScanner::scan() {
  WifiScanResult outcome;
  SemaphoreGuard scanGuard(mutex_, portMAX_DELAY);
  if (!scanGuard.locked()) {
    outcome.result = networkError("wifi scan unavailable");
    return outcome;
  }

  const uint32_t now = millis();
  if (hasRun_ && now - lastCompletedMs_ < kMinIntervalMs) {
    const uint32_t remainingMs = kMinIntervalMs - (now - lastCompletedMs_);
    outcome.result = unsupported("wifi scan rate limited");
    outcome.retryAfterSeconds = (remainingMs + 999U) / 1000U;
    return outcome;
  }

  WifiRadioGuard radioGuard(radio_, portMAX_DELAY);
  if (!radioGuard.locked()) {
    outcome.result = networkError("Wi-Fi radio unavailable");
    return outcome;
  }

  const int networkCount = WiFi.scanNetworks(false, true);
  hasRun_ = true;
  lastCompletedMs_ = millis();
  if (networkCount < 0) {
    outcome.result = networkError("wifi scan failed");
    return outcome;
  }

  int selected[kWifiScanMaxResults] = {};
  for (int index = 0; index < networkCount; ++index) {
    const int32_t rssi = WiFi.RSSI(index);
    if (rssi <= kWifiScanMinRssi) {
      continue;
    }
    size_t position = 0;
    while (position < outcome.count && WiFi.RSSI(selected[position]) >= rssi) {
      ++position;
    }
    if (position >= kWifiScanMaxResults) {
      continue;
    }
    const size_t newCount = outcome.count < kWifiScanMaxResults ? outcome.count + 1 : outcome.count;
    for (size_t cursor = newCount - 1; cursor > position; --cursor) {
      selected[cursor] = selected[cursor - 1];
    }
    selected[position] = index;
    outcome.count = newCount;
  }

  for (size_t resultIndex = 0; resultIndex < outcome.count; ++resultIndex) {
    const int sourceIndex = selected[resultIndex];
    WifiScanNetwork &network = outcome.networks[resultIndex];
    network.ssid = WiFi.SSID(sourceIndex);
    network.rssi = WiFi.RSSI(sourceIndex);
    network.channel = WiFi.channel(sourceIndex);
    const wifi_auth_mode_t encryptionType = WiFi.encryptionType(sourceIndex);
    network.encryptionType = static_cast<int>(encryptionType);
    network.encryption = encryptionTypeToString(encryptionType);
    network.hidden = network.ssid.length() == 0;
  }
  WiFi.scanDelete();
  return outcome;
}
