#include "HttpResponse.h"

#include "api/shared/ApiResponse.h"

namespace HttpResponse {

void sendJson(AsyncWebServerRequest *request, int statusCode, const String &body) {
  request->send(statusCode, "application/json", body);
}

void send(AsyncWebServerRequest *request, const Api::Response &response) {
  sendJson(request, response.statusCode, Api::serialize(response));
}

}  // namespace HttpResponse
