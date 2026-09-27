#include "SleepFeatures.h"

#if IOT_FEATURE_SLEEP
#include "modules/status_led/StatusLed.h"
#include "SleepCoordinator.h"

#include <cstring>
#include <sys/time.h>

#include "modules/captive/CaptivePortalDnsService.h"
#include "modules/config/ConfigService.h"
#include "modules/epaper/EpaperService.h"
#include "modules/http/ApiServer.h"
#include "modules/mdns/MdnsService.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/storage/UserDataStorage.h"
#include "modules/wifi/WifiManager.h"

namespace {
constexpr uint32_t kPrepareDeadlineMs = 150000;
constexpr uint32_t kRetryMs = 60000;
constexpr uint32_t kAgendaDeadlineMs = 300000;
constexpr uint32_t kStaDeadlineMs = 15000;
constexpr uint32_t kNtpDeadlineMs = 10000;
constexpr uint32_t kDrawDeadlineMs = 150000;

enum LastResult : uint8_t {
  ResultNone = 0, ResultSuccess = 1, ResultEarly = 2,
  ResultStaFailed = 3, ResultNtpFailed = 4, ResultNoImage = 5,
  ResultEpaperUnavailable = 6, ResultDrawFailed = 7,
  ResultWatchdog = 8, ResultSuperseded = 9,
};

int64_t rawClock() {
  timeval value{};
  return gettimeofday(&value, nullptr) == 0 ? value.tv_sec : 0;
}

class WriteLock {
 public:
  explicit WriteLock(SemaphoreHandle_t mutex) : mutex_(mutex) {
    held_ = mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(1000)) == pdTRUE;
  }
  ~WriteLock() { if (held_) xSemaphoreGive(mutex_); }
  bool held() const { return held_; }
 private:
  SemaphoreHandle_t mutex_;
  bool held_ = false;
};
}  // namespace

bool SleepCoordinator::begin(SleepStore &store, TimeSource &time,
                             SleepDriver &driver,
                             const SleepRtcRecord &bootRecord,
                             bool deepSleepReset, WakeCause cause,
                             int64_t bootClock) {
  store_ = &store;
  time_ = &time;
  driver_ = &driver;
  bootClock_ = bootClock;
  writeMutex_ = xSemaphoreCreateMutex();
  if (!writeMutex_) return false;
  storageState_ = store_->load(record_);
  if (storageState_ == SleepStoreState::Error) return false;
  mode_ = classifyWake(deepSleepReset, cause, bootRecord, record_.enabled,
                       record_.scheduleGeneration);
  if (!sleepWakeEvidence(bootRecord, deepSleepReset, bootClock,
                         48U * 3600U).valid) mode_ = WakeMode::Normal;
  if (mode_ == WakeMode::WakeCycle) {
    checkpointWake_ = bootRecord.timerTargetClock < bootRecord.plannedDue;
    time.restoreCarried(bootRecord.synced);
    driver.invalidateRecord();
    if (bootRecord.basis == SleepClockBasis::Absolute) {
      due_ = {bootRecord.slotIndex, bootRecord.plannedDue};
    } else {
      relativeDueClock_ = bootRecord.plannedDue;
    }
  } else {
    driver.invalidateRecord();
    rebuildDue(false);
  }
  observedTimeRevision_ = time.snapshot().revision;
#if !SLEEP_IGNORE_USB_HOST
  usbWasConnected_ = driver.usbHostConnected();
#endif
  state_ = record_.enabled ? SleepRunState::Armed : SleepRunState::Disabled;
  ready_ = true;
  return true;
}

void SleepCoordinator::attach(ConfigService &config, EpaperService *epaper,
                              UserDataStorage *storage, WifiManager &wifi,
                              MdnsService *mdns, CaptivePortalDnsService &captive,
                              RuntimeActionScheduler &runtime, ApiServer &http) {
  config_ = &config;
  epaper_ = epaper;
  storage_ = storage;
  wifi_ = &wifi;
  mdns_ = mdns;
  captive_ = &captive;
  runtime_ = &runtime;
  http_ = &http;
  if (ready_ && record_.enabled && mode_ == WakeMode::Normal) idle_.arm(millis());
}

int64_t SleepCoordinator::clockNow() const {
  return rawClock();
}

void SleepCoordinator::rebuildDue(bool inclusive) {
  if (!record_.enabled) { due_ = {}; relativeDueClock_ = 0; return; }
  const TimeSnapshot time = time_->snapshot();
  if (time.synced()) {
    SleepSchedule::nextSlot(record_.anchorEpoch, record_.periodMinutes,
                            time.epoch, record_.lastHandledSlot, inclusive, due_);
    relativeDueClock_ = 0;
  } else {
    due_ = {};
    const int64_t base = bootClock_ > 0 ? bootClock_ : clockNow();
    relativeDueClock_ = base + static_cast<int64_t>(record_.periodMinutes) * 60;
  }
}

