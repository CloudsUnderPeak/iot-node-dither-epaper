#include "FlashStorage.h"

#include <Esp.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include <cstring>

namespace {
constexpr size_t kBootloaderReservedBytes = 0x8000;
constexpr size_t kPartitionTableBytes = 0x1000;
constexpr const char *kPartitionLabels[] = {
    "nvs", "otadata", "app0", "userdata", "user_nvs", "coredump"};

const char *partitionType(const esp_partition_t &partition) {
  if (partition.type == ESP_PARTITION_TYPE_APP) return "app";
  if (partition.type == ESP_PARTITION_TYPE_DATA) return "data";
  return "unknown";
}

const char *partitionSubtype(const esp_partition_t &partition) {
  if (partition.type == ESP_PARTITION_TYPE_APP &&
      partition.subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
    return "ota_0";
  }
  if (partition.type != ESP_PARTITION_TYPE_DATA) return "unknown";
  switch (partition.subtype) {
    case ESP_PARTITION_SUBTYPE_DATA_NVS:
      return "nvs";
    case ESP_PARTITION_SUBTYPE_DATA_OTA:
      return "ota";
    case ESP_PARTITION_SUBTYPE_DATA_SPIFFS:
      return "spiffs";
    case ESP_PARTITION_SUBTYPE_DATA_COREDUMP:
      return "coredump";
    default:
      return "unknown";
  }
}

void copyPartition(const esp_partition_t &source,
                   FlashPartitionCapacity &target) {
  strlcpy(target.id, source.label, sizeof(target.id));
  strlcpy(target.type, partitionType(source), sizeof(target.type));
  strlcpy(target.subtype, partitionSubtype(source), sizeof(target.subtype));
  target.offsetBytes = source.address;
  target.sizeBytes = source.size;
}
}  // namespace

Result FlashStorage::begin(size_t frontendBytes, bool frontendBundled) {
  frontendBytes_ = frontendBytes;
  frontendBundled_ = frontendBundled;
  const esp_partition_t *running = esp_ota_get_running_partition();
  ready_ = running != nullptr &&
           strcmp(running->label, "app0") == 0 &&
           ESP.getSketchSize() <= running->size &&
           frontendBytes_ <= ESP.getSketchSize();
  return ready_ ? okResult()
                : storageError("running application layout is invalid");
}

bool FlashStorage::ready() const {
  return ready_;
}

FlashStorageSnapshot FlashStorage::snapshot() const {
  FlashStorageSnapshot value;
  value.totalBytes = ESP.getFlashChipSize();
  value.bootloaderReservedBytes = kBootloaderReservedBytes;
  value.partitionTableBytes = kPartitionTableBytes;

  for (const char *label : kPartitionLabels) {
    if (value.partitionCount >= FlashStorageSnapshot::kMaxPartitions) break;
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, label);
    if (partition == nullptr) continue;
    copyPartition(*partition, value.partitions[value.partitionCount++]);
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  if (!ready_ || running == nullptr) return value;
  strlcpy(value.app.partitionId, running->label,
          sizeof(value.app.partitionId));
  value.app.frontendBundled = frontendBundled_;
  value.app.totalBytes = running->size;
  value.app.imageBytes = ESP.getSketchSize();
  value.app.frontendBytes = frontendBytes_;
  value.app.availableBytes = value.app.totalBytes > value.app.imageBytes
                                 ? value.app.totalBytes - value.app.imageBytes
                                 : 0;
  return value;
}
