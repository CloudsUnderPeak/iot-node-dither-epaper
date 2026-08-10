#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "modules/epaper/EpaperCooldown.h"
#include "modules/epaper/EpaperFrameSource.h"
#include "modules/epaper/EpaperImageFormat.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void writeLe32(uint8_t *output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8U);
  output[2] = static_cast<uint8_t>(value >> 16U);
  output[3] = static_cast<uint8_t>(value >> 24U);
}

std::vector<uint8_t> imageWithFrame(const std::vector<uint8_t> &frame,
                                    uint64_t generation = 1) {
  EpaperImageFormat::Header header;
  header.version = EpaperImageFormat::kVersion;
  header.headerBytes = EpaperImageFormat::kHeaderBytes;
  header.width = EpaperImageFormat::kWidth;
  header.height = EpaperImageFormat::kHeight;
  header.frameBytes = EpaperImageFormat::kFrameBytes;
  header.crc32 = EpaperImageFormat::crc32(frame.data(), frame.size());
  header.generation = generation;

  std::vector<uint8_t> image(EpaperImageFormat::kImageBytes);
  expect(EpaperImageFormat::encodeHeader(header, image.data(), image.size()),
         "valid header should encode");
  std::copy(frame.begin(), frame.end(), image.begin() + EpaperImageFormat::kHeaderBytes);
  return image;
}

bool validateInChunks(const std::vector<uint8_t> &image, size_t chunkBytes) {
  EpaperImageFormat::StreamingValidator validator;
  for (size_t offset = 0; offset < image.size();) {
    const size_t remaining = image.size() - offset;
    const size_t selected = chunkBytes < remaining ? chunkBytes : remaining;
    if (!validator.consume(image.data() + offset, selected)) return false;
    offset += selected;
  }
  return validator.finish();
}

