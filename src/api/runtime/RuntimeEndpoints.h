#pragma once
#include "core/ProjectFeatures.h"
#include "modules/runtime/BootDiagnostics.h"

#include "api/shared/ApiTypes.h"
#include "modules/epaper/EpaperService.h"
#include "modules/runtime/RuntimeActionScheduler.h"

namespace RuntimeEndpoints {

Api::Response status(const Api::Request &request,
#if IOT_FEATURE_EPAPER
                     const EpaperService &epaper,
#else
                     const BootDiagnostics &bootDiagnostics,
#endif
                     RuntimeActionScheduler &runtime);

}  // namespace RuntimeEndpoints
