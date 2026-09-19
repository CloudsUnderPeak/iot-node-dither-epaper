#include "EpaperGzip.h"

#include <algorithm>
#include <cstring>
#include <new>
#include "miniz/miniz.h"

using EpaperImageFormat::ValidationError;

struct EpaperGzip::InflateState {
  tinfl_decompressor decoder{};
  uint8_t dictionary[TINFL_LZ_DICT_SIZE]{};
  size_t offset = 0;
};

EpaperGzip::EpaperGzip() = default;
EpaperGzip::~EpaperGzip() = default;
void EpaperGzip::release() { inflate_.reset(); }

bool EpaperGzip::reset() {
  if (!inflate_) inflate_.reset(new (std::nothrow) InflateState);
  if (!inflate_) return false;
  tinfl_init(&inflate_->decoder);
  inflate_->offset = 0;
  phase_ = Phase::Fixed;
  fieldBytes_ = headerBytes_ = outputBytes_ = 0;
  pendingBytes_ = pendingOffset_ = 0;
  deflateDone_ = false;
  extraBytes_ = headerCrc_ = 0;
  flags_ = 0;
  headerCrcState_ = crcState_ = 0xFFFFFFFFU;
  error_ = ValidationError::None;
  return true;
}

bool EpaperGzip::fail(ValidationError error) {
  if (error_ == ValidationError::None) error_ = error;
  return false;
}

bool EpaperGzip::done() const {
  return phase_ == Phase::Done && error_ == ValidationError::None;
}

void EpaperGzip::nextOptional() {
  fieldBytes_ = 0;
  if (flags_ & 4) { flags_ &= ~4; phase_ = Phase::ExtraLength; }
  else if (flags_ & 8) { flags_ &= ~8; phase_ = Phase::Name; }
  else if (flags_ & 16) { flags_ &= ~16; phase_ = Phase::Comment; }
  else if (flags_ & 2) { flags_ &= ~2; phase_ = Phase::HeaderCrc; }
  else phase_ = Phase::Deflate;
}

bool EpaperGzip::headerByte(uint8_t byte) {
  if (++headerBytes_ > kMaxHeaderBytes) return fail(ValidationError::InvalidGzip);
  if (phase_ != Phase::HeaderCrc) {
    headerCrcState_ = EpaperImageFormat::updateCrc32(headerCrcState_, &byte, 1);
  }
  switch (phase_) {
    case Phase::Fixed:
      fixed_[fieldBytes_++] = byte;
      if (fieldBytes_ == sizeof(fixed_)) {
        if (fixed_[0] != 0x1F || fixed_[1] != 0x8B || fixed_[2] != 8 ||
            (fixed_[3] & 0xE0)) return fail(ValidationError::InvalidGzip);
        flags_ = fixed_[3];
        nextOptional();
      }
      break;
    case Phase::ExtraLength:
      if (fieldBytes_++ == 0) extraBytes_ = byte;
      else {
        extraBytes_ |= uint16_t{byte} << 8;
        if (extraBytes_ > kMaxHeaderBytes - headerBytes_) return fail(ValidationError::InvalidGzip);
        phase_ = Phase::Extra;
        if (extraBytes_ == 0) nextOptional();
      }
      break;
    case Phase::Extra:
      if (--extraBytes_ == 0) nextOptional();
      break;
    case Phase::Name:
    case Phase::Comment:
      if (byte == 0) nextOptional();
      break;
    case Phase::HeaderCrc:
      if (fieldBytes_++ == 0) headerCrc_ = byte;
      else {
        headerCrc_ |= uint16_t{byte} << 8;
        if (headerCrc_ != static_cast<uint16_t>(headerCrcState_ ^ 0xFFFFFFFFU)) {
          return fail(ValidationError::InvalidGzip);
        }
        nextOptional();
      }
      break;
    default: return fail(ValidationError::InvalidGzip);
  }
  return true;
}

bool EpaperGzip::step(const uint8_t *input, size_t length, size_t &consumed,
                      uint8_t *output, size_t capacity, size_t &produced,
                      bool finalInput) {
  consumed = produced = 0;
  if (!inflate_ || error_ != ValidationError::None) return false;
  if ((input == nullptr && length) || output == nullptr || capacity == 0) {
    return fail(ValidationError::InvalidGzip);
  }
  while (phase_ != Phase::Deflate && phase_ != Phase::Trailer && phase_ != Phase::Done && consumed < length) {
    if (!headerByte(input[consumed++])) return false;
  }
  if (phase_ == Phase::Deflate) {
    size_t inCount = length - consumed;
    // tinfl determines the wrapping dictionary size from offset + available
    // output. Always provide the entire remainder of the 32 KiB dictionary.
    size_t outCount = TINFL_LZ_DICT_SIZE - inflate_->offset;
    // Pending output is drained below before invoking the inflater again.
    if (pendingBytes_ == 0) {
      const uint8_t empty = 0;
      // Until the first dictionary is filled, reject distances into history
      // that does not exist. Subsequent calls may wrap the now-full dictionary.
      const uint32_t flags = (finalInput ? 0 : TINFL_FLAG_HAS_MORE_INPUT) |
          (outputBytes_ < TINFL_LZ_DICT_SIZE ? TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF : 0);
      const auto status = epaper_tinfl_decompress(&inflate_->decoder,
          input ? input + consumed : &empty, &inCount,
          inflate_->dictionary, inflate_->dictionary + inflate_->offset,
          &outCount, flags);
      consumed += inCount;
      if (status < 0) return fail(ValidationError::InvalidGzip);
      if (outCount > EpaperImageFormat::kImageBytes - outputBytes_) {
        return fail(ValidationError::BadGzipSize);
      }
      crcState_ = EpaperImageFormat::updateCrc32(crcState_, inflate_->dictionary + inflate_->offset, outCount);
      outputBytes_ += outCount;
      pendingBytes_ = outCount;
      pendingOffset_ = inflate_->offset;
      inflate_->offset = (inflate_->offset + outCount) % TINFL_LZ_DICT_SIZE;
      deflateDone_ = status == TINFL_STATUS_DONE;
    }
    produced = std::min(capacity, pendingBytes_);
    if (produced) memcpy(output, inflate_->dictionary + pendingOffset_, produced);
    pendingBytes_ -= produced;
    pendingOffset_ += produced;
    if (pendingBytes_ == 0 && deflateDone_) { phase_ = Phase::Trailer; fieldBytes_ = 0; }
    if (produced) return true;
  }
  while (phase_ == Phase::Trailer && consumed < length) {
    trailer_[fieldBytes_++] = input[consumed++];
    if (fieldBytes_ == sizeof(trailer_)) {
      auto le32 = [](const uint8_t *p) {
        return uint32_t{p[0]} | (uint32_t{p[1]} << 8) | (uint32_t{p[2]} << 16) | (uint32_t{p[3]} << 24);
      };
      if (le32(trailer_) != (crcState_ ^ 0xFFFFFFFFU)) return fail(ValidationError::BadGzipCrc);
      if (le32(trailer_ + 4) != outputBytes_ || outputBytes_ != EpaperImageFormat::kImageBytes) {
        return fail(ValidationError::BadGzipSize);
      }
      phase_ = Phase::Done;
    }
  }
  if (phase_ == Phase::Done && consumed != length) return fail(ValidationError::InvalidGzip);
  return true;
}

bool EpaperGzip::finish() {
  if (done()) return true;
  return fail(ValidationError::InvalidGzip);
}
