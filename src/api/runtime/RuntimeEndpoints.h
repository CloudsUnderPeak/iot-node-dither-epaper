#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/epaper/EpaperService.h"
#include "modules/runtime/RuntimeActionScheduler.h"

namespace RuntimeEndpoints {

Api::Response status(const Api::Request &request,
                     const EpaperService &epaper,
                     RuntimeActionScheduler &runtime);

}  // namespace RuntimeEndpoints
