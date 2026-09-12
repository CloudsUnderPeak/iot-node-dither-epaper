#include "ApiRouter.h"

#include "device/DeviceEndpoints.h"
#include "shared/ApiResponse.h"
#include "storage/StorageEndpoints.h"
#include "web/WebEndpoints.h"
#include <cstring>

const ApiRouter::Route ApiRouter::kRoutes_[] = {
    {Api::Method::Get, "/api/alive",
     +[](ApiRouter &, const Api::Request &, const char *) {
       return AliveEndpoints::get();
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/device",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return DeviceEndpoints::get(*router.configService_,
                                   *router.batteryMonitor_,
                                   *router.bootDiagnostics_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/web",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return WebEndpoints::get(*router.embeddedWebAssets_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/storage",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return StorageEndpoints::get(*router.flashStorage_, *router.userData_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/storage/files",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return UserFileEndpoints::list(request, *router.userData_);
     },
     HttpBinding::Query},
    {Api::Method::Get, "/api/epaper",
     +[](ApiRouter &, const Api::Request &request, const char *) {
       return EpaperEndpoints::capabilities(request);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/epaper/status",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::status(request, *router.epaperService_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/epaper/calibration",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::calibration(
           request, *router.epaperCalibrationService_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Put, "/api/epaper/calibration",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::updateCalibration(
           request, *router.epaperCalibrationService_);
     },
     HttpBinding::JsonBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Post, "/api/epaper/calibration/reset",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::resetCalibration(
           request, *router.epaperCalibrationService_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Post, "/api/epaper/image",
     +[](ApiRouter &, const Api::Request &request, const char *) {
       return request.transport == Api::Transport::Serial
                  ? EpaperEndpoints::rawTransportUnsupported()
                  : Api::problem(404, "not_found", "not found");
     },
     HttpBinding::EpaperRawUpload, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/epaper/image",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::metadata(request, *router.epaperService_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/epaper/image/download",
     +[](ApiRouter &, const Api::Request &request, const char *) {
       return request.transport == Api::Transport::Serial
                  ? EpaperEndpoints::rawTransportUnsupported()
                  : Api::problem(404, "not_found", "not found");
     },
     HttpBinding::EpaperRawDownload, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Post, "/api/epaper/image/refresh",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::action(
           request, *router.epaperService_, EpaperDrawAction::Stored);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Post, "/api/epaper/image/white",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::action(
           request, *router.epaperService_, EpaperDrawAction::White);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Post, "/api/epaper/image/palette",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return EpaperEndpoints::action(
           request, *router.epaperService_, EpaperDrawAction::Palette);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/runtime/status",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return RuntimeEndpoints::status(
           request, *router.epaperService_, *router.runtimeActions_);
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/auth",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.authEndpoints_.info();
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Post, "/api/auth/login",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return router.authEndpoints_.login(request);
     },
     HttpBinding::JsonBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Post, "/api/auth/verify",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return router.authEndpoints_.verify(request);
     },
     HttpBinding::JsonBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Get, "/api/auth/session",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.authEndpoints_.session();
     }},
    {Api::Method::Post, "/api/auth/logout",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.authEndpoints_.logout();
     }},
    {Api::Method::Put, "/api/auth/password",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return router.authEndpoints_.updatePassword(request);
     },
     HttpBinding::JsonBody},
    {Api::Method::Get, "/api/wifi",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.wifiEndpoints_.get();
     },
     HttpBinding::NoBody, RouteMatch::Exact, RouteAccess::Public},
    {Api::Method::Put, "/api/wifi",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return router.wifiEndpoints_.update(request);
     },
     HttpBinding::JsonBody},
    {Api::Method::Get, "/api/wifi/scan",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.wifiEndpoints_.scan();
     }},
    {Api::Method::Post, "/api/wifi/connect",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return router.wifiEndpoints_.connect(request);
     },
     HttpBinding::JsonBody},
    {Api::Method::Get, "/api/wifi/connect",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.wifiEndpoints_.connectionStatus();
     }},
    {Api::Method::Post, "/api/wifi/reconnect",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.wifiEndpoints_.reconnect();
     }},
    {Api::Method::Put, "/api/system",
     +[](ApiRouter &router, const Api::Request &request, const char *) {
       return router.systemEndpoints_.update(request);
     },
     HttpBinding::JsonBody},
    {Api::Method::Post, "/api/system/reset",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.systemEndpoints_.reset(StorageResetScope::All);
     }},
    {Api::Method::Post, "/api/system/reset/settings",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.systemEndpoints_.reset(StorageResetScope::Settings);
     }},
    {Api::Method::Post, "/api/system/reset/data",
     +[](ApiRouter &router, const Api::Request &, const char *) {
       return router.systemEndpoints_.reset(StorageResetScope::Data);
     }},
    {Api::Method::Get, "/api/storage/files/{name}",
     +[](ApiRouter &, const Api::Request &request, const char *) {
       return request.transport == Api::Transport::Serial
                  ? UserFileEndpoints::transportUnsupported()
                  : Api::problem(404, "not_found", "not found");
     },
     HttpBinding::RawDownload, RouteMatch::UserFile},
    {Api::Method::Put, "/api/storage/files/{name}",
     +[](ApiRouter &, const Api::Request &request, const char *) {
       return request.transport == Api::Transport::Serial
                  ? UserFileEndpoints::transportUnsupported()
                  : Api::problem(404, "not_found", "not found");
     },
     HttpBinding::RawUpload, RouteMatch::UserFile},
    {Api::Method::Delete, "/api/storage/files/{name}",
     +[](ApiRouter &router,
         const Api::Request &request,
         const char *name) {
       return UserFileEndpoints::remove(request, name, *router.userData_);
     },
     HttpBinding::Query, RouteMatch::UserFile},
};

