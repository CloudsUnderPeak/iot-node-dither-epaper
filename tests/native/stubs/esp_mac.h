#pragma once

#include <cstdint>
#include <cstring>

constexpr int ESP_OK = 0;
constexpr int ESP_MAC_WIFI_STA = 0;

inline int esp_read_mac(uint8_t *mac, int) {
  const uint8_t fixedMac[6] = {0x02, 0x00, 0x00, 0x00, 0x12, 0x34};
  memcpy(mac, fixedMac, sizeof(fixedMac));
  return ESP_OK;
}
