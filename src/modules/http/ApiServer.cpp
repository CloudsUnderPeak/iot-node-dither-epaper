#include "ApiServer.h"

#include <WiFi.h>

#include "HttpJsonBody.h"
#include "HttpResponse.h"
#include "api/shared/ApiResponse.h"
#include "modules/storage/UserFilePolicy.h"

namespace {
struct BufferedJsonBody {
  size_t length = 0;
  bool tooLarge = false;
  uint8_t bytes[HttpJsonBody::kMaxBytes]{};
};

struct FileUploadState {
  bool initialized = false;
  bool active = false;
  bool failed = false;
  uint32_t sessionId = 0;
  int errorStatus = 500;
  char errorData[128] = "{}";
  char errorMessage[128] = "file upload failed";
};

FileUploadState *uploadState(AsyncWebServerRequest *request) {
  if (request->_tempObject == nullptr) {
    request->_tempObject = calloc(1, sizeof(FileUploadState));
    if (request->_tempObject == nullptr) return nullptr;
    auto *state = static_cast<FileUploadState *>(request->_tempObject);
    strlcpy(state->errorData, "{}", sizeof(state->errorData));
    strlcpy(state->errorMessage, "file upload failed", sizeof(state->errorMessage));
  }
  return static_cast<FileUploadState *>(request->_tempObject);
}

void saveUploadError(FileUploadState &state, const Api::Response &response) {
  state.failed = true;
  state.active = false;
  state.errorStatus = response.statusCode;
  strlcpy(state.errorData, response.data.c_str(), sizeof(state.errorData));
  strlcpy(state.errorMessage, response.message.c_str(), sizeof(state.errorMessage));
}
}  // namespace

Result ApiServer::begin(WifiManager *wifiManager,
                        const EmbeddedWebAssets *assets,
                        ApiRouter *router) {
  if (wifiManager == nullptr || assets == nullptr || router == nullptr) {
    return invalidInput("missing API server dependencies");
  }

  wifiManager_ = wifiManager;
  assets_ = assets;
  router_ = router;
  const Result routesResult = registerRoutes();
  if (!routesResult.ok()) return routesResult;
  server_.begin();
  started_ = true;
  return okResult();
}

bool ApiServer::started() const {
  return started_;
}

Result ApiServer::registerRoutes() {
  const ArBodyHandlerFunction jsonBody =
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        bufferJsonBody(request, data, len, index, total);
      };
  const ArBodyHandlerFunction fileBody =
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        bufferFileUpload(request, data, len, index, total);
      };
  const ArBodyHandlerFunction epaperBody =
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        bufferEpaperUpload(request, data, len, index, total);
      };

  for (size_t index = 0; index < ApiRouter::routeCount(); ++index) {
    ApiRouter::RouteInfo route;
    if (!ApiRouter::routeInfo(index, route)) {
      return invalidInput("failed to read API route metadata");
    }

    WebRequestMethodComposite httpMethod = HTTP_ANY;
    switch (route.method) {
      case Api::Method::Get:
        httpMethod = HTTP_GET;
        break;
      case Api::Method::Post:
        httpMethod = HTTP_POST;
        break;
      case Api::Method::Put:
        httpMethod = HTTP_PUT;
        break;
      case Api::Method::Delete:
        httpMethod = HTTP_DELETE;
        break;
      case Api::Method::Unknown:
        return invalidInput("API route has unsupported method");
    }

    AsyncURIMatcher matcher;
    if (route.routeMatch == ApiRouter::RouteMatch::Exact) {
      matcher = AsyncURIMatcher::exact(route.path);
    } else if (route.routeMatch == ApiRouter::RouteMatch::UserFile) {
      constexpr const char *kParameterSuffix = "/{name}";
      const size_t pathLength = strlen(route.path);
      const size_t suffixLength = strlen(kParameterSuffix);
      if (pathLength <= suffixLength ||
          strcmp(route.path + pathLength - suffixLength, kParameterSuffix) != 0) {
        return invalidInput("dynamic API route has invalid path pattern");
      }
      String directory(route.path);
      directory.remove(pathLength - suffixLength);
      matcher = AsyncURIMatcher::dir(directory);
    } else {
      return invalidInput("API route has unsupported matcher");
    }

    const Api::Method method = route.method;
    switch (route.httpBinding) {
      case ApiRouter::HttpBinding::Deferred:
      case ApiRouter::HttpBinding::NoBody:
        server_.on(
            matcher,
            httpMethod,
            [this, method](AsyncWebServerRequest *request) {
              dispatchNoBody(request, method);
            });
        break;
      case ApiRouter::HttpBinding::JsonBody:
        server_.on(
            matcher,
            httpMethod,
            [this, method](AsyncWebServerRequest *request) {
              dispatchJson(request, method);
            },
            nullptr,
            jsonBody);
        break;
      case ApiRouter::HttpBinding::Query:
        server_.on(
            matcher,
            httpMethod,
            [this, method](AsyncWebServerRequest *request) {
              dispatchQuery(request, method);
            });
        break;
      case ApiRouter::HttpBinding::RawUpload:
        server_.on(
            matcher,
            httpMethod,
            [this](AsyncWebServerRequest *request) {
              handleFileUpload(request);
            },
            nullptr,
            fileBody);
        break;
      case ApiRouter::HttpBinding::RawDownload:
        server_.on(
            matcher,
            httpMethod,
            [this](AsyncWebServerRequest *request) {
              handleFileDownload(request);
            });
        break;
      case ApiRouter::HttpBinding::EpaperRawUpload:
        server_.on(
            matcher,
            httpMethod,
            [this](AsyncWebServerRequest *request) {
              handleEpaperUpload(request);
            },
            nullptr,
            epaperBody);
        break;
      case ApiRouter::HttpBinding::EpaperRawDownload:
        server_.on(
            matcher,
            httpMethod,
            [this](AsyncWebServerRequest *request) {
              handleEpaperDownload(request);
            });
        break;
    }
  }

  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleIndex(request); });
  server_.on("/index.html", HTTP_GET, [this](AsyncWebServerRequest *request) { handleIndex(request); });

  server_.onNotFound([this](AsyncWebServerRequest *request) { handleNotFound(request); });
  return okResult();
}

