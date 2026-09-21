#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "CpuFrequencyGuard.h"
#include "EpaperCooldown.h"
#include "EpaperFrameSource.h"
#include "EpaperGzipReader.h"
#include "EpaperTimingDiagnostics.h"
#include "EpaperImageFormat.h"
#include "EpaperSafetyStore.h"
#include "EpaperShutdownCoordinator.h"
#include "Epd7In3E.h"
#include "EpdTransport.h"
#include "modules/storage/UserDataStorage.h"
#include "modules/runtime/BootDiagnostics.h"

enum class EpaperServiceState : uint8_t {
  Idle,
  Uploading,
  Queued,
  Drawing,
  Cooldown,
  Unavailable,
};

enum class EpaperDrawPhase : uint8_t {
  None,
  Prewake,
  Initializing,
  Transferring,
  Refreshing,
  PoweringOff,
  Sleeping,
  Quiescing,
};

enum class EpaperPanelState : uint8_t {
  Inactive,
  Active,
  Sleeping,
  Unknown,
};

enum class EpaperServiceStatusCode : uint8_t {
  Ok,
  Busy,
  Unavailable,
  InvalidImage,
  PayloadTooLarge,
  ImageNotFound,
  StorageBusy,
  StorageUnavailable,
  StorageError,
  UploadIncomplete,
};

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
  size_t storedSizeBytes = 0;
  EpaperImageFormat::Header header{};
  EpaperImageFormat::ValidationError validationError =
      EpaperImageFormat::ValidationError::None;
};

struct EpaperServiceSnapshot {
  EpaperServiceState state = EpaperServiceState::Unavailable;
  EpaperDrawPhase phase = EpaperDrawPhase::None;
  EpaperPanelState panelState = EpaperPanelState::Inactive;
  bool canUpload = false;
  bool canDraw = false;
  bool canDownload = false;
  bool recoveryRequired = false;
  bool brownoutDetected = false;
  bool brownoutDuringDraw = false;
  uint32_t retryAfterSeconds = 0;
  uint32_t cpuMhz = 0;
  EpaperStoredImageMetadata stored;
  const char *lastSource = "none";
  const char *lastResult = "none";
  const char *lastErrorCode = "none";
  const char *lastResetReason = "unknown";
  size_t transferredBytes = 0;
  EpaperTimingDiagnostics timings;
};

enum class EpaperDrawAction : uint8_t {
  Stored,
  White,
  Palette,
};

class EpaperService : public SystemRestartCoordinator {
 public:
  static constexpr const char *kImageName = "epaper-current.epd";
  static constexpr const char *kStoredImageName = "epaper-current.epd.gz";
  static constexpr size_t kMaxCompressedBytes = EpaperPanelProfile::Active::maxCompressedBytes;
  static constexpr size_t kWorkerStackBytes = 8192;

  Result begin(UserDataStorage *storage,
               Epd7In3E *driver,
               EpdTransport *transport,
               EpaperSafetyStore *safetyStore,
               CpuFrequencyDriver *frequencyDriver,
               EpaperShutdownCoordinator *shutdownCoordinator,
               const BootDiagnosticsSnapshot &bootDiagnostics);
  bool ready() const { return ready_; }
  RestartRequest requestRestart(uint32_t nowMs) override;
  void pollRestart(uint32_t nowMs, bool allowRestart = true) override;
  RestartProgress restartProgress() const override;
  bool ownsRuntime() const { return workerTask_ != nullptr; }
  void poll(uint32_t nowMs);
  EpaperServiceSnapshot snapshot(uint32_t nowMs) const;
  EpaperServiceSnapshot snapshot() const { return snapshot(millis()); }

  EpaperServiceResult beginUpload(size_t contentLength);
  EpaperServiceResult writeUpload(uint32_t sessionId,
                                  size_t index,
                                  const uint8_t *data,
                                  size_t length);
  EpaperServiceResult finishUpload(uint32_t sessionId);
  void abortUpload(uint32_t sessionId, const char *reason = "upload_aborted");

  EpaperServiceResult requestDraw(EpaperDrawAction action);
  EpaperServiceResult refreshMetadata();