Result ApiRouter::begin(const ApiRouterDeps &deps) {
  configService_ = &deps.configService;
  embeddedWebAssets_ = &deps.embeddedWebAssets;
  flashStorage_ = &deps.flashStorage;
  userData_ = &deps.userData;
  authService_ = &deps.authService;
  runtimeActions_ = &deps.runtime;
  epaperService_ = &deps.epaperService;
  epaperCalibrationService_ = &deps.epaperCalibrationService;
  batteryMonitor_ = &deps.batteryMonitor;
  bootDiagnostics_ = &deps.bootDiagnostics;

  Result result = authEndpoints_.begin(
      &deps.configService, &deps.authService, &deps.runtime);
  if (!result.ok()) return result;
  result = wifiEndpoints_.begin(
      &deps.configService,
      &deps.wifiManager,
      &deps.wifiScanner,
      &deps.runtime);
  if (!result.ok()) return result;
  return systemEndpoints_.begin(
      &deps.configService, &deps.storageLifecycle, &deps.runtime);
}

Api::Response ApiRouter::dispatch(const Api::Request &request) {
  for (const Route &route : kRoutes_) {
    char parameter[UserFilePolicy::kMaxFilenameBytes + 1]{};
    if (!routeMatches(route, request, parameter, sizeof(parameter))) continue;
    if (route.access == RouteAccess::Protected) {
      if (!authorized(request)) {
        return unauthorized();
      }
    }
    return route.handler(*this, request, parameter);
  }

  return Api::problem(404, "not_found", "not found");
}

size_t ApiRouter::routeCount() {
  return sizeof(kRoutes_) / sizeof(kRoutes_[0]);
}

bool ApiRouter::routeInfo(size_t index, RouteInfo &info) {
  if (index >= routeCount()) return false;
  const Route &route = kRoutes_[index];
  info.method = route.method;
  info.path = route.path;
  info.httpBinding = route.httpBinding;
  info.routeMatch = route.routeMatch;
  info.authRequired = route.access == RouteAccess::Protected;
  return true;
}

bool ApiRouter::routeMatches(const Route &route,
                             const Api::Request &request,
                             char *parameter,
                             size_t parameterSize) {
  if (request.method != route.method) return false;
  if (route.routeMatch == RouteMatch::Exact) {
    return request.matches(route.method, route.path);
  }
  return route.routeMatch == RouteMatch::UserFile &&
         fileNameFromPath(request.path, parameter, parameterSize);
}

bool ApiRouter::authorized(const Api::Request &request) const {
  return authorized(request.token);
}

