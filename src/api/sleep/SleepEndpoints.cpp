#include "modules/sleep/SleepFeatures.h"

#if ENABLE_SLEEP_SCHEDULER
#include "SleepEndpoints.h"

#include <cstring>
#include <limits>

#include "api/shared/ApiResponse.h"
#include "api/shared/JsonReader.h"
#include "modules/sleep/SleepSchedule.h"

namespace {
bool hasKey(JsonObjectConst root, const char *name) {
  for (JsonPairConst pair : root) {
    if (strcmp(pair.key().c_str(), name) == 0) return true;
  }
  return false;
}

const char *storageName(SleepStoreState state) {
  switch (state) {
    case SleepStoreState::Empty: return "ok";
    case SleepStoreState::Ready: return "ok";
    case SleepStoreState::Recovery: return "recovery";
    case SleepStoreState::Error: return "error";
  }
  return "error";
}

const char *stateName(SleepRunState state) {
  switch (state) {
    case SleepRunState::Disabled: return "disabled";
    case SleepRunState::Armed: return "armed";
    case SleepRunState::AgendaRunning: return "agenda_running";
    case SleepRunState::Entering: return "entering";
    case SleepRunState::Failed: return "failed";
  }
  return "failed";
}

const char *requestName(SleepRequestState state) {
  switch (state) {
    case SleepRequestState::None: return "none";
    case SleepRequestState::Pending: return "pending";
    case SleepRequestState::Entering: return "entering";
    case SleepRequestState::Cancelled: return "cancelled";
    case SleepRequestState::Failed: return "failed";
  }
  return "failed";
}

const char *resultName(uint8_t value) {
  constexpr const char *names[] = {"none", "success", "early_wake",
      "skipped_sta_failed", "skipped_ntp_failed", "skipped_no_image",
      "skipped_epaper_unavailable", "draw_failed", "watchdog_forced",
      "superseded"};
  return value < sizeof(names) / sizeof(names[0]) ? names[value] : "none";
}

const char *taskName(uint8_t value) {
  return value == 1 ? "time_sync" : value == 2 ? "panel_refresh" : "unknown";
}

const char *taskStatus(uint8_t value) {
  return value == 1 ? "pending" : value == 2 ? "done" :
         value == 3 ? "skipped" : value == 4 ? "failed" : "pending";
}

const char *taskCode(uint8_t value) {
  if (value == 1) return "success";
  if (value == 10) return "skipped_no_sta";
  if (value == 11) return "skipped_normal_mode";
  if (value == 12) return "interrupted_by_activity";
  return resultName(value);
}

void writeBlockers(JsonArray blockers, uint16_t flags) {
  struct Entry { uint16_t bit; const char *name; };
  constexpr Entry entries[] = {
      {SleepBlockerEpaperBusy, "epaper_busy"},
      {SleepBlockerEpaperUnsafe, "epaper_unsafe"},
      {SleepBlockerMarkerActive, "epaper_marker_active"},
      {SleepBlockerUpload, "upload_active"},
      {SleepBlockerRestart, "restart_pending"},
      {SleepBlockerWifi, "wifi_transition"},
      {SleepBlockerRuntime, "runtime_action_pending"},
      {SleepBlockerNoWake, "no_wake_source"},
      {SleepBlockerUsb, "usb_host_connected"},
      {SleepBlockerStorage, "sleep_storage_error"},
  };
  for (const Entry &entry : entries) {
    if (flags & entry.bit) blockers.add(entry.name);
  }
}

void writeIdle(JsonObject object, const SleepSnapshot &snapshot) {
  object["timeout_seconds"] = SLEEP_IDLE_TIMEOUT_SECONDS;
  object["armed"] = snapshot.idleArmed;
  if (snapshot.idleArmed) object["remaining_seconds"] = snapshot.idleRemainingSeconds;
  else object["remaining_seconds"] = nullptr;
}

Api::Response snapshotResponse(SleepCoordinator &sleep) {
  const SleepSnapshot snapshot = sleep.snapshot(millis());
  if (!snapshot.available) {
    return Api::problem(503, "runtime_unavailable", "sleep service unavailable");
  }
  JsonDocument data;
  data["enabled"] = snapshot.record.enabled;
  data["mode"] = snapshot.mode == WakeMode::WakeCycle ? "wake_cycle" : "normal";
  data["state"] = stateName(snapshot.state);
  data["storage_state"] = storageName(snapshot.storage);
  writeIdle(data["idle"].to<JsonObject>(), snapshot);
  JsonObject schedule = data["schedule"].to<JsonObject>();
  if (snapshot.record.enabled) {
    schedule["period_hours"] = snapshot.record.periodHours;
    if (snapshot.record.anchorEpoch > 0)
      schedule["anchor_epoch"] = snapshot.record.anchorEpoch;
    else schedule["anchor_epoch"] = nullptr;
    schedule["clock_basis"] = snapshot.basis == SleepClockBasis::Absolute
        ? "absolute" : "relative";
    if (snapshot.basis == SleepClockBasis::Absolute)
      schedule["next_wake_epoch"] = snapshot.nextWakeEpoch;
    else schedule["next_wake_epoch"] = nullptr;
    schedule["next_wake_in_seconds"] = snapshot.nextWakeInSeconds;
  } else {
    schedule["period_hours"] = nullptr;
    schedule["anchor_epoch"] = nullptr;
    schedule["clock_basis"] = nullptr;
    schedule["next_wake_epoch"] = nullptr;
    schedule["next_wake_in_seconds"] = nullptr;
  }
  JsonObject time = data["time"].to<JsonObject>();
  if (snapshot.time.synced()) time["epoch"] = snapshot.time.epoch;
  else time["epoch"] = nullptr;
  time["synced"] = snapshot.time.synced();
  time["source"] = timeOriginToString(snapshot.time.origin);
  const SleepLastWake &last = snapshot.record.lastWake;
  JsonObject wake = data["last_wake"].to<JsonObject>();
  wake["cause"] = last.cause == 1 ? "timer" : "none";
  if (last.mode != 0) wake["mode"] = last.mode == 2 ? "wake_cycle" : "normal";
  else wake["mode"] = nullptr;
  if (last.basis != 0) wake["clock_basis"] = last.basis == 1 ? "absolute" : "relative";
  else wake["clock_basis"] = nullptr;
  if (last.epoch != INT64_MIN) wake["epoch"] = last.epoch;
  else wake["epoch"] = nullptr;
  if (last.plannedDue != INT64_MIN) wake["planned_due_epoch"] = last.plannedDue;
  else wake["planned_due_epoch"] = nullptr;
  if (last.driftSeconds != INT32_MIN) wake["drift_seconds"] = last.driftSeconds;
  else wake["drift_seconds"] = nullptr;
  wake["result"] = resultName(last.result);
  wake["consecutive_failures"] = last.consecutiveFailures;
  wake["sta_attempts"] = last.staAttempts;
  wake["time_synced"] = last.timeSynced;
  if (last.lastErrorCode[0]) wake["last_error_code"] = last.lastErrorCode;
  else wake["last_error_code"] = nullptr;
  JsonArray tasks = wake["tasks"].to<JsonArray>();
  for (uint8_t index = 0; index < last.taskCount; ++index) {
    JsonObject task = tasks.add<JsonObject>();
    task["name"] = taskName(last.tasks[index].name);
    task["status"] = taskStatus(last.tasks[index].status);
    task["code"] = taskCode(last.tasks[index].code);
  }
  JsonObject request = data["sleep_request"].to<JsonObject>();
  request["state"] = requestName(snapshot.request);
  if (snapshot.requestError) request["error_code"] = snapshot.requestError;
  else request["error_code"] = nullptr;
  writeBlockers(data["blockers"].to<JsonArray>(), snapshot.blockers);
  return Api::ok(Api::json(data));
}

bool restoreClock(TimeSource &time, const TimeSnapshot &previous,
                  uint32_t startedMs) {
  if (!previous.synced()) {
    time.clear();
    return !time.snapshot().synced();
  }
  const int64_t restored = previous.epoch +
      static_cast<uint32_t>(millis() - startedMs) / 1000U;
  if (time.set(restored, previous.origin)) return true;
  time.clear();
  return false;
}

Api::Response validateEmpty(const Api::Request &request) {
  if (!request.hasBody) return Api::ok("{}");
  JsonObjectConst root;
  const Api::Response result = ApiRequest::requireObject(request, root);
  if (!result.success) return result;
  JsonDecodeError error;
  JsonReader reader(root, "", error);
  reader.finish({});
  return error.ok() ? Api::ok("{}") : Api::decodeError(error);
}
}  // namespace