bool SleepCoordinator::wakeCycle() const { return mode_ == WakeMode::WakeCycle; }
bool SleepCoordinator::entering() const { return admissionClosed_; }

DeviceConfig SleepCoordinator::effectiveWifiConfig(const DeviceConfig &persisted) const {
  DeviceConfig effective = persisted;
  if (mode_ == WakeMode::WakeCycle) {
    const TimeSnapshot time = time_ ? time_->snapshot() : TimeSnapshot{};
    const bool early = time.synced() && due_.dueEpoch > 0 &&
        SleepSchedule::earlyFor(clockNow(), due_.dueEpoch);
    effective.wifiMode = record_.wakeNetworkSyncEnabled && !early &&
        (persisted.wifiMode == WifiMode::Sta || persisted.wifiMode == WifiMode::ApSta) &&
                persisted.staSsid[0] != '\0'
            ? WifiMode::Sta : WifiMode::Off;
    effective.fallbackToAp = false;
  }
  return effective;
}

void SleepCoordinator::noteActivity(uint32_t nowMs) {
  portENTER_CRITICAL(&mux_);
  ++activityGeneration_;
  activityPending_ = true;
  portEXIT_CRITICAL(&mux_);
  idle_.noteActivity(nowMs);
}

bool SleepCoordinator::beginApiRequest(bool activity, uint32_t nowMs) {
  portENTER_CRITICAL(&mux_);
  const bool accepted = !admissionClosed_;
  if (accepted) ++activeRequests_;
  portEXIT_CRITICAL(&mux_);
  if (accepted && activity) noteActivity(nowMs);
  return accepted;
}

void SleepCoordinator::endApiRequest() {
  portENTER_CRITICAL(&mux_);
  if (activeRequests_ != 0) --activeRequests_;
  portEXIT_CRITICAL(&mux_);
}

void SleepCoordinator::onTimeChanged() {
  // The next loop iteration observes the TimeSource revision. In-flight
  // absolute agenda identity is retained until it completes.
}

bool SleepCoordinator::beginTimeUpdate() {
  portENTER_CRITICAL(&mux_);
  const bool accepted = !admissionClosed_ && !timeUpdateInProgress_;
  if (accepted) timeUpdateInProgress_ = true;
  portEXIT_CRITICAL(&mux_);
  return accepted;
}

void SleepCoordinator::endTimeUpdate() {
  portENTER_CRITICAL(&mux_);
  timeUpdateInProgress_ = false;
  portEXIT_CRITICAL(&mux_);
}

bool SleepCoordinator::update(const SleepRecord &candidate,
                              SleepRecord &committed) {
  if (!ready_ || admissionClosed_) return false;
  WriteLock writer(writeMutex_);
  if (!writer.held()) return false;
  SleepRecord merged = record_;
  const bool supersedeUnsubmitted =
      agenda_ != AgendaStep::None && agenda_ != AgendaStep::WaitDraw;
  if (supersedeUnsubmitted) {
    const TimeSnapshot currentTime = time_->snapshot();
    SleepLastWake &last = merged.lastWake;
    last.cause = mode_ == WakeMode::WakeCycle ? 1 : 0;
    last.mode = mode_ == WakeMode::WakeCycle ? 2 : 1;
    last.basis = currentTime.synced() ? 1 : 2;
    last.epoch = currentTime.synced() ? currentTime.epoch : INT64_MIN;
    last.plannedDue = due_.dueEpoch > 0 ? due_.dueEpoch : INT64_MIN;
    last.driftSeconds = driftSeconds_;
    last.result = ResultSuperseded;
    last.staAttempts = staAttempts_;
    last.timeSynced = currentTime.synced();
    memset(last.lastErrorCode, 0, sizeof(last.lastErrorCode));
    for (auto &task : last.tasks) task = {};
#if IOT_FEATURE_EPAPER
    last.taskCount = 2;
#else
    last.taskCount = 1;
#endif
    last.tasks[0] = {1, 3, static_cast<uint8_t>(ResultSuperseded)};
#if IOT_FEATURE_EPAPER
    last.tasks[1] = {2, 3, static_cast<uint8_t>(ResultSuperseded)};
#endif
  }
  merged.enabled = candidate.enabled;
  merged.wakeNetworkSyncEnabled = candidate.wakeNetworkSyncEnabled;
  merged.periodMinutes = candidate.periodMinutes;
  merged.anchorEpoch = candidate.anchorEpoch;
  merged.scheduleGeneration = candidate.scheduleGeneration;
  merged.lastHandledSlot = candidate.lastHandledSlot;
  if (store_->save(merged, committed) != SleepStoreState::Ready) {
    storageState_ = store_->state();
    return false;
  }
  portENTER_CRITICAL(&mux_);
  record_ = committed;
  storageState_ = SleepStoreState::Ready;
  ++activityGeneration_;
  activityPending_ = true;
  portEXIT_CRITICAL(&mux_);
  const bool wasWakeCycle = mode_ == WakeMode::WakeCycle;
  const bool drawAccepted = agenda_ == AgendaStep::WaitDraw;
  if (!record_.enabled) {
    mode_ = WakeMode::Normal;
    if (!drawAccepted) agenda_ = AgendaStep::None;
    state_ = SleepRunState::Disabled;
    request_ = SleepRequestState::Cancelled;
    idle_.disarm();
    if (prepare_ != PrepareStep::None) cancelPrepare("superseded", millis());
  } else {
    mode_ = WakeMode::Normal;
    rebuildDue(true);
    if (!drawAccepted) agenda_ = AgendaStep::None;
    state_ = SleepRunState::Armed;
    idle_.arm(millis());
  }
  observedTimeRevision_ = time_->snapshot().revision;
  if (wasWakeCycle && runtime_) runtime_->scheduleWifiApply(0);
  return true;
}

