#include <cstdlib>
#include <iostream>

#include "api/ApiRouter.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

struct RouterFixture {
  ConfigService config;
  WifiManager wifi;
  WifiScanner scanner;
  EmbeddedWebAssets web;
  FlashStorage flash;
  UserDataStorage userData;
  StorageLifecycle lifecycle;
  AuthService auth;
  RuntimeActionScheduler runtime;
  EpaperService epaper;
  BatteryMonitor battery;
  ApiRouter router;

  RouterFixture() {
    runtime.available = true;
    const ApiRouterDeps deps{
        config,
        wifi,
        scanner,
        web,
        flash,
        userData,
        lifecycle,
        auth,
        runtime,
        epaper,
        battery,
    };
    expect(router.begin(deps).ok(),
           "router should initialize with complete dependencies");
  }
};

struct RouteExpectation {
  Api::Method method;
  const char *path;
  ApiRouter::HttpBinding httpBinding;
  ApiRouter::RouteMatch routeMatch;
  bool protectedRoute;
  int validStatus;
};

Api::Request requestFor(Api::Method method,
                        const char *path,
                        const char *token = "",
                        Api::Transport transport = Api::Transport::Http) {
  Api::Request request;
  request.method = method;
  request.path = path;
  request.token = token;
  request.transport = transport;
  return request;
}

void testExactRouteAuthorizationMatrix() {
  const RouteExpectation routes[] = {
      {Api::Method::Get, "/api/alive", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Get, "/api/device", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Get, "/api/web", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Get, "/api/storage", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Get, "/api/storage/files", ApiRouter::HttpBinding::Query, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Get, "/api/epaper", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Get, "/api/epaper/status", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Post, "/api/epaper/image", ApiRouter::HttpBinding::EpaperRawUpload, ApiRouter::RouteMatch::Exact, false, 404},
      {Api::Method::Get, "/api/epaper/image", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 404},
      {Api::Method::Get, "/api/epaper/image/download", ApiRouter::HttpBinding::EpaperRawDownload, ApiRouter::RouteMatch::Exact, false, 404},
      {Api::Method::Post, "/api/epaper/image/refresh", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 202},
      {Api::Method::Post, "/api/epaper/image/white", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 202},
      {Api::Method::Post, "/api/epaper/image/palette", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 202},
      {Api::Method::Get, "/api/runtime/status", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Get, "/api/auth", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Post, "/api/auth/login", ApiRouter::HttpBinding::JsonBody, ApiRouter::RouteMatch::Exact, false, 400},
      {Api::Method::Post, "/api/auth/verify", ApiRouter::HttpBinding::JsonBody, ApiRouter::RouteMatch::Exact, false, 400},
      {Api::Method::Get, "/api/auth/session", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Post, "/api/auth/logout", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Put, "/api/auth/password", ApiRouter::HttpBinding::JsonBody, ApiRouter::RouteMatch::Exact, true, 400},
      {Api::Method::Get, "/api/wifi", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Put, "/api/wifi", ApiRouter::HttpBinding::JsonBody, ApiRouter::RouteMatch::Exact, true, 400},
      {Api::Method::Get, "/api/wifi/scan", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Post, "/api/wifi/connect", ApiRouter::HttpBinding::JsonBody, ApiRouter::RouteMatch::Exact, true, 400},
      {Api::Method::Get, "/api/wifi/connect", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Post, "/api/wifi/reconnect", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Put, "/api/system", ApiRouter::HttpBinding::JsonBody, ApiRouter::RouteMatch::Exact, true, 400},
      {Api::Method::Post, "/api/system/reset", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Post, "/api/system/reset/settings", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Post, "/api/system/reset/data", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, true, 200},
      {Api::Method::Get, "/api/storage/files/{name}", ApiRouter::HttpBinding::RawDownload, ApiRouter::RouteMatch::UserFile, true, 404},
      {Api::Method::Put, "/api/storage/files/{name}", ApiRouter::HttpBinding::RawUpload, ApiRouter::RouteMatch::UserFile, true, 404},
      {Api::Method::Delete, "/api/storage/files/{name}", ApiRouter::HttpBinding::Query, ApiRouter::RouteMatch::UserFile, true, 200},
  };

  const size_t expectedRouteCount = sizeof(routes) / sizeof(routes[0]);
  expect(ApiRouter::routeCount() == expectedRouteCount,
         "declarative route table must contain every expected exact route");

  for (size_t index = 0; index < expectedRouteCount; ++index) {
    const RouteExpectation &route = routes[index];
    ApiRouter::RouteInfo info;
    expect(ApiRouter::routeInfo(index, info),
           "route metadata must be inspectable by authorization tests");
    expect(info.method == route.method,
           "route table method must match the authorization matrix");
    expect(std::string(info.path) == route.path,
           "route table path must match the authorization matrix");
    expect(info.httpBinding == route.httpBinding,
           "route table HTTP binding must match the adapter matrix");
    expect(info.routeMatch == route.routeMatch,
           "route table matcher must match the route matrix");
    expect(info.authRequired == route.protectedRoute,
           "route table authorization must match the expected policy");

    const char *requestPath =
        route.routeMatch == ApiRouter::RouteMatch::Exact
            ? route.path
            : "/api/storage/files/a.txt";
    RouterFixture fixture;
    Api::Response missing = fixture.router.dispatch(
        requestFor(route.method, requestPath));
    Api::Response invalid = fixture.router.dispatch(
        requestFor(route.method, requestPath, "invalid-token"));
    Api::Response valid = fixture.router.dispatch(
        requestFor(route.method, requestPath, "valid-token"));

    if (route.protectedRoute) {
      expect(missing.statusCode == 401,
             "protected route must reject a missing token");
      expect(invalid.statusCode == 401,
             "protected route must reject an invalid token");
    } else {
      expect(missing.statusCode == route.validStatus,
             "public route must work without a token");
      expect(invalid.statusCode == route.validStatus,
             "public route must ignore an invalid token");
    }
    expect(valid.statusCode == route.validStatus,
           "route must return its handler status for a valid token");
  }

  ApiRouter::RouteInfo missingInfo;
  expect(!ApiRouter::routeInfo(expectedRouteCount, missingInfo),
         "route metadata lookup must reject an out-of-range index");
}

