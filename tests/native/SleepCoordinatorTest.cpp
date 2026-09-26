#include <cassert>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "modules/sleep/SleepCoordinator.h"
#include "modules/captive/CaptivePortalDnsService.h"
#include "modules/config/ConfigService.h"
#include "modules/epaper/EpaperService.h"
#include "modules/http/ApiServer.h"
#include "modules/mdns/MdnsService.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/storage/UserDataStorage.h"
#include "modules/wifi/WifiManager.h"

class MemoryBackend final : public PreferencesBackend {
 public:
  std::map<std::string, std::map<std::string, std::vector<uint8_t>>> spaces;
  std::string active;
  PreferencesNamespaceState inspectNamespace(const char *name) override {
    return spaces.count(name) ? PreferencesNamespaceState::Exists
                              : PreferencesNamespaceState::Missing;
  }
  bool open(const char *name, bool) override { active = name; spaces[name]; return true; }
  void close() override { active.clear(); }
  bool clear() override { spaces[active].clear(); return true; }
  bool hasKey(const char *key) const override { return spaces.at(active).count(key); }
  uint8_t getUChar(const char *, uint8_t value) const override { return value; }
  bool getUCharChecked(const char *, uint8_t &) const override { return false; }
  uint16_t getUShort(const char *, uint16_t value) const override { return value; }
  bool getBool(const char *, bool value) const override { return value; }
  bool getString(const char *, char *, size_t) const override { return false; }
  bool putUChar(const char *, uint8_t) override { return false; }
  bool putUShort(const char *, uint16_t) override { return false; }
  bool putBool(const char *, bool) override { return false; }
  bool putString(const char *, const char *) override { return false; }
  bool getBytes(const char *key, void *target, size_t size) const override {
    const auto space = spaces.find(active);
    if (space == spaces.end()) return false;
    const auto value = space->second.find(key);
    if (value == space->second.end() || value->second.size() != size) return false;
    memcpy(target, value->second.data(), size);
    return true;
  }
  bool putBytes(const char *key, const void *source, size_t size) override {
    const auto *bytes = static_cast<const uint8_t *>(source);
    spaces[active][key] = {bytes, bytes + size};
    return true;
  }
};

class FakeTime final : public TimeSource {
 public:
  TimeSnapshot value;
  TimeSnapshot snapshot() const override { return value; }
  bool set(int64_t epoch, TimeOrigin origin) override {
    value.epoch = epoch; value.origin = origin; ++value.revision; return true;
  }
  void clear() override { value = {}; }
  void restoreCarried(bool valid) override {
    value.origin = valid ? TimeOrigin::Carried : TimeOrigin::None;
    ++value.revision;
  }
};

class FakeSleepDriver final : public SleepDriver {
 public:
  bool usb = false;
  bool armValue = true;
  bool disarmValue = true;
  bool holdValue = true;
  bool releaseValue = true;
  unsigned armCalls = 0;
  unsigned disarmCalls = 0;
  unsigned holdCalls = 0;
  unsigned releaseCalls = 0;
  unsigned invalidateCalls = 0;
  unsigned deepSleepCalls = 0;
  unsigned drainCalls = 0;
  uint32_t armedSeconds = 0;
  std::function<void()> onArm;
  SleepRtcRecord record;
  bool usbHostConnected() const override { return usb; }
  WakeCause wakeupCause() const override { return WakeCause::None; }
  bool armTimer(uint32_t seconds) override {
    ++armCalls;
    armedSeconds = seconds;
    if (onArm) onArm();
    return armValue;
  }
  bool disarmTimer() override { ++disarmCalls; return disarmValue; }
  bool holdEpaperPins() override { ++holdCalls; return holdValue; }
  bool releaseEpaperPins() override { ++releaseCalls; return releaseValue; }
  SleepRtcRecord bootRecord() const override { return {}; }
  void writeRecord(const SleepRtcRecord &next) override {
    record = next; sealSleepRtcRecord(record);
  }
  void invalidateRecord() override { ++invalidateCalls; record.intent = false; }
  void drainSerial(uint32_t) override { ++drainCalls; }
  void deepSleep() override { ++deepSleepCalls; }
};

bool SntpClient::start() { running_ = true; sampleReady_ = false; if (++generation_ == 0) ++generation_; return true; }
void SntpClient::stop() { running_ = false; sampleReady_ = false; sampleGeneration_ = 0; }
bool SntpClient::takeSample(int64_t &epoch) {
  if (!running_ || !sampleReady_ || sampleGeneration_ != generation_) return false;
  epoch = sampleEpoch_; sampleReady_ = false; return true;
}
void SntpClient::acceptSample(int64_t epoch, uint32_t generation) {
  if (!running_ || generation != generation_) return;
  sampleEpoch_ = epoch;
  sampleGeneration_ = generation;
  sampleReady_ = true;
}

