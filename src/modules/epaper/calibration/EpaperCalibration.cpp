#include "EpaperCalibration.h"

#include <cstring>

namespace EpaperCalibration {
namespace {

constexpr ColorDefinition kDefinitions[] = {
    {"black", EpaperImageFormat::kColorBlack, {0, 0, 0}, {39, 39, 43}},
    {"white", EpaperImageFormat::kColorWhite, {255, 255, 255}, {237, 237, 225}},
    {"yellow", EpaperImageFormat::kColorYellow, {255, 255, 0}, {224, 212, 31}},
    {"red", EpaperImageFormat::kColorRed, {255, 0, 0}, {120, 32, 32}},
    {"blue", EpaperImageFormat::kColorBlue, {0, 0, 255}, {31, 88, 169}},
    {"green", EpaperImageFormat::kColorGreen, {0, 255, 0}, {58, 110, 72}},
};

static_assert(sizeof(kDefinitions) / sizeof(kDefinitions[0]) == kColorCount,
              "calibration definitions must match the EPD palette");

}  // namespace

const ColorDefinition &definition(size_t index) {
  return kDefinitions[index < kColorCount ? index : 0];
}

int slotForId(const char *id) {
  if (id == nullptr) return -1;
  for (size_t index = 0; index < kColorCount; ++index) {
    if (strcmp(kDefinitions[index].id, id) == 0) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

Profile defaultProfile() {
  Profile profile;
  for (size_t index = 0; index < kColorCount; ++index) {
    profile.display[index] = kDefinitions[index].defaultDisplay;
  }
  return profile;
}

Result validate(const Profile &profile) {
  if (profile.schemaVersion != kSchemaVersion) {
    return unsupported("unsupported e-paper calibration schema");
  }
  for (size_t left = 0; left < kColorCount; ++left) {
    for (size_t right = left + 1; right < kColorCount; ++right) {
      if (equal(profile.display[left], profile.display[right])) {
        return invalidInput("display colors must be unique");
      }
    }
  }
  return okResult();
}

bool equal(const Profile &left, const Profile &right) {
  if (left.schemaVersion != right.schemaVersion) return false;
  for (size_t index = 0; index < kColorCount; ++index) {
    if (!equal(left.display[index], right.display[index])) return false;
  }
  return true;
}

bool equal(const Rgb &left, const Rgb &right) {
  return left.r == right.r && left.g == right.g && left.b == right.b;
}

}  // namespace EpaperCalibration
