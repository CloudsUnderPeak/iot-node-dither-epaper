#pragma once

#include <cstddef>
#include <cstdint>

#include "modules/epaper/EpaperCooldown.h"
#include "modules/epaper/EpaperImageFormat.h"
#include "modules/epaper/CpuFrequencyGuard.h"
#include "modules/storage/UserDataStorage.h"

enum class EpaperServiceState : uint8_t { Idle, Uploading, Queued, Drawing, Cooldown, Unavailable };
enum class EpaperDrawPhase : uint8_t { None, Prewake, Initializing, Transferring, Refreshing, PoweringOff, Sleeping, Quiescing };
enum class EpaperPanelState : uint8_t { Inactive, Active, Sleeping, Unknown };
enum class EpaperServiceStatusCode : uint8_t { Ok, Busy, Unavailable, InvalidImage, ImageNotFound, StorageBusy, StorageUnavailable, StorageError, UploadIncomplete };
enum class EpaperDrawAction : uint8_t { Stored, White, Palette };

struct EpaperServiceResult {
  EpaperServiceStatusCode status = EpaperServiceStatusCode::Unavailable;
  const char *message = "e-paper unavailable";
  const char *reason = "none";
  uint32_t sessionId = 0;
  bool ok() const { return status == EpaperServiceStatusCode::Ok; }
};

struct EpaperStoredImageMetadata {
  bool present = false;
  bool valid = false;
  size_t sizeBytes = 0;
  EpaperImageFormat::Header header{};
  EpaperImageFormat::ValidationError validationError = EpaperImageFormat::ValidationError::None;
};

struct EpaperServiceSnapshot {
  EpaperServiceState state = EpaperServiceState::Idle;
  EpaperDrawPhase phase = EpaperDrawPhase::None;
  EpaperPanelState panelState = EpaperPanelState::Inactive;
  bool canUpload = true;
  bool canDraw = true;
  bool canDownload = false;
  bool recoveryRequired = false;
  bool brownoutDetected = false;
  bool brownoutDuringDraw = false;
  uint32_t retryAfterSeconds = 0;
  uint32_t cpuMhz = 160;
  EpaperStoredImageMetadata stored;
  const char *lastSource = "none";
  const char *lastResult = "none";
  const char *lastErrorCode = "none";
  const char *lastResetReason = "software";
  size_t transferredBytes = 0;
};

class EpaperService {
 public:
  static constexpr const char *kImageName = "epaper-current.epd";
  EpaperServiceSnapshot current;
  EpaperServiceResult actionResult{EpaperServiceStatusCode::Ok, "draw queued"};
  EpaperServiceResult uploadResult{EpaperServiceStatusCode::Ok, "upload started", "none", 51};
  EpaperServiceSnapshot snapshot(uint32_t) const { return current; }
  EpaperServiceSnapshot snapshot() const { return current; }
  EpaperServiceResult requestDraw(EpaperDrawAction) { return actionResult; }
  EpaperServiceResult beginUpload(size_t) { return uploadResult; }
  EpaperServiceResult writeUpload(uint32_t, size_t, const uint8_t *, size_t) {
    return {EpaperServiceStatusCode::Ok, "ok"};
  }
  EpaperServiceResult finishUpload(uint32_t) {
    return {EpaperServiceStatusCode::Ok, "queued"};
  }
  void abortUpload(uint32_t) {}
  UserDataDownloadBegin beginImageDownload(const char *) {
    return {{UserDataFileStatus::Ok, "ok"}, 52, EpaperImageFormat::kImageBytes,
            0, EpaperImageFormat::kImageBytes, false};
  }
  UserDataReadResult readImageDownload(uint32_t, uint8_t *, size_t) {
    return {{UserDataFileStatus::Ok, "ok"}, 0};
  }
  void finishImageDownload(uint32_t) {}
};

inline const char *epaperServiceStateToString(EpaperServiceState state) {
  switch (state) {
    case EpaperServiceState::Idle: return "idle";
    case EpaperServiceState::Uploading: return "uploading";
    case EpaperServiceState::Queued: return "queued";
    case EpaperServiceState::Drawing: return "drawing";
    case EpaperServiceState::Cooldown: return "cooldown";
    case EpaperServiceState::Unavailable: return "unavailable";
  }
  return "unavailable";
}
inline const char *epaperDrawPhaseToString(EpaperDrawPhase) { return "none"; }
inline const char *epaperPanelStateToString(EpaperPanelState state) {
  return state == EpaperPanelState::Sleeping ? "sleeping" :
         state == EpaperPanelState::Unknown ? "unknown" :
         state == EpaperPanelState::Active ? "active" : "inactive";
}
