#include <ArduinoJson.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "api/sleep/SleepEndpoints.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

class FakeTime final : public TimeSource {
 public:
  TimeSnapshot value{1799996400, TimeOrigin::Client, 1};
  uint32_t setCalls = 0;
  uint32_t clearCalls = 0;
  uint32_t failSetMask = 0;
  uint32_t partialFailMask = 0;
  uint32_t invalidSnapshotAt = 0;
  mutable uint32_t snapshotCalls = 0;

  TimeSnapshot snapshot() const override {
    ++snapshotCalls;
    if (invalidSnapshotAt == snapshotCalls) return {};
    return value;
  }

  bool set(int64_t epoch, TimeOrigin origin) override {
    ++setCalls;
    const uint32_t bit = setCalls < 32 ? (1U << setCalls) : 0;
    if (partialFailMask & bit) {
      value = {epoch, TimeOrigin::None, value.revision + 1};
      return false;
    }
    if (failSetMask & bit) return false;
    value = {epoch, origin, value.revision + 1};
    return true;
  }

  void clear() override {
    ++clearCalls;
    value = {0, TimeOrigin::None, value.revision + 1};
  }
};

Api::Request jsonRequest(JsonDocument &document, const char *json) {
  const DeserializationError error = deserializeJson(document, json);
  expect(!error, "test JSON must parse");
  Api::Request request;
  request.method = Api::Method::Put;
  request.path = "/api/sleep";
  request.hasBody = true;
  request.hasJsonBody = true;
  request.body = document.as<JsonVariantConst>();
  return request;
}

void expectCode(const Api::Response &response, int status, const char *code,
                const char *message) {
  expect(response.statusCode == status, message);
  expect(std::strstr(response.data.c_str(), code) != nullptr, message);
}

void testSnapshotShapeAndActions() {
  SleepCoordinator sleep;
  FakeTime time;
  Api::Response response = SleepEndpoints::get(sleep);
  expect(response.statusCode == 200 &&
             std::strstr(response.data.c_str(), "\"storage_state\":\"ok\"") &&
             std::strstr(response.data.c_str(), "\"result\":\"none\"") &&
             std::strstr(response.data.c_str(), "\"tasks\":[]"),
         "GET must expose stable storage and empty last-wake shape");

  Api::Request empty;
  response = SleepEndpoints::keepAwake(empty, sleep);
  expect(response.statusCode == 200 &&
             std::strstr(response.data.c_str(), "\"armed\":true"),
         "keep-awake must accept an omitted body and return idle state");

  JsonDocument unknown;
  Api::Request request = jsonRequest(unknown, R"({"unexpected":1})");
  response = SleepEndpoints::keepAwake(request, sleep);
  expectCode(response, 400, "unsupported_field",
             "keep-awake must reject unknown body fields");

  sleep.current.blockers = SleepBlockerUsb | SleepBlockerWifi;
  response = SleepEndpoints::now(empty, sleep);
  expect(response.statusCode == 409 &&
             std::strstr(response.data.c_str(), "sleep_blocked") &&
             std::strstr(response.data.c_str(), "usb_host_connected") &&
             std::strstr(response.data.c_str(), "wifi_transition"),
         "sleep-now must report all current blockers");

  sleep.current.blockers = 0;
  response = SleepEndpoints::now(empty, sleep);
  expect(response.statusCode == 202 &&
             std::strstr(response.data.c_str(), "\"state\":\"pending\""),
         "sleep-now must return a pending 202 intent");
}

