#include <cassert>
#include <functional>
#include <iostream>
#include <vector>
#include "modules/epaper/EpaperService.h"

class FakeFrequency final : public CpuFrequencyDriver {
 public:
  uint32_t currentMhz() const override { return current; }
  bool setMhz(uint32_t mhz) override {
    ++setCalls;
    if (onSet) onSet(mhz);
    if (rejectRestore && mhz == 160) return false;
    if (rejectSet) return false;
    if (!ignoreSet) current = mhz;
    return true;
  }
  uint32_t current = 160;
  size_t setCalls = 0;
  bool rejectSet = false;
  bool rejectRestore = false;
  std::function<void(uint32_t)> onSet;
  bool ignoreSet = false;
};

class FakeSafetyStorage final : public EpaperSafetyStorage {
 public:
  bool begin() override { return beginOk; }
  EpaperSafetyReadStatus readStage(uint8_t &value) const override {
    if (onRead) onRead();
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
  std::function<void()> onRead;
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
    if (onStep) onStep();
    commands.push_back(command);
    if (command == 0x12) refreshIssued = true;
    return command != failCommand;
  }
  bool writeData(const uint8_t *, size_t) override { return true; }
  void logicalQuiesce() override {
    ++quiesceCalls;
    resetHigh = true;
  }
  void delayMs(uint32_t durationMs) override { now += durationMs; nativeMillis += durationMs; if (onStep) onStep(); }
  void yieldCpu() override { if (onStep) onStep(); }
  uint32_t nowMs() const override { return now; }

  bool sawCommand(uint8_t value) const {
    for (uint8_t command : commands) {
      if (command == value) return true;
    }
    return false;
  }

