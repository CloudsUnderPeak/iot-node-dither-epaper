#include "DeviceEndpoints.h"

#include <esp_mac.h>

#include "api/shared/ApiResponse.h"

namespace DeviceEndpoints {

Api::Response get(const ConfigService &configService,
                  const BatteryMonitor &batteryMonitor) {
  const DeviceConfig config = configService.snapshot();
  const BatterySnapshot batterySnapshot = batteryMonitor.snapshot(millis());
  const uint32_t heapTotal = ESP.getHeapSize();
  const uint32_t heapFree = ESP.getFreeHeap();
  const uint32_t heapUsedPercent = heapTotal > 0 ? ((heapTotal - heapFree) * 100U) / heapTotal : 0;

  uint8_t mac[6] = {};
  char macText[18] = "00:00:00:00:00:00";
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    snprintf(macText, sizeof(macText), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  JsonDocument data;
  data["chip_model"] = ESP.getChipModel();
  data["chip_revision"] = ESP.getChipRevision();
  data["cpu_cores"] = ESP.getChipCores();
  data["flash_mb"] = ESP.getFlashChipSize() / (1024 * 1024);
  data["heap_used_percent"] = heapUsedPercent;
  data["mac_address"] = macText;
  data["hostname"] = config.hostname;
  data["wifi_tx_dbm"] = config.wifiTxDbm;
  data["config_state"] = configStartupStateToString(configService.startupState());
  data["config_recovery_reason"] = configRecoveryReasonToString(configService.recoveryReason());
  JsonObject power = data["power"].to<JsonObject>();
  JsonObject battery = power["battery"].to<JsonObject>();
  if (batterySnapshot.sampleValid) {
    battery["voltage_mv"] = batterySnapshot.voltageMilliVolts;
    battery["sample_age_ms"] = batterySnapshot.sampleAgeMs;
  } else {
    battery["voltage_mv"] = nullptr;
    battery["sample_age_ms"] = nullptr;
  }
  if (batterySnapshot.estimate.available) {
    battery["estimated_percent"] = batterySnapshot.estimate.percent;
  } else {
    battery["estimated_percent"] = nullptr;
  }
  return Api::ok(Api::json(data));
}

}  // namespace DeviceEndpoints
