#pragma once
#include "modules/config/model/DeviceConfig.h"
class ConfigService {
 public:
  DeviceConfig value{};
  DeviceConfig snapshot() const { return value; }
};