struct Fixture {
  MemoryBackend backend;
  SleepStore store{backend};
  FakeTime time;
  FakeSleepDriver driver;
  SleepCoordinator sleep;
  ConfigService config;
  EpaperService epaper;
  UserDataStorage storage;
  WifiManager wifi;
  MdnsService mdns;
  CaptivePortalDnsService captive;
  RuntimeActionScheduler runtime;
  ApiServer http;
  int64_t now = static_cast<int64_t>(std::time(nullptr));

  explicit Fixture(bool usb = false, bool wake = false,
                   int64_t wakeDueOffset = 3600, bool checkpoint = false) {
    nativeMillis = 0;
    SleepRecord record;
    assert(store.load(record) == SleepStoreState::Empty);
    record.enabled = true;
    record.periodHours = 24;
    record.anchorEpoch = now + 86400;
    record.scheduleGeneration = 1;
    assert(store.save(record, record) == SleepStoreState::Ready);
    time.value = {now, TimeOrigin::Client, 1};
    driver.usb = usb;
    SleepRtcRecord boot;
    if (wake) {
      boot.scheduleGeneration = 1;
      boot.basis = SleepClockBasis::Absolute;
      boot.intent = true;
      boot.synced = true;
      boot.sleepEnteredClock = now - 10;
      boot.plannedDue = now + wakeDueOffset;
      boot.timerTargetClock = checkpoint ? now - 5 : boot.plannedDue;
      boot.slotIndex = 0;
      boot.lastHandledSlot = -1;
      sealSleepRtcRecord(boot);
    }
    assert(sleep.begin(store, time, driver, boot, wake,
                       wake ? WakeCause::Timer : WakeCause::None, now));
    sleep.attach(config, epaper, storage, wifi, mdns, captive, runtime, http);
  }
};

void testSntpMailboxRejectsOldGeneration() {
  SntpClient client;
  int64_t sample = 0;
  assert(client.start());
  client.acceptSample(1800000000LL, 1);
  assert(client.takeSample(sample));
  assert(sample == 1800000000LL);
  client.stop();

  assert(client.start());
  client.acceptSample(1800000001LL, 1);
  assert(!client.takeSample(sample));
  client.acceptSample(1800000002LL, 2);
  assert(client.takeSample(sample));
  assert(sample == 1800000002LL);
  client.stop();
}

void testManualGraceAndTerminalReturn() {
  Fixture f;
  uint16_t blockers = 0;
  assert(f.sleep.requestNow(0, blockers));
  f.sleep.poll(499);
  assert(f.epaper.requestSleepCalls == 0);
  f.sleep.poll(500);
  assert(f.epaper.requestSleepCalls == 1);
  f.sleep.poll(501);
  assert(f.driver.deepSleepCalls == 1);
  assert(f.driver.drainCalls == 1);
  assert(f.http.stopCalls == 1);
  assert(!f.storage.mounted);
  assert(!f.sleep.beginApiRequest(true, 502));
  f.sleep.poll(1000);
  assert(f.driver.deepSleepCalls == 1);
  const SleepSnapshot snapshot = f.sleep.snapshot(1000);
  assert(snapshot.state == SleepRunState::Failed);
  assert(snapshot.request == SleepRequestState::Failed);
  assert(std::string(snapshot.requestError) == "deep_sleep_returned");
}

void testUsbDisconnectRestartsFullIdle() {
  Fixture f(true);
  nativeMillis = SLEEP_IDLE_TIMEOUT_SECONDS * 1000U;
  f.sleep.poll(nativeMillis);
  assert(f.epaper.requestSleepCalls == 0);
  f.driver.usb = false;
  f.sleep.poll(++nativeMillis);
  const uint32_t resetAt = nativeMillis;
  nativeMillis = resetAt + SLEEP_IDLE_TIMEOUT_SECONDS * 1000U - 1U;
  f.sleep.poll(nativeMillis);
  assert(f.epaper.requestSleepCalls == 0);
  ++nativeMillis;
  f.sleep.poll(nativeMillis);
  assert(f.epaper.requestSleepCalls == 1);
}

void testEarlyWakeSkipsNetworkAndPersistsResult() {
  Fixture f(false, true);
  f.config.value.wifiMode = WifiMode::Sta;
  strlcpy(f.config.value.staSsid, "network", sizeof(f.config.value.staSsid));
  assert(f.sleep.wakeCycle());
  assert(f.sleep.effectiveWifiConfig(f.config.value).wifiMode == WifiMode::Off);
  f.sleep.poll(0);
  const SleepSnapshot snapshot = f.sleep.snapshot(0);
  assert(snapshot.record.lastWake.result == 2);
  assert(snapshot.record.lastHandledSlot == -1);
  assert(f.wifi.applyCalls == 0);
  assert(f.epaper.requestSleepCalls == 1);
}

