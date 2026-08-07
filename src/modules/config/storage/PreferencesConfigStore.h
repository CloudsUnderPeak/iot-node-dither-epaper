#pragma once

#include "ConfigStore.h"
#include "PreferencesBackend.h"

// Owns the dual-slot config schema and commit algorithm over an injected typed
// Preferences backend.
class PreferencesConfigStore : public ConfigStore {
 public:
  explicit PreferencesConfigStore(PreferencesBackend &backend);

  Result load(DeviceConfig &config) override;
  Result save(const DeviceConfig &config) override;

 private:
  PreferencesBackend &backend_;
};
