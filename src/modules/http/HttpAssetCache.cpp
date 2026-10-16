#include "HttpAssetCache.h"

#include <cstring>

namespace {
void skipWhitespace(const char *&cursor) {
  while (*cursor == ' ' || *cursor == '\t') ++cursor;
}
}  // namespace

namespace HttpAssetCache {

bool matches(const char *ifNoneMatch, const char *etag) {
  if (ifNoneMatch == nullptr || etag == nullptr || etag[0] != '"') return false;
  size_t length = 0;
  while (length <= kMaxIfNoneMatchBytes && ifNoneMatch[length] != '\0') ++length;
  if (length > kMaxIfNoneMatchBytes) return false;

  const size_t etagLength = strlen(etag);
  const char *cursor = ifNoneMatch;
  skipWhitespace(cursor);
  if (*cursor == '*') {
    ++cursor;
    skipWhitespace(cursor);
    return *cursor == '\0';
  }

  bool matched = false;
  while (*cursor != '\0') {
    skipWhitespace(cursor);
    // HTTP list syntax permits empty elements between comma separators.
    if (*cursor == ',') {
      ++cursor;
      continue;
    }
    if (*cursor == '\0') break;
    if (cursor[0] == 'W' && cursor[1] == '/') cursor += 2;
    if (*cursor != '"') return false;
    const char *tag = cursor++;
    while (*cursor != '\0' && *cursor != '"') {
      const unsigned char byte = static_cast<unsigned char>(*cursor);
      if (byte < 0x21 || byte == 0x7f) return false;
      ++cursor;
    }
    if (*cursor != '"') return false;
    ++cursor;
    const size_t tagLength = static_cast<size_t>(cursor - tag);
    matched = matched || (tagLength == etagLength && memcmp(tag, etag, tagLength) == 0);
    skipWhitespace(cursor);
    if (*cursor != '\0' && *cursor != ',') return false;
  }
  return matched;
}

}  // namespace HttpAssetCache
