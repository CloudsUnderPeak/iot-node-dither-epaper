#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "../../core/Result.h"
#include "api/ApiRouter.h"
#include "../storage/EmbeddedWebAssets.h"
#include "../wifi/WifiManager.h"

// HTTP adapter for static files, captive-portal redirects, and REST transport.
// API business rules live in ApiRouter so serial and HTTP stay aligned.
class ApiServer {
 public:
  Result begin(WifiManager *wifiManager,
               const EmbeddedWebAssets *assets,
               ApiRouter *router);
  bool started() const;

 private:
  AsyncWebServer server_{80};
  WifiManager *wifiManager_ = nullptr;
  const EmbeddedWebAssets *assets_ = nullptr;
  ApiRouter *router_ = nullptr;
  bool started_ = false;

  Result registerRoutes();
  void dispatchNoBody(AsyncWebServerRequest *request, Api::Method method);
  void dispatchQuery(AsyncWebServerRequest *request, Api::Method method);
  void dispatchJson(AsyncWebServerRequest *request, Api::Method method);
  void bufferJsonBody(AsyncWebServerRequest *request,
                      uint8_t *data,
                      size_t len,
                      size_t index,
                      size_t total);
  void bufferFileUpload(AsyncWebServerRequest *request,
                        uint8_t *data,
                        size_t len,
                        size_t index,
                        size_t total);
  void handleFileUpload(AsyncWebServerRequest *request);
  void handleFileDownload(AsyncWebServerRequest *request);
  void prepareFileUpload(AsyncWebServerRequest *request, size_t callbackTotal);
  void populateQuery(AsyncWebServerRequest *request, Api::Request &apiRequest) const;
  String bearerTokenFromRequest(AsyncWebServerRequest *request) const;
  void sendApiResponse(AsyncWebServerRequest *request, const Api::Response &response) const;
  bool sendEmbeddedAsset(AsyncWebServerRequest *request, const char *path);
  void handleIndex(AsyncWebServerRequest *request);
  void handleCaptivePortal(AsyncWebServerRequest *request);
  void handleNotFound(AsyncWebServerRequest *request);
};