void ApiServer::dispatchNoBody(AsyncWebServerRequest *request, Api::Method method) {
  const String path = request->url();
  Api::Request apiRequest;
  apiRequest.method = method;
  apiRequest.path = path.c_str();
  apiRequest.hasJsonBody = false;
  apiRequest.hasBody = request->contentLength() != 0;
  apiRequest.transport = Api::Transport::Http;
  apiRequest.token = bearerTokenFromRequest(request);
  const auto response = router_->dispatch(apiRequest);
  if (response.pending.id == 0) {
    sendApiResponse(request, response);
    return;
  }
  const auto weak = request->pause();
  if (!pending_.store(weak, response.pending)) {
    router_->cancelPending(response.pending);
    sendApiResponse(request, Api::problem(409, "wifi_scan_busy", "response slot is busy"));
    return;
  }
  const uint32_t id = response.pending.id;
  request->onDisconnect([this, id]() {
    PendingResponseSlot<AsyncWebServerRequestPtr>::Record detached;
    if (pending_.take(id, detached)) router_->cancelPending(detached.pending);
  });
}

void ApiServer::poll() {
  if (!started_) return;
  const auto record = pending_.snapshot();
  if (record.pending.id == 0) return;
  PendingResponseSlot<AsyncWebServerRequestPtr>::Record detached;
  if (record.request.expired()) {
    if (pending_.take(record.pending.id, detached)) router_->cancelPending(detached.pending);
    return;
  }
  Api::Response response;
  if (!router_->pollPending(record.pending, response)) return;
  if (!pending_.take(record.pending.id, detached)) return;
  if (auto request = detached.request.lock()) sendApiResponse(request.get(), response);
}

void ApiServer::dispatchQuery(AsyncWebServerRequest *request,
                              Api::Method method) {
  const String path = request->url();
  Api::Request apiRequest;
  apiRequest.method = method;
  apiRequest.path = path.c_str();
  apiRequest.hasBody = request->contentLength() != 0;
  apiRequest.transport = Api::Transport::Http;
  apiRequest.token = bearerTokenFromRequest(request);
  populateQuery(request, apiRequest);
  sendApiResponse(request, router_->dispatch(apiRequest));
}

