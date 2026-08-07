#include "EmbeddedWebAssets.h"

#include <cstring>

#include "EmbeddedWebAssets.generated.h"

namespace {
bool validSource(const char *source) {
  return strcmp(source, "builtin") == 0 ||
         strcmp(source, "user") == 0 ||
         strcmp(source, "none") == 0;
}

bool validSha256(const char *sha256) {
  if (sha256 == nullptr) return false;
  if (strlen(sha256) != 64) return false;
  for (const char *cursor = sha256; *cursor != '\0'; ++cursor) {
    if ((*cursor < '0' || *cursor > '9') &&
        (*cursor < 'a' || *cursor > 'f')) {
      return false;
    }
  }
  return true;
}
}  // namespace

Result EmbeddedWebAssets::begin() {
  const bool noFrontend = strcmp(source(), "none") == 0;
  const bool assetsValid = noFrontend
                               ? EmbeddedWebGenerated::kAssetCount == 0 &&
                                     find("/index.html") == nullptr
                               : EmbeddedWebGenerated::kAssetCount > 0 &&
                                     find("/index.html") != nullptr;
  const bool identityValid = noFrontend
                                 ? sha256() == nullptr
                                 : validSha256(sha256());
  ready_ = validSource(source()) && assetsValid && identityValid;
  return ready_ ? okResult()
                : storageError("embedded frontend assets are invalid");
}

bool EmbeddedWebAssets::ready() const {
  return ready_;
}

bool EmbeddedWebAssets::bundled() const {
  return strcmp(source(), "none") != 0;
}

const EmbeddedWebAsset *EmbeddedWebAssets::find(const char *path) const {
  if (path == nullptr) return nullptr;
  const char *normalized = strcmp(path, "/") == 0 ? "/index.html" : path;
  for (size_t i = 0; i < EmbeddedWebGenerated::kAssetCount; ++i) {
    if (strcmp(normalized, EmbeddedWebGenerated::kAssets[i].path) == 0) {
      return &EmbeddedWebGenerated::kAssets[i];
    }
  }
  return nullptr;
}

size_t EmbeddedWebAssets::count() const {
  return EmbeddedWebGenerated::kAssetCount;
}

size_t EmbeddedWebAssets::payloadBytes() const {
  return EmbeddedWebGenerated::kPayloadBytes;
}

const char *EmbeddedWebAssets::source() const {
  return EmbeddedWebGenerated::kWebSource;
}

const char *EmbeddedWebAssets::sha256() const {
  return EmbeddedWebGenerated::kWebSha256;
}
