#pragma once

#include <cstddef>
#include <cstring>

struct FlashPartitionCapacity {
  char id[17] = "";
  char type[8] = "";
  char subtype[16] = "";
  size_t offsetBytes = 0;
  size_t sizeBytes = 0;
};

struct AppImageCapacity {
  char partitionId[17] = "";
  bool frontendBundled = false;
  size_t totalBytes = 0;
  size_t imageBytes = 0;
  size_t frontendBytes = 0;
  size_t availableBytes = 0;
};

struct FlashStorageSnapshot {
  static constexpr size_t kMaxPartitions = 8;
  size_t totalBytes = 0;
  size_t bootloaderReservedBytes = 0;
  size_t partitionTableBytes = 0;
  size_t partitionCount = 0;
  FlashPartitionCapacity partitions[kMaxPartitions]{};
  AppImageCapacity app;
};

class FlashStorage {
 public:
  bool readyValue = true;
  bool frontendBundledValue = true;
  size_t frontendBytesValue = 40000;

  bool ready() const { return readyValue; }

  FlashStorageSnapshot snapshot() const {
    FlashStorageSnapshot value;
    value.totalBytes = 4194304;
    value.bootloaderReservedBytes = 32768;
    value.partitionTableBytes = 4096;
    value.partitionCount = 6;
    setPartition(value.partitions[0], "nvs", "data", "nvs", 36864, 20480);
    setPartition(value.partitions[1], "otadata", "data", "ota", 57344, 8192);
    setPartition(value.partitions[2], "app0", "app", "ota_0", 65536, 2031616);
    setPartition(value.partitions[3], "userdata", "data", "spiffs", 2097152, 1998848);
    setPartition(value.partitions[4], "user_nvs", "data", "nvs", 4096000, 32768);
    setPartition(value.partitions[5], "coredump", "data", "coredump", 4128768, 65536);
    std::strcpy(value.app.partitionId, "app0");
    value.app.frontendBundled = frontendBundledValue;
    value.app.totalBytes = 2031616;
    value.app.imageBytes = 1261568;
    value.app.frontendBytes = frontendBytesValue;
    value.app.availableBytes = 770048;
    return value;
  }

 private:
  static void setPartition(FlashPartitionCapacity &partition,
                           const char *id,
                           const char *type,
                           const char *subtype,
                           size_t offset,
                           size_t size) {
    std::strcpy(partition.id, id);
    std::strcpy(partition.type, type);
    std::strcpy(partition.subtype, subtype);
    partition.offsetBytes = offset;
    partition.sizeBytes = size;
  }
};
