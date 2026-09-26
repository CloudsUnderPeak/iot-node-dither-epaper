#include <cassert>
#include <iostream>
#include "api/ApiRouter.h"
#include "api/shared/ApiResponse.h"

int main() {
  ConfigService config;
  WifiManager wifi;
  WifiScanner scan;
  EmbeddedWebAssets web;
  FlashStorage flash;
  StorageLifecycle lifecycle;
  RuntimeActionScheduler runtime;
  BootDiagnostics boot;
#if IOT_FEATURE_STORAGE
  UserDataStorage storage;
#endif
#if IOT_FEATURE_AUTH
  AuthService auth;
  auth.config = &config;
#endif
#if IOT_FEATURE_EPAPER
  EpaperService panel;
  EpaperCalibrationService calibration;
#endif
#if IOT_FEATURE_BATTERY
  BatteryMonitor battery;
#endif
  runtime.available = true;
  ApiRouter router;
  assert(router.begin({config,wifi,scan,web,flash,
#if IOT_FEATURE_STORAGE
    storage,
#endif
    lifecycle,
#if IOT_FEATURE_AUTH
    auth,
#endif
    runtime,
#if IOT_FEATURE_EPAPER
    panel,calibration,
#endif
#if IOT_FEATURE_BATTERY
    battery,
#endif
    boot}).ok());
  auto get = [&](const char *path, const char *token="") {
    Api::Request request; request.method=Api::Method::Get; request.path=path; request.token=token;
    return router.dispatch(request);
  };
  auto response=get("/api/features");
  assert(response.statusCode == 200);
  JsonDocument body; assert(!deserializeJson(body,response.data.c_str()));
  assert(body["features"]["auth"].as<bool>() == bool(IOT_FEATURE_AUTH));
  assert(body["features"]["epaper"].as<bool>() == bool(IOT_FEATURE_EPAPER));
  Api::Request query; query.path="/api/features"; query.method=Api::Method::Get;
  query.queryCount=1; query.query[0].name="name"; query.query[0].value="user_files";
  response=router.dispatch(query); assert(response.statusCode == 200);
  assert(!deserializeJson(body,response.data.c_str()));
  assert(body["supported"].as<bool>() == bool(IOT_FEATURE_USER_FILES));
  query.query[0].value="unknown"; assert(router.dispatch(query).statusCode == 400);
  Api::Request write; write.method=Api::Method::Put; write.path="/api/system";
  assert(router.dispatch(write).statusCode == (IOT_FEATURE_AUTH ? 401 : 400));
  write.token="bad-token";
  assert(router.dispatch(write).statusCode == (IOT_FEATURE_AUTH ? 401 : 400));
  assert(get("/api/auth").statusCode == (IOT_FEATURE_AUTH ? 200 : 404));
  assert(get("/api/epaper").statusCode == (IOT_FEATURE_EPAPER ? 200 : 404));
  assert(get("/api/storage/files", "valid-token").statusCode == (IOT_FEATURE_USER_FILES ? 200 : 404));
  assert(get("/api/runtime/status").statusCode == 200);
  auto preflight=router.checkStreamingFileAccess(Api::Method::Put,"", "/api/storage/files/a.txt");
  assert(preflight.statusCode == (!IOT_FEATURE_USER_FILES ? 404 : IOT_FEATURE_AUTH ? 401 : 200));
  if (!IOT_FEATURE_AUTH) {
    for (size_t i=0;i<router.routeCount();++i) {
      ApiRouter::RouteInfo route; assert(router.routeInfo(i,route)); assert(!route.authRequired);
    }
    scan.value.operationId=7;
    auto started=get("/api/wifi/scan");
    assert(started.pending.id == 7);
    Api::Response completed;
    assert(router.pollPending(started.pending,completed));
    assert(completed.statusCode == 200);
    JsonDocument payload; payload["password"]="new-password";
    write.path="/api/wifi/ap/password"; write.hasBody=true; write.hasJsonBody=true;
    write.body=payload.as<JsonVariantConst>();
    assert(router.dispatch(write).statusCode == 200);
    assert(std::string(config.value.adminPassword) == "new-password");
  }
  std::cout << "Feature API, route exclusion, auth policy and deferred scan passed\n";
}
