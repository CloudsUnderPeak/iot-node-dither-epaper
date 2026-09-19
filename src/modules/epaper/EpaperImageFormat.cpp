#include "EpaperImageFormat.h"

#include <cstring>
#include <array>

namespace EpaperImageFormat {
namespace {

constexpr uint8_t kMagic[kMagicBytes] = {'E', 'P', 'D', 'I', 'M', 'G', 0, 0};

uint32_t readLe32(const uint8_t *input) {
  return static_cast<uint32_t>(input[0]) |
         (static_cast<uint32_t>(input[1]) << 8U) |
         (static_cast<uint32_t>(input[2]) << 16U) |
         (static_cast<uint32_t>(input[3]) << 24U);
}

uint64_t readLe64(const uint8_t *input) {
  return static_cast<uint64_t>(readLe32(input)) |
         (static_cast<uint64_t>(readLe32(input + 4)) << 32U);
}

void writeLe32(uint8_t *output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8U);
  output[2] = static_cast<uint8_t>(value >> 16U);
  output[3] = static_cast<uint8_t>(value >> 24U);
}

void writeLe64(uint8_t *output, uint64_t value) {
  writeLe32(output, static_cast<uint32_t>(value));
  writeLe32(output + 4, static_cast<uint32_t>(value >> 32U));
}

}  // namespace

namespace {
constexpr std::array<uint32_t, 256> crcTable() {
  std::array<uint32_t, 256> table{};
  for (size_t i = 0; i < table.size(); ++i) {
    uint32_t value = i;
    for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((0U - (value & 1U)) & 0xEDB88320U);
    table[i] = value;
  }
  return table;
}
constexpr auto kCrcTable = crcTable();
}

uint32_t updateCrc32(uint32_t state, const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    state = kCrcTable[(state ^ data[index]) & 0xFF] ^ (state >> 8);
  }
  return state;
}

const char *errorCode(ValidationError error) {
  switch (error) {
    case ValidationError::None: return "none";
    case ValidationError::NullInput: return "null_input";
    case ValidationError::IncompleteHeader: return "incomplete_header";
    case ValidationError::ShortBody: return "short_body";
    case ValidationError::LongBody: return "long_body";
    case ValidationError::BadMagic: return "bad_magic";
    case ValidationError::BadVersion: return "bad_version";
    case ValidationError::BadHeaderSize: return "bad_header_size";
    case ValidationError::BadDimensions: return "bad_dimensions";
    case ValidationError::BadFrameSize: return "bad_frame_size";
    case ValidationError::ZeroGeneration: return "zero_generation";
    case ValidationError::InvalidPalette: return "invalid_palette";
    case ValidationError::BadCrc: return "bad_crc";
    case ValidationError::InvalidGzip: return "invalid_gzip";
    case ValidationError::BadGzipCrc: return "bad_gzip_crc";
    case ValidationError::BadGzipSize: return "bad_gzip_size";
  }
  return "unknown";
}

bool paletteCodeValid(uint8_t code) {
  return code <= kColorGreen && code != 4;
}

bool paletteByteValid(uint8_t value) {
  return paletteCodeValid(static_cast<uint8_t>(value >> 4U)) &&
         paletteCodeValid(static_cast<uint8_t>(value & 0x0FU));
}

uint32_t crc32(const uint8_t *data, size_t length) {
  if (data == nullptr && length != 0) return 0;
  return updateCrc32(0xFFFFFFFFU, data, length) ^ 0xFFFFFFFFU;
}

bool encodeHeader(const Header &header, uint8_t *output, size_t outputLength) {
  if (output == nullptr || outputLength < kHeaderBytes) return false;
  memcpy(output, kMagic, sizeof(kMagic));
  writeLe32(output + 8, header.version);
  writeLe32(output + 12, header.headerBytes);
  writeLe32(output + 16, header.width);
  writeLe32(output + 20, header.height);
  writeLe32(output + 24, header.frameBytes);
  writeLe32(output + 28, header.crc32);
  writeLe64(output + 32, header.generation);
  return true;
}

