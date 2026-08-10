#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/epaper/EpaperService.h"
#include "modules/epaper/calibration/EpaperCalibrationService.h"

namespace EpaperEndpoints {

Api::Response capabilities(const Api::Request &request);
Api::Response status(const Api::Request &request,
                     const EpaperService &service);
Api::Response metadata(const Api::Request &request,
                       const EpaperService &service);
Api::Response calibration(const Api::Request &request,
                          const EpaperCalibrationService &service);
Api::Response updateCalibration(const Api::Request &request,
                                EpaperCalibrationService &service);
Api::Response resetCalibration(const Api::Request &request,
                               EpaperCalibrationService &service);
Api::Response action(const Api::Request &request,
                     EpaperService &service,
                     EpaperDrawAction action);
Api::Response fromServiceResult(const EpaperServiceResult &result,
                                uint32_t retryAfterSeconds = 0);
Api::Response rawTransportUnsupported();

}  // namespace EpaperEndpoints