  UserDataDownloadBegin beginImageDownload(const char *rangeHeader);
  UserDataReadResult readImageDownload(uint32_t sessionId,
                                       uint8_t *buffer,
                                       size_t bufferLength);
  void finishImageDownload(uint32_t sessionId);

 private:
  class StoredFrameSource final : public EpaperFrameSource {
   public:
    explicit StoredFrameSource(EpaperGzipReader *reader) : reader_(reader) {}
    size_t size() const override { return EpaperImageFormat::kFrameBytes; }
    size_t read(size_t offset, uint8_t *output, size_t capacity) const override;
    bool rewind() const override;
    void close() const override { reader_->close(); }
    uint32_t readMs() const { return readMs_; }
   private:
    EpaperGzipReader *reader_;
    mutable size_t expectedOffset_ = 0;
    mutable uint32_t readMs_ = 0;
  };

  UserDataStorage *storage_ = nullptr;
  Epd7In3E *driver_ = nullptr;
  EpdTransport *transport_ = nullptr;
  EpaperSafetyStore *safetyStore_ = nullptr;
  CpuFrequencyDriver *frequencyDriver_ = nullptr;
  EpaperShutdownCoordinator *shutdownCoordinator_ = nullptr;
  SemaphoreHandle_t mutex_ = nullptr;
  QueueHandle_t queue_ = nullptr;
  TaskHandle_t workerTask_ = nullptr;
  bool ready_ = false;
  bool admissionClosed_ = false;
  bool restartAllowed_ = false;
  bool markerClearPending_ = false;
  bool markerClearRunning_ = false;
  uint32_t markerClearRetryAtMs_ = 0;
  bool markerClearRetryWaiting_ = false;
  RestartProgress restartProgress_ = RestartProgress::Idle;
  uint32_t restartStartedMs_ = 0;
  uint32_t operationGeneration_ = 0;
  uint32_t cpuMhz_ = 0;
  static constexpr uint32_t kUploadDrainMs = 30000;
  static constexpr uint32_t kRestartDrainMs = 150000;
  static constexpr uint32_t kMarkerClearRetryMs = 1000;
  EpaperServiceState state_ = EpaperServiceState::Unavailable;
  EpaperDrawPhase phase_ = EpaperDrawPhase::None;
  EpaperPanelState panelState_ = EpaperPanelState::Inactive;
  bool recoveryRequired_ = false;
  EpaperCooldown cooldown_;
  EpaperStoredImageMetadata stored_;
  const char *lastResult_ = "none";
  const char *lastErrorCode_ = "none";
  const char *lastSource_ = "none";
  const char *queuedSource_ = "none";
  const char *lastResetReason_ = "unknown";
  bool brownoutDetected_ = false;
  bool brownoutDuringDraw_ = false;
  size_t transferredBytes_ = 0;
  EpaperTimingDiagnostics timings_;
  uint32_t operationStartedMs_ = 0;
  uint32_t uploadSessionId_ = 0;
  EpaperImageFormat::StreamingValidator uploadValidator_;
  std::unique_ptr<EpaperGzip> uploadDecoder_;
  size_t uploadCompressedBytes_ = 0;
  size_t uploadDeclaredBytes_ = 0;
  std::unique_ptr<EpaperGzipReader> downloadReader_;
  size_t downloadRemaining_ = 0;
  bool consumeUpload(const uint8_t *data, size_t length, bool finalInput);
  const char *uploadError() const;


  static void workerEntry(void *context);
  void workerLoop();
  void processControl(uint32_t nowMs);
  void executeDraw(EpaperDrawAction action);
  bool runDraw(const EpaperFrameSource &source);
  EpaperServiceResult queueDraw(EpaperDrawAction action, const char *source);
  EpaperServiceResult validateStoredImage();
  void setOperation(EpaperServiceState state,
                    EpaperDrawPhase phase,
                    EpaperPanelState panelState);
  void finishDraw(bool drawSucceeded,
                  bool shutdownSafe,
                  bool frequencyRestored,
                  EpdDriverError driverError);
  void setIdleAfterUploadFailure(const char *errorCode);
  static EpaperServiceResult fromStorage(const UserDataFileResult &result);
};

const char *epaperServiceStateToString(EpaperServiceState state);
const char *epaperDrawPhaseToString(EpaperDrawPhase phase);
const char *epaperPanelStateToString(EpaperPanelState state);
