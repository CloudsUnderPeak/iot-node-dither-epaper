#include "EpaperFrameSource.h"

#include <cstring>

namespace {

constexpr uint8_t kPalette[] = {0, 1, 2, 3, 5, 6};

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
  if (selected != 0) memset(output, 0x11, selected);
  return selected;
}

uint8_t EpaperPaletteFrameSource::colorAt(uint32_t x, uint32_t y) {
  if (x >= EpaperImageFormat::kWidth || y >= EpaperImageFormat::kHeight) return 0;
  if (x < kBorderPixels || x >= EpaperImageFormat::kWidth - kBorderPixels ||
      y < kBorderPixels || y >= EpaperImageFormat::kHeight - kBorderPixels) {
    return 0;
  }
  constexpr uint32_t kInnerWidth = EpaperImageFormat::kWidth - 2 * kBorderPixels;
  uint32_t bar = ((x - kBorderPixels) * (sizeof(kPalette) / sizeof(kPalette[0]))) /
                 kInnerWidth;
  if (bar >= sizeof(kPalette) / sizeof(kPalette[0])) {
    bar = sizeof(kPalette) / sizeof(kPalette[0]) - 1;
  }
  return kPalette[bar];
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