void ApiServer::dispatchJson(AsyncWebServerRequest *request, Api::Method method) {
  const auto *buffer = static_cast<const BufferedJsonBody *>(request->_tempObject);
  size_t bodyLength = buffer == nullptr ? 0 : buffer->length;
  if (request->contentLength() > HttpJsonBody::kMaxBytes ||
      (buffer != nullptr && buffer->tooLarge)) {
    bodyLength = HttpJsonBody::kMaxBytes + 1;
  }
  JsonDocument document;
  const Api::Response parseResult = HttpJsonBody::parse(
      request->contentType().c_str(), buffer == nullptr ? nullptr : buffer->bytes, bodyLength, document);
  if (!parseResult.success) {
    sendApiResponse(request, parseResult);
    return;
  }

  const String path = request->url();
  Api::Request apiRequest;
  apiRequest.method = method;
  apiRequest.path = path.c_str();
  apiRequest.body = document.as<JsonVariantConst>();
  apiRequest.hasJsonBody = true;
  apiRequest.hasBody = true;
  apiRequest.transport = Api::Transport::Http;
  apiRequest.token = bearerTokenFromRequest(request);
  sendApiResponse(request, router_->dispatch(apiRequest));
}

void ApiServer::prepareFileUpload(AsyncWebServerRequest *request,
                                  size_t callbackTotal) {
  FileUploadState *state = uploadState(request);
  if (state == nullptr || state->initialized) return;
  state->initialized = true;

  for (size_t index = 0; index < request->params(); ++index) {
    const AsyncWebParameter *parameter = request->getParam(index);
    if (parameter != nullptr && !parameter->isPost() && !parameter->isFile()) {
      saveUploadError(*state, Api::problem(
          400, "unsupported_field", "file upload does not accept query fields"));
      return;
    }
  }

  if (request->hasHeader("Transfer-Encoding") ||
      !request->hasHeader("Content-Length")) {
    saveUploadError(*state, Api::problem(
        411, "content_length_required", "file upload requires Content-Length"));
    return;
  }
  size_t declaredBytes = 0;
  const String declaredHeader = request->getHeader("Content-Length")->value();
  if (!UserFilePolicy::parseSize(declaredHeader.c_str(), declaredBytes) ||
      declaredBytes != request->contentLength() ||
      (callbackTotal != 0 && callbackTotal != declaredBytes)) {
    saveUploadError(*state, Api::problem(
        411, "content_length_required", "invalid Content-Length"));
    return;
  }

  if (request->contentType().length() != 0) {
    String contentType = request->contentType();
    contentType.toLowerCase();
    // ESPAsyncWebServer consumes form bodies, and may heuristically consume
    // text/plain bodies that resemble key=value, before invoking our raw body
    // callback. Reject those media types so byte delivery is deterministic.
    if (contentType.startsWith("multipart/form-data") ||
        contentType.startsWith("application/x-www-form-urlencoded") ||
        contentType.startsWith("text/plain")) {
      saveUploadError(*state, Api::problem(
          415,
          "unsupported_media_type",
          "raw file upload requires application/octet-stream or no Content-Type"));
      return;
    }
  }

  const ApiRouter::FileUploadStart start = streams_.run([&]() { return router_->prepareFileUpload(
      bearerTokenFromRequest(request), request->url().c_str(), declaredBytes); });
  if (!start.ready) {
    saveUploadError(*state, start.response);
    return;
  }
  state->sessionId = start.sessionId;
  state->active = true;
  const uint32_t sessionId = state->sessionId;
  request->onDisconnect([this, sessionId]() {
    streams_.run([&]() { return router_->abortFileUpload(sessionId); });
  });
}

void ApiServer::bufferFileUpload(AsyncWebServerRequest *request,
                                 uint8_t *data,
                                 size_t len,
                                 size_t index,
                                 size_t total) {
  prepareFileUpload(request, total);
  FileUploadState *state = static_cast<FileUploadState *>(request->_tempObject);
  if (state == nullptr || state->failed || !state->active) return;
  const Api::Response writeResult = streams_.run([&]() { return router_->writeFileUpload(
      state->sessionId, index, data, len); });
  if (!writeResult.success) saveUploadError(*state, writeResult);
}

void ApiServer::handleFileUpload(AsyncWebServerRequest *request) {
  prepareFileUpload(request, request->contentLength());
  FileUploadState *state = static_cast<FileUploadState *>(request->_tempObject);
  if (state == nullptr) {
    sendApiResponse(request, Api::problem(500, "storage_error", "failed to allocate upload state"));
    return;
  }
  if (state->failed) {
    sendApiResponse(request, Api::error(
        state->errorStatus, state->errorData, state->errorMessage));
    return;
  }
  const uint32_t sessionId = state->sessionId;
  state->active = false;
  sendApiResponse(request, streams_.run([&]() { return router_->finishFileUpload(
      sessionId, request->url().c_str()); }));
}

