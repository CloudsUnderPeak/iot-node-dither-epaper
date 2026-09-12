#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include "modules/epaper/EpaperImageFormat.h"

using namespace EpaperImageFormat;
ValidationError validate(const std::vector<uint8_t> &bytes) {
  StreamingValidator validator;
  for (size_t offset = 0; offset < bytes.size(); offset += 997) {
    if (!validator.consume(bytes.data() + offset, std::min(size_t(997), bytes.size() - offset))) break;
  }
  validator.finish();
  return validator.error();
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream stream(argv[1], std::ios::binary);
  assert(stream.good());
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), {});
  assert(bytes.size() == kImageBytes && validate(bytes) == ValidationError::None);
  Header header;
  ValidationError error;
  assert(decodeHeader(bytes.data(), bytes.size(), header, error));
  assert(header.generation != 0 && header.width == 800 && header.height == 480);
  for (size_t i = kHeaderBytes; i < bytes.size(); ++i) {
    const uint8_t pattern[] = {0x01, 0x23, 0x56};
    assert(bytes[i] == pattern[(i - kHeaderBytes) % 3]);
  }
  auto corrupt = bytes;
  corrupt[28] ^= 1;
  assert(validate(corrupt) == ValidationError::BadCrc);
  corrupt = bytes; corrupt[40] = 0x44;
  assert(validate(corrupt) == ValidationError::InvalidPalette);
  corrupt = bytes; corrupt.pop_back();
  assert(validate(corrupt) == ValidationError::ShortBody);
  corrupt = bytes; corrupt.push_back(0);
  assert(validate(corrupt) == ValidationError::LongBody);
  corrupt = bytes;
  for (size_t i = 32; i < 40; ++i) corrupt[i] = 0;
  assert(validate(corrupt) == ValidationError::ZeroGeneration);
  std::cout << "User encoder bytes -> firmware streaming validator contract: PASS\n";
}