bool SleepCoordinator::keepAwake(uint32_t nowMs) {
  portENTER_CRITICAL(&mux_);
  const bool accepted = ready_ && keepAwakeAllowed_;
  if (accepted) {
    ++activityGeneration_;
    activityPending_ = true;
  }
  portEXIT_CRITICAL(&mux_);
  if (!accepted) return false;
  idle_.noteActivity(nowMs);
  return true;
}

uint16_t SleepCoordinator::blockers(uint32_t nowMs, bool ignoreCurrentRequest) {
#if !IOT_FEATURE_EPAPER
  (void)nowMs;
#endif
  uint16_t result = 0;
  if (!ready_ || storageState_ == SleepStoreState::Error ||
      storageState_ == SleepStoreState::Recovery) result |= SleepBlockerStorage;
  if (!record_.enabled) result |= SleepBlockerNoWake;
  const TimeSnapshot time = time_->snapshot();
  const int64_t due = time.synced() ? due_.dueEpoch : relativeDueClock_;
  const int64_t remaining = due - clockNow();
  if (due <= 0 || remaining <= 5) result |= SleepBlockerNoWake;
#if !SLEEP_IGNORE_USB_HOST
  if (driver_->usbHostConnected()) result |= SleepBlockerUsb;
#endif
#if IOT_FEATURE_EPAPER
  if (epaper_) {
    const EpaperServiceSnapshot panel = epaper_->snapshot(nowMs);
    if (panel.state == EpaperServiceState::Uploading ||
        panel.state == EpaperServiceState::Queued ||
        panel.state == EpaperServiceState::Drawing) result |= SleepBlockerEpaperBusy;
    if (panel.recoveryRequired || panel.panelState == EpaperPanelState::Unknown ||
        panel.panelState == EpaperPanelState::Active) result |= SleepBlockerEpaperUnsafe;
    if (panel.recoveryRequired) result |= SleepBlockerMarkerActive;
  } else result |= SleepBlockerEpaperUnsafe;
#endif
#if IOT_FEATURE_STORAGE
  if (storage_ && storage_->operationBusy() && !storageReserved_)
    result |= SleepBlockerUpload;
#endif
  if (runtime_) {
    const RuntimeActionSnapshot actions = runtime_->snapshot();
    if (actions.restartPending) result |= SleepBlockerRestart;
    if (actions.runtimeActionPending) result |= SleepBlockerRuntime;
  }
  if (wifi_ && (wifi_->testBlocksScan() || wifi_->scanBlocksRadio()))
    result |= SleepBlockerWifi;
  portENTER_CRITICAL(&mux_);
  const uint32_t allowedRequests = ignoreCurrentRequest ? 1U : 0U;
  if (activeRequests_ > allowedRequests || timeUpdateInProgress_)
    result |= SleepBlockerRuntime;
  portEXIT_CRITICAL(&mux_);
  return result;
}

SleepSnapshot SleepCoordinator::snapshot(uint32_t nowMs) {
  SleepSnapshot value;
  value.available = ready_;
  value.record = record_;
  value.storage = storageState_;
  value.mode = mode_;
  value.state = state_;
  value.request = request_;
  value.requestError = requestError_;
  value.idleArmed = record_.enabled && mode_ == WakeMode::Normal && idle_.armed();
  if (value.idleArmed) value.idleRemainingSeconds = idle_.remainingSeconds(nowMs);
  value.time = time_->snapshot();
  value.basis = value.time.synced() ? SleepClockBasis::Absolute : SleepClockBasis::Relative;
  const int64_t due = value.time.synced() ? due_.dueEpoch : relativeDueClock_;
  value.nextWakeEpoch = value.time.synced() ? due : 0;
  const int64_t left = due - clockNow();
  value.nextWakeInSeconds = left > 0
      ? static_cast<uint32_t>(left > UINT32_MAX ? UINT32_MAX : left) : 0;
  value.blockers = blockers(nowMs);
  return value;
}

