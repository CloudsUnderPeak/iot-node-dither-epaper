#pragma once

#include <Arduino.h>

#include "core/Result.h"

enum class StorageResetScope : uint8_t {
  Settings,
  Data,
  All,
};

class StorageLifecycle {
 public:
  bool available = true;
  Result requestResult = okResult();
  unsigned requestCount = 0;
  StorageResetScope lastScope = StorageResetScope::Settings;

  Result requestReset(StorageResetScope scope) {
    ++requestCount;
    lastScope = scope;
    return available ? requestResult : storageError("storage lifecycle unavailable");
  }
};