void testStrictValidationAndCommit() {
  for (const char *invalid : {
           R"({})",
           R"({"enabled":"true"})",
           R"({"enabled":true,"period_minutes":1440,"first_wake_delay_minutes":60})",
           R"({"enabled":true,"period_hours":24,"first_wake_delay_minutes":1,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":0,"first_wake_delay_minutes":1,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":-1,"first_wake_delay_minutes":1,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":1.5,"first_wake_delay_minutes":1,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":1,"first_wake_delay_minutes":2,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":4294967295,"first_wake_delay_minutes":1,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":2881,"first_wake_delay_minutes":60,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":1440,"first_wake_delay_minutes":0,"client_time":1799996400})",
           R"({"enabled":true,"period_minutes":1440,"first_wake_delay_minutes":60,"client_time":1704067199})",
           R"({"enabled":false,"period_minutes":1440})",
           R"({"enabled":false,"unexpected":1})",
       }) {
    SleepCoordinator sleep;
    FakeTime time;
    JsonDocument document;
    const Api::Response response =
        SleepEndpoints::update(jsonRequest(document, invalid), sleep, time);
    expect(response.statusCode == 400 && sleep.updateCount == 0,
           "invalid sleep schedule must fail before clock or persistence");
  }

  SleepCoordinator sleep;
  FakeTime time;
  JsonDocument document;
  Api::Request request = jsonRequest(
      document,
      R"({"enabled":true,"period_minutes":1440,"first_wake_delay_minutes":90,"client_time":1800000000})");
  Api::Response response = SleepEndpoints::update(request, sleep, time);
  expect(response.statusCode == 200 && sleep.updateCount == 1 &&
             sleep.current.record.enabled &&
             sleep.current.record.periodMinutes == 1440 &&
             sleep.current.record.anchorEpoch == 1800005400 &&
             sleep.current.record.scheduleGeneration == 2 &&
             sleep.current.record.lastHandledSlot == -1 &&
             time.value.epoch == 1800000000 &&
             time.value.origin == TimeOrigin::Client,
         "valid sleep update must atomically set clock, anchor, and generation");

  SleepCoordinator disabled;
  FakeTime unchanged;
  const int64_t oldAnchor = disabled.current.record.anchorEpoch;
  document.clear();
  request = jsonRequest(document, R"({"enabled":false})");
  response = SleepEndpoints::update(request, disabled, unchanged);
  expect(response.statusCode == 200 && !disabled.current.record.enabled &&
             disabled.current.record.anchorEpoch == oldAnchor &&
             disabled.current.record.periodMinutes == 1440 &&
             disabled.current.record.scheduleGeneration == 2 &&
             unchanged.setCalls == 0,
         "minimal disable must preserve schedule values and not set the clock");

  SleepCoordinator disabledWithSchedule;
  FakeTime replacement;
  document.clear();
  request = jsonRequest(
      document,
      R"({"enabled":false,"period_minutes":720,"first_wake_delay_minutes":30,"client_time":1800000000})");
  response = SleepEndpoints::update(request, disabledWithSchedule, replacement);
  expect(response.statusCode == 200 &&
             !disabledWithSchedule.current.record.enabled &&
             disabledWithSchedule.current.record.periodMinutes == 720 &&
             disabledWithSchedule.current.record.anchorEpoch == 1800001800 &&
             replacement.setCalls == 1,
         "complete disabled schedule fields must validate and replace saved values");

  SleepCoordinator exhausted;
  exhausted.current.record.scheduleGeneration = UINT64_MAX;
  FakeTime untouched;
  document.clear();
  request = jsonRequest(
      document,
      R"({"enabled":true,"period_minutes":1440,"first_wake_delay_minutes":60,"client_time":1800000000})");
  response = SleepEndpoints::update(request, exhausted, untouched);
  expectCode(response, 500, "storage_error",
             "exhausted schedule generation must fail closed");
  expect(exhausted.updateCount == 0 && untouched.setCalls == 0,
         "generation exhaustion must not touch clock or persistence");
}

void testNetworkSyncPreference() {
  SleepCoordinator sleep;
  FakeTime time;
  JsonDocument document;
  Api::Response response = SleepEndpoints::get(sleep);
  expect(std::strstr(response.data.c_str(), "\"wake_network_sync_enabled\":true"),
         "default network preference must be exposed");
  response = SleepEndpoints::update(jsonRequest(document,
      R"({"enabled":false,"wake_network_sync_enabled":false})"), sleep, time);
  expect(response.statusCode == 200 && !sleep.current.record.wakeNetworkSyncEnabled,
         "disabled schedule can save network preference without clock fields");
  response = SleepEndpoints::update(jsonRequest(document,
      R"({"enabled":true,"period_minutes":1440,"first_wake_delay_minutes":60,"client_time":1800000000})"), sleep, time);
  expect(response.statusCode == 200 && !sleep.current.record.wakeNetworkSyncEnabled,
         "legacy client schedule update must preserve false");
  for (const char *invalid : {"null", "0", "1", "\"false\"", "[]", "{}"}) {
    const std::string body = std::string("{\"enabled\":false,\"wake_network_sync_enabled\":") + invalid + "}";
    const unsigned updates = sleep.updateCount;
    response = SleepEndpoints::update(jsonRequest(document, body.c_str()), sleep, time);
    expectCode(response, 400, "invalid_field", "network preference must be boolean");
    expect(sleep.updateCount == updates && !sleep.current.record.wakeNetworkSyncEnabled,
           "invalid preference must not save");
  }
  sleep.updateValue = false;
  response = SleepEndpoints::update(jsonRequest(document,
      R"({"enabled":false,"wake_network_sync_enabled":true})"), sleep, time);
  expectCode(response, 500, "storage_error", "preference save failure must be reported");
  expect(!sleep.current.record.wakeNetworkSyncEnabled, "failed save must preserve preference");
  sleep.updateValue = true;
  response = SleepEndpoints::update(jsonRequest(document,
      R"({"enabled":false,"wake_network_sync_enabled":true})"), sleep, time);
  expect(response.statusCode == 200 && sleep.current.record.wakeNetworkSyncEnabled,
         "preference can be re-enabled");
  sleep.current.record.lastWake.taskCount = 1;
  sleep.current.record.lastWake.tasks[0] = {1, 3, 13};
  response = SleepEndpoints::get(sleep);
  expect(std::strstr(response.data.c_str(), "disabled_by_setting"), "skip reason must identify setting");
}