void ApiServer::handleFileDownload(AsyncWebServerRequest *request) {
  if (request->contentLength() != 0) {
    sendApiResponse(request, Api::problem(
        400, "unsupported_field", "file download does not accept a request body"));
    return;
  }
  for (size_t index = 0; index < request->params(); ++index) {
    const AsyncWebParameter *parameter = request->getParam(index);
    if (parameter != nullptr && !parameter->isPost() && !parameter->isFile()) {
      sendApiResponse(request, Api::problem(
          400, "unsupported_field", "file download does not accept query fields"));
      return;
    }
  }
  const String path = request->url();
  const String range = request->hasHeader("Range")
                           ? request->getHeader("Range")->value()
                           : String();
  const ApiRouter::FileDownloadStart start = streams_.run([&]() { return router_->prepareFileDownload(
      bearerTokenFromRequest(request), path.c_str(), range.c_str()); });
  if (!start.ready) {
    if (start.response.statusCode == 416) {
      AsyncWebServerResponse *response = request->beginResponse(
          416, "application/json", Api::serialize(start.response));
      String contentRange = "bytes */";
      contentRange += String(start.fileSize);
      response->addHeader("Content-Range", contentRange);
      request->send(response);
    } else {
      sendApiResponse(request, start.response);
    }
    return;
  }

  const UserFilePolicy::MediaPolicy media =
      UserFilePolicy::mediaPolicyForName(start.name);
  const uint32_t sessionId = start.sessionId;
  // A zero-length callback response never asks its filler for data, so release
  // the storage gate here instead of waiting for a client disconnect.
  if (start.contentLength == 0) streams_.run([&]() { return router_->finishFileDownload(sessionId); });
  AsyncWebServerResponse *response = request->beginResponse(
      media.contentType,
      start.contentLength,
      [this, sessionId](uint8_t *buffer, size_t maxLength, size_t) {
        const UserDataReadResult result = streams_.run([&]() { return router_->readFileDownload(
            sessionId, buffer, maxLength); });
        return result.result.ok() ? result.bytesRead : 0;
      });
  response->setCode(start.partial ? 206 : 200);
  response->addHeader("Accept-Ranges", "bytes");
  response->addHeader("Cache-Control", "no-store");
  response->addHeader("X-Content-Type-Options", "nosniff");
  String disposition = media.inlineDisposition ? "inline; filename=\"" : "attachment; filename=\"";
  disposition += start.name;
  disposition += '"';
  response->addHeader("Content-Disposition", disposition);
  if (start.contentLength == 0) response->addHeader("Content-Length", "0");
  if (start.partial) {
    String contentRange = "bytes ";
    contentRange += String(start.rangeStart);
    contentRange += '-';
    contentRange += String(start.rangeStart + start.contentLength - 1);
    contentRange += '/';
    contentRange += String(start.fileSize);
    response->addHeader("Content-Range", contentRange);
  }
  request->onDisconnect([this, sessionId]() {
    streams_.run([&]() { return router_->finishFileDownload(sessionId); });
  });
  request->send(response);
}

