#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "api/shared/ApiTypes.h"

namespace HttpResponse {

void sendJson(AsyncWebServerRequest *request, int statusCode, const String &body);
void send(AsyncWebServerRequest *request, const Api::Response &response);

}  // namespace HttpResponse
