#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace EpaperPanelProfile {
constexpr size_t kHeaderBytes = 40;
constexpr uint32_t kPixelsPerByte = 2;

constexpr bool validGeometry(uint64_t width, uint64_t height) {
  return width > 0 && height > 0 && width <= UINT16_MAX &&
         height <= UINT16_MAX && width % kPixelsPerByte == 0 &&
         width * height / kPixelsPerByte <= UINT32_MAX &&
         width * height / kPixelsPerByte <=
             std::numeric_limits<size_t>::max() - kHeaderBytes;
}

template <uint32_t Width, uint32_t Height> struct Geometry {
  static_assert(validGeometry(Width, Height), "invalid packed panel geometry");
  static constexpr uint32_t width = Width;
  static constexpr uint32_t height = Height;
  static constexpr uint64_t pixelCount = uint64_t{Width} * Height;
  static constexpr size_t rowBytes = Width / kPixelsPerByte;
  static constexpr size_t frameBytes = pixelCount / kPixelsPerByte;
  static constexpr size_t imageBytes = kHeaderBytes + frameBytes;
  // Includes incompressible DEFLATE overhead and bounded optional gzip header.
  static constexpr size_t maxCompressedBytes = imageBytes + imageBytes / 100 + 2048;
};

// The only physical panel identity/geometry definition. Same controller and
// six-color packed protocol only; a new controller requires a separate driver.
constexpr char kModel[] = "waveshare-7in3e";
using Active = Geometry<800, 480>;
static_assert(sizeof(kModel) > 1, "panel model must not be empty");

// Mounting compensation, applied by firmware at draw time (also stored refresh).
// Both true rotate 180 degrees. Logical files and downloads remain unchanged.
constexpr bool kFlipHorizontal = true;
constexpr bool kFlipVertical = true;
}  // namespace EpaperPanelProfile