void ApiServer::prepareEpaperUpload(AsyncWebServerRequest *request,
                                    size_t callbackTotal) {
  FileUploadState *state = uploadState(request);
  if (state == nullptr || state->initialized) return;
  state->initialized = true;
  for (size_t index = 0; index < request->params(); ++index) {
    const AsyncWebParameter *parameter = request->getParam(index);
    if (parameter != nullptr && !parameter->isPost() && !parameter->isFile()) {
      saveUploadError(*state, Api::problem(
          400, "unsupported_field", "e-paper upload does not accept query fields"));
      return;
    }
  }
  if (request->hasHeader("Transfer-Encoding") ||
      !request->hasHeader("Content-Length")) {
    saveUploadError(*state, Api::problem(
        411, "content_length_required", "e-paper upload requires Content-Length"));
    return;
  }
  size_t declaredBytes = 0;
  const String declaredHeader = request->getHeader("Content-Length")->value();
  if (!UserFilePolicy::parseSize(declaredHeader.c_str(), declaredBytes) ||
      declaredBytes != request->contentLength() ||
      (callbackTotal != 0 && callbackTotal != declaredBytes)) {
    saveUploadError(*state, Api::problem(
        411, "content_length_required",
        "invalid compressed Content-Length"));
    return;
  }
  if (request->contentType().length() != 0) {
    String contentType = request->contentType();
    contentType.toLowerCase();
    if (contentType != "application/octet-stream") {
      saveUploadError(*state, Api::problem(
          415, "unsupported_media_type",
          "e-paper upload requires application/octet-stream or no Content-Type"));
      return;
    }
  }
  String encoding = request->hasHeader("Content-Encoding")
      ? request->getHeader("Content-Encoding")->value() : String();
  encoding.trim();
  encoding.toLowerCase();
  const ApiRouter::FileUploadStart start =
      streams_.run([&]() { return router_->prepareEpaperUpload(declaredBytes, encoding.c_str()); });
  if (!start.ready) {
    saveUploadError(*state, start.response);
    return;
  }
  state->sessionId = start.sessionId;
  state->active = true;
  const uint32_t sessionId = state->sessionId;
  request->onDisconnect([this, sessionId]() {
    streams_.run([&]() { return router_->abortEpaperUpload(sessionId); });
  });
}

void ApiServer::bufferEpaperUpload(AsyncWebServerRequest *request,
                                   uint8_t *data,
                                   size_t len,
                                   size_t index,
                                   size_t total) {
  prepareEpaperUpload(request, total);
  auto *state = static_cast<FileUploadState *>(request->_tempObject);
  if (state == nullptr || state->failed || !state->active) return;
  const Api::Response result =
      streams_.run([&]() { return router_->writeEpaperUpload(state->sessionId, index, data, len); });
  if (!result.success) saveUploadError(*state, result);
}

void ApiServer::handleEpaperUpload(AsyncWebServerRequest *request) {
  prepareEpaperUpload(request, request->contentLength());
  auto *state = static_cast<FileUploadState *>(request->_tempObject);
  if (state == nullptr) {
    sendApiResponse(request, Api::problem(
        500, "storage_error", "failed to allocate upload state"));
    return;
  }
  if (state->failed) {
    sendApiResponse(request, Api::error(
        state->errorStatus, state->errorData, state->errorMessage));
    return;
  }
  const uint32_t sessionId = state->sessionId;
  state->active = false;
  sendApiResponse(request, streams_.run([&]() { return router_->finishEpaperUpload(sessionId); }));
}

void ApiServer::handleEpaperDownload(AsyncWebServerRequest *request) {
  if (request->contentLength() != 0 || request->params() != 0) {
    sendApiResponse(request, Api::problem(
        400, "unsupported_field",
        "e-paper download does not accept fields or a request body"));
    return;
  }
  const String range = request->hasHeader("Range")
                           ? request->getHeader("Range")->value()
                           : String();
  const ApiRouter::FileDownloadStart start =
      streams_.run([&]() { return router_->prepareEpaperDownload(range.c_str()); });
  if (!start.ready) {
    if (start.response.statusCode == 416) {
      AsyncWebServerResponse *response = request->beginResponse(
          416, "application/json", Api::serialize(start.response));
      String contentRange = "bytes */";
      contentRange += String(start.fileSize);
      response->addHeader("Content-Range", contentRange);
      request->send(response);
    } else {
      sendApiResponse(request, start.response);
    }
    return;
  }
  const uint32_t sessionId = start.sessionId;
  if (start.contentLength == 0) streams_.run([&]() { return router_->finishEpaperDownload(sessionId); });
  AsyncWebServerResponse *response = request->beginResponse(
      "application/octet-stream",
      start.contentLength,
      [this, sessionId](uint8_t *buffer, size_t maxLength, size_t) {
        const UserDataReadResult result = streams_.run([&]() { return router_->readEpaperDownload(
            sessionId, buffer, maxLength); });
        return result.result.ok() ? result.bytesRead : 0;
      });
  response->setCode(start.partial ? 206 : 200);
  response->addHeader("Accept-Ranges", "bytes");
  response->addHeader("Cache-Control", "no-store");
  response->addHeader("X-Content-Type-Options", "nosniff");
  response->addHeader(
      "Content-Disposition",
      "attachment; filename=\"epaper-current.epd\"");
  if (start.partial) {
    String contentRange = "bytes ";
    contentRange += String(start.rangeStart);
    contentRange += '-';
    contentRange += String(start.rangeStart + start.contentLength - 1);
    contentRange += '/';
    contentRange += String(start.fileSize);
    response->addHeader("Content-Range", contentRange);
  }
  request->onDisconnect([this, sessionId]() {
    streams_.run([&]() { return router_->finishEpaperDownload(sessionId); });
  });
  request->send(response);
}

