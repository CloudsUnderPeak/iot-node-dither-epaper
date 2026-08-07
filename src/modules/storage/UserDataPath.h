#pragma once

#include <cstddef>

namespace UserDataPath {

constexpr size_t kMaxNormalizedBytes = 127;

enum class Error {
  None,
  ParentTraversal,
  UnsupportedCharacter,
  TooLong,
};

// Normalizes a console path relative to the userdata root. The output always
// begins with '/' and is never truncated.
Error normalize(const char *input, char *output, size_t outputSize);
const char *message(Error error);

}  // namespace UserDataPath
