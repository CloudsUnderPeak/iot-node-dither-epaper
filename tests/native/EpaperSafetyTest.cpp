#include <cstdint>
#include <iostream>
#include <vector>

#include "modules/epaper/CpuFrequencyGuard.h"
#include "modules/epaper/EpaperSafetyStore.h"
#include "modules/epaper/EpaperShutdownCoordinator.h"
#include "modules/epaper/EpaperPowerProbe.h"
#include "modules/epaper/EpaperRefreshProbe.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

class FakeFrequency final : public CpuFrequencyDriver {
 public:
  uint32_t currentMhz() const override { return current; }
  bool setMhz(uint32_t mhz) override {
    ++setCalls;
    if (rejectSet) return false;
    if (!ignoreSet) current = mhz;
    return true;
  }
  uint32_t current = 160;
  size_t setCalls = 0;
  bool rejectSet = false;
  bool ignoreSet = false;
};

class FakeSafetyStorage final : public EpaperSafetyStorage {
 public:
  bool begin() override { return beginOk; }
  EpaperSafetyReadStatus readStage(uint8_t &value) const override {
    if (readError) return EpaperSafetyReadStatus::StorageError;
    if (!present) return EpaperSafetyReadStatus::Missing;
    value = stage;
    return invalidValue ? EpaperSafetyReadStatus::InvalidValue
                        : EpaperSafetyReadStatus::Ok;
  }
  bool writeStage(uint8_t value) override {
    if (!writeOk) return false;
    present = true;
    if (!ignoreWrite) stage = value;
    return true;
  }
  bool clearStage() override {
    if (!clearOk) return false;
    present = false;
    return true;
  }
  bool beginOk = true;
  mutable bool readError = false;
  bool writeOk = true;
  bool clearOk = true;
  bool ignoreWrite = false;
  bool invalidValue = false;
  bool present = false;
  uint8_t stage = 0;
};

class FakeTransport final : public EpdTransport {
 public:
  bool ready() const override { return true; }
  bool setReset(bool high) override {
    resetHigh = high;
    return true;
  }
  bool busyHigh() const override {
    if (!refreshIssued) return true;
    if (refreshBusyReads++ == 0) return false;
    return true;
  }
  bool writeCommand(uint8_t command) override {
    commands.push_back(command);
    if (command == 0x12) refreshIssued = true;
    return command != failCommand;
  }
  bool writeData(const uint8_t *, size_t) override { return true; }
  void logicalQuiesce() override {
    ++quiesceCalls;
    resetHigh = false;
  }
  void delayMs(uint32_t durationMs) override { now += durationMs; }
  void yieldCpu() override {}
  uint32_t nowMs() const override { return now; }

  bool sawCommand(uint8_t value) const {
    for (uint8_t command : commands) {
      if (command == value) return true;
    }
    return false;
  }

  bool resetHigh = false;
  uint8_t failCommand = 0xFF;
  uint32_t now = 0;
  size_t quiesceCalls = 0;
  mutable size_t refreshBusyReads = 0;
  mutable bool refreshIssued = false;
  std::vector<uint8_t> commands;
};

class FakeRestart final : public RestartDriver {
 public:
  void restart() override { ++calls; }
  size_t calls = 0;
};

void testFrequencyGuard() {
  FakeFrequency frequency;
  CpuFrequencyGuard guard;
  expect(guard.acquire(&frequency), "frequency guard should acquire 80 MHz");
  expect(guard.active() && frequency.current == 80 &&
             guard.originalMhz() == 160,
         "frequency guard must record and read back original/target clocks");
  expect(!guard.acquire(&frequency), "frequency guard must reject nesting");
  expect(guard.release() && frequency.current == 160,
         "frequency guard must restore and read back 160 MHz");

  FakeFrequency ignored;
  ignored.ignoreSet = true;
  CpuFrequencyGuard mismatch;
  expect(!mismatch.acquire(&ignored) && !mismatch.active() &&
             ignored.current == 160,
         "frequency read-back mismatch must fail before panel wake");

  FakeFrequency rejected;
  rejected.rejectSet = true;
  CpuFrequencyGuard setFailure;
  expect(!setFailure.acquire(&rejected) && !setFailure.active(),
         "frequency set failure must fail closed");
}

void testSafetyStore() {
  FakeSafetyStorage storage;
  EpaperSafetyStore store;
  expect(store.begin(&storage) && store.stage() == EpaperProtectionStage::None,
         "missing marker should load as none");
  expect(store.markActive() && store.stage() == EpaperProtectionStage::Active,
         "active marker must write and read back");
  expect(store.markShutdownConfirmed() &&
             store.stage() == EpaperProtectionStage::ShutdownConfirmed,
         "shutdown marker must write and read back");
  expect(store.clear() && store.stage() == EpaperProtectionStage::None,
         "marker clear must verify missing state");

  FakeSafetyStorage corrupted;
  corrupted.present = true;
  corrupted.stage = 99;
  corrupted.invalidValue = true;
  EpaperSafetyStore badStore;
  expect(!badStore.begin(&corrupted) && !badStore.ready(),
         "invalid persisted marker must fail startup closed");

  FakeSafetyStorage mismatchStorage;
  EpaperSafetyStore mismatchStore;
  expect(mismatchStore.begin(&mismatchStorage), "mismatch store should begin");
  mismatchStorage.present = true;
  mismatchStorage.stage =
      static_cast<uint8_t>(EpaperProtectionStage::ShutdownConfirmed);
  mismatchStorage.ignoreWrite = true;
  expect(!mismatchStore.markActive(),
         "marker write without matching read-back must fail");
}

