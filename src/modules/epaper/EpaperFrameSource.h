#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "EpaperImageFormat.h"

class EpaperFrameSource {
 public:
  virtual ~EpaperFrameSource() = default;
  virtual size_t size() const = 0;
  virtual size_t read(size_t offset, uint8_t *output, size_t capacity) const = 0;
  virtual bool rewind() const { return true; }
  virtual void close() const {}
};

// Sequential adapter with a bounded row band. Vertical flip rewinds and
// discards a prefix for each band; it never requests random gzip offsets.
class EpaperOrientedFrameSource final : public EpaperFrameSource {
 public:
  EpaperOrientedFrameSource(const EpaperFrameSource &source, uint32_t width,
                           uint32_t height, bool horizontal, bool vertical);
  bool ready() const { return buffer_ != nullptr; }
  size_t size() const override { return source_.size(); }
  size_t read(size_t offset, uint8_t *output, size_t capacity) const override;
 private:
  const EpaperFrameSource &source_;
  size_t rowBytes_;
  size_t height_;
  size_t bandRows_;
  bool horizontal_;
  bool vertical_;
  std::unique_ptr<uint8_t[]> buffer_;
  mutable size_t expectedOffset_ = 0;
  mutable size_t bandStart_ = 0;
  mutable size_t bandBytes_ = 0;
  mutable size_t sourceOffset_ = 0;
  bool fillBand() const;
};

class EpaperWhiteFrameSource final : public EpaperFrameSource {
 public:
  size_t size() const override { return EpaperImageFormat::kFrameBytes; }
  size_t read(size_t offset, uint8_t *output, size_t capacity) const override;
};

class EpaperPaletteFrameSource final : public EpaperFrameSource {
 public:
  static constexpr uint32_t kBorderPixels = 4;

  size_t size() const override { return EpaperImageFormat::kFrameBytes; }
  size_t read(size_t offset, uint8_t *output, size_t capacity) const override;

  static uint8_t colorAt(uint32_t x, uint32_t y);
};
