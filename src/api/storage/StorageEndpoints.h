#pragma once
#include "core/ProjectFeatures.h"

#include "api/shared/ApiTypes.h"
#include "modules/storage/FlashStorage.h"
#include "modules/storage/UserDataStorage.h"

// Handles GET /api/storage.
namespace StorageEndpoints {

Api::Response get(const FlashStorage &flashStorage
#if IOT_FEATURE_STORAGE
                  , const UserDataStorage &userData
#endif
);

}  // namespace StorageEndpoints
