#pragma once

#include "modules/config/model/DeviceConfig.h"

// Persistence boundary used by ConfigService. Production uses NVS while native
// tests provide an in-memory store to exercise serialized config mutations.
class ConfigStore {
 public:
  virtual ~ConfigStore() = default;
  virtual Result load(DeviceConfig &config) = 0;
  virtual Result save(const DeviceConfig &config) = 0;
};
