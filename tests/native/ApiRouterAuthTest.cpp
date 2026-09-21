#include <cstdlib>
#include <iostream>

#include "api/ApiRouter.h"
#include "modules/http/UserFileHttpPreflight.h"

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
  EpaperCalibrationService calibration;
  BatteryMonitor battery;
  BootDiagnostics diagnostics;
  ApiRouter router;

  explicit RouterFixture(
      DeviceResetReason resetReason = DeviceResetReason::Software)
      : diagnostics(resetReason) {
    auth.config = &config;
    runtime.available = true;
    epaper.current.lastResetReason = deviceResetReasonToString(resetReason);
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
        calibration,
        battery,
        diagnostics,
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
      {Api::Method::Get, "/api/epaper/calibration", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
      {Api::Method::Put, "/api/epaper/calibration", ApiRouter::HttpBinding::JsonBody, ApiRouter::RouteMatch::Exact, false, 400},
      {Api::Method::Post, "/api/epaper/calibration/reset", ApiRouter::HttpBinding::NoBody, ApiRouter::RouteMatch::Exact, false, 200},
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
      {Api::Method::Get, "/api/wifi/scan", ApiRouter::HttpBinding::Deferred, ApiRouter::RouteMatch::Exact, true, 200},
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

  for (const char *encoding : {static_cast<const char *>(nullptr), "", "identity", "br", "gzip, gzip"}) {
    const auto upload = fixture.router.prepareEpaperUpload(100, encoding);
    expect(!upload.ready && upload.response.statusCode == 415, "non-gzip upload rejected");
  }
  expect(fixture.router.prepareEpaperUpload(100, "gzip").ready, "gzip upload admitted");
  expect(fixture.router.prepareFileUpload("valid-token", "/api/storage/files/epaper-current.epd.gz", 12).response.statusCode == 403,
         "compressed image reserved from generic PUT");
  expect(fixture.router.dispatch(requestFor(Api::Method::Delete, "/api/storage/files/epaper-current.epd.gz", "valid-token")).statusCode == 403,
         "compressed image reserved from generic DELETE");
  expect(capabilityData.find("\"stored_encoding\":\"gzip\"") != std::string::npos, "gzip capability");
  const Api::Response defaultCalibration = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/epaper/calibration"));
  const std::string defaultCalibrationData = defaultCalibration.data.c_str();
  expect(defaultCalibration.statusCode == 200 &&
             defaultCalibrationData.find("\"source\":\"default\"") !=
                 std::string::npos &&
             defaultCalibrationData.find(
                 "\"id\":\"yellow\",\"code\":2") != std::string::npos &&
             defaultCalibrationData.find(
                 "\"id\":\"red\",\"code\":3") != std::string::npos,
         "calibration GET should expose defaults in EPD code order");

  JsonDocument updateBody;
  JsonObject colors = updateBody["colors"].to<JsonObject>();
  const EpaperCalibration::Profile defaults = EpaperCalibration::defaultProfile();
  for (size_t index = 0; index < EpaperCalibration::kColorCount; ++index) {
    const char *id = EpaperCalibration::definition(index).id;
    JsonObject color = colors[id].to<JsonObject>();
    color["r"] = defaults.display[index].r;
    color["g"] = defaults.display[index].g;
    color["b"] = defaults.display[index].b;
  }
  colors["red"]["r"] = 131;
  Api::Request update = requestFor(Api::Method::Put,
                                   "/api/epaper/calibration",
                                   "invalid-token");
  update.hasBody = true;
  update.hasJsonBody = true;
  update.body = updateBody.as<JsonVariantConst>();
  const Api::Response updated = fixture.router.dispatch(update);
  expect(updated.statusCode == 200 &&
             std::string(updated.data.c_str()).find(
                 "\"source\":\"persisted\"") != std::string::npos &&
             fixture.calibration.current.profile.display[3].r == 131,
         "public calibration PUT should persist a complete valid profile");

  colors["green"]["r"] = 131;
  colors["green"]["g"] = defaults.display[3].g;
  colors["green"]["b"] = defaults.display[3].b;
  const Api::Response duplicate = fixture.router.dispatch(update);
  expect(duplicate.statusCode == 400 &&
             std::string(duplicate.data.c_str()).find(
                 "\"code\":\"invalid_field\"") != std::string::npos &&
             fixture.calibration.current.profile.display[3].r == 131,
         "duplicate display RGB should fail without replacing canonical data");

  const Api::Response reset = fixture.router.dispatch(
      requestFor(Api::Method::Post, "/api/epaper/calibration/reset"));
  expect(reset.statusCode == 200 &&
             fixture.calibration.current.source == EpaperCalibrationSource::Default &&
             fixture.calibration.current.profile.display[3].r == 120,
         "public calibration reset should restore firmware defaults");

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

void testDeviceDiagnosticsAndPowerContract() {
  RouterFixture fixture(DeviceResetReason::Brownout);
  fixture.battery.current.sampleValid = true;
  fixture.battery.current.voltageMilliVolts = 3980;
  fixture.battery.current.estimate.available = true;
  fixture.battery.current.estimate.percent = 83;
  fixture.battery.current.sampleAgeMs = 42;

  const Api::Response response = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/device"));
  JsonDocument document;
  const DeserializationError error =
      deserializeJson(document, response.data.c_str());
  const JsonObjectConst diagnostics = document["diagnostics"].as<JsonObjectConst>();
  const JsonObjectConst power = document["power"].as<JsonObjectConst>();
  expect(response.statusCode == 200 && !error &&
             document["chip_model"].is<const char *>() &&
             document["hostname"].is<const char *>() &&
             document["wifi_tx_dbm"].is<int>() &&
             document["config_state"].is<const char *>() &&
             !diagnostics.isNull() &&
             diagnostics["reset_reason"].is<const char *>() &&
             std::string(diagnostics["reset_reason"].as<const char *>()) ==
                 "brownout" &&
             !power.isNull() && power["battery"].isUnbound() &&
             power["voltage_mv"].as<uint32_t>() == 3980 &&
             power["sample_age_ms"].as<uint32_t>() == 42 &&
             power["estimated_percent"].as<uint8_t>() == 83 &&
             power["power_source"].isUnbound() &&
             power["usb_present"].isUnbound() &&
             power["battery_present"].isUnbound() &&
             power["charging"].isUnbound() &&
             power["charge_state"].isUnbound(),
         "device resource should expose diagnostics and flat power fields");

  const Api::Response epaperStatus = fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/epaper/status"));
  expect(std::string(epaperStatus.data.c_str()).find(
             "\"last_reset_reason\":\"brownout\"") != std::string::npos,
         "device and e-paper resources should use the same boot reset reason");

  RouterFixture deepSleep(DeviceResetReason::DeepSleep);
  expect(std::string(deepSleep.router.dispatch(
             requestFor(Api::Method::Get, "/api/device")).data.c_str()).find(
             "\"reset_reason\":\"deep_sleep\"") != std::string::npos,
         "device resource should expose deep-sleep reset reasons");
  RouterFixture unknown(DeviceResetReason::Unknown);
  expect(std::string(unknown.router.dispatch(
             requestFor(Api::Method::Get, "/api/device")).data.c_str()).find(
             "\"reset_reason\":\"unknown\"") != std::string::npos,
         "device resource should expose the unknown fallback");

  fixture.battery.current = {};
  JsonDocument unavailable;
  deserializeJson(unavailable, fixture.router.dispatch(
      requestFor(Api::Method::Get, "/api/device")).data.c_str());
  expect(unavailable["power"]["voltage_mv"].isNull() &&
             unavailable["power"]["sample_age_ms"].isNull() &&
             unavailable["power"]["estimated_percent"].isNull() &&
             unavailable["diagnostics"]["reset_reason"].is<const char *>(),
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
void testHttpFilePreflightOrder() {
  RouterFixture f;
  const char *path = "/api/storage/files/a.txt";
  for (const String &token : {String(), String("invalid-token")}) {
    expect(UserFileHttpPreflight::upload(f.router, token, path, true, true,
        nullptr, 2, 3, "text/plain").statusCode == 401,
        "HTTP upload must authorize before query, length and media checks");
    expect(UserFileHttpPreflight::download(f.router, token, path, true, true).statusCode == 401,
        "HTTP download must authorize before body and query checks");
  }
  expect(f.userData.uploadBeginCount == 0 && f.userData.downloadBeginCount == 0,
         "HTTP authorization and shape gate must not open storage sessions");
  expect(UserFileHttpPreflight::upload(f.router, "valid-token", path, true, false,
      "2", 2, 2, "").statusCode == 400, "authorized upload query remains 400");
  expect(UserFileHttpPreflight::upload(f.router, "valid-token", path, false, false,
      nullptr, 2, 2, "").statusCode == 411, "authorized missing length remains 411");
  expect(UserFileHttpPreflight::upload(f.router, "valid-token", path, false, false,
      "bogus", 2, 2, "").statusCode == 411, "authorized invalid length remains 411");
  expect(UserFileHttpPreflight::upload(f.router, "valid-token", path, false, false,
      "2", 2, 2, "text/plain").statusCode == 415, "authorized media rejection remains 415");
  expect(UserFileHttpPreflight::download(f.router, "valid-token", path, true, false).statusCode == 400,
         "authorized download body remains 400");
  expect(UserFileHttpPreflight::download(f.router, "valid-token", path, false, true).statusCode == 400,
         "authorized download query remains 400");
  expect(UserFileHttpPreflight::upload(f.router, "", "/api/storage/files/bad/name", false,
      false, nullptr, 0, 0, "").statusCode == 404, "unknown dynamic route remains 404");
  expect(f.userData.uploadBeginCount == 0 && f.userData.downloadBeginCount == 0,
         "shape failures must not open storage sessions");
}
void testDeferredScanPrincipal() {
  for (auto transport : {Api::Transport::Http, Api::Transport::Serial}) {
    RouterFixture f;
    f.scanner.value.operationId = 77;
    auto request = requestFor(Api::Method::Get, "/api/wifi/scan", "valid-token");
    request.transport = transport;
    auto response = f.router.dispatch(request);
    expect(response.pending.id == 77 && response.pending.principal == "valid-token" &&
               response.pending.path == "/api/wifi/scan", "scan continuation binds route and principal");
    const auto pending = response.pending;
    f.auth.validToken = "new-token";
    expect(f.router.pollPending(pending, response) && response.statusCode == 401,
           "rotated principal cannot receive old scan result");
    f.auth.validToken = "valid-token";
    f.scanner.value.operationId = 0;
    expect(f.router.pollPending(pending, response) && response.statusCode == 200,
           "HTTP and serial scan completion retains 200 envelope");
  }
}
}  // namespace

int main() {
  testHttpFilePreflightOrder();
  testDeferredScanPrincipal();
  testExactRouteAuthorizationMatrix();
  testWebIdentityMatchesAcrossTransports();
  testDynamicFileAuthorizationAndTransport();
  testEpaperPublicContract();
  testDeviceDiagnosticsAndPowerContract();
  testUnknownRoutesRemainNotFound();
  if (failures != 0) {
    std::cerr << failures << " API router authorization test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "API router authorization matrix tests passed\n";
  return EXIT_SUCCESS;
}