bool ApiRouter::authorized(const String &token) const {
  return token.length() > 0 && authService_->tokenValid(token);
}

Api::Response ApiRouter::unauthorized() const {
  return Api::unauthorized();
}

ApiRouter::FileUploadStart ApiRouter::prepareFileUpload(
    const String &token,
    const char *path,
    size_t contentLength) {
  FileUploadStart start;
  const StreamingFileRequest request =
      prepareStreamingFileRequest(Api::Method::Put, token, path);
  if (!request.ready) {
    start.response = request.response;
    return start;
  }
  if (strcmp(request.name, EpaperService::kImageName) == 0) {
    start.response = Api::problem(
        403, "reserved_file", "e-paper image is managed by /api/epaper/image");
    return start;
  }
  const UserDataUploadBegin storageStart =
      userData_->beginUpload(request.name, contentLength);
  start.maxUploadBytes = storageStart.maxUploadBytes;
  if (!storageStart.result.ok()) {
    start.response = fileStorageError(
        storageStart.result, storageStart.maxUploadBytes);
    return start;
  }
  start.ready = true;
  start.sessionId = storageStart.sessionId;
  start.response = Api::ok("{}");
  return start;
}

Api::Response ApiRouter::writeFileUpload(uint32_t sessionId,
                                         size_t index,
                                         const uint8_t *data,
                                         size_t length) {
  const UserDataFileResult result = userData_->writeUpload(
      sessionId, index, data, length);
  return UserFileEndpoints::fromStorageResult(result);
}

Api::Response ApiRouter::finishFileUpload(uint32_t sessionId,
                                          const char *path) {
  char name[UserFilePolicy::kMaxFilenameBytes + 1]{};
  if (!fileNameFromPath(path, name, sizeof(name))) {
    userData_->abortUpload(sessionId);
    return Api::problem(404, "not_found", "not found");
  }
  return UserFileEndpoints::uploadCommitted(
      name, userData_->finishUpload(sessionId));
}

void ApiRouter::abortFileUpload(uint32_t sessionId) {
  userData_->abortUpload(sessionId);
}

ApiRouter::FileDownloadStart ApiRouter::prepareFileDownload(
    const String &token,
    const char *path,
    const char *rangeHeader) {
  FileDownloadStart start;
  const StreamingFileRequest request =
      prepareStreamingFileRequest(Api::Method::Get, token, path);
  if (!request.ready) {
    start.response = request.response;
    return start;
  }
  strlcpy(start.name, request.name, sizeof(start.name));
  const UserDataDownloadBegin storageStart =
      userData_->beginDownload(request.name, rangeHeader);
  start.fileSize = storageStart.fileSize;
  if (!storageStart.result.ok()) {
    start.response = fileStorageError(
        storageStart.result, storageStart.fileSize);
    return start;
  }
  start.ready = true;
  start.sessionId = storageStart.sessionId;
  start.rangeStart = storageStart.rangeStart;
  start.contentLength = storageStart.contentLength;
  start.partial = storageStart.partial;
  start.response = Api::ok("{}");
  return start;
}

UserDataReadResult ApiRouter::readFileDownload(uint32_t sessionId,
                                               uint8_t *buffer,
                                               size_t bufferLength) {
  return userData_->readDownload(sessionId, buffer, bufferLength);
}

void ApiRouter::finishFileDownload(uint32_t sessionId) {
  userData_->finishDownload(sessionId);
}

ApiRouter::FileUploadStart ApiRouter::prepareEpaperUpload(
    size_t contentLength) {
  FileUploadStart start;
  const EpaperServiceResult result = epaperService_->beginUpload(contentLength);
  if (!result.ok()) {
    start.response = EpaperEndpoints::fromServiceResult(
        result, epaperService_->snapshot().retryAfterSeconds);
    return start;
  }
  start.ready = true;
  start.sessionId = result.sessionId;
  start.maxUploadBytes = EpaperImageFormat::kImageBytes;
  start.response = Api::ok("{}");
  return start;
}

Api::Response ApiRouter::writeEpaperUpload(uint32_t sessionId,
                                           size_t index,
                                           const uint8_t *data,
                                           size_t length) {
  return EpaperEndpoints::fromServiceResult(
      epaperService_->writeUpload(sessionId, index, data, length));
}