void testShutdownCoordinator() {
  FakeSafetyStorage storage;
  EpaperSafetyStore store;
  expect(store.begin(&storage), "coordinator store should begin");
  FakeTransport transport;
  Epd7In3E driver;
  expect(driver.begin(&transport), "driver should begin quiesced");
  FakeRestart restart;
  EpaperShutdownCoordinator coordinator;
  expect(coordinator.begin(&driver, &store, &restart),
         "shutdown coordinator should begin");

  expect(store.markActive(), "no-wake operation should mark active");
  expect(coordinator.finishOperation() &&
             coordinator.lastOutcome() ==
                 EpaperShutdownOutcome::SafeWithoutWake &&
             store.stage() == EpaperProtectionStage::None,
         "pre-wake failure should quiesce and clear its marker");

  expect(driver.begin(&transport),
         "driver should be re-armed after a safe pre-wake abort");
  expect(store.markActive() && driver.initialize(),
         "powered operation should initialize after marker");
  expect(driver.panelMayBeActive(), "Power ON should mark panel potentially active");
  expect(coordinator.finishOperation() &&
             store.stage() == EpaperProtectionStage::ShutdownConfirmed &&
             transport.sawCommand(0x02) && transport.sawCommand(0x07),
         "powered operation must Power OFF, Deep Sleep, and confirm marker");
  expect(coordinator.restartNow() && restart.calls == 1,
         "confirmed sleeping panel should permit coordinated restart");
}

void testShutdownFailureAndInterruptedBoot() {
  FakeSafetyStorage storage;
  EpaperSafetyStore store;
  expect(store.begin(&storage) && store.markActive(),
         "failure test should persist active marker");
  FakeTransport transport;
  Epd7In3E driver;
  expect(driver.begin(&transport) && driver.initialize(),
         "failure test should reach powered state");
  transport.failCommand = 0x02;
  FakeRestart restart;
  EpaperShutdownCoordinator coordinator;
  expect(coordinator.begin(&driver, &store, &restart),
         "failure coordinator should begin");
  expect(!coordinator.finishOperation() && coordinator.unavailable() &&
             store.stage() == EpaperProtectionStage::Active &&
             restart.calls == 0,
         "Power OFF failure must preserve active marker and fail closed");

  FakeSafetyStorage interruptedStorage;
  interruptedStorage.present = true;
  interruptedStorage.stage =
      static_cast<uint8_t>(EpaperProtectionStage::Active);
  EpaperSafetyStore interruptedStore;
  expect(interruptedStore.begin(&interruptedStorage),
         "valid interrupted marker should load");
  FakeTransport interruptedTransport;
  Epd7In3E interruptedDriver;
  expect(interruptedDriver.begin(&interruptedTransport),
         "interrupted driver should begin quiesced");
  FakeRestart interruptedRestart;
  EpaperShutdownCoordinator interrupted;
  expect(interrupted.begin(
             &interruptedDriver, &interruptedStore, &interruptedRestart) &&
             interrupted.unavailable() && !interrupted.restartNow() &&
             interruptedRestart.calls == 0,
         "active marker loaded at boot must block software restart recovery");
}

