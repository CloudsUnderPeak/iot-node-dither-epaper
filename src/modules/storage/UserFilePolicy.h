#pragma once

#include <cstddef>

namespace UserFilePolicy {

constexpr size_t kMaxFilenameBytes = 64;
constexpr size_t kDefaultListLimit = 50;
constexpr size_t kMaxListLimit = 100;

struct MediaPolicy {
  const char *contentType = "application/octet-stream";
  bool inlineDisposition = false;
};

struct ByteRange {
  bool partial = false;
  size_t start = 0;
  size_t length = 0;
};

bool validPublicName(const char *name);
MediaPolicy mediaPolicyForName(const char *name);

// Strict decimal parser used for Content-Length, list limits, and offsets.
// Signs, whitespace, empty values, and size_t overflow are rejected.
bool parseSize(const char *value, size_t &parsed);

// An empty range header selects the complete representation. Non-empty values
// accept exactly one RFC byte range: start-end, start-, or -suffix-length.
bool parseRange(const char *header, size_t fileSize, ByteRange &range);

// Cursor values are deliberately opaque to clients. Empty selects offset 0.
bool encodeCursor(size_t offset, char *output, size_t outputSize);
bool decodeCursor(const char *cursor, size_t &offset);

}  // namespace UserFilePolicy
