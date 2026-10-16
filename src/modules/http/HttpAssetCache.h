#pragma once

#include <cstddef>

namespace HttpAssetCache {

constexpr const char *kCacheControl = "private, no-cache";
constexpr size_t kMaxIfNoneMatchBytes = 1024;

// GET cache validation uses weak comparison. Malformed/oversized fields miss
// the cache so the caller sends the current representation in full.
bool matches(const char *ifNoneMatch, const char *etag);

}  // namespace HttpAssetCache