Api::Response ApiRouter::finishEpaperUpload(uint32_t sessionId) {
  const EpaperServiceResult result = epaperService_->finishUpload(sessionId);
  if (!result.ok()) return EpaperEndpoints::fromServiceResult(result);
  JsonDocument data;
  data["state"] = "queued";
  return Api::accepted(Api::json(data), "e-paper image uploaded and draw queued");
}

void ApiRouter::abortEpaperUpload(uint32_t sessionId) {
  epaperService_->abortUpload(sessionId);
}

ApiRouter::FileDownloadStart ApiRouter::prepareEpaperDownload(
    const char *rangeHeader) {
  FileDownloadStart start;
  const EpaperStoredImageMetadata stored = epaperService_->snapshot().stored;
  if (!stored.present) {
    start.response = Api::problem(404, "epaper_image_not_found",
                                  "stored e-paper image not found");
    return start;
  }
  if (!stored.valid) {
    JsonDocument data;
    data["code"] = "invalid_epaper_image";
    data["reason"] = EpaperImageFormat::errorCode(stored.validationError);
    start.response = Api::error(
        422, Api::json(data), "stored EPDIMG is invalid");
    return start;
  }
  const UserDataDownloadBegin begin =
      epaperService_->beginImageDownload(rangeHeader);
  start.fileSize = begin.fileSize;
  if (!begin.result.ok()) {
    if (begin.result.status == UserDataFileStatus::NotFound) {
      start.response = Api::problem(404, "epaper_image_not_found",
                                    "stored e-paper image not found");
    } else {
      start.response = fileStorageError(begin.result, begin.fileSize);
    }
    return start;
  }
  start.ready = true;
  start.sessionId = begin.sessionId;
  strlcpy(start.name, EpaperService::kImageName, sizeof(start.name));
  start.rangeStart = begin.rangeStart;
  start.contentLength = begin.contentLength;
  start.partial = begin.partial;
  start.response = Api::ok("{}");
  return start;
}

UserDataReadResult ApiRouter::readEpaperDownload(
    uint32_t sessionId,
    uint8_t *buffer,
    size_t bufferLength) {
  return epaperService_->readImageDownload(sessionId, buffer, bufferLength);
}

void ApiRouter::finishEpaperDownload(uint32_t sessionId) {
  epaperService_->finishImageDownload(sessionId);
}

ApiRouter::StreamingFileRequest ApiRouter::prepareStreamingFileRequest(
    Api::Method method,
    const String &token,
    const char *path) const {
  StreamingFileRequest request;
  Api::Request routeRequest;
  routeRequest.method = method;
  routeRequest.path = path;
  for (const Route &route : kRoutes_) {
    if (!routeMatches(
            route, routeRequest, request.name, sizeof(request.name))) {
      continue;
    }
    const bool expectedBinding =
        (method == Api::Method::Get &&
         route.httpBinding == HttpBinding::RawDownload) ||
        (method == Api::Method::Put &&
         route.httpBinding == HttpBinding::RawUpload);
    if (!expectedBinding) break;
    if (route.access == RouteAccess::Protected && !authorized(token)) {
      request.response = unauthorized();
      return request;
    }
    request.ready = true;
    request.response = Api::ok("{}");
    return request;
  }
  request.response = Api::problem(404, "not_found", "not found");
  return request;
}

Api::Response ApiRouter::fileStorageError(
    const UserDataFileResult &result,
    size_t detailBytes) {
  return UserFileEndpoints::fromStorageResult(result, detailBytes);
}

bool ApiRouter::fileNameFromPath(const char *path,
                                 char *name,
                                 size_t nameSize) {
  constexpr const char *kPrefix = "/api/storage/files/";
  const size_t prefixLength = strlen(kPrefix);
  if (path == nullptr || name == nullptr || strncmp(path, kPrefix, prefixLength) != 0) {
    return false;
  }
  const char *candidate = path + prefixLength;
  if (candidate[0] == '\0' || strchr(candidate, '/') != nullptr ||
      !UserFilePolicy::validPublicName(candidate) || strlen(candidate) >= nameSize) {
    return false;
  }
  strlcpy(name, candidate, nameSize);
  return true;
}
