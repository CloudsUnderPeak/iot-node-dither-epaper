#pragma once

#include "SleepFeatures.h"

#if IOT_FEATURE_SLEEP

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "IdleTimer.h"
#include "SleepDriver.h"
#include "SleepSchedule.h"
#include "SleepStore.h"
#include "SntpClient.h"
#include "modules/time/TimeSource.h"
#include "modules/config/model/DeviceConfig.h"

class ConfigService;
class EpaperService;
class UserDataStorage;
class WifiManager;
class MdnsService;
class CaptivePortalDnsService;
class RuntimeActionScheduler;
class ApiServer;

enum class SleepRunState : uint8_t { Disabled, Armed, AgendaRunning, Entering, Failed };
enum class SleepRequestState : uint8_t { None, Pending, Entering, Cancelled, Failed };

struct SleepSnapshot {
  bool available = false;
  SleepRecord record;
  SleepStoreState storage = SleepStoreState::Empty;
  WakeMode mode = WakeMode::Normal;
  SleepRunState state = SleepRunState::Disabled;
  SleepRequestState request = SleepRequestState::None;
  const char *requestError = nullptr;
  bool idleArmed = false;
  uint32_t idleRemainingSeconds = 0;
  SleepClockBasis basis = SleepClockBasis::Relative;
  int64_t nextWakeEpoch = 0;
  uint32_t nextWakeInSeconds = 0;
  TimeSnapshot time;
  uint16_t blockers = 0;
};

enum SleepBlocker : uint16_t {
  SleepBlockerEpaperBusy = 1U << 0,
  SleepBlockerEpaperUnsafe = 1U << 1,
  SleepBlockerMarkerActive = 1U << 2,
  SleepBlockerUpload = 1U << 3,
  SleepBlockerRestart = 1U << 4,
  SleepBlockerWifi = 1U << 5,
  SleepBlockerRuntime = 1U << 6,
  SleepBlockerNoWake = 1U << 7,
  SleepBlockerUsb = 1U << 8,
  SleepBlockerStorage = 1U << 9,
};

class SleepCoordinator {
 public:
  bool begin(SleepStore &store, TimeSource &time, SleepDriver &driver,
             const SleepRtcRecord &bootRecord, bool deepSleepReset,
             WakeCause cause, int64_t bootClock);
  void attach(ConfigService &config, EpaperService *epaper, UserDataStorage *storage,
              WifiManager &wifi, MdnsService *mdns, CaptivePortalDnsService &captive,
              RuntimeActionScheduler &runtime, ApiServer &http);
  void attach(ConfigService &config, EpaperService &epaper, UserDataStorage &storage,
              WifiManager &wifi, MdnsService &mdns, CaptivePortalDnsService &captive,
              RuntimeActionScheduler &runtime, ApiServer &http) {
    attach(config, &epaper, &storage, wifi, &mdns, captive, runtime, http);
  }
  void poll(uint32_t nowMs);
  SleepSnapshot snapshot(uint32_t nowMs);
  bool keepAwake(uint32_t nowMs);
  bool requestNow(uint32_t nowMs, uint16_t &blockers);
  bool update(const SleepRecord &candidate, SleepRecord &committed);
  void noteActivity(uint32_t nowMs);
  bool beginApiRequest(bool activity, uint32_t nowMs);
  void endApiRequest();
  bool wakeCycle() const;
  DeviceConfig effectiveWifiConfig(const DeviceConfig &persisted) const;
  void onTimeChanged();
  bool beginTimeUpdate();
  void endTimeUpdate();
  bool entering() const;

 private:
  enum class AgendaStep : uint8_t { None, Sta, Ntp, Draw, WaitDraw };
  enum class PrepareStep : uint8_t { None, Epaper, Storage, Teardown };
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  SemaphoreHandle_t writeMutex_ = nullptr;
  SleepStore *store_ = nullptr;
  TimeSource *time_ = nullptr;
  SleepDriver *driver_ = nullptr;
  ConfigService *config_ = nullptr;
  EpaperService *epaper_ = nullptr;
  UserDataStorage *storage_ = nullptr;
  WifiManager *wifi_ = nullptr;
  MdnsService *mdns_ = nullptr;
  CaptivePortalDnsService *captive_ = nullptr;
  RuntimeActionScheduler *runtime_ = nullptr;
  ApiServer *http_ = nullptr;
  SleepRecord record_;
  SleepStoreState storageState_ = SleepStoreState::Empty;
  WakeMode mode_ = WakeMode::Normal;
  SleepRunState state_ = SleepRunState::Disabled;
  SleepRequestState request_ = SleepRequestState::None;
  const char *requestError_ = nullptr;
  IdleTimer idle_{SLEEP_IDLE_TIMEOUT_SECONDS};
  SleepSchedule::Slot due_{};
  int64_t relativeDueClock_ = 0;
  int64_t bootClock_ = 0;
  uint32_t observedTimeRevision_ = 0;
  uint32_t activityGeneration_ = 0;
  uint32_t prepareGeneration_ = 0;
  uint32_t activeRequests_ = 0;
  uint32_t requestStartedMs_ = 0;
  uint32_t retryAfterMs_ = 0;
  bool activityPending_ = false;
  bool timeUpdateInProgress_ = false;
  bool ready_ = false;
  bool admissionClosed_ = false;
  bool keepAwakeAllowed_ = true;
  AgendaStep agenda_ = AgendaStep::None;
  PrepareStep prepare_ = PrepareStep::None;
  uint32_t agendaStartedMs_ = 0;
  uint64_t agendaGeneration_ = 0;
  uint32_t stepStartedMs_ = 0;
  uint8_t staAttempts_ = 0;
  uint32_t drawOperationId_ = 0;
  bool ntpFailed_ = false;
  bool staFailed_ = false;
  bool drawFailed_ = false;
  bool agendaWasWakeCycle_ = false;
  bool activityInterrupted_ = false;
  int32_t driftSeconds_ = INT32_MIN;
  bool storageReserved_ = false;
  bool timerArmed_ = false;
  bool pinsHeld_ = false;
  bool storageRecoveryFailed_ = false;
  bool serviceRecoveryFailed_ = false;
  bool hardwareRecoveryFailed_ = false;
  bool deepSleepReturned_ = false;
  bool checkpointWake_ = false;
  bool usbWasConnected_ = false;
  SntpClient sntp_;

  int64_t clockNow() const;
  void rebuildDue(bool inclusive);
  void startAgenda(uint32_t nowMs);
  void pollAgenda(uint32_t nowMs);
  void finishAgenda(uint32_t nowMs, uint8_t result);
  void beginPrepare(uint32_t nowMs);
  void pollPrepare(uint32_t nowMs);
  void cancelPrepare(const char *error, uint32_t nowMs);
  uint16_t blockers(uint32_t nowMs, bool ignoreCurrentRequest = false);
};

#endif
