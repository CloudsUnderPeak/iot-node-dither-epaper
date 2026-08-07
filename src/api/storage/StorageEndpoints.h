#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/storage/FlashStorage.h"
#include "modules/storage/UserDataStorage.h"

// Handles GET /api/storage.
namespace StorageEndpoints {

Api::Response get(const FlashStorage &flashStorage,
                  const UserDataStorage &userData);

}  // namespace StorageEndpoints