void testWebIdentityMatchesAcrossTransports() {
  RouterFixture fixture;
  const Api::Response http = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/web", "", Api::Transport::Http));
  const Api::Response serial = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/web", "", Api::Transport::Serial));
  expect(http.success && serial.success && http.data == serial.data &&
             http.data.c_str() == std::string(
                 "{\"source\":\"builtin\",\"sha256\":"
                 "\"0123456789abcdef0123456789abcdef0123456789abcdef"
                 "0123456789abcdef\"}"),
         "HTTP and serial must share the same public web identity contract");
}

void testDynamicFileAuthorizationAndTransport() {
  RouterFixture fixture;

  for (Api::Method method : {Api::Method::Get, Api::Method::Put, Api::Method::Delete}) {
    Api::Response missing = fixture.router.dispatch(
        requestFor(method, "/api/storage/files/a.txt", "", Api::Transport::Serial));
    Api::Response invalid = fixture.router.dispatch(
        requestFor(method, "/api/storage/files/a.txt", "invalid-token",
                   Api::Transport::Serial));
    expect(missing.statusCode == 401,
           "dynamic file route must reject a missing token");
    expect(invalid.statusCode == 401,
           "dynamic file route must reject an invalid token");
  }

  expect(fixture.router.dispatch(
             requestFor(Api::Method::Get, "/api/storage/files/a.txt",
                        "valid-token", Api::Transport::Serial)).statusCode == 415,
         "serial file download must reject the raw transport after authorization");
  expect(fixture.router.dispatch(
             requestFor(Api::Method::Put, "/api/storage/files/a.txt",
                        "valid-token", Api::Transport::Serial)).statusCode == 415,
         "serial file upload must reject the raw transport after authorization");
  expect(fixture.router.dispatch(
             requestFor(Api::Method::Delete, "/api/storage/files/a.txt",
                        "valid-token", Api::Transport::Serial)).statusCode == 200,
         "serial file delete must reach the shared JSON handler");
  expect(fixture.router.dispatch(
             requestFor(Api::Method::Get, "/api/storage/files/a.txt",
                        "valid-token", Api::Transport::Http)).statusCode == 404,
         "HTTP raw download dispatch must remain owned by the streaming adapter");

  ApiRouter::FileUploadStart upload =
      fixture.router.prepareFileUpload("", "/api/storage/files/a.txt", 12);
  expect(!upload.ready && upload.response.statusCode == 401 &&
             fixture.userData.uploadBeginCount == 0,
         "upload preparation must authorize before opening storage");
  upload = fixture.router.prepareFileUpload(
      "valid-token", "/api/storage/files/a.txt", 12);
  expect(upload.ready && upload.response.statusCode == 200 &&
             upload.sessionId == 41 && fixture.userData.uploadBeginCount == 1,
         "authorized upload preparation must open a storage session");

  ApiRouter::FileDownloadStart download =
      fixture.router.prepareFileDownload(
          "invalid-token", "/api/storage/files/a.txt", "");
  expect(!download.ready && download.response.statusCode == 401 &&
             fixture.userData.downloadBeginCount == 0,
         "download preparation must authorize before opening storage");
  download = fixture.router.prepareFileDownload(
      "valid-token", "/api/storage/files/a.txt", "");
  expect(download.ready && download.response.statusCode == 200 &&
             download.sessionId == 42 &&
             std::string(download.name) == "a.txt" &&
             fixture.userData.downloadBeginCount == 1,
         "authorized download preparation must open a storage session");

  upload = fixture.router.prepareFileUpload(
      "valid-token", "/api/storage/files/bad/name", 12);
  expect(!upload.ready && upload.response.statusCode == 404 &&
             fixture.userData.uploadBeginCount == 1,
         "invalid upload path must not open another storage session");
  download = fixture.router.prepareFileDownload(
      "valid-token", "/api/storage/files/bad/name", "");
  expect(!download.ready && download.response.statusCode == 404 &&
             fixture.userData.downloadBeginCount == 1,
         "invalid download path must not open another storage session");

  upload = fixture.router.prepareFileUpload(
      "valid-token", "/api/storage/files/epaper-current.epd", 12);
  expect(!upload.ready && upload.response.statusCode == 403 &&
             upload.response.data.c_str() == std::string("{\"code\":\"reserved_file\"}") &&
             fixture.userData.uploadBeginCount == 1,
         "generic upload must not replace the reserved e-paper image");
  expect(fixture.router.dispatch(
             requestFor(Api::Method::Delete,
                        "/api/storage/files/epaper-current.epd",
                        "valid-token", Api::Transport::Serial)).statusCode == 403,
         "generic delete must not remove the reserved e-paper image");
}

