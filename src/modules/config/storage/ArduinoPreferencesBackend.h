#pragma once

#include <Preferences.h>

#include "PreferencesBackend.h"

class ArduinoPreferencesBackend : public PreferencesBackend {
 public:
  PreferencesNamespaceState inspectNamespace(const char *name) override;
  bool open(const char *name, bool readOnly) override;
  void close() override;
  bool clear() override;
  bool hasKey(const char *key) const override;
  uint8_t getUChar(const char *key, uint8_t fallback) const override;
  bool getUCharChecked(const char *key, uint8_t &value) const override;
  uint16_t getUShort(const char *key, uint16_t fallback) const override;
  bool getBool(const char *key, bool fallback) const override;
  bool getString(const char *key, char *target, size_t targetSize) const override;
  bool putUChar(const char *key, uint8_t value) override;
  bool putUShort(const char *key, uint16_t value) override;
  bool putBool(const char *key, bool value) override;
  bool putString(const char *key, const char *value) override;

 private:
  mutable Preferences preferences_;
};
