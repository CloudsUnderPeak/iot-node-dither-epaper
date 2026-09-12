#include "EpaperService.h"

#include <cstdio>
#include <cstring>

namespace {

class SemaphoreLock {
 public:
  explicit SemaphoreLock(SemaphoreHandle_t mutex)
      : mutex_(mutex), locked_(mutex != nullptr && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {}
  ~SemaphoreLock() {
    if (locked_) xSemaphoreGive(mutex_);
  }
  bool locked() const { return locked_; }

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  bool locked_ = false;
};

const char *driverErrorCode(EpdDriverError error) {
  switch (error) {
    case EpdDriverError::None: return "none";
    case EpdDriverError::InvalidState: return "invalid_state";
    case EpdDriverError::TransportFailure: return "spi_failure";
    case EpdDriverError::BusyTimeout: return "busy_timeout";
    case EpdDriverError::BusyNeverAsserted: return "busy_never_asserted";
    case EpdDriverError::OperationWatchdogTimeout: return "operation_watchdog_timeout";
    case EpdDriverError::SourceReadFailed: return "frame_read_failed";
    case EpdDriverError::FrameSizeMismatch: return "frame_size_mismatch";
    case EpdDriverError::PowerOffFailed: return "power_off_failed";
    case EpdDriverError::SleepFailed: return "sleep_failed";
  }
  return "driver_error";
}

}  // namespace

Result EpaperService::begin(
    UserDataStorage *storage,
    Epd7In3E *driver,
    EpdTransport *transport,
    EpaperSafetyStore *safetyStore,
    CpuFrequencyDriver *frequencyDriver,
    EpaperShutdownCoordinator *shutdownCoordinator,
    const BootDiagnosticsSnapshot &bootDiagnostics) {
  // Establish the restart boundary even when userdata could not mount.
  storage_ = storage;
  mutex_ = xSemaphoreCreateMutex();
  shutdownCoordinator_ = shutdownCoordinator;
  safetyStore_ = safetyStore;
  driver_ = driver;
  frequencyDriver_ = frequencyDriver;
  cpuMhz_ = frequencyDriver == nullptr ? 0 : frequencyDriver->currentMhz();
  if (mutex_ == nullptr) return storageError("failed to allocate e-paper mutex");
  if (storage == nullptr || !storage->mounted() || driver == nullptr ||
      transport == nullptr || !transport->ready() || safetyStore == nullptr ||
      !safetyStore->ready() || frequencyDriver == nullptr ||
      shutdownCoordinator == nullptr || !shutdownCoordinator->ready()) {
    return invalidInput("missing e-paper service dependencies");
  }
  storage_ = storage;
  driver_ = driver;
  transport_ = transport;
  safetyStore_ = safetyStore;
  frequencyDriver_ = frequencyDriver;
  shutdownCoordinator_ = shutdownCoordinator;
  lastResetReason_ = deviceResetReasonToString(bootDiagnostics.resetReason);
  brownoutDetected_ = bootDiagnostics.resetReason == DeviceResetReason::Brownout;
  queue_ = xQueueCreate(1, sizeof(EpaperDrawAction));
  if (mutex_ == nullptr || queue_ == nullptr) {
    return storageError("failed to allocate e-paper worker resources");
  }

  state_ = EpaperServiceState::Idle;
  panelState_ = EpaperPanelState::Inactive;
  if (shutdownCoordinator_->unavailable() ||
      safetyStore_->stage() == EpaperProtectionStage::Active) {
    state_ = EpaperServiceState::Unavailable;
    panelState_ = EpaperPanelState::Unknown;
    recoveryRequired_ = true;
    lastResult_ = "interrupted";
    lastErrorCode_ = brownoutDetected_ ? "brownout" : "panel_state_unknown";
    brownoutDuringDraw_ = brownoutDetected_;
  } else if (safetyStore_->stage() ==
             EpaperProtectionStage::ShutdownConfirmed) {
    state_ = EpaperServiceState::Cooldown;
    panelState_ = EpaperPanelState::Sleeping;
    cooldown_.begin(millis());
  }

  const EpaperServiceResult metadata = validateStoredImage();
  if (!metadata.ok() &&
      metadata.status != EpaperServiceStatusCode::ImageNotFound &&
      metadata.status != EpaperServiceStatusCode::InvalidImage) {
    return storageError("failed to inspect stored e-paper image");
  }
  if (xTaskCreate(workerEntry, "epaper-worker", kWorkerStackBytes, this, 1,
                  &workerTask_) != pdPASS) {
    return storageError("failed to start e-paper worker");
  }
  ready_ = true;
  return okResult();
}

RestartRequest EpaperService::requestRestart(uint32_t nowMs) {
  SemaphoreLock lock(mutex_);
  if (!lock.locked()) return RestartRequest::Rejected;
  if (restartProgress_ == RestartProgress::Draining ||
      restartProgress_ == RestartProgress::Ready) return RestartRequest::AlreadyPending;
  if (admissionClosed_ || storage_ == nullptr || driver_ == nullptr || safetyStore_ == nullptr ||
      shutdownCoordinator_ == nullptr ||
      !shutdownCoordinator_->ready() || recoveryRequired_) return RestartRequest::Rejected;
  admissionClosed_ = true;
  restartAllowed_ = false;
  restartStartedMs_ = nowMs;
  restartProgress_ = RestartProgress::Draining;
  return RestartRequest::Accepted;
}

RestartProgress EpaperService::restartProgress() const {
  SemaphoreLock lock(mutex_);
  return lock.locked() ? restartProgress_ : RestartProgress::Failed;
}

void EpaperService::pollRestart(uint32_t nowMs, bool allowRestart) {
  {
    SemaphoreLock lock(mutex_);
    if (!lock.locked()) return;
    restartAllowed_ = allowRestart;
    if (restartProgress_ == RestartProgress::Draining &&
        (nowMs - restartStartedMs_ >= kRestartDrainMs ||
         (state_ == EpaperServiceState::Uploading &&
          nowMs - restartStartedMs_ >= kUploadDrainMs))) {
      restartProgress_ = RestartProgress::Failed;
    }
  }
  // With no task created, boot remains the only owner. It may approve only
  // already safe panel state, never perform a drain on somebody else's task.
  if (!ownsRuntime()) processControl(nowMs);
}

void EpaperService::poll(uint32_t nowMs) {
  if (!ready_) return;
  SemaphoreLock lock(mutex_);
  if (lock.locked() && !admissionClosed_ &&
      (state_ == EpaperServiceState::Cooldown ||
       (state_ == EpaperServiceState::Unavailable &&
        strcmp(lastErrorCode_, "marker_clear_failed") == 0)) &&
      cooldown_.elapsed(nowMs) &&
      !markerClearRunning_ &&
      (!markerClearRetryWaiting_ ||
       static_cast<uint32_t>(nowMs - markerClearRetryAtMs_) < 0x80000000UL)) {
    markerClearPending_ = true;
  }
}

void EpaperService::processControl(uint32_t nowMs) {
  bool clear = false;
  bool restart = false;
  uint32_t generation = 0;
  {
    SemaphoreLock lock(mutex_);
    if (!lock.locked()) return;
    const bool busy = state_ == EpaperServiceState::Uploading ||
                      state_ == EpaperServiceState::Queued ||
                      state_ == EpaperServiceState::Drawing || markerClearRunning_;
    if (busy) return;
    if (restartProgress_ == RestartProgress::Draining) {
      if (nowMs - restartStartedMs_ >= kRestartDrainMs || recoveryRequired_ ||
          shutdownCoordinator_->unavailable() || driver_->panelMayBeActive() ||
          safetyStore_->stage() == EpaperProtectionStage::Active) {
        restartProgress_ = RestartProgress::Failed;
        if (recoveryRequired_ || shutdownCoordinator_->unavailable() ||
            driver_->panelMayBeActive() ||
            safetyStore_->stage() == EpaperProtectionStage::Active) {
          recoveryRequired_ = true;
          state_ = EpaperServiceState::Unavailable;
          panelState_ = EpaperPanelState::Unknown;
        }
      } else if (restartAllowed_ && storage_->reserveRestart()) {
        // Irrevocably claim this ACK once. Even a returning fake restart
        // driver must leave admission closed and execute no further work.
        restartProgress_ = RestartProgress::Ready;
        markerClearPending_ = false;
        restart = true;
      }
    }
    if (restartProgress_ == RestartProgress::Failed &&
        (!recoveryRequired_ || markerClearRetryWaiting_) &&
        shutdownCoordinator_ != nullptr && !shutdownCoordinator_->unavailable() &&
        !driver_->panelMayBeActive() &&
        safetyStore_->stage() != EpaperProtectionStage::Active) {
      admissionClosed_ = false;
    }
    if (!admissionClosed_ && markerClearPending_) {
      markerClearPending_ = false;
      markerClearRunning_ = true;
      generation = operationGeneration_;
      clear = true;
    }
  }
  if (restart && !shutdownCoordinator_->restartNow()) {
    SemaphoreLock lock(mutex_);
    restartProgress_ = RestartProgress::Failed;
    recoveryRequired_ = true;
  }
  if (!clear) return;
  const bool cleared = safetyStore_->clear();
  SemaphoreLock lock(mutex_);
  markerClearRunning_ = false;
  if (generation != operationGeneration_) return;
  if (cooldown_.releaseIfElapsed(nowMs, cleared)) {
    state_ = EpaperServiceState::Idle;
    phase_ = EpaperDrawPhase::None;
    panelState_ = EpaperPanelState::Sleeping;
    recoveryRequired_ = false;
    markerClearRetryWaiting_ = false;
    if (restartProgress_ == RestartProgress::Failed) admissionClosed_ = false;
  } else if (!cleared) {
    // A transient NVS/write/read-back failure must not permanently strand the
    // panel. Keep the safety gate closed and retry after a short backoff.
    // Preserve unavailable until a retry succeeds so clients can see that
    // protection cleanup failed, rather than treating it as idle.
    state_ = EpaperServiceState::Unavailable;
    panelState_ = EpaperPanelState::Unknown;
    recoveryRequired_ = true;
    markerClearRetryAtMs_ = static_cast<uint32_t>(nowMs + kMarkerClearRetryMs);
    markerClearRetryWaiting_ = true;
    lastResult_ = "failed";
    lastErrorCode_ = "marker_clear_failed";
  }
}

EpaperServiceSnapshot EpaperService::snapshot(uint32_t nowMs) const {
  EpaperServiceSnapshot snapshot;
  SemaphoreLock lock(mutex_);
  if (!lock.locked()) return snapshot;
  snapshot.state = state_;
  snapshot.phase = phase_;
  snapshot.panelState = panelState_;
  snapshot.canUpload = ready_ && !admissionClosed_ && state_ == EpaperServiceState::Idle;
  snapshot.canDraw = snapshot.canUpload;
  const char *drawSource = state_ == EpaperServiceState::Queued
                               ? queuedSource_
                               : lastSource_;
  const bool storedDraw = strcmp(drawSource, "uploaded") == 0 ||
                          strcmp(drawSource, "refresh") == 0;
  snapshot.canDownload = ready_ && !admissionClosed_ && stored_.present &&
                         state_ != EpaperServiceState::Uploading &&
                         !((state_ == EpaperServiceState::Queued ||
                            state_ == EpaperServiceState::Drawing) &&
                           storedDraw);
  snapshot.recoveryRequired = recoveryRequired_;
  snapshot.brownoutDetected = brownoutDetected_;
  snapshot.brownoutDuringDraw = brownoutDuringDraw_;
  snapshot.retryAfterSeconds = state_ == EpaperServiceState::Cooldown
                                   ? cooldown_.retryAfterSeconds(nowMs)
                                   : 0;
  snapshot.cpuMhz = cpuMhz_;
  snapshot.stored = stored_;
  snapshot.lastSource = lastSource_;
  snapshot.lastResult = lastResult_;
  snapshot.lastErrorCode = lastErrorCode_;
  snapshot.lastResetReason = lastResetReason_;
  snapshot.transferredBytes = transferredBytes_;
  return snapshot;
}

EpaperServiceResult EpaperService::beginUpload(size_t contentLength) {
  if (!ready_) return {EpaperServiceStatusCode::Unavailable, "e-paper unavailable"};
  if (contentLength != EpaperImageFormat::kImageBytes) {
    return {EpaperServiceStatusCode::InvalidImage,
            "EPDIMG Content-Length must be exactly 192040 bytes",
            "bad_size"};
  }
  {
    SemaphoreLock lock(mutex_);
    if (!lock.locked()) return {EpaperServiceStatusCode::Unavailable, "e-paper unavailable"};
    if (admissionClosed_ || state_ != EpaperServiceState::Idle) {
      return {EpaperServiceStatusCode::Busy, "e-paper is busy"};
    }
    ++operationGeneration_;
    state_ = EpaperServiceState::Uploading;
    phase_ = EpaperDrawPhase::None;
    uploadValidator_.reset();
  }
  const UserDataUploadBegin begin = storage_->beginUpload(kImageName, contentLength);
  if (!begin.result.ok()) {
    setIdleAfterUploadFailure("storage_error");
    return fromStorage(begin.result);
  }
  uploadSessionId_ = begin.sessionId;
  return {EpaperServiceStatusCode::Ok, "upload started", "none", begin.sessionId};
}

EpaperServiceResult EpaperService::writeUpload(
    uint32_t sessionId,
    size_t index,
    const uint8_t *data,
    size_t length) {
  if (sessionId == 0 || sessionId != uploadSessionId_) {
    return {EpaperServiceStatusCode::UploadIncomplete, "upload session is not active"};
  }
  if (sessionId == 0 || sessionId != uploadSessionId_ ||
      index != uploadValidator_.bytesReceived() ||
      !uploadValidator_.consume(data, length)) {
    storage_->abortUpload(sessionId);
    uploadSessionId_ = 0;
    const char *reason = EpaperImageFormat::errorCode(uploadValidator_.error());
    setIdleAfterUploadFailure(reason);
    return {EpaperServiceStatusCode::InvalidImage, "invalid EPDIMG", reason};
  }
  const UserDataFileResult written =
      storage_->writeUpload(sessionId, index, data, length);
  if (!written.ok()) {
    uploadSessionId_ = 0;
    setIdleAfterUploadFailure("storage_error");
    return fromStorage(written);
  }
  return {EpaperServiceStatusCode::Ok, "upload chunk accepted"};
}

EpaperServiceResult EpaperService::finishUpload(uint32_t sessionId) {
  if (sessionId == 0 || sessionId != uploadSessionId_) {
    return {EpaperServiceStatusCode::UploadIncomplete, "upload session is not active"};
  }
  if (sessionId == 0 || sessionId != uploadSessionId_ ||
      !uploadValidator_.finish()) {
    storage_->abortUpload(sessionId);
    uploadSessionId_ = 0;
    const char *reason = EpaperImageFormat::errorCode(uploadValidator_.error());
    setIdleAfterUploadFailure(reason);
    return {EpaperServiceStatusCode::InvalidImage, "invalid EPDIMG", reason};
  }
  const UserDataUploadCommit commit = storage_->finishUpload(sessionId);
  uploadSessionId_ = 0;
  if (!commit.result.ok()) {
    setIdleAfterUploadFailure("storage_error");
    return fromStorage(commit.result);
  }
  {
    SemaphoreLock lock(mutex_);
    stored_.present = true;
    stored_.valid = true;
    stored_.sizeBytes = commit.sizeBytes;
    stored_.header = uploadValidator_.header();
    stored_.validationError = EpaperImageFormat::ValidationError::None;
    state_ = EpaperServiceState::Queued;
    queuedSource_ = "uploaded";
    const EpaperDrawAction action = EpaperDrawAction::Stored;
    if (xQueueSend(queue_, &action, 0) != pdTRUE) {
      state_ = EpaperServiceState::Idle;
      queuedSource_ = "none";
      return {EpaperServiceStatusCode::Busy, "e-paper queue is full"};
    }
  }
  return {EpaperServiceStatusCode::Ok, "draw queued"};
}

void EpaperService::abortUpload(uint32_t sessionId) {
  if (sessionId == 0 || sessionId != uploadSessionId_) return;
  storage_->abortUpload(sessionId);
  uploadSessionId_ = 0;
  setIdleAfterUploadFailure("upload_aborted");
}

EpaperServiceResult EpaperService::requestDraw(EpaperDrawAction action) {
  const char *source = action == EpaperDrawAction::Stored
                           ? "refresh"
                           : (action == EpaperDrawAction::White ? "white"
                                                                : "palette");
  return queueDraw(action, source);
}

EpaperServiceResult EpaperService::queueDraw(EpaperDrawAction action,
                                             const char *source) {
  if (!ready_) return {EpaperServiceStatusCode::Unavailable, "e-paper unavailable"};
  {
    SemaphoreLock lock(mutex_);
    if (!lock.locked()) return {EpaperServiceStatusCode::Unavailable, "e-paper unavailable"};
    if (admissionClosed_ || state_ != EpaperServiceState::Idle) {
      return {EpaperServiceStatusCode::Busy, "e-paper is busy"};
    }
    ++operationGeneration_;
    state_ = EpaperServiceState::Queued;
    phase_ = EpaperDrawPhase::None;
    queuedSource_ = source;
  }
  if (action == EpaperDrawAction::Stored) {
    const EpaperServiceResult metadata = validateStoredImage();
    if (!metadata.ok()) {
      SemaphoreLock lock(mutex_);
      state_ = EpaperServiceState::Idle;
      queuedSource_ = "none";
      return metadata;
    }
  }
  SemaphoreLock lock(mutex_);
  if (xQueueSend(queue_, &action, 0) != pdTRUE) {
    state_ = EpaperServiceState::Idle;
    queuedSource_ = "none";
    return {EpaperServiceStatusCode::Busy, "e-paper queue is full"};
  }
  return {EpaperServiceStatusCode::Ok, "draw queued"};
}

EpaperServiceResult EpaperService::refreshMetadata() {
  {
    SemaphoreLock lock(mutex_);
    if (!lock.locked() || !ready_) return {EpaperServiceStatusCode::Unavailable, "e-paper unavailable"};
    if (admissionClosed_ || state_ != EpaperServiceState::Idle) {
      return {EpaperServiceStatusCode::Busy, "e-paper is busy"};
    }
    ++operationGeneration_;
    state_ = EpaperServiceState::Queued; // reservation, no worker item yet
  }
  const auto result = validateStoredImage();
  SemaphoreLock lock(mutex_);
  state_ = EpaperServiceState::Idle;
  return result;
}

EpaperServiceResult EpaperService::validateStoredImage() {
  if (storage_ == nullptr) {
    return {EpaperServiceStatusCode::StorageUnavailable, "userdata is unavailable"};
  }
  uint32_t generation;
  {
    SemaphoreLock lock(mutex_);
    generation = operationGeneration_;
  }
  EpaperStoredImageMetadata candidate;
  const UserDataDownloadBegin begin = storage_->beginDownload(kImageName, "");
  if (!begin.result.ok()) {
    if (begin.result.status == UserDataFileStatus::NotFound) {
      SemaphoreLock lock(mutex_);
      if (generation == operationGeneration_) stored_ = {};
    }
    return fromStorage(begin.result);
  }
  EpaperImageFormat::StreamingValidator validator;
  uint8_t buffer[UserDataStorage::kFileIoChunkBytes];
  size_t receivedBytes = 0;
  bool contentValid = true;
  while (receivedBytes < begin.contentLength) {
    const auto read = storage_->readDownload(begin.sessionId, buffer, sizeof(buffer));
    if (!read.result.ok() || read.bytesRead == 0) {
      storage_->finishDownload(begin.sessionId);
      return {EpaperServiceStatusCode::StorageError, "failed to read stored image"};
    }
    receivedBytes += read.bytesRead;
    // Consume the whole file even after invalid syntax, so an I/O failure
    // cannot be mislabeled as a fully validated corrupt file.
    if (contentValid) contentValid = validator.consume(buffer, read.bytesRead);
  }
  const bool valid = contentValid && validator.finish() &&
                     begin.fileSize == EpaperImageFormat::kImageBytes;
  candidate.present = true;
  candidate.valid = valid;
  candidate.sizeBytes = begin.fileSize;
  candidate.header = validator.header();
  candidate.validationError = validator.error();
  bool published = false;
  {
    SemaphoreLock lock(mutex_);
    // Every runtime validator owns admission before opening storage. This
    // reservation excludes upload/rename until candidate publication, even
    // if readDownload auto-releases its gate at EOF. Generic routes cannot
    // mutate the reserved image filename.
    published = generation == operationGeneration_;
    if (published) stored_ = candidate;
  }
  storage_->finishDownload(begin.sessionId);
  if (!published) return {EpaperServiceStatusCode::Busy, "stored image validation superseded"};
  if (!valid) {
    return {EpaperServiceStatusCode::InvalidImage, "stored EPDIMG is invalid",
            EpaperImageFormat::errorCode(candidate.validationError)};
  }
  return {EpaperServiceStatusCode::Ok, "stored image valid"};
}

UserDataDownloadBegin EpaperService::beginImageDownload(const char *rangeHeader) {
  {
    SemaphoreLock lock(mutex_);
    if (!lock.locked() || admissionClosed_) {
      UserDataDownloadBegin denied;
      denied.result = {UserDataFileStatus::Busy, "e-paper restart drain is pending"};
      return denied;
    }
  }
  return storage_->beginDownload(kImageName, rangeHeader);
}

UserDataReadResult EpaperService::readImageDownload(
    uint32_t sessionId,
    uint8_t *buffer,
    size_t bufferLength) {
  return storage_->readDownload(sessionId, buffer, bufferLength);
}

void EpaperService::finishImageDownload(uint32_t sessionId) {
  storage_->finishDownload(sessionId);
}

size_t EpaperService::StoredFrameSource::read(
    size_t offset,
    uint8_t *output,
    size_t capacity) const {
  if (failed_ || storage_ == nullptr || offset != expectedOffset_) {
    failed_ = true;
    return 0;
  }
  const UserDataReadResult result = storage_->readDownload(sessionId_, output, capacity);
  if (!result.result.ok()) {
    failed_ = true;
    return 0;
  }
  expectedOffset_ += result.bytesRead;
  return result.bytesRead;
}

void EpaperService::workerEntry(void *context) {
  static_cast<EpaperService *>(context)->workerLoop();
}

void EpaperService::workerLoop() {
  while (true) {
    // Cooldown must progress even if the main loop is delayed by a console
    // client. Only this worker performs marker I/O.
    poll(millis());
    processControl(millis());
    EpaperDrawAction action = EpaperDrawAction::White;
    if (xQueueReceive(queue_, &action, pdMS_TO_TICKS(10)) == pdTRUE) executeDraw(action);
  }
}

void EpaperService::executeDraw(EpaperDrawAction action) {
  {
    SemaphoreLock lock(mutex_);
    lastSource_ = queuedSource_;
    queuedSource_ = "none";
  }
  if (action == EpaperDrawAction::White) {
    EpaperWhiteFrameSource source;
    runDraw(source);
    return;
  }
  if (action == EpaperDrawAction::Palette) {
    EpaperPaletteFrameSource source;
    runDraw(source);
    return;
  }
  const EpaperServiceResult validation = validateStoredImage();
  if (!validation.ok()) {
    SemaphoreLock lock(mutex_);
    state_ = EpaperServiceState::Idle;
    lastResult_ = "failed";
    lastErrorCode_ = validation.reason;
    return;
  }
  char range[48]{};
  snprintf(range, sizeof(range), "bytes=%u-%u",
           static_cast<unsigned>(EpaperImageFormat::kHeaderBytes),
           static_cast<unsigned>(EpaperImageFormat::kImageBytes - 1));
  const UserDataDownloadBegin begin = storage_->beginDownload(kImageName, range);
  if (!begin.result.ok() || begin.contentLength != EpaperImageFormat::kFrameBytes) {
    if (begin.result.ok()) storage_->finishDownload(begin.sessionId);
    SemaphoreLock lock(mutex_);
    state_ = EpaperServiceState::Idle;
    lastResult_ = "failed";
    lastErrorCode_ = "storage_error";
    return;
  }
  StoredFrameSource source(storage_, begin.sessionId);
  runDraw(source, begin.sessionId);
}

bool EpaperService::runDraw(const EpaperFrameSource &source,
                            uint32_t storageSessionId) {
  setOperation(EpaperServiceState::Drawing, EpaperDrawPhase::Prewake,
               EpaperPanelState::Inactive);
  if (!safetyStore_->markActive()) {
    if (storageSessionId != 0) storage_->finishDownload(storageSessionId);
    finishDraw(false, false, true, EpdDriverError::None);
    return false;
  }
  if (!driver_->begin(transport_)) {
    if (storageSessionId != 0) storage_->finishDownload(storageSessionId);
    const bool safe = shutdownCoordinator_->finishOperation();
    finishDraw(false, safe, true, driver_->lastError());
    return false;
  }
  const uint32_t originalMhz = frequencyDriver_->currentMhz();
  CpuFrequencyGuard frequencyGuard;
  if (!frequencyGuard.acquire(frequencyDriver_)) {
    if (storageSessionId != 0) storage_->finishDownload(storageSessionId);
    const bool safe = shutdownCoordinator_->finishOperation();
    finishDraw(false, safe, frequencyDriver_->currentMhz() == originalMhz, EpdDriverError::None);
    return false;
  }
  {
    SemaphoreLock lock(mutex_);
    cpuMhz_ = 80;
  }
  transport_->delayMs(2000);
  setOperation(EpaperServiceState::Drawing, EpaperDrawPhase::Initializing,
               EpaperPanelState::Inactive);
  const bool initialized = driver_->initialize();
  if (initialized) {
    setOperation(EpaperServiceState::Drawing, EpaperDrawPhase::Transferring,
                 EpaperPanelState::Active);
  }
  const bool transferred = initialized && driver_->transferFrame(source);
  if (storageSessionId != 0) storage_->finishDownload(storageSessionId);
  const bool drawn = transferred && [&]() {
    setOperation(EpaperServiceState::Drawing, EpaperDrawPhase::Refreshing,
                 EpaperPanelState::Active);
    return driver_->refresh();
  }();
  const EpdDriverError error = driver_->lastError();
  {
    SemaphoreLock lock(mutex_);
    transferredBytes_ = driver_->transferredBytes();
  }
  setOperation(EpaperServiceState::Drawing, EpaperDrawPhase::PoweringOff,
               driver_->panelMayBeActive() ? EpaperPanelState::Active
                                           : EpaperPanelState::Inactive);
  const bool shutdownSafe = shutdownCoordinator_->finishOperation();
  const bool frequencyRestored = frequencyGuard.release();
  const uint32_t restoredMhz = frequencyDriver_->currentMhz();
  {
    SemaphoreLock lock(mutex_);
    cpuMhz_ = restoredMhz;
  }
  finishDraw(drawn, shutdownSafe, frequencyRestored, error);
  return drawn && shutdownSafe && frequencyRestored;
}

void EpaperService::finishDraw(bool drawSucceeded,
                               bool shutdownSafe,
                               bool frequencyRestored,
                               EpdDriverError driverError) {
  SemaphoreLock lock(mutex_);
  phase_ = EpaperDrawPhase::None;
  if (!shutdownSafe) {
    state_ = EpaperServiceState::Unavailable;
    panelState_ = EpaperPanelState::Unknown;
    recoveryRequired_ = true;
    lastResult_ = "failed";
    lastErrorCode_ = "shutdown_failed";
    cooldown_.markUnavailable();
    return;
  }
  if (safetyStore_->stage() == EpaperProtectionStage::ShutdownConfirmed) {
    state_ = EpaperServiceState::Cooldown;
    panelState_ = EpaperPanelState::Sleeping;
    cooldown_.begin(millis());
    markerClearRetryAtMs_ = 0;
    markerClearRetryWaiting_ = false;
  } else {
    state_ = EpaperServiceState::Idle;
    panelState_ = EpaperPanelState::Inactive;
  }
  if (!frequencyRestored) {
    state_ = EpaperServiceState::Unavailable;
    recoveryRequired_ = true;
    lastResult_ = "failed";
    lastErrorCode_ = "cpu_restore_failed";
  } else if (drawSucceeded) {
    lastResult_ = "success";
    lastErrorCode_ = "none";
  } else {
    lastResult_ = "failed";
    lastErrorCode_ = driverErrorCode(driverError);
  }
}

void EpaperService::setOperation(EpaperServiceState state,
                                 EpaperDrawPhase phase,
                                 EpaperPanelState panelState) {
  SemaphoreLock lock(mutex_);
  state_ = state;
  phase_ = phase;
  panelState_ = panelState;
}

void EpaperService::setIdleAfterUploadFailure(const char *errorCode) {
  SemaphoreLock lock(mutex_);
  if (state_ == EpaperServiceState::Uploading) state_ = EpaperServiceState::Idle;
  lastResult_ = "failed";
  lastErrorCode_ = errorCode;
}

EpaperServiceResult EpaperService::fromStorage(const UserDataFileResult &result) {
  switch (result.status) {
    case UserDataFileStatus::Ok:
      return {EpaperServiceStatusCode::Ok, result.message};
    case UserDataFileStatus::Busy:
      return {EpaperServiceStatusCode::StorageBusy, result.message};
    case UserDataFileStatus::Unavailable:
      return {EpaperServiceStatusCode::StorageUnavailable, result.message};
    case UserDataFileStatus::NotFound:
      return {EpaperServiceStatusCode::ImageNotFound, result.message};
    case UserDataFileStatus::UploadIncomplete:
      return {EpaperServiceStatusCode::UploadIncomplete, result.message};
    case UserDataFileStatus::InvalidName:
    case UserDataFileStatus::PayloadTooLarge:
    case UserDataFileStatus::RangeNotSatisfiable:
    case UserDataFileStatus::InsufficientStorage:
    case UserDataFileStatus::StorageError:
      return {EpaperServiceStatusCode::StorageError, result.message};
  }
  return {EpaperServiceStatusCode::StorageError, "storage error"};
}

const char *epaperServiceStateToString(EpaperServiceState state) {
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

const char *epaperDrawPhaseToString(EpaperDrawPhase phase) {
  switch (phase) {
    case EpaperDrawPhase::None: return "none";
    case EpaperDrawPhase::Prewake: return "prewake";
    case EpaperDrawPhase::Initializing: return "initializing";
    case EpaperDrawPhase::Transferring: return "transferring";
    case EpaperDrawPhase::Refreshing: return "refreshing";
    case EpaperDrawPhase::PoweringOff: return "powering_off";
    case EpaperDrawPhase::Sleeping: return "sleeping";
    case EpaperDrawPhase::Quiescing: return "quiescing";
  }
  return "none";
}

const char *epaperPanelStateToString(EpaperPanelState state) {
  switch (state) {
    case EpaperPanelState::Inactive: return "inactive";
    case EpaperPanelState::Active: return "active";
    case EpaperPanelState::Sleeping: return "sleeping";
    case EpaperPanelState::Unknown: return "unknown";
  }
  return "unknown";
}
