#include "UserDataPath.h"

#include <cstring>

namespace UserDataPath {
namespace {

bool unsupported(char value) {
  const unsigned char byte = static_cast<unsigned char>(value);
  if (byte <= 0x20 || byte > 0x7e) return true;
  switch (value) {
    case '\\':
    case '\'':
    case '"':
    case '*':
    case '?':
    case '[':
    case ']':
    case '{':
    case '}':
    case '$':
    case '%':
    case '!':
    case '^':
    case '~':
    case '`':
    case ';':
    case '|':
    case '&':
    case '<':
    case '>':
      return true;
    default:
      return false;
  }
}

}  // namespace

Error normalize(const char *input, char *output, size_t outputSize) {
  if (output == nullptr || outputSize < 2) return Error::TooLong;
  output[0] = '/';
  output[1] = '\0';
  if (input == nullptr || input[0] == '\0') return Error::None;

  const char *cursor = input;
  while (*cursor == '/') ++cursor;
  size_t outputLength = 1;
  while (*cursor != '\0') {
    while (*cursor == '/') ++cursor;
    if (*cursor == '\0') break;

    const char *component = cursor;
    while (*cursor != '\0' && *cursor != '/') {
      if (unsupported(*cursor)) return Error::UnsupportedCharacter;
      ++cursor;
    }
    const size_t componentLength = static_cast<size_t>(cursor - component);
    if (componentLength == 1 && component[0] == '.') continue;
    if (componentLength == 2 && component[0] == '.' && component[1] == '.') {
      return Error::ParentTraversal;
    }

    const size_t separatorBytes = outputLength > 1 ? 1 : 0;
    if (outputLength + separatorBytes + componentLength > kMaxNormalizedBytes ||
        outputLength + separatorBytes + componentLength + 1 > outputSize) {
      return Error::TooLong;
    }
    if (separatorBytes != 0) output[outputLength++] = '/';
    memcpy(output + outputLength, component, componentLength);
    outputLength += componentLength;
    output[outputLength] = '\0';
  }
  return Error::None;
}

const char *message(Error error) {
  switch (error) {
    case Error::None: return "ok";
    case Error::ParentTraversal: return "parent traversal is not allowed";
    case Error::UnsupportedCharacter: return "path contains unsupported characters";
    case Error::TooLong: return "path is too long";
  }
  return "invalid path";
}

}  // namespace UserDataPath
