#pragma once

#include <Arduino.h>

#include "../../core/Result.h"

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

// Reports the flashed partition table and current app-image usage.
class FlashStorage {
 public:
  Result begin(size_t frontendBytes, bool frontendBundled);
  bool ready() const;
  FlashStorageSnapshot snapshot() const;

 private:
  size_t frontendBytes_ = 0;
  bool frontendBundled_ = false;
  bool ready_ = false;
};