void testMinuteLimits() {
  for (unsigned minutes : {1U, 2880U}) {
    SleepCoordinator sleep;
    FakeTime time;
    JsonDocument document;
    const std::string body = "{\"enabled\":true,\"period_minutes\":" +
        std::to_string(minutes) + ",\"first_wake_delay_minutes\":1,\"client_time\":1800000000}";
    auto response = SleepEndpoints::update(jsonRequest(document, body.c_str()), sleep, time);
    expect(response.statusCode == 200 && sleep.current.record.periodMinutes == minutes,
           "minute period must accept both limits without truncation");
    expect(std::strstr(response.data.c_str(), "period_minutes") != nullptr &&
           std::strstr(response.data.c_str(), "period_hours") == nullptr,
           "snapshot must expose minute units only");
  }
}

void testClockAndStoreRollback() {
  JsonDocument document;
  const char *body =
      R"({"enabled":true,"period_minutes":1440,"first_wake_delay_minutes":60,"client_time":1800000000})";

  SleepCoordinator initialSetFailure;
  FakeTime partial;
  partial.partialFailMask = 1U << 1;
  Api::Response response = SleepEndpoints::update(
      jsonRequest(document, body), initialSetFailure, partial);
  expectCode(response, 500, "time_error",
             "partial initial clock failure must return time_error");
  expect(partial.setCalls == 2 && partial.value.epoch == 1799996400 &&
             partial.value.origin == TimeOrigin::Client &&
             initialSetFailure.updateCount == 0,
         "partial initial clock failure must restore the previous clock");

  SleepCoordinator storeFailure;
  storeFailure.updateValue = false;
  FakeTime rollback;
  document.clear();
  response = SleepEndpoints::update(
      jsonRequest(document, body), storeFailure, rollback);
  expectCode(response, 500, "storage_error",
             "store failure with successful rollback must report storage_error");
  expect(rollback.setCalls == 2 && rollback.value.epoch == 1799996400 &&
             rollback.value.origin == TimeOrigin::Client,
         "store failure must restore the previous clock and origin");

  SleepCoordinator rollbackFailure;
  rollbackFailure.updateValue = false;
  FakeTime cannotRollback;
  cannotRollback.failSetMask = 1U << 2;
  document.clear();
  response = SleepEndpoints::update(
      jsonRequest(document, body), rollbackFailure, cannotRollback);
  expectCode(response, 500, "time_error",
             "failed rollback must report time_error");
  expect(cannotRollback.clearCalls == 1 && !cannotRollback.value.synced(),
         "failed rollback must clear synchronization instead of publishing stale origin");

  SleepCoordinator busy;
  busy.timeUpdateValue = false;
  FakeTime blockedTime;
  document.clear();
  response = SleepEndpoints::update(
      jsonRequest(document, body), busy, blockedTime);
  expectCode(response, 409, "sleep_entering",
             "clock transaction admission failure must return sleep_entering");
  expect(blockedTime.setCalls == 0 && busy.updateCount == 0,
         "rejected transaction must not touch clock or persistence");
}
}  // namespace

int main() {
  testSnapshotShapeAndActions();
  testStrictValidationAndCommit();
  testClockAndStoreRollback();
  testMinuteLimits();
  testNetworkSyncPreference();
  if (failures != 0) {
    std::cerr << failures << " sleep endpoint test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Sleep endpoint validation and transaction tests passed\n";
  return EXIT_SUCCESS;
}