void testCheckpointWakeDoesNotPersistDiagnostics() {
  Fixture f(false, true, 90000, true);
  const SleepSnapshot before = f.sleep.snapshot(0);
  f.sleep.poll(0);
  const SleepSnapshot after = f.sleep.snapshot(0);
  assert(after.record.revision == before.record.revision);
  assert(after.record.lastWake.result == 0);
  assert(after.record.lastHandledSlot == -1);
  assert(f.epaper.requestSleepCalls == 1);
}

void testScheduleUpdatePersistsSupersededAgenda() {
  Fixture f(false, true, -1);
  f.sleep.poll(0);
  SleepRecord candidate = f.sleep.snapshot(0).record;
  ++candidate.scheduleGeneration;
  candidate.anchorEpoch = f.now + 7200;
  candidate.lastHandledSlot = -1;
  SleepRecord committed;
  assert(f.sleep.update(candidate, committed));
  assert(committed.lastWake.result == 9);
#if IOT_FEATURE_EPAPER
  assert(committed.lastWake.taskCount == 2);
#else
  assert(committed.lastWake.taskCount == 1);
#endif
  assert(committed.lastWake.tasks[0].code == 9);
#if IOT_FEATURE_EPAPER
  assert(committed.lastWake.tasks[1].code == 9);
#else
  assert(committed.lastWake.tasks[1].name == 0);
#endif
  assert(committed.lastWake.consecutiveFailures == 0);
}

void testKeepAwakeUsesLoopOwnerAndFinalGate() {
  uint16_t blockers = 0;

  Fixture owner;
  assert(owner.sleep.requestNow(0, blockers));
  owner.sleep.poll(500);
  assert(owner.sleep.keepAwake(501));
  assert(owner.epaper.cancelSleepCalls == 0);
  assert(owner.storage.cancelCalls == 0);
  owner.sleep.poll(501);
  assert(owner.epaper.cancelSleepCalls == 1);
  assert(owner.sleep.snapshot(501).request == SleepRequestState::Cancelled);
  assert(owner.sleep.beginApiRequest(false, 502));
  owner.sleep.endApiRequest();

  Fixture beforeGate;
  bool acceptedBeforeGate = false;
  assert(beforeGate.sleep.requestNow(0, blockers));
  beforeGate.sleep.poll(500);
  beforeGate.storage.onReserve = [&]() {
    acceptedBeforeGate = beforeGate.sleep.keepAwake(501);
  };
  beforeGate.sleep.poll(501);
  assert(acceptedBeforeGate);
  assert(beforeGate.driver.deepSleepCalls == 0);
  assert(beforeGate.storage.cancelCalls == 1);
  assert(beforeGate.sleep.snapshot(501).request == SleepRequestState::Cancelled);

  Fixture afterGate;
  bool acceptedAfterGate = true;
  assert(afterGate.sleep.requestNow(0, blockers));
  afterGate.sleep.poll(500);
  afterGate.driver.onArm = [&]() {
    acceptedAfterGate = afterGate.sleep.keepAwake(501);
  };
  afterGate.sleep.poll(501);
  assert(!acceptedAfterGate);
  assert(afterGate.driver.deepSleepCalls == 1);
}

