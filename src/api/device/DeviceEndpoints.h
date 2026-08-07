#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/config/ConfigService.h"

// Handles GET /api/device.
namespace DeviceEndpoints {

Api::Response get(const ConfigService &configService);

}  // namespace DeviceEndpoints
