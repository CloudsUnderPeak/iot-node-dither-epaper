#include "UserFilePolicy.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace UserFilePolicy {
namespace {

bool asciiAlphaNumeric(char value) {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9');
}

char asciiLower(char value) {
  return value >= 'A' && value <= 'Z'
             ? static_cast<char>(value + ('a' - 'A'))
             : value;
}

bool extensionEquals(const char *name, const char *extension) {
  if (name == nullptr || extension == nullptr) return false;
  const size_t nameLength = strlen(name);
  const size_t extensionLength = strlen(extension);
  if (nameLength < extensionLength) return false;
  const char *candidate = name + nameLength - extensionLength;
  for (size_t index = 0; index < extensionLength; ++index) {
    if (asciiLower(candidate[index]) != asciiLower(extension[index])) return false;
  }
  return true;
}

bool parseRangeNumber(const char *begin, const char *end, size_t &parsed) {
  if (begin == nullptr || end == nullptr || begin >= end) return false;
  size_t value = 0;
  for (const char *cursor = begin; cursor < end; ++cursor) {
    if (*cursor < '0' || *cursor > '9') return false;
    const size_t digit = static_cast<size_t>(*cursor - '0');
    if (value > (std::numeric_limits<size_t>::max() - digit) / 10U) return false;
    value = value * 10U + digit;
  }
  parsed = value;
  return true;
}

}  // namespace

bool validPublicName(const char *name) {
  if (name == nullptr) return false;
  const size_t length = strlen(name);
  if (length == 0 || length > kMaxFilenameBytes || !asciiAlphaNumeric(name[0])) {
    return false;
  }
  for (size_t index = 1; index < length; ++index) {
    const char current = name[index];
    if (!asciiAlphaNumeric(current) && current != '.' && current != '_' && current != '-') {
      return false;
    }
    if (current == '.' && name[index - 1] == '.') return false;
  }
  return true;
}

MediaPolicy mediaPolicyForName(const char *name) {
  if (extensionEquals(name, ".txt")) return {"text/plain; charset=utf-8", true};
  if (extensionEquals(name, ".json")) return {"application/json", true};
  if (extensionEquals(name, ".mp4")) return {"video/mp4", true};
  if (extensionEquals(name, ".png")) return {"image/png", true};
  if (extensionEquals(name, ".jpg") || extensionEquals(name, ".jpeg")) {
    return {"image/jpeg", true};
  }
  if (extensionEquals(name, ".bmp")) return {"image/bmp", true};
  if (extensionEquals(name, ".gif")) return {"image/gif", true};
  if (extensionEquals(name, ".webp")) return {"image/webp", true};
  return {};
}

bool parseSize(const char *value, size_t &parsed) {
  if (value == nullptr || value[0] == '\0') return false;
  return parseRangeNumber(value, value + strlen(value), parsed);
}

bool parseRange(const char *header, size_t fileSize, ByteRange &range) {
  range = {false, 0, fileSize};
  if (header == nullptr || header[0] == '\0') return true;
  if (strncmp(header, "bytes=", 6) != 0 || fileSize == 0) return false;

  const char *spec = header + 6;
  if (*spec == '\0' || strchr(spec, ',') != nullptr || strchr(spec, ' ') != nullptr ||
      strchr(spec, '\t') != nullptr) {
    return false;
  }
  const char *dash = strchr(spec, '-');
  if (dash == nullptr || strchr(dash + 1, '-') != nullptr) return false;

  size_t start = 0;
  size_t end = 0;
  if (dash == spec) {
    size_t suffixLength = 0;
    if (!parseRangeNumber(dash + 1, spec + strlen(spec), suffixLength) || suffixLength == 0) {
      return false;
    }
    const size_t selected = suffixLength < fileSize ? suffixLength : fileSize;
    start = fileSize - selected;
    end = fileSize - 1;
  } else {
    if (!parseRangeNumber(spec, dash, start) || start >= fileSize) return false;
    if (*(dash + 1) == '\0') {
      end = fileSize - 1;
    } else {
      if (!parseRangeNumber(dash + 1, spec + strlen(spec), end) || end < start) {
        return false;
      }
      if (end >= fileSize) end = fileSize - 1;
    }
  }

  range.partial = true;
  range.start = start;
  range.length = end - start + 1;
  return true;
}

bool encodeCursor(size_t offset, char *output, size_t outputSize) {
  if (output == nullptr || outputSize == 0 || offset == 0) return false;
  const int written = snprintf(output, outputSize, "v1:%zu", offset);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

bool decodeCursor(const char *cursor, size_t &offset) {
  offset = 0;
  if (cursor == nullptr || cursor[0] == '\0') return true;
  if (strncmp(cursor, "v1:", 3) != 0 || !parseSize(cursor + 3, offset)) return false;
  return offset > 0;
}

}  // namespace UserFilePolicy
