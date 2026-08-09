#pragma once

#include "alive/AliveEndpoints.h"
#include "auth/AuthEndpoints.h"
#include "epaper/EpaperEndpoints.h"
#include "runtime/RuntimeEndpoints.h"
#include "shared/ApiTypes.h"
#include "storage/UserFileEndpoints.h"
#include "system/SystemEndpoints.h"
#include "wifi/WifiEndpoints.h"
#include "modules/auth/AuthService.h"
#include "modules/config/ConfigService.h"
#include "modules/epaper/EpaperService.h"
#include "modules/power/BatteryMonitor.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/storage/EmbeddedWebAssets.h"
#include "modules/storage/FlashStorage.h"
#include "modules/storage/StorageLifecycle.h"
#include "modules/storage/UserDataStorage.h"
#include "modules/storage/UserFilePolicy.h"
#include "modules/wifi/WifiManager.h"
#include "modules/wifi/WifiScanner.h"

struct ApiRouterDeps {
  ConfigService &configService;
  WifiManager &wifiManager;
  WifiScanner &wifiScanner;
  const EmbeddedWebAssets &embeddedWebAssets;
  const FlashStorage &flashStorage;
  UserDataStorage &userData;
  StorageLifecycle &storageLifecycle;
  AuthService &authService;
  RuntimeActionScheduler &runtime;
  EpaperService &epaperService;
  BatteryMonitor &batteryMonitor;
};

// Lists every REST/serial URL and delegates directly to the matching endpoint.
class ApiRouter {
 public:
  enum class RouteMatch : uint8_t {
    Exact,
    UserFile,
  };

  enum class HttpBinding : uint8_t {
    NoBody,
    JsonBody,
    Query,
    RawUpload,
    RawDownload,
    EpaperRawUpload,
    EpaperRawDownload,
  };

  struct RouteInfo {
    Api::Method method = Api::Method::Get;
    const char *path = "";
    HttpBinding httpBinding = HttpBinding::NoBody;
    RouteMatch routeMatch = RouteMatch::Exact;
    bool authRequired = true;
  };

  struct FileUploadStart {
    bool ready = false;
    uint32_t sessionId = 0;
    size_t maxUploadBytes = 0;
    Api::Response response;
  };

  struct FileDownloadStart {
    bool ready = false;
    uint32_t sessionId = 0;
    char name[UserFilePolicy::kMaxFilenameBytes + 1]{};
    size_t fileSize = 0;
    size_t rangeStart = 0;
    size_t contentLength = 0;
    bool partial = false;
    Api::Response response;
  };

  Result begin(const ApiRouterDeps &deps);
  Api::Response dispatch(const Api::Request &request);
  static size_t routeCount();
  static bool routeInfo(size_t index, RouteInfo &info);
  FileUploadStart prepareFileUpload(const String &token,
                                    const char *path,
                                    size_t contentLength);
  Api::Response writeFileUpload(uint32_t sessionId,
                                size_t index,
                                const uint8_t *data,
                                size_t length);
  Api::Response finishFileUpload(uint32_t sessionId, const char *path);
  void abortFileUpload(uint32_t sessionId);
  FileDownloadStart prepareFileDownload(const String &token,
                                        const char *path,
                                        const char *rangeHeader);
  UserDataReadResult readFileDownload(uint32_t sessionId,
                                      uint8_t *buffer,
                                      size_t bufferLength);
  void finishFileDownload(uint32_t sessionId);
  FileUploadStart prepareEpaperUpload(size_t contentLength);
  Api::Response writeEpaperUpload(uint32_t sessionId,
                                  size_t index,
                                  const uint8_t *data,
                                  size_t length);
  Api::Response finishEpaperUpload(uint32_t sessionId);
  void abortEpaperUpload(uint32_t sessionId);
  FileDownloadStart prepareEpaperDownload(const char *rangeHeader);
  UserDataReadResult readEpaperDownload(uint32_t sessionId,
                                        uint8_t *buffer,
                                        size_t bufferLength);
  void finishEpaperDownload(uint32_t sessionId);

 private:
  enum class RouteAccess : uint8_t {
    Protected,
    Public,
  };

  using RouteHandler = Api::Response (*)(
      ApiRouter &router,
      const Api::Request &request,
      const char *parameter);

  struct Route {
    Api::Method method;
    const char *path;
    RouteHandler handler;
    HttpBinding httpBinding = HttpBinding::NoBody;
    RouteMatch routeMatch = RouteMatch::Exact;
    RouteAccess access = RouteAccess::Protected;
  };

  struct StreamingFileRequest {
    bool ready = false;
    char name[UserFilePolicy::kMaxFilenameBytes + 1]{};
    Api::Response response;
  };

  ConfigService *configService_ = nullptr;
  const EmbeddedWebAssets *embeddedWebAssets_ = nullptr;
  const FlashStorage *flashStorage_ = nullptr;
  UserDataStorage *userData_ = nullptr;
  AuthService *authService_ = nullptr;
  RuntimeActionScheduler *runtimeActions_ = nullptr;
  EpaperService *epaperService_ = nullptr;
  BatteryMonitor *batteryMonitor_ = nullptr;
  AuthEndpoints authEndpoints_;
  WifiEndpoints wifiEndpoints_;
  SystemEndpoints systemEndpoints_;

  static const Route kRoutes_[];

  static bool routeMatches(const Route &route,
                           const Api::Request &request,
                           char *parameter,
                           size_t parameterSize);
  bool authorized(const Api::Request &request) const;
  bool authorized(const String &token) const;
  StreamingFileRequest prepareStreamingFileRequest(
      Api::Method method,
      const String &token,
      const char *path) const;
  static Api::Response fileStorageError(
      const UserDataFileResult &result,
      size_t detailBytes = 0);
  static bool fileNameFromPath(const char *path,
                               char *name,
                               size_t nameSize);
  Api::Response unauthorized() const;
};
