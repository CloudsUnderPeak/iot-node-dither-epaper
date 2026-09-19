#pragma once

#include <memory>
#include "EpaperImageFormat.h"

// A single gzip member with a bounded header and exact logical output size.
// No storage/HTTP ownership. Input and output may stop on any byte boundary.
class EpaperGzip {
 public:
  static constexpr size_t kMaxHeaderBytes = 1024;
  EpaperGzip();
  ~EpaperGzip();
  EpaperGzip(const EpaperGzip &) = delete;
  EpaperGzip &operator=(const EpaperGzip &) = delete;
  bool reset();
  void release();
  bool step(const uint8_t *input, size_t length, size_t &consumed,
            uint8_t *output, size_t capacity, size_t &produced,
            bool finalInput = false);
  bool finish();
  bool done() const;
  size_t outputBytes() const { return outputBytes_; }
  EpaperImageFormat::ValidationError error() const { return error_; }

 private:
  struct InflateState;
  std::unique_ptr<InflateState> inflate_;
  enum class Phase { Fixed, ExtraLength, Extra, Name, Comment, HeaderCrc, Deflate, Trailer, Done };
  Phase phase_ = Phase::Fixed;
  uint8_t fixed_[10]{};
  uint8_t trailer_[8]{};
  size_t fieldBytes_ = 0;
  size_t headerBytes_ = 0;
  uint16_t extraBytes_ = 0;
  uint16_t headerCrc_ = 0;
  uint8_t flags_ = 0;
  uint32_t headerCrcState_ = 0xFFFFFFFFU;
  uint32_t crcState_ = 0xFFFFFFFFU;
  size_t outputBytes_ = 0;
  size_t pendingBytes_ = 0;
  size_t pendingOffset_ = 0;
  bool deflateDone_ = false;
  EpaperImageFormat::ValidationError error_ = EpaperImageFormat::ValidationError::None;
  bool fail(EpaperImageFormat::ValidationError error);
  void nextOptional();
  bool headerByte(uint8_t byte);
};
