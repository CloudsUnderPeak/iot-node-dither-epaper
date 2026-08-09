#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/config/ConfigService.h"
#include "modules/power/BatteryMonitor.h"

// Handles GET /api/device.
namespace DeviceEndpoints {

Api::Response get(const ConfigService &configService,
                  const BatteryMonitor &batteryMonitor);

}  // namespace DeviceEndpoints
