#pragma once

#include <cstddef>
#include <cstdint>

enum class PreferencesNamespaceState : uint8_t {
  Exists,
  Missing,
  StorageError,
};

// Typed key-value boundary used by PreferencesConfigStore. Implementations own
// one open namespace at a time; slot selection and commit semantics stay in the
// store.
class PreferencesBackend {
 public:
  virtual ~PreferencesBackend() = default;
  virtual PreferencesNamespaceState inspectNamespace(const char *name) = 0;
  virtual bool open(const char *name, bool readOnly) = 0;
  virtual void close() = 0;
  virtual bool clear() = 0;
  virtual bool hasKey(const char *key) const = 0;
  virtual uint8_t getUChar(const char *key, uint8_t fallback) const = 0;
  virtual bool getUCharChecked(const char *key, uint8_t &value) const = 0;
  virtual uint16_t getUShort(const char *key, uint16_t fallback) const = 0;
  virtual bool getBool(const char *key, bool fallback) const = 0;
  virtual bool getString(const char *key, char *target, size_t targetSize) const = 0;
  virtual bool putUChar(const char *key, uint8_t value) = 0;
  virtual bool putUShort(const char *key, uint16_t value) = 0;
  virtual bool putBool(const char *key, bool value) = 0;
  virtual bool putString(const char *key, const char *value) = 0;
};