void testRuntimeBlockerAndStorageRecovery() {
  Fixture leases;
  uint16_t reasons = 0;
  assert(leases.sleep.beginApiRequest(true, 0));
  assert(leases.sleep.requestNow(0, reasons));
  leases.sleep.endApiRequest();

  Fixture concurrent;
  assert(concurrent.sleep.beginApiRequest(false, 0));
  assert(concurrent.sleep.beginApiRequest(false, 0));
  assert(!concurrent.sleep.requestNow(0, reasons));
  assert((reasons & SleepBlockerRuntime) != 0);
  concurrent.sleep.endApiRequest();
  concurrent.sleep.endApiRequest();

  Fixture blocked;
  blocked.runtime.current.runtimeActionPending = true;
  reasons = 0;
  assert(!blocked.sleep.requestNow(0, reasons));
  assert((reasons & SleepBlockerRuntime) != 0);

  Fixture recovering;
  recovering.driver.armValue = false;
  recovering.storage.cancelValue = false;
  assert(recovering.sleep.requestNow(0, reasons));
  recovering.sleep.poll(500);
  recovering.sleep.poll(501);
  assert(recovering.storage.cancelCalls == 1);
  assert(!recovering.sleep.beginApiRequest(false, 502));
  recovering.storage.cancelValue = true;
  recovering.sleep.poll(502);
  assert(recovering.storage.cancelCalls == 2);
  assert(recovering.http.started());
  assert(recovering.sleep.beginApiRequest(false, 503));
  recovering.sleep.endApiRequest();

  Fixture serviceRecovery;
  serviceRecovery.wifi.applyValue = false;
  assert(serviceRecovery.sleep.requestNow(0, reasons));
  serviceRecovery.sleep.poll(500);
  serviceRecovery.sleep.poll(501);
  assert(!serviceRecovery.sleep.beginApiRequest(false, 502));
  serviceRecovery.wifi.applyValue = true;
  serviceRecovery.sleep.poll(502);
  assert(serviceRecovery.sleep.beginApiRequest(false, 503));
  serviceRecovery.sleep.endApiRequest();

  Fixture hardwareRecovery;
  hardwareRecovery.driver.holdValue = false;
  hardwareRecovery.driver.releaseValue = false;
  assert(hardwareRecovery.sleep.requestNow(0, reasons));
  hardwareRecovery.sleep.poll(500);
  hardwareRecovery.sleep.poll(501);
  assert(hardwareRecovery.driver.releaseCalls == 1);
  assert(!hardwareRecovery.sleep.keepAwake(502));
  assert(!hardwareRecovery.sleep.beginApiRequest(false, 502));
  hardwareRecovery.driver.releaseValue = true;
  hardwareRecovery.sleep.poll(502);
  assert(hardwareRecovery.driver.releaseCalls == 2);
  assert(hardwareRecovery.sleep.beginApiRequest(false, 503));
  hardwareRecovery.sleep.endApiRequest();
}

#if !IOT_FEATURE_EPAPER
void testWithoutOptionalOwners() {
  Fixture f;
  f.sleep.attach(f.config, nullptr,
#if IOT_FEATURE_STORAGE
                 &f.storage,
#else
                 nullptr,
#endif
                 f.wifi, nullptr, f.captive, f.runtime, f.http);
  uint16_t reasons = 0;
  f.storage.busy = true;
#if IOT_FEATURE_STORAGE
  assert(!f.sleep.requestNow(0, reasons));
  assert(reasons & SleepBlockerUpload);
#else
  assert(f.sleep.requestNow(0, reasons));
  assert(reasons == 0);
#endif
  f.storage.busy = false;
  assert(f.sleep.requestNow(0, reasons));
  f.sleep.poll(500);
  f.sleep.poll(501);
  assert(f.driver.deepSleepCalls == 1);
  assert(f.epaper.requestSleepCalls == 0);
#if IOT_FEATURE_STORAGE
  assert(f.storage.reserveCalls == 1 && f.storage.unmountCalls == 1);
#else
  assert(f.storage.reserveCalls == 0 && f.storage.unmountCalls == 0);
#endif
  assert(!f.sleep.beginApiRequest(false, 502));

  Fixture cancelled;
  assert(cancelled.sleep.requestNow(0, reasons));
  cancelled.sleep.poll(500);
  assert(cancelled.sleep.keepAwake(501));
  cancelled.sleep.poll(501);
  assert(cancelled.sleep.beginApiRequest(false, 502));
  cancelled.sleep.endApiRequest();
  assert(cancelled.driver.deepSleepCalls == 0);

  Fixture early(false, true);
  early.sleep.poll(0);
  const auto wake = early.sleep.snapshot(0).record.lastWake;
  assert(wake.result == 2 && wake.taskCount == 1);
  assert(wake.tasks[0].name == 1 && wake.tasks[1].name == 0);
  assert(early.epaper.requestSleepCalls == 0);
}
#endif

int main() {
#if !IOT_FEATURE_EPAPER
  testWithoutOptionalOwners();
  testScheduleUpdatePersistsSupersededAgenda();
  std::cout << "Timer-only sleep admission, optional owners and wake diagnostics passed\n";
#else
  testSntpMailboxRejectsOldGeneration();
  testManualGraceAndTerminalReturn();
  testUsbDisconnectRestartsFullIdle();
  testEarlyWakeSkipsNetworkAndPersistsResult();
  testCheckpointWakeDoesNotPersistDiagnostics();
  testScheduleUpdatePersistsSupersededAgenda();
  testKeepAwakeUsesLoopOwnerAndFinalGate();
  testRuntimeBlockerAndStorageRecovery();
  std::cout << "Sleep coordinator grace, blockers, recovery, and terminal tests passed\n";
#endif
}
