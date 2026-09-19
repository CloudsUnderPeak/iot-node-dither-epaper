#include "EpaperFrameSource.h"

#include <cstring>
#include <algorithm>
#include <new>

namespace {

size_t readableBytes(size_t offset, size_t capacity) {
  if (offset >= EpaperImageFormat::kFrameBytes) return 0;
  const size_t remaining = EpaperImageFormat::kFrameBytes - offset;
  return capacity < remaining ? capacity : remaining;
}

}  // namespace

size_t EpaperWhiteFrameSource::read(size_t offset,
                                    uint8_t *output,
                                    size_t capacity) const {
  if (output == nullptr && capacity != 0) return 0;
  const size_t selected = readableBytes(offset, capacity);
  constexpr uint8_t kWhiteByte = static_cast<uint8_t>(
      (EpaperImageFormat::kColorWhite << 4U) | EpaperImageFormat::kColorWhite);
  if (selected != 0) memset(output, kWhiteByte, selected);
  return selected;
}

uint8_t EpaperPaletteFrameSource::colorAt(uint32_t x, uint32_t y) {
  if (x >= EpaperImageFormat::kWidth || y >= EpaperImageFormat::kHeight) {
    return EpaperImageFormat::kColorBlack;
  }
  if (EpaperImageFormat::kWidth <= 2 * kBorderPixels ||
      EpaperImageFormat::kHeight <= 2 * kBorderPixels ||
      x < kBorderPixels || x >= EpaperImageFormat::kWidth - kBorderPixels ||
      y < kBorderPixels || y >= EpaperImageFormat::kHeight - kBorderPixels) {
    return EpaperImageFormat::kColorBlack;
  }
  constexpr uint32_t kInnerWidth = EpaperImageFormat::kWidth > 2 * kBorderPixels
      ? EpaperImageFormat::kWidth - 2 * kBorderPixels : 1;
  uint32_t bar =
      ((x - kBorderPixels) * EpaperImageFormat::kPaletteColorCount) / kInnerWidth;
  if (bar >= EpaperImageFormat::kPaletteColorCount) {
    bar = EpaperImageFormat::kPaletteColorCount - 1;
  }
  return EpaperImageFormat::kPaletteCodes[bar];
}

size_t EpaperPaletteFrameSource::read(size_t offset,
                                      uint8_t *output,
                                      size_t capacity) const {
  if (output == nullptr && capacity != 0) return 0;
  const size_t selected = readableBytes(offset, capacity);
  for (size_t index = 0; index < selected; ++index) {
    const size_t byteIndex = offset + index;
    const size_t pixelIndex = byteIndex * 2;
    const uint32_t y = static_cast<uint32_t>(pixelIndex / EpaperImageFormat::kWidth);
    const uint32_t x = static_cast<uint32_t>(pixelIndex % EpaperImageFormat::kWidth);
    output[index] = static_cast<uint8_t>((colorAt(x, y) << 4U) | colorAt(x + 1, y));
  }
  return selected;
}

EpaperOrientedFrameSource::EpaperOrientedFrameSource(
    const EpaperFrameSource &source, uint32_t width, uint32_t height,
    bool horizontal, bool vertical)
    : source_(source), rowBytes_(width / 2), height_(height),
      bandRows_(rowBytes_ ? std::max(size_t{1}, size_t{16384} / rowBytes_) : 0),
      horizontal_(horizontal), vertical_(vertical) {
  if (EpaperPanelProfile::validGeometry(width, height) && source.size() == rowBytes_ * height_) {
    bandRows_ = std::min(bandRows_, height_);
    buffer_.reset(new (std::nothrow) uint8_t[bandRows_ * rowBytes_]);
  }
}

bool EpaperOrientedFrameSource::fillBand() const {
  const size_t row = expectedOffset_ / rowBytes_;
  const size_t rows = std::min(bandRows_, height_ - row);
  const size_t start = (vertical_ ? height_ - row - rows : row) * rowBytes_;
  if (start < sourceOffset_) {
    if (!source_.rewind()) return false;
    sourceOffset_ = 0;
  }
  while (sourceOffset_ < start) {
    const size_t count = source_.read(sourceOffset_, buffer_.get(),
        std::min(size_t{4096}, start - sourceOffset_));
    if (!count) return false;
    sourceOffset_ += count;
  }
  bandBytes_ = rows * rowBytes_;
  size_t filled = 0;
  while (filled < bandBytes_) {
    const size_t count = source_.read(sourceOffset_, buffer_.get() + filled,
        std::min(size_t{4096}, bandBytes_ - filled));
    if (!count) return false;
    filled += count;
    sourceOffset_ += count;
  }
  bandStart_ = expectedOffset_;
  return true;
}

size_t EpaperOrientedFrameSource::read(size_t offset, uint8_t *output, size_t capacity) const {
  if (!ready() || !output || offset != expectedOffset_ || offset >= size()) return 0;
  const size_t count = std::min(capacity, size() - offset);
  for (size_t i = 0; i < count; ++i) {
    if (!bandBytes_ || expectedOffset_ == bandStart_ + bandBytes_) {
      if (!fillBand()) return 0;
    }
    const size_t local = expectedOffset_ - bandStart_;
    size_t row = local / rowBytes_;
    size_t column = local % rowBytes_;
    if (vertical_) row = bandBytes_ / rowBytes_ - 1 - row;
    if (horizontal_) column = rowBytes_ - 1 - column;
    uint8_t value = buffer_[row * rowBytes_ + column];
    output[i] = horizontal_ ? static_cast<uint8_t>((value << 4) | (value >> 4)) : value;
    ++expectedOffset_;
  }
  return count;
}
