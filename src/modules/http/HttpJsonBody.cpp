#include "HttpJsonBody.h"

#include "api/shared/ApiResponse.h"

namespace {
bool asciiEqualIgnoreCase(char left, char right) {
  if (left >= 'A' && left <= 'Z') left = static_cast<char>(left + ('a' - 'A'));
  if (right >= 'A' && right <= 'Z') right = static_cast<char>(right + ('a' - 'A'));
  return left == right;
}

bool isJsonContentType(const char *value) {
  constexpr char kJson[] = "application/json";
  if (value == nullptr) return false;
  size_t index = 0;
  for (; kJson[index] != '\0'; ++index) {
    if (value[index] == '\0' || !asciiEqualIgnoreCase(value[index], kJson[index])) return false;
  }
  while (value[index] == ' ' || value[index] == '\t') ++index;
  return value[index] == '\0' || value[index] == ';';
}
}  // namespace

namespace HttpJsonBody {

Api::Response parse(const char *contentType,
                    const uint8_t *body,
                    size_t bodyLength,
                    JsonDocument &document) {
  if (bodyLength > kMaxBytes) {
    return Api::problem(413, "payload_too_large", "JSON body exceeds 2048 bytes");
  }
  if (!isJsonContentType(contentType)) {
    return Api::problem(400, "invalid_json", "Content-Type must be application/json");
  }
  if (body == nullptr || bodyLength == 0) {
    return Api::problem(400, "invalid_json", "JSON body is required");
  }
  const DeserializationError error = deserializeJson(document, body, bodyLength);
  if (error) {
    return Api::problem(400, "invalid_json", "invalid JSON body");
  }
  return Api::ok("{}");
}

}  // namespace HttpJsonBody
