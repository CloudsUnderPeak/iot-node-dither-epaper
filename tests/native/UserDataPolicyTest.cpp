#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

#include "modules/storage/UserDataPath.h"
#include "modules/storage/UserFilePolicy.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void testPublicNames() {
  expect(UserFilePolicy::validPublicName("a"), "one-character filename should pass");
  expect(UserFilePolicy::validPublicName("A9-photo_1.JPG"), "public filename alphabet should pass");
  expect(!UserFilePolicy::validPublicName(""), "empty filename should fail");
  expect(!UserFilePolicy::validPublicName(".hidden"), "filename must begin alphanumeric");
  expect(!UserFilePolicy::validPublicName("a..b"), "consecutive dots should fail");
  expect(!UserFilePolicy::validPublicName("a/b"), "path separators should fail");
  expect(!UserFilePolicy::validPublicName("a%2fb"), "percent encoding should fail the filename alphabet");
  expect(!UserFilePolicy::validPublicName("space name"), "spaces should fail");

  char maximum[UserFilePolicy::kMaxFilenameBytes + 1]{};
  memset(maximum, 'a', sizeof(maximum) - 1);
  expect(UserFilePolicy::validPublicName(maximum), "64-byte filename should pass");
  char oversized[UserFilePolicy::kMaxFilenameBytes + 2]{};
  memset(oversized, 'a', sizeof(oversized) - 1);
  expect(!UserFilePolicy::validPublicName(oversized), "65-byte filename should fail");
}

void testMediaPolicy() {
  const UserFilePolicy::MediaPolicy json = UserFilePolicy::mediaPolicyForName("DATA.JSON");
  expect(strcmp(json.contentType, "application/json") == 0 && json.inlineDisposition,
         "known extension should have a safe inline media policy");
  const UserFilePolicy::MediaPolicy unknown = UserFilePolicy::mediaPolicyForName("archive.bin");
  expect(strcmp(unknown.contentType, "application/octet-stream") == 0 &&
             !unknown.inlineDisposition,
         "unknown extension should download as opaque bytes");
}

void testStrictNumbersAndRanges() {
  size_t parsed = 99;
  expect(UserFilePolicy::parseSize("0", parsed) && parsed == 0,
         "strict decimal parser should accept zero");
  expect(!UserFilePolicy::parseSize("" , parsed), "empty decimal should fail");
  expect(!UserFilePolicy::parseSize("+1", parsed), "signed decimal should fail");
  expect(!UserFilePolicy::parseSize(" 1", parsed), "whitespace decimal should fail");
  expect(!UserFilePolicy::parseSize("184467440737095516160", parsed),
         "overflowing decimal should fail");

  UserFilePolicy::ByteRange range;
  expect(UserFilePolicy::parseRange("", 10, range) && !range.partial &&
             range.start == 0 && range.length == 10,
         "empty Range should select the full representation");
  expect(UserFilePolicy::parseRange("bytes=0-4", 10, range) && range.partial &&
             range.start == 0 && range.length == 5,
         "closed byte range should pass");
  expect(UserFilePolicy::parseRange("bytes=5-", 10, range) &&
             range.start == 5 && range.length == 5,
         "open byte range should pass");
  expect(UserFilePolicy::parseRange("bytes=-3", 10, range) &&
             range.start == 7 && range.length == 3,
         "suffix byte range should pass");
  expect(UserFilePolicy::parseRange("bytes=8-99", 10, range) &&
             range.start == 8 && range.length == 2,
         "range end should clamp to file size");
  expect(UserFilePolicy::parseRange("bytes=-99", 10, range) &&
             range.start == 0 && range.length == 10,
         "large suffix should select the whole file");
  expect(!UserFilePolicy::parseRange("bytes=0-1,3-4", 10, range),
         "multiple ranges should fail");
  expect(!UserFilePolicy::parseRange("bytes=10-", 10, range),
         "range starting at EOF should fail");
  expect(!UserFilePolicy::parseRange("bytes=5-4", 10, range),
         "reversed range should fail");
  expect(!UserFilePolicy::parseRange("bytes=-0", 10, range),
         "zero suffix should fail");
  expect(!UserFilePolicy::parseRange("bytes=0-0", 0, range),
         "range on an empty file should fail");
}

void testCursors() {
  char cursor[32]{};
  expect(UserFilePolicy::encodeCursor(42, cursor, sizeof(cursor)) &&
             strcmp(cursor, "v1:42") == 0,
         "cursor should encode an opaque versioned offset");
  size_t offset = 0;
  expect(UserFilePolicy::decodeCursor(cursor, offset) && offset == 42,
         "cursor should round-trip");
  expect(UserFilePolicy::decodeCursor("", offset) && offset == 0,
         "empty cursor should select the first page");
  expect(!UserFilePolicy::decodeCursor("42", offset), "unversioned cursor should fail");
  expect(!UserFilePolicy::decodeCursor("v1:0", offset), "zero cursor should fail");
}

void testInspectionPaths() {
  char path[UserDataPath::kMaxNormalizedBytes + 1]{};
  expect(UserDataPath::normalize("files//./photo.jpg", path, sizeof(path)) ==
             UserDataPath::Error::None && strcmp(path, "/files/photo.jpg") == 0,
         "inspection path should collapse separators and dot components");
  expect(UserDataPath::normalize("/", path, sizeof(path)) == UserDataPath::Error::None &&
             strcmp(path, "/") == 0,
         "root path should normalize to slash");
  expect(UserDataPath::normalize("files/../secret", path, sizeof(path)) ==
             UserDataPath::Error::ParentTraversal,
         "parent traversal should fail");
  expect(UserDataPath::normalize("files/*.jpg", path, sizeof(path)) ==
             UserDataPath::Error::UnsupportedCharacter,
         "glob syntax should fail");
  expect(UserDataPath::normalize("files/a b", path, sizeof(path)) ==
             UserDataPath::Error::UnsupportedCharacter,
         "whitespace should fail");
  for (const char *unsupported : {"files/%TEMP%", "files/$HOME", "files/!name!",
                                  "files/^escape", "files/~/secret"}) {
    expect(UserDataPath::normalize(unsupported, path, sizeof(path)) ==
               UserDataPath::Error::UnsupportedCharacter,
           "environment and shell shorthand should fail");
  }
  char longPath[UserDataPath::kMaxNormalizedBytes + 2]{};
  memset(longPath, 'a', sizeof(longPath) - 1);
  expect(UserDataPath::normalize(longPath, path, sizeof(path)) == UserDataPath::Error::TooLong,
         "overlong path should fail without truncation");
}
}  // namespace

int main() {
  testPublicNames();
  testMediaPolicy();
  testStrictNumbersAndRanges();
  testCursors();
  testInspectionPaths();
  if (failures != 0) {
    std::cerr << failures << " userdata policy test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All userdata policy tests passed\n";
  return EXIT_SUCCESS;
}
