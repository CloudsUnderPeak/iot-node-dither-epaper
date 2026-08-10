#include "PreferencesEpaperCalibrationStore.h"

#include <cstring>

namespace {

constexpr const char *kSlotNamespaces[] = {"epcal_a", "epcal_b"};
constexpr const char *kMetaNamespace = "epcal_meta";
constexpr const char *kActiveSlotKey = "active";
constexpr const char *kSchemaKey = "schema";
constexpr const char *kColorsKey = "colors";
constexpr size_t kEncodedColorChars = EpaperCalibration::kColorCount * 6;

char hexDigit(uint8_t value) {
  return value < 10 ? static_cast<char>('0' + value)
                    : static_cast<char>('A' + value - 10);
}

int hexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

void encodeByte(uint8_t value, char *output) {
  output[0] = hexDigit(static_cast<uint8_t>(value >> 4U));
  output[1] = hexDigit(static_cast<uint8_t>(value & 0x0FU));
}

bool decodeByte(const char *input, uint8_t &value) {
  const int high = hexValue(input[0]);
  const int low = hexValue(input[1]);
  if (high < 0 || low < 0) return false;
  value = static_cast<uint8_t>((high << 4U) | low);
  return true;
}

void encodeColors(const EpaperCalibration::Profile &profile,
                  char (&encoded)[kEncodedColorChars + 1]) {
  size_t offset = 0;
  for (const EpaperCalibration::Rgb &color : profile.display) {
    encodeByte(color.r, encoded + offset);
    encodeByte(color.g, encoded + offset + 2);
    encodeByte(color.b, encoded + offset + 4);
    offset += 6;
  }
  encoded[offset] = '\0';
}

bool decodeColors(const char *encoded, EpaperCalibration::Profile &profile) {
  if (encoded == nullptr || strlen(encoded) != kEncodedColorChars) return false;
  size_t offset = 0;
  for (EpaperCalibration::Rgb &color : profile.display) {
    if (!decodeByte(encoded + offset, color.r) ||
        !decodeByte(encoded + offset + 2, color.g) ||
        !decodeByte(encoded + offset + 4, color.b)) {
      return false;
    }
    offset += 6;
  }
  return true;
}

Result loadSlot(PreferencesBackend &backend,
                uint8_t slot,
                EpaperCalibration::Profile &profile) {
  const PreferencesNamespaceState namespaceState =
      backend.inspectNamespace(kSlotNamespaces[slot]);
  if (namespaceState == PreferencesNamespaceState::Missing) {
    return notFound("calibration slot is empty");
  }
  if (namespaceState == PreferencesNamespaceState::StorageError) {
    return storageError("failed to inspect calibration slot");
  }
  if (!backend.open(kSlotNamespaces[slot], true)) {
    return storageError("failed to open calibration slot for read");
  }
  if (!backend.hasKey(kSchemaKey)) {
    backend.close();
    return notFound("calibration slot is empty");
  }
  const uint16_t schema = backend.getUShort(kSchemaKey, 0);
  if (schema != EpaperCalibration::kSchemaVersion) {
    backend.close();
    return unsupported("unsupported e-paper calibration schema");
  }
  char encoded[kEncodedColorChars + 1]{};
  const bool loaded = backend.getString(kColorsKey, encoded, sizeof(encoded));
  backend.close();
  if (!loaded) return storageError("calibration slot is incomplete");

  EpaperCalibration::Profile decoded;
  decoded.schemaVersion = schema;
  if (!decodeColors(encoded, decoded)) {
    return storageError("calibration colors are corrupt");
  }
  const Result validation = EpaperCalibration::validate(decoded);
  if (!validation.ok()) return storageError(validation.message);
  profile = decoded;
  return okResult();
}

bool writeSlot(PreferencesBackend &backend,
               uint8_t slot,
               const EpaperCalibration::Profile &profile) {
  char encoded[kEncodedColorChars + 1]{};
  encodeColors(profile, encoded);
  if (!backend.open(kSlotNamespaces[slot], false)) return false;
  bool saved = backend.clear();
  saved = saved && backend.putString(kColorsKey, encoded);
  // Schema commits the slot only after every value is present.
  saved = saved && backend.putUShort(kSchemaKey, EpaperCalibration::kSchemaVersion);
  backend.close();
  return saved;
}

bool readActiveSlot(PreferencesBackend &backend,
                    uint8_t &slot,
                    bool &storageFailure) {
  storageFailure = false;
  const PreferencesNamespaceState namespaceState =
      backend.inspectNamespace(kMetaNamespace);
  if (namespaceState == PreferencesNamespaceState::Missing) return false;
  if (namespaceState == PreferencesNamespaceState::StorageError) {
    storageFailure = true;
    return false;
  }
  if (!backend.open(kMetaNamespace, true)) {
    storageFailure = true;
    return false;
  }
  const bool found = backend.hasKey(kActiveSlotKey);
  if (found) slot = backend.getUChar(kActiveSlotKey, 0xff);
  backend.close();
  storageFailure = found && slot >= 2;
  return found && slot < 2;
}

bool writeActiveSlot(PreferencesBackend &backend, uint8_t slot) {
  if (!backend.open(kMetaNamespace, false)) return false;
  const bool saved = backend.putUChar(kActiveSlotKey, slot);
  backend.close();
  if (!saved || !backend.open(kMetaNamespace, true)) return false;
  const bool verified = backend.hasKey(kActiveSlotKey) &&
                        backend.getUChar(kActiveSlotKey, 0xff) == slot;
  backend.close();
  return verified;
}

bool clearNamespace(PreferencesBackend &backend, const char *name) {
  const PreferencesNamespaceState state = backend.inspectNamespace(name);
  if (state == PreferencesNamespaceState::Missing) return true;
  if (state == PreferencesNamespaceState::StorageError ||
      !backend.open(name, false)) {
    return false;
  }
  const bool cleared = backend.clear();
  backend.close();
  return cleared;
}

}  // namespace