bool decodeHeader(const uint8_t *input,
                  size_t inputLength,
                  Header &header,
                  ValidationError &error) {
  header = {};
  error = ValidationError::None;
  if (input == nullptr) {
    error = ValidationError::NullInput;
    return false;
  }
  if (inputLength < kHeaderBytes) {
    error = ValidationError::IncompleteHeader;
    return false;
  }
  if (memcmp(input, kMagic, sizeof(kMagic)) != 0) {
    error = ValidationError::BadMagic;
    return false;
  }

  header.version = readLe32(input + 8);
  header.headerBytes = readLe32(input + 12);
  header.width = readLe32(input + 16);
  header.height = readLe32(input + 20);
  header.frameBytes = readLe32(input + 24);
  header.crc32 = readLe32(input + 28);
  header.generation = readLe64(input + 32);

  if (header.version != kVersion) error = ValidationError::BadVersion;
  else if (header.headerBytes != kHeaderBytes) error = ValidationError::BadHeaderSize;
  else if (header.width != kWidth || header.height != kHeight) {
    error = ValidationError::BadDimensions;
  } else if (header.frameBytes != kFrameBytes) {
    error = ValidationError::BadFrameSize;
  } else if (header.generation == 0) {
    error = ValidationError::ZeroGeneration;
  }
  return error == ValidationError::None;
}

StreamingValidator::StreamingValidator() {
  reset();
}

void StreamingValidator::reset() {
  memset(headerBuffer_, 0, sizeof(headerBuffer_));
  headerBytes_ = 0;
  frameBytes_ = 0;
  totalBytes_ = 0;
  crcState_ = 0xFFFFFFFFU;
  header_ = {};
  error_ = ValidationError::None;
  headerValidated_ = false;
  finished_ = false;
}

bool StreamingValidator::validateHeader() {
  if (headerValidated_) return true;
  if (!decodeHeader(headerBuffer_, headerBytes_, header_, error_)) return false;
  headerValidated_ = true;
  return true;
}

void StreamingValidator::consumeFrame(const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    if (!paletteByteValid(data[index])) {
      error_ = ValidationError::InvalidPalette;
      return;
    }
  }
  crcState_ = updateCrc32(crcState_, data, length);
  frameBytes_ += length;
}

bool StreamingValidator::consume(const uint8_t *data, size_t length) {
  if (error_ != ValidationError::None || finished_) return false;
  if (data == nullptr && length != 0) {
    error_ = ValidationError::NullInput;
    return false;
  }
  if (length > kImageBytes - totalBytes_) {
    error_ = ValidationError::LongBody;
    return false;
  }
  totalBytes_ += length;

  size_t offset = 0;
  if (headerBytes_ < kHeaderBytes) {
    const size_t missing = kHeaderBytes - headerBytes_;
    const size_t selected = length < missing ? length : missing;
    if (selected != 0) memcpy(headerBuffer_ + headerBytes_, data, selected);
    headerBytes_ += selected;
    offset += selected;
    if (headerBytes_ == kHeaderBytes && !validateHeader()) return false;
  }
  if (offset < length) consumeFrame(data + offset, length - offset);
  return error_ == ValidationError::None;
}

bool StreamingValidator::finish() {
  if (finished_) return error_ == ValidationError::None;
  finished_ = true;
  if (error_ != ValidationError::None) return false;
  if (headerBytes_ < kHeaderBytes) {
    error_ = ValidationError::IncompleteHeader;
  } else if (totalBytes_ < kImageBytes || frameBytes_ < kFrameBytes) {
    error_ = ValidationError::ShortBody;
  } else if ((crcState_ ^ 0xFFFFFFFFU) != header_.crc32) {
    error_ = ValidationError::BadCrc;
  }
  return error_ == ValidationError::None;
}

}  // namespace EpaperImageFormat
