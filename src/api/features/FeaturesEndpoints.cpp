#include "FeaturesEndpoints.h"
#include "api/shared/ApiResponse.h"
#include "core/ProjectFeatures.h"
#include <cstring>

namespace FeaturesEndpoints {
Api::Response get(const Api::Request &request) {
  if (request.hasBody || request.hasJsonBody || request.queryOverflow || request.queryCount > 1)
    return Api::problem(400, "unsupported_field", "features accepts only an optional name query");
  const char *names[] = {"sleep", "epaper", "storage", "auth", "user_files", "mdns", "battery", "console", "status_led"};
  const bool supported[] = {IOT_FEATURE_SLEEP != 0, IOT_FEATURE_EPAPER != 0,
    IOT_FEATURE_STORAGE != 0, IOT_FEATURE_AUTH != 0, IOT_FEATURE_USER_FILES != 0,
    IOT_FEATURE_MDNS != 0, IOT_FEATURE_BATTERY != 0, IOT_FEATURE_CONSOLE != 0, IOT_FEATURE_STATUS_LED != 0};
  JsonDocument data;
  if (request.queryCount) {
    if (!(request.query[0].name == "name"))
      return Api::problem(400, "unsupported_field", "unsupported query field");
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
      if (request.query[0].value == names[i]) {
        data["feature"] = names[i];
        data["supported"] = supported[i];
        return Api::ok(Api::json(data));
      }
    }
    return Api::problem(400, "invalid_field", "unknown feature", "name");
  }
  for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    data["features"][names[i]] = supported[i];
  return Api::ok(Api::json(data));
}
}