bool SleepCoordinator::requestNow(uint32_t nowMs, uint16_t &blocked) {
  // The authenticated request itself holds one API lease until this handler
  // returns. Other request leases, time updates, and pending runtime actions
  // must remain visible as blockers.
  blocked = blockers(nowMs, true);
  if (!ready_ || admissionClosed_ || blocked != 0) return false;
  if (request_ == SleepRequestState::Pending) return true;
  request_ = SleepRequestState::Pending;
  requestError_ = nullptr;
  requestStartedMs_ = nowMs;
  return true;
}

void SleepCoordinator::startAgenda(uint32_t nowMs) {
  state_ = SleepRunState::AgendaRunning;
  agendaStartedMs_ = nowMs;
  agendaGeneration_ = record_.scheduleGeneration;
  stepStartedMs_ = nowMs;
  staAttempts_ = 0;
  ntpFailed_ = false;
  staFailed_ = false;
  drawFailed_ = false;
  agendaWasWakeCycle_ = mode_ == WakeMode::WakeCycle;
  activityInterrupted_ = false;
  driftSeconds_ = INT32_MIN;
  drawOperationId_ = 0;
  if (mode_ == WakeMode::WakeCycle && record_.wakeNetworkSyncEnabled && wifi_ &&
      effectiveWifiConfig(config_->snapshot()).wifiMode == WifiMode::Sta) {
    agenda_ = AgendaStep::Sta;
    staAttempts_ = 1;  // startWifi already performed the first apply.
  } else {
    agenda_ = AgendaStep::Draw;
  }
}

void SleepCoordinator::pollAgenda(uint32_t nowMs) {
  if (static_cast<uint32_t>(nowMs - agendaStartedMs_) >= kAgendaDeadlineMs) {
    finishAgenda(nowMs, ResultWatchdog); return;
  }
  if (agenda_ == AgendaStep::Sta) {
    const WifiStatus wifi = wifi_->status();
    if (wifi.staState == WifiLinkState::Connected &&
        wifi.staIp != IPAddress(0, 0, 0, 0)) {
      if (sntp_.start()) {
        agenda_ = AgendaStep::Ntp;
        stepStartedMs_ = nowMs;
      } else { ntpFailed_ = true; agenda_ = AgendaStep::Draw; }
      return;
    }
    if (static_cast<uint32_t>(nowMs - stepStartedMs_) < kStaDeadlineMs) return;
    if (staAttempts_ >= 3) { staFailed_ = true; agenda_ = AgendaStep::Draw; return; }
    ++staAttempts_;
    WifiStatus ignored{};
    wifi_->apply(effectiveWifiConfig(config_->snapshot()), ignored);
    wifi_->applyPowerSave(true);
    stepStartedMs_ = nowMs;
    return;
  }
  if (agenda_ == AgendaStep::Ntp) {
    int64_t sample = 0;
    if (sntp_.takeSample(sample)) {
      sntp_.stop();
      if (!time_->set(sample, TimeOrigin::Ntp)) {
        ntpFailed_ = true;
      } else if (due_.dueEpoch > 0) {
        const int64_t correctedBoot =
            sample - static_cast<int64_t>(nowMs / 1000U);
        const int64_t drift = correctedBoot - due_.dueEpoch;
        if (drift >= INT32_MIN && drift <= INT32_MAX)
          driftSeconds_ = static_cast<int32_t>(drift);
      }
      if (!ntpFailed_ && due_.dueEpoch > 0 &&
          SleepSchedule::earlyFor(time_->snapshot().epoch, due_.dueEpoch)) {
        finishAgenda(nowMs, ResultEarly);
        return;
      }
      agenda_ = AgendaStep::Draw;
    } else if (static_cast<uint32_t>(nowMs - stepStartedMs_) >= kNtpDeadlineMs) {
      sntp_.stop();
      ntpFailed_ = true;
      agenda_ = AgendaStep::Draw;
    }
    return;
  }
  if (agenda_ == AgendaStep::Draw) {
#if IOT_FEATURE_EPAPER
    if (!epaper_ || !epaper_->ready()) {
      finishAgenda(nowMs, ResultEpaperUnavailable); return;
    }
    const EpaperServiceSnapshot panel = epaper_->snapshot(nowMs);
    if (!panel.stored.present || !panel.stored.valid) {
      finishAgenda(nowMs, ResultNoImage); return;
    }
    if (!panel.canDraw) {
      finishAgenda(nowMs, ResultEpaperUnavailable); return;
    }
    const EpaperServiceResult request = epaper_->requestDraw(EpaperDrawAction::Stored);
    if (!request.ok() || request.operationId == 0) {
      finishAgenda(nowMs, ResultEpaperUnavailable); return;
    }
    drawOperationId_ = request.operationId;
    agenda_ = AgendaStep::WaitDraw;
    stepStartedMs_ = nowMs;
    return;
  }
  if (agenda_ == AgendaStep::WaitDraw) {
    const EpaperServiceSnapshot panel = epaper_->snapshot(nowMs);
    if (panel.completedOperationId == drawOperationId_) {
      finishAgenda(nowMs, strcmp(panel.lastResult, "success") == 0
                              ? ResultSuccess : ResultDrawFailed);
    } else if (static_cast<uint32_t>(nowMs - stepStartedMs_) >= kDrawDeadlineMs) {
      finishAgenda(nowMs, ResultWatchdog);
    }
  }
#else
    finishAgenda(nowMs, ResultSuccess);
  }
