#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/storage/UserDataStorage.h"

namespace UserFileEndpoints {

Api::Response list(const Api::Request &request, UserDataStorage &storage);
Api::Response remove(const Api::Request &request,
                     const char *name,
                     UserDataStorage &storage);
Api::Response transportUnsupported();
Api::Response fromStorageResult(const UserDataFileResult &result,
                                size_t detailBytes = 0);
Api::Response uploadCommitted(const char *name,
                              const UserDataUploadCommit &commit);

}  // namespace UserFileEndpoints
