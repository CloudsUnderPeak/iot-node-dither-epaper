#pragma once

#include <cstddef>
#include <cstdint>

namespace EpaperImageFormat {

constexpr size_t kMagicBytes = 8;
constexpr size_t kHeaderBytes = 40;
constexpr uint32_t kVersion = 1;
constexpr uint32_t kWidth = 800;
constexpr uint32_t kHeight = 480;
constexpr size_t kFrameBytes = 192000;
constexpr size_t kImageBytes = kHeaderBytes + kFrameBytes;

constexpr uint8_t kColorBlack = 0;
constexpr uint8_t kColorWhite = 1;
constexpr uint8_t kColorYellow = 2;
constexpr uint8_t kColorRed = 3;
constexpr uint8_t kColorBlue = 5;
constexpr uint8_t kColorGreen = 6;
constexpr uint8_t kPaletteCodes[] = {
    kColorBlack, kColorWhite, kColorYellow,
    kColorRed,   kColorBlue,  kColorGreen,
};
constexpr size_t kPaletteColorCount =
    sizeof(kPaletteCodes) / sizeof(kPaletteCodes[0]);

struct Header {
  uint32_t version = 0;
  uint32_t headerBytes = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t frameBytes = 0;
  uint32_t crc32 = 0;
  uint64_t generation = 0;
};

enum class ValidationError {
  None,
  NullInput,
  IncompleteHeader,
  ShortBody,
  LongBody,
  BadMagic,
  BadVersion,
  BadHeaderSize,
  BadDimensions,
  BadFrameSize,
  ZeroGeneration,
  InvalidPalette,
  BadCrc,
};

const char *errorCode(ValidationError error);
bool paletteCodeValid(uint8_t code);
bool paletteByteValid(uint8_t value);
uint32_t crc32(const uint8_t *data, size_t length);

bool encodeHeader(const Header &header, uint8_t *output, size_t outputLength);
bool decodeHeader(const uint8_t *input,
                  size_t inputLength,
                  Header &header,
                  ValidationError &error);

class StreamingValidator {
 public:
  StreamingValidator();

  void reset();
  bool consume(const uint8_t *data, size_t length);
  bool finish();

  ValidationError error() const { return error_; }
  const Header &header() const { return header_; }
  size_t bytesReceived() const { return totalBytes_; }
  size_t frameBytesReceived() const { return frameBytes_; }

 private:
  bool validateHeader();
  void consumeFrame(const uint8_t *data, size_t length);

  uint8_t headerBuffer_[kHeaderBytes]{};
  size_t headerBytes_ = 0;
  size_t frameBytes_ = 0;
  size_t totalBytes_ = 0;
  uint32_t crcState_ = 0xFFFFFFFFU;
  Header header_{};
  ValidationError error_ = ValidationError::None;
  bool headerValidated_ = false;
  bool finished_ = false;
};

}  // namespace EpaperImageFormat