#endif
}

void SleepCoordinator::finishAgenda(uint32_t nowMs, uint8_t result) {
  sntp_.stop();
  agenda_ = AgendaStep::None;
  WriteLock writer(writeMutex_);
  if (!writer.held()) {
    state_ = SleepRunState::Failed;
    requestError_ = "sleep_storage_error";
    return;
  }
  SleepRecord candidate = record_;
  const bool superseded = candidate.scheduleGeneration != agendaGeneration_;
  const TimeSnapshot time = time_->snapshot();
  SleepLastWake &last = candidate.lastWake;
  last.cause = agendaWasWakeCycle_ ? 1 : 0;
  last.mode = agendaWasWakeCycle_ ? 2 : 1;
  last.basis = time.synced() ? 1 : 2;
  last.epoch = time.synced() ? time.epoch : INT64_MIN;
  last.plannedDue = due_.dueEpoch > 0 ? due_.dueEpoch : INT64_MIN;
  last.driftSeconds = driftSeconds_;
  last.result = superseded ? static_cast<uint8_t>(ResultSuperseded) :
              result == ResultSuccess && staFailed_
                  ? static_cast<uint8_t>(ResultStaFailed)
              : result == ResultSuccess && ntpFailed_
                  ? static_cast<uint8_t>(ResultNtpFailed) : result;
  last.staAttempts = staAttempts_;
  last.timeSynced = time.synced();
  memset(last.lastErrorCode, 0, sizeof(last.lastErrorCode));
  const char *error = nullptr;
  if (!superseded) {
    if (result == ResultWatchdog) error = "agenda_watchdog";
    else if (result == ResultDrawFailed) error = "draw_failed";
    else if (result == ResultEpaperUnavailable) error = "epaper_unavailable";
    else if (staFailed_) error = "sta_connect_failed";
    else if (ntpFailed_) error = "ntp_timeout";
  }
  if (error) strlcpy(last.lastErrorCode, error, sizeof(last.lastErrorCode));

  uint8_t timeStatus = 2;
  uint8_t timeCode = 1;
  if (result == ResultEarly && staAttempts_ == 0) {
    timeStatus = 3;
    timeCode = ResultEarly;
  } else if (activityInterrupted_) {
    timeStatus = 3;
    timeCode = 12;
  } else if (!agendaWasWakeCycle_) {
    timeStatus = 3;
    timeCode = 11;
  } else if (!record_.wakeNetworkSyncEnabled) {
    timeStatus = 3;
    timeCode = 13;
  } else if (staAttempts_ == 0) {
    timeStatus = 3;
    timeCode = 10;
  } else if (staFailed_) {
    timeStatus = 4;
    timeCode = ResultStaFailed;
  } else if (ntpFailed_) {
    timeStatus = 4;
    timeCode = ResultNtpFailed;
  }
#if IOT_FEATURE_EPAPER
  const uint8_t panelStatus =
      result == ResultSuccess ? 2 :
      (result == ResultEarly || result == ResultNoImage ? 3 : 4);
  last.taskCount = 2;
  last.tasks[0] = {1, timeStatus, timeCode};
  last.tasks[1] = {2, panelStatus, result};
#else
  last.taskCount = 1;
  for (auto &task : last.tasks) task = {};
  last.tasks[0] = {1, timeStatus, timeCode};
#endif

  const bool failed = staFailed_ || ntpFailed_ || result == ResultDrawFailed ||
                      result == ResultEpaperUnavailable || result == ResultWatchdog;
  if (!superseded && failed && last.consecutiveFailures != UINT16_MAX)
    ++last.consecutiveFailures;
  else if (!superseded && result == ResultSuccess)
    last.consecutiveFailures = 0;
  if (!superseded && result != ResultEarly && time.synced() && due_.index >= 0)
    candidate.lastHandledSlot = due_.index;

  SleepRecord committed;
  if (store_->save(candidate, committed) != SleepStoreState::Ready) {
    storageState_ = SleepStoreState::Error;
    state_ = SleepRunState::Failed;
    requestError_ = "sleep_storage_error";
    return;
  }
  record_ = committed;
  storageState_ = SleepStoreState::Ready;
  if (superseded) {
    rebuildDue(false);
  } else if (result == ResultEarly) {
    // Retain the same planned slot and sleep toward it again.
  } else if (time.synced()) {
    SleepSchedule::advancePast(record_.anchorEpoch, record_.periodMinutes,
                               time.epoch, record_.lastHandledSlot, due_);
  } else {
    SleepSchedule::Slot next;
    if (SleepSchedule::advancePast(relativeDueClock_, record_.periodMinutes,
                                   clockNow(), 0, next))
      relativeDueClock_ = next.dueEpoch;
  }
  state_ = record_.enabled ? SleepRunState::Armed : SleepRunState::Disabled;
  if (mode_ == WakeMode::WakeCycle) beginPrepare(nowMs);
}