Api::Response SleepEndpoints::get(SleepCoordinator &sleep) {
  return snapshotResponse(sleep);
}

Api::Response SleepEndpoints::update(const Api::Request &request,
                                     SleepCoordinator &sleep, TimeSource &time) {
  JsonObjectConst root;
  const Api::Response body = ApiRequest::requireObject(request, root);
  if (!body.success) return body;
  JsonDecodeError error;
  JsonReader reader(root, "", error);
  const bool enabled = reader.requiredBool("enabled", "enabled is required");
  const bool periodPresent = hasKey(root, "period_hours");
  const bool delayPresent = hasKey(root, "first_wake_delay_minutes");
  const bool timePresent = hasKey(root, "client_time");
  uint32_t period = 0, delay = 0, epoch = 0;
  if (enabled || periodPresent) period = reader.requiredUint32("period_hours", "period_hours is required");
  if (enabled || delayPresent) delay = reader.requiredUint32("first_wake_delay_minutes", "first_wake_delay_minutes is required");
  if (enabled || timePresent) epoch = reader.requiredUint32("client_time", "client_time is required");
  reader.finish({"enabled", "period_hours", "first_wake_delay_minutes", "client_time"});
  if (!error.ok()) return Api::decodeError(error);
  if (!enabled && (periodPresent || delayPresent || timePresent) &&
      !(periodPresent && delayPresent && timePresent)) {
    return Api::problem(400, "missing_field", "all schedule fields are required together");
  }
  if (enabled || periodPresent) {
    if (!SleepSchedule::validPeriodHours(period))
      return Api::problem(400, "invalid_field", "period_hours must be 12, 24, or 48", "period_hours");
    if (!SleepSchedule::validDelayMinutes(period, delay))
      return Api::problem(400, "invalid_field", "invalid first wake delay", "first_wake_delay_minutes");
    if (!SleepSchedule::validClientEpoch(epoch))
      return Api::problem(400, "invalid_field", "client_time is outside supported range", "client_time");
  }
  if (sleep.entering()) return Api::problem(409, "sleep_entering", "sleep entry is in progress");
  if (!sleep.beginTimeUpdate())
    return Api::problem(409, "sleep_entering", "sleep entry or clock update is in progress");
  struct TimeUpdateLease {
    SleepCoordinator &owner;
    ~TimeUpdateLease() { owner.endTimeUpdate(); }
  } lease{sleep};
  const SleepSnapshot current = sleep.snapshot(millis());
  if (!current.available) return Api::problem(503, "runtime_unavailable", "sleep service unavailable");
  SleepRecord candidate = current.record;
  candidate.enabled = enabled;
  if (candidate.scheduleGeneration == UINT64_MAX) {
    return Api::problem(500, "storage_error", "schedule generation exhausted");
  }
  const TimeSnapshot oldTime = time.snapshot();
  const uint32_t oldAtMs = millis();
  if (enabled || timePresent) {
    if (!time.set(epoch, TimeOrigin::Client)) {
      const bool restored = restoreClock(time, oldTime, oldAtMs);
      return Api::problem(500, "time_error",
                          restored ? "failed to set system clock"
                                   : "clock update and rollback failed");
    }
    const TimeSnapshot updatedTime = time.snapshot();
    if (!updatedTime.synced() || updatedTime.epoch < epoch ||
        updatedTime.epoch > static_cast<int64_t>(epoch) + 1) {
      const bool restored = restoreClock(time, oldTime, oldAtMs);
      return Api::problem(500, "time_error",
                          restored ? "failed to verify system clock"
                                   : "clock verification and rollback failed");
    }
    int64_t anchor = 0;
    if (!SleepSchedule::anchorFor(epoch, delay, anchor)) {
      restoreClock(time, oldTime, oldAtMs);
      return Api::problem(400, "invalid_field", "invalid first wake delay",
                          "first_wake_delay_minutes");
    }
    candidate.periodHours = period;
    candidate.anchorEpoch = anchor;
  }
  ++candidate.scheduleGeneration;
  candidate.lastHandledSlot = -1;
  SleepRecord committed;
  if (!sleep.update(candidate, committed)) {
    if (!restoreClock(time, oldTime, oldAtMs))
      return Api::problem(500, "time_error", "clock rollback failed");
    return Api::problem(500, "storage_error", "failed to save sleep schedule");
  }
  return snapshotResponse(sleep);
}

Api::Response SleepEndpoints::keepAwake(const Api::Request &request,
                                        SleepCoordinator &sleep) {
  const Api::Response valid = validateEmpty(request);
  if (!valid.success) return valid;
  if (!sleep.keepAwake(millis()))
    return Api::problem(409, "sleep_entering", "sleep entry is in progress");
  const SleepSnapshot snapshot = sleep.snapshot(millis());
  JsonDocument data;
  writeIdle(data["idle"].to<JsonObject>(), snapshot);
  return Api::ok(Api::json(data));
}

Api::Response SleepEndpoints::now(const Api::Request &request,
                                  SleepCoordinator &sleep) {
  const Api::Response valid = validateEmpty(request);
  if (!valid.success) return valid;
  uint16_t blocked = 0;
  if (!sleep.requestNow(millis(), blocked)) {
    JsonDocument data;
    data["code"] = blocked & SleepBlockerNoWake ? "no_wake_source" : "sleep_blocked";
    writeBlockers(data["blockers"].to<JsonArray>(), blocked);
    return Api::error(409, Api::json(data), "sleep blocked");
  }
  JsonDocument data;
  data["sleep_request"]["state"] = "pending";
  return Api::accepted(Api::json(data), "sleep scheduled");
}
#endif
