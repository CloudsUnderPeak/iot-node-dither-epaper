#include "ArduinoPreferencesBackend.h"

#include <cstring>
#include <nvs.h>
#include <nvs_flash.h>

namespace {
constexpr const char *kPartitionLabel = "user_nvs";
}

PreferencesNamespaceState ArduinoPreferencesBackend::inspectNamespace(
    const char *name) {
  if (nvs_flash_init_partition(kPartitionLabel) != ESP_OK) {
    return PreferencesNamespaceState::StorageError;
  }

  nvs_handle_t handle = 0;
  const esp_err_t result =
      nvs_open_from_partition(kPartitionLabel, name, NVS_READONLY, &handle);
  if (result == ESP_ERR_NVS_NOT_FOUND) {
    return PreferencesNamespaceState::Missing;
  }
  if (result != ESP_OK) {
    return PreferencesNamespaceState::StorageError;
  }
  nvs_close(handle);
  return PreferencesNamespaceState::Exists;
}

bool ArduinoPreferencesBackend::open(const char *name, bool readOnly) {
  return preferences_.begin(name, readOnly, kPartitionLabel);
}

void ArduinoPreferencesBackend::close() {
  preferences_.end();
}

bool ArduinoPreferencesBackend::clear() {
  return preferences_.clear();
}

bool ArduinoPreferencesBackend::hasKey(const char *key) const {
  return preferences_.isKey(key);
}

uint8_t ArduinoPreferencesBackend::getUChar(
    const char *key, uint8_t fallback) const {
  return preferences_.getUChar(key, fallback);
}

uint16_t ArduinoPreferencesBackend::getUShort(
    const char *key, uint16_t fallback) const {
  return preferences_.getUShort(key, fallback);
}

bool ArduinoPreferencesBackend::getBool(const char *key, bool fallback) const {
  return preferences_.getBool(key, fallback);
}

bool ArduinoPreferencesBackend::getString(
    const char *key, char *target, size_t targetSize) const {
  if (!preferences_.isKey(key)) {
    return false;
  }
  const String value = preferences_.getString(key, "");
  if (value.length() >= targetSize) {
    return false;
  }
  strlcpy(target, value.c_str(), targetSize);
  return true;
}

bool ArduinoPreferencesBackend::putUChar(const char *key, uint8_t value) {
  return preferences_.putUChar(key, value) > 0;
}

bool ArduinoPreferencesBackend::putUShort(const char *key, uint16_t value) {
  return preferences_.putUShort(key, value) > 0;
}

bool ArduinoPreferencesBackend::putBool(const char *key, bool value) {
  return preferences_.putBool(key, value) > 0;
}

bool ArduinoPreferencesBackend::putString(
    const char *key, const char *value) {
  preferences_.putString(key, value);
  return preferences_.isKey(key) && preferences_.getString(key, "") == value;
}