void SleepCoordinator::beginPrepare(uint32_t nowMs) {
  if (prepare_ != PrepareStep::None || admissionClosed_) return;
  const uint16_t blocked = blockers(nowMs);
  if (blocked != 0) {
    if (request_ == SleepRequestState::Pending) requestError_ = "sleep_blocked";
    return;
  }
  portENTER_CRITICAL(&mux_);
  if (activeRequests_ != 0 || timeUpdateInProgress_) {
    portEXIT_CRITICAL(&mux_); return;
  }
  admissionClosed_ = true;
  keepAwakeAllowed_ = true;
  prepareGeneration_ = activityGeneration_;
  portEXIT_CRITICAL(&mux_);
#if IOT_FEATURE_EPAPER
  if (!epaper_->requestSleep(nowMs)) {
    portENTER_CRITICAL(&mux_);
    admissionClosed_ = false;
    keepAwakeAllowed_ = true;
    portEXIT_CRITICAL(&mux_);
    return;
  }
#endif
#if IOT_FEATURE_STATUS_LED
  if (statusLed_ && !statusLed_->setNormal(false)) {
    cancelPrepare("status_led_off_failed", nowMs);
    return;
  }
#endif
  prepare_ = PrepareStep::Epaper;
  requestStartedMs_ = nowMs;
  request_ = SleepRequestState::Entering;
  state_ = SleepRunState::Entering;
}

void SleepCoordinator::cancelPrepare(const char *error, uint32_t nowMs) {
  sntp_.stop();
  driver_->invalidateRecord();
  bool hardwareRestored = true;
  if (pinsHeld_) {
    if (driver_->releaseEpaperPins()) pinsHeld_ = false;
    else hardwareRestored = false;
  }
  if (timerArmed_) {
    if (driver_->disarmTimer()) timerArmed_ = false;
    else hardwareRestored = false;
  }
#if IOT_FEATURE_STATUS_LED
  if (statusLed_ && !statusLed_->cancelSleep()) hardwareRestored = false;
#endif
  hardwareRecoveryFailed_ = !hardwareRestored;
#if IOT_FEATURE_STORAGE
  if (storageReserved_) {
    if (!storage_->cancelSleep()) {
      storageRecoveryFailed_ = true;
      portENTER_CRITICAL(&mux_);
      keepAwakeAllowed_ = false;
      portEXIT_CRITICAL(&mux_);
      prepare_ = PrepareStep::None;
      request_ = SleepRequestState::Failed;
      requestError_ = "storage_restore_failed";
      state_ = SleepRunState::Failed;
      retryAfterMs_ = nowMs;
      return;
    }
    storageReserved_ = false;
  }
#endif
  storageRecoveryFailed_ = false;
#if IOT_FEATURE_EPAPER
  if (epaper_) epaper_->cancelSleep();
#endif
  bool servicesRestored = true;
  if (http_ && !http_->started() && !http_->resumeAfterSleep())
    servicesRestored = false;
  if (wifi_ && config_) {
    WifiStatus status{};
    const DeviceConfig restored = effectiveWifiConfig(config_->snapshot());
    if (!wifi_->apply(restored, status).ok()) servicesRestored = false;
    if (!wifi_->applyPowerSave(mode_ == WakeMode::WakeCycle).ok())
      servicesRestored = false;
    if (mode_ == WakeMode::Normal) {
#if IOT_FEATURE_MDNS
      if (mdns_ && !mdns_->restart(restored, status).ok())
        servicesRestored = false;
#endif
      if (captive_ && !captive_->restart(status).ok())
        servicesRestored = false;
    }
  }
  if (!servicesRestored || hardwareRecoveryFailed_) {
    serviceRecoveryFailed_ = !servicesRestored;
    portENTER_CRITICAL(&mux_);
    keepAwakeAllowed_ = false;
    portEXIT_CRITICAL(&mux_);
    prepare_ = PrepareStep::None;
    request_ = SleepRequestState::Failed;
    requestError_ = hardwareRecoveryFailed_ ? "hardware_restore_failed"
                                            : "runtime_restore_failed";
    state_ = SleepRunState::Failed;
    retryAfterMs_ = nowMs;
    return;
  }
  serviceRecoveryFailed_ = false;
  hardwareRecoveryFailed_ = false;
  portENTER_CRITICAL(&mux_);
  admissionClosed_ = false;
  keepAwakeAllowed_ = true;
  portEXIT_CRITICAL(&mux_);
  prepare_ = PrepareStep::None;
  request_ = strcmp(error, "activity") == 0 || strcmp(error, "superseded") == 0
                 ? SleepRequestState::Cancelled : SleepRequestState::Failed;
  requestError_ = error;
  state_ = record_.enabled ? SleepRunState::Armed : SleepRunState::Disabled;
  retryAfterMs_ = nowMs;
}

