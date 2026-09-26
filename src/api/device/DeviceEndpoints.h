#pragma once
#include "core/ProjectFeatures.h"

#include "api/shared/ApiTypes.h"
#include "modules/config/ConfigService.h"
#include "modules/power/BatteryMonitor.h"
#include "modules/runtime/BootDiagnostics.h"
#include "modules/time/TimeSource.h"

// Handles GET /api/device.
namespace DeviceEndpoints {

Api::Response get(const ConfigService &configService,
                  #if IOT_FEATURE_BATTERY
                  const BatteryMonitor &batteryMonitor,
#endif
                  const BootDiagnostics &bootDiagnostics,
                  const TimeSource *timeSource = nullptr);

}  // namespace DeviceEndpoints
