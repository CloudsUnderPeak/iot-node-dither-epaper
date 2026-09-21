#include <cassert>
#include <functional>
#include <iostream>
#include <vector>
#include "modules/epaper/EpaperService.h"
#include "support/EpaperGzipFixture.h"

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

void testPrewakeMarkerFailures() {
  for (int fault = 0; fault < 4; ++fault) {
    Fixture f;
    if (fault == 0 || fault == 3) f.persistence.writeOk = false;
    if (fault == 1) f.persistence.readError = true;
    if (fault == 2) {
      f.persistence.present = true;
      f.persistence.stage = static_cast<uint8_t>(EpaperProtectionStage::ShutdownConfirmed);
      f.persistence.ignoreWrite = true;
    }
    if (fault == 3) {
      f.persistence.present = true;
      f.persistence.stage = static_cast<uint8_t>(EpaperProtectionStage::Active);
      f.persistence.clearOk = false;
    }
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    nativeRunWorker();
    const auto state = f.service.snapshot();
    assert(state.state == EpaperServiceState::Unavailable);
    assert(state.panelState == EpaperPanelState::Inactive);
    assert(std::string(state.lastErrorCode) == "marker_active_failed");
    assert(!state.canDraw && !state.canUpload);
    assert(f.transport.commands.empty() && f.frequency.setCalls == 0);
    const bool cleared = fault == 0 || fault == 2;
    assert(state.recoveryRequired == !cleared);
    EpaperSafetyStore reloaded;
    assert(reloaded.begin(&f.persistence) == (fault != 1));
    if (cleared) {
      assert(reloaded.stage() == EpaperProtectionStage::None);
      assert(f.service.requestRestart(millis()) == RestartRequest::Accepted);
      f.service.pollRestart(millis());
      nativeRunWorker();
      assert(f.restart.calls == 1);
    } else {
      if (fault == 3) assert(reloaded.stage() == EpaperProtectionStage::Active);
      assert(f.service.requestRestart(millis()) == RestartRequest::Rejected);
    }
  }
}

