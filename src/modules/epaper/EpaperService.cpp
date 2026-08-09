#include "EpaperService.h"

#include <cstdio>
#include <cstring>
#include <esp_system.h>

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

const char *resetReasonLabel(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power_on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "unknown";
  }
}

}  // namespace

Result EpaperService::begin(
    UserDataStorage *storage,
    Epd7In3E *driver,
    EpdTransport *transport,
    EpaperSafetyStore *safetyStore,
    CpuFrequencyDriver *frequencyDriver,
    EpaperShutdownCoordinator *shutdownCoordinator) {
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
  const esp_reset_reason_t resetReason = esp_reset_reason();
  lastResetReason_ = resetReasonLabel(resetReason);
  brownoutDetected_ = resetReason == ESP_RST_BROWNOUT;
  mutex_ = xSemaphoreCreateMutex();
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

void EpaperService::poll(uint32_t nowMs) {
  if (!ready_) return;
  SemaphoreLock lock(mutex_);
  if (!lock.locked() || state_ != EpaperServiceState::Cooldown ||
      !cooldown_.elapsed(nowMs)) {
    return;
  }
  const bool cleared = safetyStore_->clear();
  if (cooldown_.releaseIfElapsed(nowMs, cleared)) {
    state_ = EpaperServiceState::Idle;
    phase_ = EpaperDrawPhase::None;
    panelState_ = EpaperPanelState::Sleeping;
  } else if (!cleared) {
    state_ = EpaperServiceState::Unavailable;
    panelState_ = EpaperPanelState::Unknown;
    recoveryRequired_ = true;
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
  snapshot.canUpload = ready_ && state_ == EpaperServiceState::Idle;
  snapshot.canDraw = snapshot.canUpload;
  const char *drawSource = state_ == EpaperServiceState::Queued
                               ? queuedSource_
                               : lastSource_;
  const bool storedDraw = strcmp(drawSource, "uploaded") == 0 ||
                          strcmp(drawSource, "refresh") == 0;
  snapshot.canDownload = ready_ && stored_.present &&
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
  snapshot.cpuMhz = frequencyDriver_ == nullptr ? 0 : frequencyDriver_->currentMhz();
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
    if (state_ != EpaperServiceState::Idle) {
      return {EpaperServiceStatusCode::Busy, "e-paper is busy"};
    }
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
    state_ = EpaperServiceState::Idle;
  }
  return queueDraw(EpaperDrawAction::Stored, "uploaded");
}

void EpaperService::abortUpload(uint32_t sessionId) {
  if (sessionId == 0 || sessionId != uploadSessionId_) return;
  storage_->abortUpload(sessionId);
  uploadSessionId_ = 0;
  setIdleAfterUploadFailure("upload_aborted");
}

EpaperServiceResult EpaperService::requestDraw(EpaperDrawAction action) {
  if (action == EpaperDrawAction::Stored) {
    const EpaperServiceResult metadata = validateStoredImage();
    if (!metadata.ok()) return metadata;
  }
  const char *source = action == EpaperDrawAction::Stored
                           ? "refresh"
                           : (action == EpaperDrawAction::White ? "white"
                                                                : "palette");
  return queueDraw(action, source);
}

EpaperServiceResult EpaperService::queueDraw(EpaperDrawAction action,
                                             const char *source) {
  if (!ready_) return {EpaperServiceStatusCode::Unavailable, "e-paper unavailable"};
  SemaphoreLock lock(mutex_);
  if (!lock.locked()) return {EpaperServiceStatusCode::Unavailable, "e-paper unavailable"};
  if (state_ != EpaperServiceState::Idle) {
    return {EpaperServiceStatusCode::Busy, "e-paper is busy"};
  }
  if (action == EpaperDrawAction::Stored && !stored_.valid) {
    return {EpaperServiceStatusCode::ImageNotFound, "stored e-paper image not found"};
  }
  state_ = EpaperServiceState::Queued;
  phase_ = EpaperDrawPhase::None;
  queuedSource_ = source;
  if (xQueueSend(queue_, &action, 0) != pdTRUE) {
    state_ = EpaperServiceState::Idle;
    queuedSource_ = "none";
    return {EpaperServiceStatusCode::Busy, "e-paper queue is full"};
  }
  return {EpaperServiceStatusCode::Ok, "draw queued"};
}

EpaperServiceResult EpaperService::refreshMetadata() {
  return validateStoredImage();
}

EpaperServiceResult EpaperService::validateStoredImage() {
  if (storage_ == nullptr) {
    return {EpaperServiceStatusCode::StorageUnavailable, "userdata is unavailable"};
  }
  const UserDataDownloadBegin begin = storage_->beginDownload(kImageName, "");
  if (!begin.result.ok()) {
    SemaphoreLock lock(mutex_);
    stored_ = {};
    if (begin.result.status == UserDataFileStatus::NotFound) {
      return {EpaperServiceStatusCode::ImageNotFound, "stored e-paper image not found"};
    }
    return fromStorage(begin.result);
  }
  EpaperImageFormat::StreamingValidator validator;
  uint8_t buffer[UserDataStorage::kFileIoChunkBytes];
  bool readOk = true;
  size_t receivedBytes = 0;
  while (receivedBytes < begin.contentLength) {
    const UserDataReadResult read =
        storage_->readDownload(begin.sessionId, buffer, sizeof(buffer));
    if (!read.result.ok() || read.bytesRead == 0) {
      readOk = false;
      break;
    }
    if (!validator.consume(buffer, read.bytesRead)) break;
    receivedBytes += read.bytesRead;
  }
  storage_->finishDownload(begin.sessionId);
  const bool valid = readOk && receivedBytes == begin.contentLength &&
                     begin.fileSize == EpaperImageFormat::kImageBytes &&
                     validator.finish();
  {
    SemaphoreLock lock(mutex_);
    stored_.present = true;
    stored_.valid = valid;
    stored_.sizeBytes = begin.fileSize;
    stored_.header = validator.header();
    stored_.validationError = valid ? EpaperImageFormat::ValidationError::None
                                    : validator.error();
  }
  if (!valid) {
    return {EpaperServiceStatusCode::InvalidImage,
            "stored EPDIMG is invalid",
            EpaperImageFormat::errorCode(validator.error())};
  }
  return {EpaperServiceStatusCode::Ok, "stored image valid"};
}

UserDataDownloadBegin EpaperService::beginImageDownload(const char *rangeHeader) {
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
    EpaperDrawAction action = EpaperDrawAction::White;
    if (xQueueReceive(queue_, &action, portMAX_DELAY) == pdTRUE) executeDraw(action);
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
    finishDraw(false, true, true, EpdDriverError::None);
    return false;
  }
  if (!driver_->begin(transport_)) {
    if (storageSessionId != 0) storage_->finishDownload(storageSessionId);
    shutdownCoordinator_->finishOperation();
    finishDraw(false, true, true, driver_->lastError());
    return false;
  }
  CpuFrequencyGuard frequencyGuard;
  if (!frequencyGuard.acquire(frequencyDriver_)) {
    if (storageSessionId != 0) storage_->finishDownload(storageSessionId);
    const bool safe = shutdownCoordinator_->finishOperation();
    finishDraw(false, safe, true, EpdDriverError::None);
    return false;
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
