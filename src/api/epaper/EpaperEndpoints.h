#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/epaper/EpaperService.h"

namespace EpaperEndpoints {

Api::Response capabilities(const Api::Request &request);
Api::Response status(const Api::Request &request,
                     const EpaperService &service);
Api::Response metadata(const Api::Request &request,
                       const EpaperService &service);
Api::Response action(const Api::Request &request,
                     EpaperService &service,
                     EpaperDrawAction action);
Api::Response fromServiceResult(const EpaperServiceResult &result,
                                uint32_t retryAfterSeconds = 0);
Api::Response rawTransportUnsupported();

}  // namespace EpaperEndpoints