void testCrcAndHeaderContract() {
  const uint8_t sample[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  expect(EpaperImageFormat::crc32(sample, sizeof(sample)) == 0xCBF43926U,
         "CRC32 should match the standard test vector");

  std::vector<uint8_t> frame(EpaperImageFormat::kFrameBytes, 0x11);
  const std::vector<uint8_t> image = imageWithFrame(frame, 0xFEDCBA9876543210ULL);
  EpaperImageFormat::Header header;
  EpaperImageFormat::ValidationError error;
  expect(EpaperImageFormat::decodeHeader(image.data(), image.size(), header, error) &&
             header.version == 1 && header.headerBytes == 40 &&
             header.width == 800 && header.height == 480 &&
             header.frameBytes == 192000 &&
             header.generation == 0xFEDCBA9876543210ULL,
         "little-endian 40-byte header should round-trip");

  for (size_t chunk : {size_t{1}, size_t{7}, size_t{39}, size_t{40},
                       size_t{41}, size_t{4093}, size_t{4096}, size_t{65537}}) {
    expect(validateInChunks(image, chunk),
           "valid image should parse across arbitrary chunk boundaries");
  }
}

void expectHeaderError(size_t offset,
                       uint32_t replacement,
                       EpaperImageFormat::ValidationError expected,
                       const char *message) {
  std::vector<uint8_t> frame(EpaperImageFormat::kFrameBytes, 0x11);
  std::vector<uint8_t> image = imageWithFrame(frame);
  writeLe32(image.data() + offset, replacement);
  EpaperImageFormat::StreamingValidator validator;
  expect(!validator.consume(image.data(), image.size()) && validator.error() == expected,
         message);
}

void testValidationFailures() {
  std::vector<uint8_t> frame(EpaperImageFormat::kFrameBytes, 0x11);
  std::vector<uint8_t> image = imageWithFrame(frame);

  EpaperImageFormat::StreamingValidator validator;
  expect(validator.consume(image.data(), 39) && !validator.finish() &&
             validator.error() == EpaperImageFormat::ValidationError::IncompleteHeader,
         "short header should be distinguished from short frame");

  validator.reset();
  expect(validator.consume(image.data(), image.size() - 1) && !validator.finish() &&
             validator.error() == EpaperImageFormat::ValidationError::ShortBody,
         "short frame body should fail at finish");

  validator.reset();
  expect(validator.consume(image.data(), image.size()) &&
             !validator.consume(image.data(), 1) &&
             validator.error() == EpaperImageFormat::ValidationError::LongBody,
         "long body should fail without accepting the extra byte");

  image[0] = 'X';
  validator.reset();
  expect(!validator.consume(image.data(), image.size()) &&
             validator.error() == EpaperImageFormat::ValidationError::BadMagic,
         "bad magic should fail");

  expectHeaderError(8, 2, EpaperImageFormat::ValidationError::BadVersion,
                    "bad version should fail");
  expectHeaderError(12, 36, EpaperImageFormat::ValidationError::BadHeaderSize,
                    "bad header size should fail");
  expectHeaderError(16, 799, EpaperImageFormat::ValidationError::BadDimensions,
                    "bad width should fail");
  expectHeaderError(20, 481, EpaperImageFormat::ValidationError::BadDimensions,
                    "bad height should fail");
  expectHeaderError(24, 191999, EpaperImageFormat::ValidationError::BadFrameSize,
                    "bad frame size should fail");

  image = imageWithFrame(frame);
  for (size_t index = 32; index < 40; ++index) image[index] = 0;
  validator.reset();
  expect(!validator.consume(image.data(), image.size()) &&
             validator.error() == EpaperImageFormat::ValidationError::ZeroGeneration,
         "zero generation should fail");

  image = imageWithFrame(frame);
  image[EpaperImageFormat::kHeaderBytes + 123] = 0x14;
  validator.reset();
  expect(!validator.consume(image.data(), image.size()) &&
             validator.error() == EpaperImageFormat::ValidationError::InvalidPalette,
         "either invalid nibble should fail palette validation");

  image = imageWithFrame(frame);
  image[28] ^= 0x01;
  validator.reset();
  expect(validator.consume(image.data(), image.size()) && !validator.finish() &&
             validator.error() == EpaperImageFormat::ValidationError::BadCrc,
         "CRC mismatch should fail at finish");
}

void testDynamicFrameSources() {
  EpaperWhiteFrameSource white;
  std::vector<uint8_t> whiteFrame(EpaperImageFormat::kFrameBytes);
  expect(white.read(0, whiteFrame.data(), whiteFrame.size()) == whiteFrame.size(),
         "white source should fill the complete frame");
  expect(std::all_of(whiteFrame.begin(), whiteFrame.end(),
                     [](uint8_t value) { return value == 0x11; }),
         "every white source byte should contain two white pixels");
  expect(white.read(EpaperImageFormat::kFrameBytes, whiteFrame.data(), 1) == 0,
         "frame source should stop exactly at frame size");

  EpaperPaletteFrameSource palette;
  expect(palette.colorAt(0, 0) == EpaperImageFormat::kColorBlack &&
             palette.colorAt(799, 479) == EpaperImageFormat::kColorBlack &&
             palette.colorAt(3, 240) == EpaperImageFormat::kColorBlack &&
             palette.colorAt(796, 240) == EpaperImageFormat::kColorBlack,
         "palette should have an exact four-pixel black border");
  constexpr uint32_t innerWidth = 800 - 8;
  const uint8_t expected[] = {0, 1, 2, 3, 5, 6};
  expect(EpaperImageFormat::kPaletteColorCount == sizeof(expected),
         "palette should expose exactly six EPD codes");
  for (uint32_t bar = 0; bar < EpaperImageFormat::kPaletteColorCount; ++bar) {
    expect(EpaperImageFormat::kPaletteCodes[bar] == expected[bar],
           "shared palette should follow EPD code order");
    const uint32_t x =
        4 + (innerWidth * bar / EpaperImageFormat::kPaletteColorCount) + 8;
    expect(palette.colorAt(x, 240) == EpaperImageFormat::kPaletteCodes[bar],
           "palette bars should follow the six-color order");
  }

  std::array<uint8_t, 4097> paletteChunk{};
  const size_t offset = 313;
  expect(palette.read(offset, paletteChunk.data(), paletteChunk.size()) ==
             paletteChunk.size(),
         "palette source should stream bounded chunks");
  for (size_t index = 0; index < paletteChunk.size(); ++index) {
    const size_t pixel = (offset + index) * 2;
    const uint32_t y = static_cast<uint32_t>(pixel / 800);
    const uint32_t x = static_cast<uint32_t>(pixel % 800);
    const uint8_t packed = static_cast<uint8_t>((palette.colorAt(x, y) << 4U) |
                                                palette.colorAt(x + 1, y));
    expect(paletteChunk[index] == packed,
           "palette byte should pack the even pixel high and odd pixel low");
  }

  std::vector<uint8_t> paletteFrame(EpaperImageFormat::kFrameBytes);
  size_t written = 0;
  while (written < paletteFrame.size()) {
    const size_t capacity = std::min<size_t>(4093, paletteFrame.size() - written);
    const size_t count = palette.read(written, paletteFrame.data() + written, capacity);
    if (count == 0) break;
    written += count;
  }
  bool allPackedBytesExact = written == paletteFrame.size();
  for (size_t byteIndex = 0; allPackedBytesExact && byteIndex < paletteFrame.size();
       ++byteIndex) {
    const size_t pixel = byteIndex * 2;
    const uint32_t y = static_cast<uint32_t>(pixel / 800);
    const uint32_t x = static_cast<uint32_t>(pixel % 800);
    const uint8_t expectedByte = static_cast<uint8_t>((palette.colorAt(x, y) << 4U) |
                                                      palette.colorAt(x + 1, y));
    allPackedBytesExact = paletteFrame[byteIndex] == expectedByte;
  }
  expect(allPackedBytesExact,
         "the complete palette frame should preserve every border, bar, and nibble");
  expect(validateInChunks(imageWithFrame(paletteFrame, 42), 4093),
         "generated palette should satisfy the EPDIMG streaming validator");
}

void testCooldownState() {
  EpaperCooldown cooldown;
  expect(cooldown.canDraw() && cooldown.state() == EpaperCooldown::State::Idle,
         "cooldown should start idle");
  cooldown.begin(1000);
  expect(!cooldown.canDraw() && cooldown.retryAfterSeconds(1000) == 180,
         "cooldown should block for a full 180 seconds");
  expect(cooldown.retryAfterSeconds(1001) == 180,
         "retry seconds should round up");
  expect(cooldown.retryAfterSeconds(2000) == 179,
         "whole elapsed seconds should reduce retry time");
  expect(!cooldown.releaseIfElapsed(180999, true),
         "cooldown should remain blocked one millisecond before deadline");
  expect(!cooldown.releaseIfElapsed(181000, false) && !cooldown.canDraw(),
         "elapsed cooldown should remain blocked if marker clear fails");
  expect(cooldown.releaseIfElapsed(181000, true) && cooldown.canDraw(),
         "elapsed cooldown should release only after marker clear succeeds");

  const uint32_t nearWrap = std::numeric_limits<uint32_t>::max() - 99999U;
  cooldown.begin(nearWrap);
  const uint32_t afterWrap = static_cast<uint32_t>(nearWrap + EpaperCooldown::kDurationMs);
  expect(cooldown.elapsed(afterWrap) && cooldown.retryAfterSeconds(afterWrap) == 0,
         "cooldown comparison should be safe across millis rollover");

  cooldown.markUnavailable();
  expect(cooldown.state() == EpaperCooldown::State::Unavailable &&
             !cooldown.canDraw() && !cooldown.releaseIfElapsed(afterWrap, true),
         "unknown panel state must not unlock when a timer expires");
}

}  // namespace

int main() {
  testCrcAndHeaderContract();
  testValidationFailures();
  testDynamicFrameSources();
  testCooldownState();
  if (failures != 0) {
    std::cerr << failures << " e-paper pure-logic test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "E-paper image, pattern, and cooldown tests passed\n";
  return EXIT_SUCCESS;
}
