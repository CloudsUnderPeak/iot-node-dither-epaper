#pragma once

#include <cstddef>
#include <cstdint>

#include "core/Result.h"
#include "modules/epaper/EpaperImageFormat.h"

namespace EpaperCalibration {

constexpr uint16_t kSchemaVersion = 1;
constexpr size_t kColorCount = EpaperImageFormat::kPaletteColorCount;

struct Rgb {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

struct ColorDefinition {
  const char *id;
  uint8_t code;
  Rgb protocol;
  Rgb defaultDisplay;
};

struct Profile {
  uint16_t schemaVersion = kSchemaVersion;
  Rgb display[kColorCount]{};
};

const ColorDefinition &definition(size_t index);
int slotForId(const char *id);
Profile defaultProfile();
Result validate(const Profile &profile);
bool equal(const Profile &left, const Profile &right);
bool equal(const Rgb &left, const Rgb &right);

}  // namespace EpaperCalibration