  std::function<void()> onStep;
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


struct Fixture {
  UserDataStorage storage;
  FakeTransport transport;
  Epd7In3E driver;
  FakeSafetyStorage persistence;
  EpaperSafetyStore safety;
  FakeFrequency frequency;
  FakeRestart restart;
  EpaperShutdownCoordinator shutdown;
  EpaperService service;
  explicit Fixture(bool mount = true) {
    nativeMillis = 0;
    nativefs::backend = {};
    if (mount) assert(storage.begin().ok());
    assert(safety.begin(&persistence));
    assert(driver.begin(&transport));
    assert(shutdown.begin(&driver, &safety, &restart));
    assert(service.begin(&storage, &driver, &transport, &safety, &frequency, &shutdown, {}).ok() == (mount && !nativeTaskCreateFails));
  }
};

void testRestartPhases() {
  for (auto phase : {EpaperDrawPhase::Prewake, EpaperDrawPhase::Initializing,
                     EpaperDrawPhase::Transferring, EpaperDrawPhase::Refreshing,
                     EpaperDrawPhase::PoweringOff}) {
    Fixture f;
    bool requested = false;
    f.transport.onStep = [&] {
      assert(f.restart.calls == 0);
      if (!requested && f.service.snapshot().phase == phase) {
        requested = true;
        assert(f.service.requestRestart(millis()) == RestartRequest::Accepted);
        f.service.pollRestart(millis());
        f.service.pollRestart(millis());
        assert(f.service.restartProgress() == RestartProgress::Draining);
        assert(!f.service.snapshot().canDraw);
        assert(!f.service.requestDraw(EpaperDrawAction::White).ok());
      }
    };
    f.frequency.onSet = [&](uint32_t) { assert(f.restart.calls == 0); };
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    nativeRunWorker();
    assert(requested && f.restart.calls == 1);
    assert(f.frequency.current == 160);
    assert(f.safety.stage() == EpaperProtectionStage::ShutdownConfirmed);
    assert(f.service.restartProgress() == RestartProgress::Ready);
    for (int i = 0; i < 100; ++i) {
      assert(f.service.requestRestart(millis()) == RestartRequest::AlreadyPending);
      f.service.pollRestart(millis());
      f.service.poll(millis() + EpaperCooldown::kDurationMs);
      nativeRunWorker();
    }
    assert(f.restart.calls == 1 && !f.service.snapshot().canDraw);
  }
}

void testControlAndTimeouts() {
  {
    Fixture f;
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    assert(f.service.requestRestart(0) == RestartRequest::Accepted);
    f.service.pollRestart(0);
    nativeRunWorker();
    assert(f.restart.calls == 1); // full draw queue did not lose control
  }
  for (bool fail : {false, true}) {
    Fixture f;
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    nativeRunWorker();
    nativeMillis += EpaperCooldown::kDurationMs;
    bool observed = false;
    f.persistence.onRead = [&] {
      observed = true;
      assert(!f.service.snapshot().canDraw); // no status lock held during I/O
      assert(f.service.requestRestart(millis()) == RestartRequest::Accepted);
        f.service.pollRestart(millis());
      assert(f.restart.calls == 0);
    };
    f.persistence.readError = fail;
    f.service.poll(millis());
    assert(!observed);
    nativeRunWorker();
    assert(observed);
    nativeRunWorker();
    assert(f.restart.calls == (fail ? 0U : 1U));
  }
  {
    Fixture f;
    auto upload = f.service.beginUpload(EpaperImageFormat::kImageBytes);
    assert(upload.ok());
    nativeMillis = UINT32_MAX - 10;
    assert(f.service.requestRestart(millis()) == RestartRequest::Accepted);
        f.service.pollRestart(millis());
    nativeMillis += 30000;
    f.service.pollRestart(millis());
    assert(f.service.restartProgress() == RestartProgress::Failed);
    assert(!f.service.snapshot().canUpload);
    assert(nativefs::backend.handles == 1); // timeout does not close callback's file
    f.service.abortUpload(upload.sessionId);
    nativeRunWorker();
    assert(f.service.snapshot().canUpload && f.restart.calls == 0);
    auto next = f.service.beginUpload(EpaperImageFormat::kImageBytes);
    assert(next.ok());
    assert(!f.service.finishUpload(upload.sessionId).ok());
    assert(f.service.snapshot().state == EpaperServiceState::Uploading);
    f.service.abortUpload(next.sessionId);
  }
  {
    Fixture f;
    bool timedOut = false;
    f.transport.onStep = [&] {
      if (timedOut || f.service.snapshot().phase != EpaperDrawPhase::Transferring) return;
      timedOut = true;
      assert(f.service.requestRestart(millis()) == RestartRequest::Accepted);
        f.service.pollRestart(millis());
      f.service.pollRestart(millis() + 150000);
      assert(!f.service.snapshot().canDraw && f.restart.calls == 0);
    };
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    nativeRunWorker();
    assert(timedOut && f.restart.calls == 0);
    assert(f.service.restartProgress() == RestartProgress::Failed);
  }
  for (int failure : {0, 1, 2}) {
    Fixture f;
    if (failure == 0) f.transport.failCommand = 0x02;
    if (failure == 1) f.frequency.rejectRestore = true;
    if (failure == 2) f.persistence.writeOk = false;
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    assert(f.service.requestRestart(0) == RestartRequest::Accepted);
    f.service.pollRestart(0);
    nativeRunWorker();
    assert(f.restart.calls == 0 && !f.service.snapshot().canDraw);
    assert(f.service.restartProgress() == RestartProgress::Failed);
  }
  {
    Fixture f(false);
    assert(f.service.requestRestart(0) == RestartRequest::Accepted);
    f.service.pollRestart(0);
    f.service.pollRestart(0);
    assert(f.restart.calls == 1);
  }
}

void testExternalDrainGate() {
  Fixture f;
  assert(f.service.requestRestart(0) == RestartRequest::Accepted);
  f.service.pollRestart(0, false);
  nativeRunWorker();
  assert(f.restart.calls == 0 && !f.service.snapshot().canUpload);
  f.service.pollRestart(1, true);
  nativeRunWorker();
  assert(f.restart.calls == 1);
}

void testStartupAndUnknownOwner() {
  nativeTaskCreateFails = true;
  {
    Fixture f;
    assert(!f.service.ownsRuntime());
    assert(f.service.requestRestart(0) == RestartRequest::Accepted);
    f.service.pollRestart(0);
    f.service.pollRestart(0);
    assert(f.restart.calls == 1);
  }
  nativeTaskCreateFails = false;
  {
    Fixture f;
    assert(f.safety.markActive());
    assert(f.service.requestRestart(0) == RestartRequest::Accepted);
    f.service.pollRestart(0);
    nativeRunWorker();
    assert(f.restart.calls == 0);
    assert(f.service.restartProgress() == RestartProgress::Failed);
    assert(!f.service.snapshot().canDraw);
  }
}

void testAcceptedUploadDrain() {
  Fixture f;
  std::vector<uint8_t> image(EpaperImageFormat::kImageBytes, 0x11);
  EpaperImageFormat::Header header{1, 40, 800, 480, 192000,
      EpaperImageFormat::crc32(image.data() + 40, 192000), 1};
  assert(EpaperImageFormat::encodeHeader(header, image.data(), image.size()));
  auto upload = f.service.beginUpload(image.size());
  assert(upload.ok());
  assert(f.service.requestRestart(millis()) == RestartRequest::Accepted);
        f.service.pollRestart(millis());
  f.service.pollRestart(millis());
  nativeRunWorker();
  assert(f.restart.calls == 0);
  assert(f.service.writeUpload(upload.sessionId, 0, image.data(), image.size()).ok());
  assert(f.service.finishUpload(upload.sessionId).ok());
  assert(!f.service.snapshot().canDraw);
  nativeRunWorker();
  assert(f.restart.calls == 1 && nativefs::backend.handles == 0);
}

void testMetadataFailuresAndReservation() {
  Fixture f;
  std::vector<uint8_t> image(EpaperImageFormat::kImageBytes, 0x11);
  EpaperImageFormat::Header header{1, 40, 800, 480, 192000,
      EpaperImageFormat::crc32(image.data() + 40, 192000), 9};
  assert(EpaperImageFormat::encodeHeader(header, image.data(), image.size()));
  auto upload = f.storage.beginUpload(EpaperService::kImageName, image.size());
  assert(f.storage.writeUpload(upload.sessionId, 0, image.data(), image.size()).ok());
  assert(f.storage.finishUpload(upload.sessionId).result.ok());
  assert(f.service.refreshMetadata().ok());
  assert(f.service.snapshot().stored.header.generation == 9);
  nativefs::backend.fail = "open";
  assert(f.service.requestDraw(EpaperDrawAction::Stored).status == EpaperServiceStatusCode::StorageError);
  nativefs::backend.fail.clear();
  assert(f.service.snapshot().stored.valid);
  size_t reads = 0;
  nativefs::backend.before = [&](const char *operation) {
    if (std::strcmp(operation, "read") != 0) return;
    assert(!f.service.beginUpload(image.size()).ok());
    if (++reads == 2) nativefs::backend.fail = "read";
  };
  assert(f.service.refreshMetadata().status == EpaperServiceStatusCode::StorageError);
  nativefs::backend.before = {};
  nativefs::backend.fail.clear();
  assert(f.service.snapshot().stored.valid);
  assert(f.service.snapshot().stored.header.generation == 9);
  assert(f.service.snapshot().canUpload && nativefs::backend.handles == 0);
  // A later successful version can commit once validation releases admission.
  auto next = f.service.beginUpload(image.size());
  assert(next.ok());
  f.service.abortUpload(next.sessionId);
  assert(f.storage.deleteFile(EpaperService::kImageName).ok());
  assert(f.service.refreshMetadata().status == EpaperServiceStatusCode::ImageNotFound);
  assert(!f.service.snapshot().stored.present);
}

void testRestartWaitsForStorage() {
  for (bool timeout : {false, true}) {
    Fixture f;
    auto upload = f.storage.beginUpload("generic.bin", 1);
    assert(upload.result.ok());
    assert(f.service.requestRestart(0) == RestartRequest::Accepted);
    f.service.pollRestart(0);
    nativeRunWorker();
    assert(f.restart.calls == 0);
    assert(f.service.beginImageDownload("").result.status == UserDataFileStatus::Busy);
    if (timeout) f.service.pollRestart(150000);
    f.storage.abortUpload(upload.sessionId); // only original callback releases it
    nativeRunWorker();
    assert(f.restart.calls == (timeout ? 0U : 1U));
    auto next = f.storage.beginUpload("next.bin", 0);
    if (timeout) {
      assert(next.result.ok());
      f.storage.abortUpload(next.sessionId);
    } else assert(next.result.status == UserDataFileStatus::Busy);
  }
}

int main(int argc, char **) {
  if (argc > 1) {
    Fixture f;
    auto uploaded = f.storage.beginUpload(EpaperService::kImageName, 1);
    const uint8_t corrupt = 0;
    assert(f.storage.writeUpload(uploaded.sessionId, 0, &corrupt, 1).ok());
    assert(f.storage.finishUpload(uploaded.sessionId).result.ok());
    assert(!f.service.refreshMetadata().ok());
    assert(f.service.snapshot().stored.present);
    auto occupied = f.storage.beginDownload(EpaperService::kImageName, "");
    assert(occupied.result.ok());
    assert(f.service.refreshMetadata().status == EpaperServiceStatusCode::StorageBusy);
    assert(f.service.snapshot().stored.present);
    f.storage.finishDownload(occupied.sessionId);
  }
  testRestartWaitsForStorage();
  testExternalDrainGate();
  testMetadataFailuresAndReservation();
  testStartupAndUnknownOwner();
  testAcceptedUploadDrain();
  testRestartPhases();
  testControlAndTimeouts();
  std::cout << "Real EpaperService restart phases, timeout and marker ownership tests passed\n";
}