void SleepCoordinator::pollPrepare(uint32_t nowMs) {
  if (static_cast<uint32_t>(nowMs - requestStartedMs_) >= kPrepareDeadlineMs) {
    cancelPrepare("prepare_timeout", nowMs); return;
  }
  portENTER_CRITICAL(&mux_);
  const bool changed = activityGeneration_ != prepareGeneration_;
  const bool busy = activeRequests_ != 0;
  portEXIT_CRITICAL(&mux_);
  if (changed || busy || (runtime_ && runtime_->snapshot().restartPending)) {
    cancelPrepare(changed ? "activity" : "runtime_action_pending", nowMs);
    return;
  }
  if (prepare_ == PrepareStep::Epaper) {
#if IOT_FEATURE_EPAPER
    if (!epaper_->sleepReady()) return;
#endif
    prepare_ = PrepareStep::Storage;
  }
  if (prepare_ == PrepareStep::Storage) {
#if IOT_FEATURE_STORAGE
    if (!storage_->reserveSleep()) return;
    storageReserved_ = true;
#endif
    prepare_ = PrepareStep::Teardown;
  }
  if (prepare_ != PrepareStep::Teardown) return;
  const TimeSnapshot time = time_->snapshot();
  const int64_t now = clockNow();
  const int64_t due = time.synced() ? due_.dueEpoch : relativeDueClock_;
  const uint32_t duration = SleepSchedule::timerSecondsUntil(now, due,
                                                              record_.periodMinutes);
  if (duration <= 5) { cancelPrepare("due_imminent", nowMs); return; }

  const bool restartPending = runtime_ && runtime_->snapshot().restartPending;
  portENTER_CRITICAL(&mux_);
  const bool finalChanged = activityGeneration_ != prepareGeneration_;
  const bool finalBusy = activeRequests_ != 0 || timeUpdateInProgress_;
  if (!finalChanged && !finalBusy && !restartPending)
    keepAwakeAllowed_ = false;
  portEXIT_CRITICAL(&mux_);
  if (finalChanged || finalBusy || restartPending) {
    cancelPrepare(finalChanged ? "activity" : "runtime_action_pending", nowMs);
    return;
  }

  if (!driver_->armTimer(duration)) { cancelPrepare("timer_arm_failed", nowMs); return; }
  timerArmed_ = true;
  if (!driver_->holdEpaperPins()) {
    // A partial hold may have succeeded. Release all configured pins
    // conservatively and require the adapter to prove recovery.
    pinsHeld_ = true;
    cancelPrepare("pin_hold_failed", nowMs);
    return;
  }
  pinsHeld_ = true;
#if IOT_FEATURE_STATUS_LED
  if (statusLed_ && !statusLed_->prepareSleep()) {
    cancelPrepare("status_led_hold_failed", nowMs); return;
  }
#endif
  SleepRtcRecord rtc;
  rtc.scheduleGeneration = record_.scheduleGeneration;
  rtc.basis = time.synced() ? SleepClockBasis::Absolute : SleepClockBasis::Relative;
  rtc.intent = true;
  rtc.synced = time.synced();
  rtc.plannedDue = due;
  rtc.slotIndex = time.synced() ? due_.index : -1;
  rtc.lastHandledSlot = record_.lastHandledSlot;
  rtc.timerTargetClock = now + duration;
  rtc.sleepEnteredClock = clockNow();
  sntp_.stop();
#if IOT_FEATURE_MDNS
  if (mdns_) mdns_->stop();
#endif
  if (captive_) captive_->stop();
  if (http_) http_->stopForSleep();
  WifiStatus status{};
  DeviceConfig off = config_->snapshot();
  off.wifiMode = WifiMode::Off;
  if (!wifi_->apply(off, status).ok()) {
    cancelPrepare("wifi_stop_failed", nowMs); return;
  }
#if IOT_FEATURE_STORAGE
  if (!storage_->unmountForSleep()) {
    cancelPrepare("storage_unmount_failed", nowMs); return;
  }
#endif
  rtc.sleepEnteredClock = clockNow();
  driver_->writeRecord(rtc);
  driver_->drainSerial(50);
  driver_->deepSleep();
  deepSleepReturned_ = true;
  request_ = SleepRequestState::Failed;
  requestError_ = "deep_sleep_returned";
  state_ = SleepRunState::Failed;
}

