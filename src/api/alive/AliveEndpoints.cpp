#include "AliveEndpoints.h"

#include "api/shared/ApiResponse.h"

namespace AliveEndpoints {

Api::Response get() {
  return Api::ok("{}", "ok");
}

}  // namespace AliveEndpoints
