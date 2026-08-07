#pragma once

#include <Arduino.h>

#include "../../core/Result.h"
#include "UserDataStorage.h"

enum class StorageResetScope : uint8_t {
  Settings,
  Data,
  All,
};

// Owns persistent initialization and reset intent stored in the default NVS
// partition. The user_nvs and userdata partitions never depend on each other.
class StorageLifecycle {
 public:
  Result begin(UserDataStorage *userData);
  Result requestReset(StorageResetScope scope);
  bool ready() const;

 private:
  bool ready_ = false;

  Result resetUserNvs();
  Result readMetadata(bool &userNvsInitialized,
                      bool &userDataInitialized,
                      String &pending,
                      bool &pendingStored) const;
  Result writeInitialized(const char *key, bool initialized) const;
  Result writePending(const char *pending) const;
};
