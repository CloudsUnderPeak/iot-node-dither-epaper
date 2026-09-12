#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/config/ConfigService.h"
#include "modules/power/BatteryMonitor.h"
#include "modules/runtime/BootDiagnostics.h"

// Handles GET /api/device.
namespace DeviceEndpoints {

Api::Response get(const ConfigService &configService,
                  const BatteryMonitor &batteryMonitor,
                  const BootDiagnostics &bootDiagnostics);

}  // namespace DeviceEndpoints
