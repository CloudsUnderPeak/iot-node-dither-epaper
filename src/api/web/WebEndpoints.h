#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/storage/EmbeddedWebAssets.h"

// Handles GET /api/web.
namespace WebEndpoints {

Api::Response get(const EmbeddedWebAssets &assets);

}  // namespace WebEndpoints