void SleepCoordinator::poll(uint32_t nowMs) {
  if (!ready_ || !config_ ||
#if IOT_FEATURE_EPAPER
      !epaper_ ||
#endif
#if IOT_FEATURE_STORAGE
      !storage_ ||
#endif
      !wifi_ || !runtime_ || !http_ || deepSleepReturned_) return;
  if (storageRecoveryFailed_ || serviceRecoveryFailed_ ||
      hardwareRecoveryFailed_) {
    cancelPrepare(storageRecoveryFailed_ ? "storage_restore_failed" :
                  hardwareRecoveryFailed_ ? "hardware_restore_failed"
                                          : "runtime_restore_failed", nowMs);
    return;
  }
#if !SLEEP_IGNORE_USB_HOST
  const bool usbConnected = driver_->usbHostConnected();
  if (usbWasConnected_ && !usbConnected && record_.enabled &&
      mode_ == WakeMode::Normal) {
    idle_.arm(nowMs);
    retryAfterMs_ = nowMs;
  }
  usbWasConnected_ = usbConnected;
#endif
  portENTER_CRITICAL(&mux_);
  const bool activity = activityPending_;
  activityPending_ = false;
  portEXIT_CRITICAL(&mux_);
  if (activity) {
    if (mode_ == WakeMode::WakeCycle) {
      mode_ = WakeMode::Normal;
      idle_.arm(nowMs);
      runtime_->scheduleWifiApply(0);
    }
    sntp_.stop();
    if (agenda_ == AgendaStep::Sta || agenda_ == AgendaStep::Ntp) {
      activityInterrupted_ = true;
      agenda_ = AgendaStep::Draw;
    }
  }
  portENTER_CRITICAL(&mux_);
  const bool updateInProgress = timeUpdateInProgress_;
  portEXIT_CRITICAL(&mux_);
  if (updateInProgress) return;
  if (!record_.enabled && agenda_ != AgendaStep::WaitDraw) return;
  if (prepare_ != PrepareStep::None) { pollPrepare(nowMs); return; }
  const TimeSnapshot time = time_->snapshot();
  if (time.revision != observedTimeRevision_) {
    observedTimeRevision_ = time.revision;
    if (agenda_ == AgendaStep::None && mode_ == WakeMode::Normal) {
      if (time.synced() && due_.dueEpoch == 0) rebuildDue(false);
      else if (!time.synced() && relativeDueClock_ == 0) {
        due_ = {};
        relativeDueClock_ = clockNow() +
            static_cast<int64_t>(record_.periodMinutes) * 60;
      }
    }
  }
  if (agenda_ != AgendaStep::None) { pollAgenda(nowMs); return; }
  const int64_t now = clockNow();
  const int64_t due = time.synced() ? due_.dueEpoch : relativeDueClock_;
  if (mode_ == WakeMode::WakeCycle && time.synced() &&
      SleepSchedule::earlyFor(now, due)) {
    if (checkpointWake_) {
      beginPrepare(nowMs);
    } else {
      startAgenda(nowMs);
      finishAgenda(nowMs, ResultEarly);
    }
    return;
  }
  if (due > 0 && (now >= due ||
      (mode_ == WakeMode::WakeCycle && due - now <= 60))) {
    startAgenda(nowMs); return;
  }
  if (request_ == SleepRequestState::Pending &&
      static_cast<uint32_t>(nowMs - requestStartedMs_) >= 500U) {
    beginPrepare(nowMs); return;
  }
  if (mode_ == WakeMode::WakeCycle ||
      (idle_.expired(nowMs) && static_cast<uint32_t>(nowMs - retryAfterMs_) >= kRetryMs)) {
    beginPrepare(nowMs);
  }
}
#endif
