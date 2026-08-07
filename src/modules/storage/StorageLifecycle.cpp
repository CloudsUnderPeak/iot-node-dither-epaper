#include "StorageLifecycle.h"

#include <Preferences.h>
#include <nvs_flash.h>

namespace {
constexpr const char *kSystemNvsPartition = "nvs";
constexpr const char *kUserNvsPartition = "user_nvs";
constexpr const char *kMetadataNamespace = "storage_meta";
constexpr const char *kUserNvsInitializedKey = "user_nvs_init";
constexpr const char *kUserDataInitializedKey = "userdata_init";
constexpr const char *kResetPendingKey = "reset_pending";
constexpr const char *kPendingNone = "none";
constexpr const char *kPendingSettings = "settings";
constexpr const char *kPendingData = "data";
constexpr const char *kPendingAll = "all";

const char *pendingValue(StorageResetScope scope) {
  switch (scope) {
    case StorageResetScope::Settings: return kPendingSettings;
    case StorageResetScope::Data: return kPendingData;
    case StorageResetScope::All: return kPendingAll;
  }
  return kPendingNone;
}

bool knownPendingValue(const String &value) {
  return value == kPendingNone || value == kPendingSettings ||
         value == kPendingData || value == kPendingAll;
}
}  // namespace

Result StorageLifecycle::begin(UserDataStorage *userData) {
  ready_ = false;
  if (userData == nullptr) {
    return invalidInput("missing user data storage");
  }

  bool userNvsInitialized = false;
  bool userDataInitialized = false;
  bool pendingStored = false;
  String pending;
  const Result metadataResult = readMetadata(
      userNvsInitialized, userDataInitialized, pending, pendingStored);
  if (!metadataResult.ok()) return metadataResult;
  ready_ = true;

  if (!knownPendingValue(pending)) {
    return storageError("invalid pending storage reset");
  }

  const bool resetSettings = pending == kPendingSettings || pending == kPendingAll;
  const bool resetData = pending == kPendingData || pending == kPendingAll;
  const bool initializeUserNvs = !userNvsInitialized || resetSettings;
  const bool initializeUserData = !userDataInitialized || resetData;

  if (initializeUserNvs) {
    Result result = writeInitialized(kUserNvsInitializedKey, false);
    if (!result.ok()) return result;
    result = resetUserNvs();
    if (!result.ok()) return result;
    result = writeInitialized(kUserNvsInitializedKey, true);
    if (!result.ok()) return result;
  }

  Result userDataResult = okResult();
  if (initializeUserData) {
    Result result = writeInitialized(kUserDataInitializedKey, false);
    if (!result.ok()) return result;
    userDataResult = userData->reset();
    if (!userDataResult.ok()) return userDataResult;
    result = writeInitialized(kUserDataInitializedKey, true);
    if (!result.ok()) return result;
  }

  if (pending != kPendingNone || !pendingStored) {
    const Result clearResult = writePending(kPendingNone);
    if (!clearResult.ok()) return clearResult;
  }

  if (!initializeUserData) {
    userDataResult = userData->begin();
  }
  return userDataResult;
}

Result StorageLifecycle::requestReset(StorageResetScope scope) {
  if (!ready_) {
    return storageError("storage lifecycle unavailable");
  }
  return writePending(pendingValue(scope));
}

bool StorageLifecycle::ready() const {
  return ready_;
}

Result StorageLifecycle::resetUserNvs() {
  const esp_err_t deinitResult = nvs_flash_deinit_partition(kUserNvsPartition);
  if (deinitResult != ESP_OK && deinitResult != ESP_ERR_NVS_NOT_INITIALIZED) {
    return storageError("failed to deinitialize user settings storage");
  }
  if (nvs_flash_erase_partition(kUserNvsPartition) != ESP_OK) {
    return storageError("failed to erase user settings storage");
  }
  if (nvs_flash_init_partition(kUserNvsPartition) != ESP_OK) {
    return storageError("failed to initialize user settings storage");
  }
  return okResult();
}

Result StorageLifecycle::readMetadata(bool &userNvsInitialized,
                                      bool &userDataInitialized,
                                      String &pending,
                                      bool &pendingStored) const {
  Preferences metadata;
  if (!metadata.begin(kMetadataNamespace, false, kSystemNvsPartition)) {
    return storageError("failed to open storage metadata");
  }
  userNvsInitialized = metadata.getBool(kUserNvsInitializedKey, false);
  userDataInitialized = metadata.getBool(kUserDataInitializedKey, false);
  pendingStored = metadata.isKey(kResetPendingKey);
  pending = metadata.getString(kResetPendingKey, kPendingNone);
  metadata.end();
  return okResult();
}

Result StorageLifecycle::writeInitialized(const char *key, bool initialized) const {
  Preferences metadata;
  if (!metadata.begin(kMetadataNamespace, false, kSystemNvsPartition)) {
    return storageError("failed to open storage metadata");
  }
  const bool saved = metadata.putBool(key, initialized) == sizeof(bool) &&
                     metadata.getBool(key, !initialized) == initialized;
  metadata.end();
  return saved ? okResult() : storageError("failed to save storage initialization state");
}

Result StorageLifecycle::writePending(const char *pending) const {
  Preferences metadata;
  if (!metadata.begin(kMetadataNamespace, false, kSystemNvsPartition)) {
    return storageError("failed to open storage metadata");
  }
  const bool saved = metadata.putString(kResetPendingKey, pending) > 0 &&
                     metadata.getString(kResetPendingKey, "") == pending;
  metadata.end();
  return saved ? okResult() : storageError("failed to save pending storage reset");
}
