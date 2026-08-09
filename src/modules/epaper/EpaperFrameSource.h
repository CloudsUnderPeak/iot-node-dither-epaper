#pragma once

#include <cstddef>
#include <cstdint>

#include "EpaperImageFormat.h"

class EpaperFrameSource {
 public:
  virtual ~EpaperFrameSource() = default;
  virtual size_t size() const = 0;
  virtual size_t read(size_t offset, uint8_t *output, size_t capacity) const = 0;
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