void testEpaperPublicContract() {
  RouterFixture fixture;

  const Api::Response capabilities = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/epaper"));
  const std::string capabilityData = capabilities.data.c_str();
  expect(capabilities.statusCode == 200 &&
             capabilityData.find("\"upload_bytes\":192040") != std::string::npos &&
             capabilityData.find("\"cooldown_seconds\":180") != std::string::npos,
         "e-paper capabilities must expose the fixed image and cooldown contract");

  fixture.epaper.current.canDownload = true;
  fixture.epaper.current.stored.present = true;
  fixture.epaper.current.stored.valid = true;
  fixture.epaper.current.lastSource = "uploaded";
  fixture.epaper.current.lastResult = "success";
  const Api::Response status = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/epaper/status"));
  const std::string statusData = status.data.c_str();
  expect(status.statusCode == 200 &&
             statusData.find("\"can_download\":true") != std::string::npos &&
             statusData.find("\"available\":true") != std::string::npos &&
             statusData.find("\"source\":\"uploaded\"") != std::string::npos &&
             statusData.find("\"last_reset_reason\":\"software\"") != std::string::npos,
         "e-paper status must expose the documented cached operation fields");

  expect(fixture.router.dispatch(
             requestFor(Api::Method::Post, "/api/epaper/image", "",
                        Api::Transport::Serial)).statusCode == 415,
         "serial e-paper upload must reject the raw transport");
  expect(fixture.router.dispatch(
             requestFor(Api::Method::Get, "/api/epaper/image/download", "",
                        Api::Transport::Serial)).statusCode == 415,
         "serial e-paper download must reject the raw transport");
}

void testDeviceBatteryContract() {
  RouterFixture fixture;
  fixture.battery.current.sampleValid = true;
  fixture.battery.current.voltageMilliVolts = 3980;
  fixture.battery.current.estimate.available = true;
  fixture.battery.current.estimate.percent = 83;
  fixture.battery.current.sampleAgeMs = 42;

  const Api::Response response = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/device"));
  const std::string data = response.data.c_str();
  expect(response.statusCode == 200 &&
             data.find("\"power\":{\"battery\":{") != std::string::npos &&
             data.find("\"voltage_mv\":3980") != std::string::npos &&
             data.find("\"sample_age_ms\":42") != std::string::npos &&
             data.find("\"estimated_percent\":83") != std::string::npos &&
             data.find("charge_state") == std::string::npos &&
             data.find("charging") == std::string::npos,
         "device resource should expose voltage estimate without charging claims");

  fixture.battery.current = {};
  const std::string unavailable = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/device")).data.c_str();
  expect(unavailable.find("\"voltage_mv\":null") != std::string::npos &&
             unavailable.find("\"sample_age_ms\":null") != std::string::npos &&
             unavailable.find("\"estimated_percent\":null") != std::string::npos,
         "device resource should keep a stable null shape before a valid sample");
}

void testUnknownRoutesRemainNotFound() {
  RouterFixture fixture;
  for (const char *token : {"", "invalid-token", "valid-token"}) {
    const Api::Response response = fixture.router.dispatch(
        requestFor(Api::Method::Get, "/api/not-a-route", token));
    expect(response.statusCode == 404,
           "unknown routes must remain not found regardless of credentials");
  }
}
}  // namespace

int main() {
  testExactRouteAuthorizationMatrix();
  testWebIdentityMatchesAcrossTransports();
  testDynamicFileAuthorizationAndTransport();
  testEpaperPublicContract();
  testDeviceBatteryContract();
  testUnknownRoutesRemainNotFound();
  if (failures != 0) {
    std::cerr << failures << " API router authorization test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "API router authorization matrix tests passed\n";
  return EXIT_SUCCESS;
}
