#include "WebEndpoints.h"

#include "api/shared/ApiResponse.h"

namespace WebEndpoints {

Api::Response get(const EmbeddedWebAssets &assets) {
  JsonDocument data;
  data["source"] = assets.source();
  data["sha256"] = assets.sha256();
  return Api::ok(Api::json(data));
}

}  // namespace WebEndpoints
