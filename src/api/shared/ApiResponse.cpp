#include "ApiResponse.h"

#include <cstring>

namespace Api {

String quote(const char *value) {
  const char *safeValue = value == nullptr ? "" : value;
  String escaped;
  escaped.reserve(strlen(safeValue) + 2);
  escaped += '"';
  constexpr char kHex[] = "0123456789abcdef";
  for (const unsigned char *cursor = reinterpret_cast<const unsigned char *>(safeValue);
       *cursor != '\0'; ++cursor) {
    switch (*cursor) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (*cursor < 0x20) {
          escaped += "\\u00";
          escaped += kHex[(*cursor >> 4) & 0x0f];
          escaped += kHex[*cursor & 0x0f];
        } else {
          escaped += static_cast<char>(*cursor);
        }
        break;
    }
  }
  escaped += '"';
  return escaped;
}

String quote(const String &value) {
  return quote(value.c_str());
}

Response ok(const String &data, const String &message) {
  return {200, true, data.length() == 0 ? "{}" : data, message};
}

Response created(const String &data, const String &message) {
  return {201, true, data.length() == 0 ? "{}" : data, message};
}

Response accepted(const String &data, const String &message) {
  return {202, true, data.length() == 0 ? "{}" : data, message};
}

Response error(int statusCode, const String &data, const String &message) {
  return {statusCode, false, data.length() == 0 ? "{}" : data, message};
}

Response problem(int statusCode,
                 const char *code,
                 const char *message,
                 const char *field) {
  JsonDocument data;
  data["code"] = code;
  if (field != nullptr && field[0] != '\0') {
    data["fields"].to<JsonArray>().add(field);
  }
  return error(statusCode, json(data), message);
}

Response unauthorized(const char *message) {
  JsonDocument data;
  data["code"] = "unauthorized";
  data["authenticated"] = false;
  return error(401, json(data), message);
}

Response decodeError(const JsonDecodeError &error) {
  return problem(400, error.code, error.message, error.field.c_str());
}

String json(JsonDocument &document) {
  String output;
  serializeJson(document, output);
  return output;
}

String serialize(const Response &response) {
  String body;
  body.reserve(response.data.length() + response.message.length() + 64);
  body += "{\"success\":";
  body += response.success ? "true" : "false";
  body += ",\"data\":";
  body += response.data.length() == 0 ? "{}" : response.data;
  body += ",\"message\":";
  body += quote(response.message);
  body += "}";
  return body;
}

}  // namespace Api