void testUploadIdleCleanup() {
  Fixture f;
  const auto upload = f.service.beginUpload(100);
  assert(upload.ok() && nativefs::backend.handles == 1);
  f.service.abortUpload(upload.sessionId, "upload_timeout");
  const auto state = f.service.snapshot();
  assert(state.state == EpaperServiceState::Idle &&
         std::string(state.lastErrorCode) == "upload_timeout" &&
         nativefs::backend.handles == 0);
  assert(!f.service.finishUpload(upload.sessionId).ok());
  const auto next = f.service.beginUpload(100);
  assert(next.ok() && next.sessionId != upload.sessionId);
  f.service.abortUpload(upload.sessionId);
  assert(nativefs::backend.handles == 1);
  f.service.abortUpload(next.sessionId);
  assert(nativefs::backend.handles == 0 && f.service.snapshot().canUpload);
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
    assert(f.restart.calls == (failure == 2 ? 1U : 0U) &&
           !f.service.snapshot().canDraw);
    assert(f.service.restartProgress() == (failure == 2
        ? RestartProgress::Ready : RestartProgress::Failed));
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
  image = gzipBytes(image, 0);
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
  image = gzipBytes(image, 0);
  auto upload = f.storage.beginUpload(EpaperService::kStoredImageName, image.size());
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
  // A syntax error in the first chunk must not hide a later storage failure
  // and replace the last fully observed metadata snapshot with a partial one.
  auto &storedBytes = nativefs::backend.files.at(
      std::string("/files/") + EpaperService::kStoredImageName)->bytes;
  storedBytes[0] ^= 1;
  reads = 0;
  nativefs::backend.before = [&](const char *operation) {
    if (std::strcmp(operation, "read") == 0 && ++reads == 2) nativefs::backend.fail = "read";
  };
  assert(f.service.refreshMetadata().status == EpaperServiceStatusCode::StorageError);
  nativefs::backend.before = {};
  nativefs::backend.fail.clear();
  storedBytes[0] ^= 1;
  assert(f.service.snapshot().stored.valid);
  assert(f.service.snapshot().stored.header.generation == 9);
  assert(nativefs::backend.handles == 0);
  // A later successful version can commit once validation releases admission.
  auto next = f.service.beginUpload(image.size());
  assert(next.ok());
  f.service.abortUpload(next.sessionId);
  assert(f.storage.deleteFile(EpaperService::kStoredImageName).ok());
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

void testMarkerRetryRecovery() {
  for (bool readFailure : {false, true}) {
    Fixture f;
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    nativeRunWorker();
    nativeMillis += EpaperCooldown::kDurationMs;
    f.persistence.clearOk = readFailure;
    f.persistence.readError = readFailure;
    f.service.poll(millis());
    nativeRunWorker();
    assert(!f.service.snapshot().canDraw);
    f.persistence.clearOk = true;
    f.persistence.readError = false;
    nativeMillis += 999;
    f.service.poll(millis());
    nativeRunWorker();
    assert(!f.service.snapshot().canDraw);
    nativeMillis += 1;
    f.service.poll(millis());
    nativeRunWorker();
    assert(f.service.snapshot().canDraw);
    assert(!f.service.snapshot().recoveryRequired);
    assert(f.safety.stage() == EpaperProtectionStage::None);
  }
}

void testCooldownWithoutMainLoop() {
  for (uint32_t start : {0U, 0x90000000U, UINT32_MAX - 100000U}) {
    Fixture f;
    nativeMillis = start;
    assert(f.service.requestDraw(EpaperDrawAction::White).ok());
    nativeRunWorker();
    nativeMillis += EpaperCooldown::kDurationMs - 1;
    nativeRunWorker();
    assert(!f.service.snapshot().canDraw);
    nativeMillis += 1;
    nativeRunWorker();
    assert(f.service.snapshot().canDraw);
    assert(f.safety.stage() == EpaperProtectionStage::None);
  }
}


void storeGzip(Fixture &f, const std::vector<uint8_t> &bytes) {
  auto start = f.storage.beginUpload(EpaperService::kStoredImageName, bytes.size());
  assert(start.result.ok());
  assert(f.storage.writeUpload(start.sessionId, 0, bytes.data(), bytes.size()).ok());
  assert(f.storage.finishUpload(start.sessionId).result.ok());
}

void testGzipStorageDownloadsAndFailures() {
  const auto raw = epaperBytes(71);
  const auto gzip = gzipBytes(raw);
  const std::string storedPath = std::string("/files/") + EpaperService::kStoredImageName;
  {
    Fixture f;
    assert(f.service.beginUpload(EpaperService::kMaxCompressedBytes + 1).status == EpaperServiceStatusCode::PayloadTooLarge);
    auto upload = f.service.beginUpload(gzip.size());
    assert(upload.ok());
    for (size_t i = 0; i < gzip.size(); ++i) {
      assert(f.service.writeUpload(upload.sessionId, i, &gzip[i], 1).ok());
    }
    assert(f.service.finishUpload(upload.sessionId).ok());
    assert(!f.service.finishUpload(upload.sessionId).ok());
    assert(nativefs::backend.files.at(storedPath)->bytes == gzip);
    assert(nativefs::backend.files.count("/files/epaper-current.epd") == 0);
    auto metadata = f.service.snapshot().stored;
    assert(metadata.valid && metadata.sizeBytes == raw.size() && metadata.storedSizeBytes == gzip.size());
    nativeRunWorker();
    assert(std::count(f.transport.commands.begin(), f.transport.commands.end(), 0x12) == 1);
    assert(f.service.snapshot().lastResult == std::string("success"));
    for (const char *range : {"", "bytes=0-39", "bytes=100-999", "bytes=191900-", "bytes=-17"}) {
      auto begin = f.service.beginImageDownload(range);
      assert(begin.result.ok() && begin.fileSize == raw.size());
      assert(f.storage.beginUpload("other.bin", 1).result.status == UserDataFileStatus::Busy);
      std::vector<uint8_t> downloaded;
      uint8_t buffer[137];
      while (downloaded.size() < begin.contentLength) {
        auto read = f.service.readImageDownload(begin.sessionId, buffer, sizeof(buffer));
        assert(read.result.ok() && read.bytesRead);
        downloaded.insert(downloaded.end(), buffer, buffer + read.bytesRead);
      }
      assert(downloaded == std::vector<uint8_t>(raw.begin() + begin.rangeStart,
                                              raw.begin() + begin.rangeStart + begin.contentLength));
      assert(nativefs::backend.handles == 0);
    }
    assert(f.service.beginImageDownload("bytes=999999-").result.status == UserDataFileStatus::RangeNotSatisfiable);
    auto download = f.service.beginImageDownload("");
    assert(download.result.ok());
    f.service.finishImageDownload(download.sessionId);
    assert(nativefs::backend.handles == 0);
    // Boot revalidates compressed storage and publishes logical metadata.
    EpaperService reboot;
    assert(reboot.begin(&f.storage, &f.driver, &f.transport, &f.safety, &f.frequency, &f.shutdown, {}).ok());
    assert(reboot.snapshot().stored.header.generation == 71);
    assert(reboot.snapshot().stored.storedSizeBytes == gzip.size());
  }
  for (const char *failure : {"decode", "crc", "isize", "truncate", "disconnect", "write", "flush", "close", "rename"}) {
    Fixture f;
    storeGzip(f, gzip);
    assert(f.service.refreshMetadata().ok());
    auto candidate = gzipBytes(epaperBytes(72));
    auto begin = f.service.beginUpload(candidate.size());
    assert(begin.ok());
    if (!strcmp(failure, "decode")) candidate[0] = 0;
    if (!strcmp(failure, "crc")) candidate[candidate.size() - 8] ^= 1;
    if (!strcmp(failure, "isize")) candidate[candidate.size() - 4] ^= 1;
    if (!strcmp(failure, "truncate")) candidate.pop_back();
    if (!strcmp(failure, "write")) nativefs::backend.fail = failure;
    auto written = f.service.writeUpload(begin.sessionId, 0, candidate.data(), candidate.size());
    if (!strcmp(failure, "disconnect")) f.service.abortUpload(begin.sessionId);
    else {
      if (!strcmp(failure, "flush") || !strcmp(failure, "close") || !strcmp(failure, "rename")) nativefs::backend.fail = failure;
      assert(!written.ok() || !f.service.finishUpload(begin.sessionId).ok());
    }
    nativefs::backend.fail.clear();
    assert(nativefs::backend.files.at(storedPath)->bytes == gzip);
    assert(nativefs::backend.handles == 0 && f.service.snapshot().canUpload);
    assert(nativefs::backend.files.count("/files/.upload.tmp") == 0);
    nativeRunWorker();
    assert(!f.transport.sawCommand(0x04));
    assert(f.service.snapshot().stored.header.generation == 71);
    auto next = f.service.beginUpload(gzip.size());
    assert(next.ok());
    f.service.abortUpload(next.sessionId);
  }
  {
    Fixture f;
    storeGzip(f, gzipBytes(raw, 0));
    auto begin = f.service.beginImageDownload("bytes=0-39");
    assert(begin.result.ok());
    size_t reads = 0;
    nativefs::backend.before = [&](const char *operation) {
      if (!strcmp(operation, "read") && ++reads == 2) nativefs::backend.fail = "read";
    };
    uint8_t header[40];
    const auto failed = f.service.readImageDownload(begin.sessionId, header, sizeof(header));
    nativefs::backend.before = {};
    nativefs::backend.fail.clear();
    assert(!failed.result.ok() && failed.bytesRead == 0);
    assert(f.service.snapshot().timings.downloadFailures == 1);
    assert(nativefs::backend.handles == 0);
    const auto next = f.service.beginImageDownload("bytes=-17");
    assert(next.result.ok());
    f.service.finishImageDownload(next.sessionId);
  }
  {
    Fixture f;
    storeGzip(f, gzip);
    assert(f.service.refreshMetadata().ok());
    assert(f.service.requestDraw(EpaperDrawAction::Stored).ok());
    nativefs::backend.files.at(storedPath)->bytes.back() ^= 1;
    nativeRunWorker();
    assert(!f.transport.sawCommand(0x04) && f.frequency.setCalls == 0);
    assert(f.safety.stage() == EpaperProtectionStage::None && nativefs::backend.handles == 0);
  }
  {
    Fixture f;
    storeGzip(f, gzipBytes(raw, 0));
    assert(f.service.requestDraw(EpaperDrawAction::Stored).ok());
    nativefs::backend.before = [&](const char *operation) {
      if (!strcmp(operation, "read") && f.service.snapshot().phase == EpaperDrawPhase::Transferring) {
        nativefs::backend.fail = "read";
      }
    };
    nativeRunWorker();
    nativefs::backend.before = {};
    nativefs::backend.fail.clear();
    assert(f.transport.sawCommand(0x02) && f.transport.sawCommand(0x07));
    assert(!f.transport.sawCommand(0x12));
    assert(f.service.snapshot().lastErrorCode == std::string("frame_read_failed"));
    assert(f.frequency.current == 160 && nativefs::backend.handles == 0);
  }
}

void testGzipMountingOrientation() {
  Fixture f;
  auto raw = epaperBytes(73);
  EpaperPaletteFrameSource palette;
  assert(palette.read(0, raw.data() + 40, palette.size()) == palette.size());
  // Asymmetric corner/row data catches vertical direction and nibble order.
  raw[40] = 0x35; raw[41] = 0x61; raw.back() = 0x23;
  EpaperImageFormat::Header header{1,40,EpaperImageFormat::kWidth,EpaperImageFormat::kHeight,
      EpaperImageFormat::kFrameBytes, EpaperImageFormat::crc32(raw.data()+40, raw.size()-40),73};
  EpaperImageFormat::encodeHeader(header, raw.data(), raw.size());
  storeGzip(f, gzipBytes(raw));
  class Source final : public EpaperFrameSource {
   public:
    explicit Source(EpaperGzipReader &reader) : reader_(reader) {}
    size_t size() const override { return EpaperImageFormat::kFrameBytes; }
    size_t read(size_t offset, uint8_t *out, size_t capacity) const override {
      if (reader_.offset() != offset + 40) return 0;
      return reader_.read(out, capacity);
    }
    bool rewind() const override { return reader_.rewind() && reader_.skip(40); }
   private: EpaperGzipReader &reader_;
  };
  for (bool horizontal : {false, true}) for (bool vertical : {false, true}) {
    EpaperGzipReader reader(&f.storage);
    assert(reader.open(EpaperService::kStoredImageName).result.ok());
    assert(reader.complete() && reader.rewind() && reader.skip(40));
    Source source(reader);
    EpaperOrientedFrameSource oriented(source, EpaperImageFormat::kWidth, EpaperImageFormat::kHeight, horizontal, vertical);
    std::vector<uint8_t> result(source.size());
    for (size_t offset = 0; offset < result.size();) {
      const auto count = oriented.read(offset, result.data() + offset, std::min(size_t{4096}, result.size()-offset));
      assert(count); offset += count;
      assert(f.storage.beginUpload("unrelated", 1).result.status == UserDataFileStatus::Busy);
    }
    const size_t rowBytes = EpaperPanelProfile::Active::rowBytes;
    for (size_t y = 0; y < EpaperImageFormat::kHeight; ++y) for (size_t x = 0; x < rowBytes; ++x) {
      uint8_t expected = raw[40 + (vertical ? EpaperImageFormat::kHeight-1-y : y)*rowBytes + (horizontal ? rowBytes-1-x : x)];
      if (horizontal) expected = static_cast<uint8_t>((expected << 4) | (expected >> 4));
      assert(result[y*rowBytes+x] == expected);
    }
  }
  assert(nativefs::backend.handles == 0);
}

int main(int argc, char **) {
  testUploadIdleCleanup();
  testPrewakeMarkerFailures();
  testGzipStorageDownloadsAndFailures();
  testGzipMountingOrientation();
  testMarkerRetryRecovery();
  testCooldownWithoutMainLoop();
  if (argc > 1) {
    Fixture f;
    auto uploaded = f.storage.beginUpload(EpaperService::kStoredImageName, 1);
    const uint8_t corrupt = 0;
    assert(f.storage.writeUpload(uploaded.sessionId, 0, &corrupt, 1).ok());
    assert(f.storage.finishUpload(uploaded.sessionId).result.ok());
    assert(!f.service.refreshMetadata().ok());
    assert(f.service.snapshot().stored.present);
    auto occupied = f.storage.beginDownload(EpaperService::kStoredImageName, "");
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
