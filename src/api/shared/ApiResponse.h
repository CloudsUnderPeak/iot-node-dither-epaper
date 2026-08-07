#pragma once

#include "ApiTypes.h"
#include "JsonReader.h"

namespace Api {

String quote(const char *value);
String quote(const String &value);
Response ok(const String &data, const String &message = "ok");
Response created(const String &data, const String &message);
Response accepted(const String &data, const String &message);
Response error(int statusCode, const String &data, const String &message);
Response problem(int statusCode,
                 const char *code,
                 const char *message,
                 const char *field = nullptr);
Response unauthorized(const char *message = "unauthorized");
Response decodeError(const JsonDecodeError &error);
String json(JsonDocument &document);
String serialize(const Response &response);

}  // namespace Api