void ApiServer::populateQuery(AsyncWebServerRequest *request,
                              Api::Request &apiRequest) const {
  for (size_t index = 0; index < request->params(); ++index) {
    const AsyncWebParameter *parameter = request->getParam(index);
    if (parameter != nullptr && !parameter->isPost() && !parameter->isFile()) {
      apiRequest.addQueryParameter(
          parameter->name().c_str(), parameter->value().c_str());
    }
  }
}

void ApiServer::bufferJsonBody(AsyncWebServerRequest *request,
                               uint8_t *data,
                               size_t len,
                               size_t index,
                               size_t total) {
  if (total > HttpJsonBody::kMaxBytes) return;
  if (request->_tempObject == nullptr) {
    request->_tempObject = calloc(1, sizeof(BufferedJsonBody));
    if (request->_tempObject == nullptr) return;
  }
  auto *buffer = static_cast<BufferedJsonBody *>(request->_tempObject);
  if (index > HttpJsonBody::kMaxBytes || len > HttpJsonBody::kMaxBytes - index) {
    buffer->tooLarge = true;
    return;
  }
  memcpy(buffer->bytes + index, data, len);
  const size_t received = index + len;
  if (received > buffer->length) buffer->length = received;
}

String ApiServer::bearerTokenFromRequest(AsyncWebServerRequest *request) const {
  if (!request->hasHeader("Authorization")) {
    return "";
  }

  const String authorization = request->getHeader("Authorization")->value();
  if (!authorization.startsWith("Bearer ")) {
    return "";
  }
  return authorization.substring(7);
}

void ApiServer::sendApiResponse(AsyncWebServerRequest *request, const Api::Response &response) const {
  HttpResponse::send(request, response);
}

void ApiServer::handleIndex(AsyncWebServerRequest *request) {
  if (!sendEmbeddedAsset(request, "/index.html")) {
    const char *message = assets_->bundled()
                              ? "index not found"
                              : "frontend not bundled";
    HttpResponse::send(
        request, Api::error(404, "{\"code\":\"not_found\"}", message));
  }
}

bool ApiServer::sendEmbeddedAsset(AsyncWebServerRequest *request,
                                  const char *path) {
  if (!assets_->ready()) return false;
  const EmbeddedWebAsset *asset = assets_->find(path);
  if (asset == nullptr) return false;

  AsyncWebServerResponse *response = request->beginResponse(
      200, asset->contentType, asset->bytes, asset->size);
  if (asset->contentEncoding != nullptr && asset->contentEncoding[0] != '\0') {
    response->addHeader("Content-Encoding", asset->contentEncoding);
    response->addHeader("Vary", "Accept-Encoding");
  }
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
  return true;
}

void ApiServer::handleCaptivePortal(AsyncWebServerRequest *request) {
  const WifiStatus status = wifiManager_->status();
  String target = "/";
  if (status.apIp != IPAddress(0, 0, 0, 0)) {
    target = "http://";
    target += status.apIp.toString();
    target += "/";
  }
  request->redirect(target);
}

void ApiServer::handleNotFound(AsyncWebServerRequest *request) {
  const String url = request->url();
  if (request->method() == HTTP_GET && sendEmbeddedAsset(request, url.c_str())) {
    return;
  }
  const WifiStatus status = wifiManager_->status();
  const bool arrivedOnAp = status.apState == WifiApState::Active &&
                           request->client()->localIP() == status.apIp;
  const bool setupPageRequest =
      request->method() == HTTP_GET &&
      arrivedOnAp &&
      !url.startsWith("/api/") &&
      !url.startsWith("/assets/");
  if (setupPageRequest) {
    handleCaptivePortal(request);
    return;
  }

  String message = "not found ";
  message += url;
  HttpResponse::send(request, Api::error(404, "{\"code\":\"not_found\"}", message));
}