PreferencesEpaperCalibrationStore::PreferencesEpaperCalibrationStore(
    PreferencesBackend &backend)
    : backend_(backend) {}

Result PreferencesEpaperCalibrationStore::load(
    EpaperCalibration::Profile &profile) {
  uint8_t activeSlot = 0;
  bool metaFailure = false;
  const bool hasActiveSlot = readActiveSlot(backend_, activeSlot, metaFailure);
  bool sawUnsupported = false;
  bool sawCorruption = metaFailure;
  if (hasActiveSlot) {
    const Result activeResult = loadSlot(backend_, activeSlot, profile);
    if (activeResult.ok()) return activeResult;
    // A newer committed schema must never be silently replaced by an older
    // inactive slot. Keep it recoverable through the explicit reset API.
    if (activeResult.code == ResultCode::Unsupported) return activeResult;
    sawCorruption = activeResult.code != ResultCode::NotFound &&
                    activeResult.code != ResultCode::Unsupported;
  }

  for (uint8_t slot = 0; slot < 2; ++slot) {
    if (hasActiveSlot && slot == activeSlot) continue;
    const Result result = loadSlot(backend_, slot, profile);
    if (result.ok()) {
      writeActiveSlot(backend_, slot);
      return result;
    }
    sawUnsupported = sawUnsupported || result.code == ResultCode::Unsupported;
    sawCorruption = sawCorruption ||
                    (result.code != ResultCode::NotFound &&
                     result.code != ResultCode::Unsupported);
  }
  if (sawUnsupported) {
    return unsupported("unsupported e-paper calibration schema");
  }
  if (sawCorruption) return storageError("no valid calibration slot");
  return notFound("e-paper calibration is missing");
}

Result PreferencesEpaperCalibrationStore::save(
    const EpaperCalibration::Profile &profile) {
  const Result validation = EpaperCalibration::validate(profile);
  if (!validation.ok()) return validation;

  uint8_t activeSlot = 0;
  bool metaFailure = false;
  const bool hasActiveSlot = readActiveSlot(backend_, activeSlot, metaFailure);
  if (metaFailure) return storageError("failed to read calibration active slot");
  const uint8_t targetSlot =
      hasActiveSlot ? static_cast<uint8_t>(1U - activeSlot) : 0;
  if (!writeSlot(backend_, targetSlot, profile)) {
    return storageError("failed to save calibration values");
  }
  EpaperCalibration::Profile verified;
  const Result verifyResult = loadSlot(backend_, targetSlot, verified);
  if (!verifyResult.ok() || !EpaperCalibration::equal(profile, verified)) {
    return storageError("failed to verify saved calibration values");
  }
  if (!writeActiveSlot(backend_, targetSlot)) {
    return storageError("failed to commit calibration slot");
  }
  return okResult();
}

Result PreferencesEpaperCalibrationStore::reset() {
  if (!clearNamespace(backend_, kSlotNamespaces[0]) ||
      !clearNamespace(backend_, kSlotNamespaces[1]) ||
      !clearNamespace(backend_, kMetaNamespace)) {
    return storageError("failed to clear e-paper calibration");
  }
  return okResult();
}