void testPowerProbe() {
  FakeSafetyStorage storage;
  EpaperSafetyStore store;
  expect(store.begin(&storage), "probe store should begin");
  FakeTransport transport;
  Epd7In3E driver;
  expect(driver.begin(&transport), "probe driver should begin");
  FakeRestart restart;
  EpaperShutdownCoordinator coordinator;
  expect(coordinator.begin(&driver, &store, &restart),
         "probe coordinator should begin");
  FakeFrequency frequency;
  EpaperPowerProbe probe;
  expect(probe.run(&driver, &transport, &store, &frequency, &coordinator) &&
             probe.result() == EpaperPowerProbeResult::Success &&
             frequency.current == 160 &&
             store.stage() == EpaperProtectionStage::ShutdownConfirmed &&
             transport.sawCommand(0x04) && transport.sawCommand(0x02) &&
             transport.sawCommand(0x07) && !transport.sawCommand(0x12),
         "power-only probe must run at 80 MHz, shut down, restore CPU, and never refresh");

  FakeSafetyStorage cpuStorage;
  EpaperSafetyStore cpuStore;
  expect(cpuStore.begin(&cpuStorage), "CPU failure store should begin");
  FakeTransport cpuTransport;
  Epd7In3E cpuDriver;
  expect(cpuDriver.begin(&cpuTransport), "CPU failure driver should begin");
  FakeRestart cpuRestart;
  EpaperShutdownCoordinator cpuCoordinator;
  expect(cpuCoordinator.begin(&cpuDriver, &cpuStore, &cpuRestart),
         "CPU failure coordinator should begin");
  FakeFrequency badFrequency;
  badFrequency.ignoreSet = true;
  EpaperPowerProbe cpuProbe;
  expect(!cpuProbe.run(&cpuDriver, &cpuTransport, &cpuStore, &badFrequency,
                       &cpuCoordinator) &&
             cpuProbe.result() == EpaperPowerProbeResult::CpuFrequencyFailed &&
             cpuStore.stage() == EpaperProtectionStage::None &&
             !cpuTransport.sawCommand(0x04),
         "frequency failure must clear pre-wake marker without a panel command");

  FakeSafetyStorage initStorage;
  EpaperSafetyStore initStore;
  expect(initStore.begin(&initStorage), "init failure store should begin");
  FakeTransport initTransport;
  initTransport.failCommand = 0xAA;
  Epd7In3E initDriver;
  expect(initDriver.begin(&initTransport), "init failure driver should begin");
  FakeRestart initRestart;
  EpaperShutdownCoordinator initCoordinator;
  expect(initCoordinator.begin(&initDriver, &initStore, &initRestart),
         "init failure coordinator should begin");
  FakeFrequency initFrequency;
  EpaperPowerProbe initProbe;
  expect(!initProbe.run(&initDriver, &initTransport, &initStore, &initFrequency,
                        &initCoordinator) &&
             initProbe.result() == EpaperPowerProbeResult::InitializeFailed &&
             initStore.stage() == EpaperProtectionStage::None &&
             initFrequency.current == 160,
         "pre-Power-ON init failure must quiesce, clear marker, and restore CPU");
}

void testRefreshProbe() {
  FakeSafetyStorage storage;
  EpaperSafetyStore store;
  expect(store.begin(&storage), "refresh probe store should begin");
  FakeTransport transport;
  Epd7In3E driver;
  expect(driver.begin(&transport), "refresh probe driver should begin");
  FakeRestart restart;
  EpaperShutdownCoordinator coordinator;
  expect(coordinator.begin(&driver, &store, &restart),
         "refresh probe coordinator should begin");
  FakeFrequency frequency;
  EpaperPaletteFrameSource palette;
  EpaperRefreshProbe probe;
  expect(probe.run(&driver, &transport, &palette, &store, &frequency,
                   &coordinator) &&
             probe.result() == EpaperRefreshProbeResult::Success &&
             probe.transferredBytes() == EpaperImageFormat::kFrameBytes &&
             frequency.current == 160 &&
             store.stage() == EpaperProtectionStage::ShutdownConfirmed &&
             transport.sawCommand(0x10) && transport.sawCommand(0x12) &&
             transport.sawCommand(0x02) && transport.sawCommand(0x07),
         "refresh probe must stream one complete frame, refresh once, shut down, "
         "and restore CPU frequency");

  FakeSafetyStorage failedStorage;
  EpaperSafetyStore failedStore;
  expect(failedStore.begin(&failedStorage), "failed refresh store should begin");
  FakeTransport failedTransport;
  failedTransport.failCommand = 0x12;
  Epd7In3E failedDriver;
  expect(failedDriver.begin(&failedTransport), "failed refresh driver should begin");
  FakeRestart failedRestart;
  EpaperShutdownCoordinator failedCoordinator;
  expect(failedCoordinator.begin(&failedDriver, &failedStore, &failedRestart),
         "failed refresh coordinator should begin");
  FakeFrequency failedFrequency;
  EpaperRefreshProbe failedProbe;
  expect(!failedProbe.run(&failedDriver, &failedTransport, &palette,
                          &failedStore, &failedFrequency, &failedCoordinator) &&
             failedProbe.result() == EpaperRefreshProbeResult::RefreshFailed &&
             failedProbe.transferredBytes() == EpaperImageFormat::kFrameBytes &&
             failedStore.stage() == EpaperProtectionStage::ShutdownConfirmed &&
             failedFrequency.current == 160 && failedTransport.sawCommand(0x02) &&
             failedTransport.sawCommand(0x07),
         "refresh command failure must still Power OFF, Deep Sleep, confirm the "
         "marker, and restore CPU frequency");
}
}  // namespace

int main() {
  testFrequencyGuard();
  testSafetyStore();
  testShutdownCoordinator();
  testShutdownFailureAndInterruptedBoot();
  testPowerProbe();
  testRefreshProbe();
  if (failures != 0) {
    std::cerr << failures << " e-paper safety test(s) failed\n";
    return 1;
  }
  std::cout << "E-paper frequency, marker, and shutdown safety tests passed\n";
  return 0;
}
