#include <cassert>
#include <future>
#include <iostream>
#include "api/ApiRouter.h"
#include "modules/http/StreamingSessionBridge.h"

struct RouterFixture {
  ConfigService config;
  WifiManager wifi;
  WifiScanner scanner;
  EmbeddedWebAssets web;
  FlashStorage flash;
  UserDataStorage userData;
  StorageLifecycle lifecycle;
  AuthService auth;
  RuntimeActionScheduler runtime;
  EpaperService epaper;
  EpaperCalibrationService calibration;
  BatteryMonitor battery;
  BootDiagnostics diagnostics;
  ApiRouter router;

  explicit RouterFixture(
      DeviceResetReason resetReason = DeviceResetReason::Software)
      : diagnostics(resetReason) {
    auth.config = &config;
    runtime.available = true;
    epaper.current.lastResetReason = deviceResetReasonToString(resetReason);
    const ApiRouterDeps deps{
        config,
        wifi,
        scanner,
        web,
        flash,
        userData,
        lifecycle,
        auth,
        runtime,
        epaper,
        calibration,
        battery,
        diagnostics,
    };
    assert(router.begin(deps).ok());
    assert(userData.begin().ok());
  }
};


int main() {
  nativeMillis = 0;
  RouterFixture f;
  StreamingSessionBridge bridge;
  const char *path = "/api/storage/files/bridge.bin";
  const uint8_t bytes[] = {1, 2, 3};
  auto start = bridge.run([&] { return f.router.prepareFileUpload("valid-token", path, 3); });
  assert(start.ready);
  std::promise<void> entered, release, disconnectStarted;
  auto releaseFuture = release.get_future();
  nativefs::backend.before = [&](const char *operation) {
    if (std::string(operation) == "write") {
      entered.set_value();
      releaseFuture.wait();
    }
  };
  auto writer = std::async(std::launch::async, [&] {
    return bridge.run([&] { return f.router.writeFileUpload(start.sessionId, 0, bytes, 3); });
  });
  entered.get_future().wait();
  auto disconnect = std::async(std::launch::async, [&] {
    disconnectStarted.set_value();
    bridge.run([&] { f.router.abortFileUpload(start.sessionId); });
  });
  disconnectStarted.get_future().wait();
  assert(nativefs::backend.handles == 1); // blocked write still owns its handle
  release.set_value();
  assert(writer.get().success);
  disconnect.get();
  nativefs::backend.before = {};
  assert(nativefs::backend.handles == 0);
  auto next = bridge.run([&] { return f.router.prepareFileUpload("valid-token", path, 3); });
  assert(next.ready);
  bridge.run([&] { f.router.abortFileUpload(start.sessionId); });
  assert(bridge.run([&] { return f.router.writeFileUpload(next.sessionId, 0, bytes, 3); }).success);
  assert(bridge.run([&] { return f.router.finishFileUpload(next.sessionId, path); }).success);
  auto download = bridge.run([&] { return f.router.prepareFileDownload("valid-token", path, "bytes=1-2"); });
  assert(download.ready && download.contentLength == 2);
  uint8_t buffer[3]{};
  assert(bridge.run([&] { return f.router.readFileDownload(download.sessionId, buffer, 1); }).bytesRead == 1);
  assert(buffer[0] == 2);
  bridge.run([&] { f.router.finishFileDownload(download.sessionId); });
  auto newer = bridge.run([&] { return f.router.prepareFileDownload("valid-token", path, ""); });
  assert(newer.ready);
  bridge.run([&] { f.router.finishFileDownload(download.sessionId); });
  assert(bridge.run([&] { return f.router.readFileDownload(newer.sessionId, buffer, 3); }).bytesRead == 3);
  assert(buffer[0] == 1 && buffer[2] == 3 && nativefs::backend.handles == 0);
  bridge.run([&] { f.router.finishFileDownload(newer.sessionId); });
  const char *slowPath = "/api/storage/files/slow.bin";
  auto slow = bridge.run([&] { return f.router.prepareFileUpload("valid-token", slowPath, 4); });
  assert(slow.ready);
  nativeMillis = ApiRouter::kUploadIdleTimeoutMs - 1;
  assert(bridge.run([&] { return f.router.expireIdleUpload(nativeMillis); }).sessionId == 0);
  assert(bridge.run([&] { return f.router.writeFileUpload(slow.sessionId, 0, bytes, 2); }).success);
  nativeMillis += ApiRouter::kUploadIdleTimeoutMs - 1;
  assert(bridge.run([&] { return f.router.expireIdleUpload(nativeMillis); }).sessionId == 0);
  assert(bridge.run([&] { return f.router.writeFileUpload(slow.sessionId, 2, bytes, 2); }).success);
  assert(bridge.run([&] { return f.router.finishFileUpload(slow.sessionId, slowPath); }).success);
  auto stalled = bridge.run([&] { return f.router.prepareFileUpload("valid-token", slowPath, 2); });
  assert(stalled.ready);
  nativeMillis += ApiRouter::kUploadIdleTimeoutMs;
  const auto expired = bridge.run([&] { return f.router.expireIdleUpload(nativeMillis); });
  assert(expired.sessionId == stalled.sessionId && !expired.epaper);
  assert(nativefs::backend.handles == 0);
  assert(!bridge.run([&] { return f.router.writeFileUpload(stalled.sessionId, 0, bytes, 2); }).success);
  auto preserved = bridge.run([&] { return f.router.prepareFileDownload("valid-token", slowPath, ""); });
  assert(preserved.ready && preserved.contentLength == 4);
  bridge.run([&] { f.router.finishFileDownload(preserved.sessionId); });
  nativeMillis = UINT32_MAX - 10;
  auto wrapped = bridge.run([&] { return f.router.prepareFileUpload("valid-token", slowPath, 2); });
  assert(wrapped.ready);
  nativeMillis += ApiRouter::kUploadIdleTimeoutMs - 1;
  assert(bridge.run([&] { return f.router.expireIdleUpload(nativeMillis); }).sessionId == 0);
  bridge.run([&] { f.router.abortFileUpload(stalled.sessionId); });
  assert(bridge.run([&] { return f.router.writeFileUpload(wrapped.sessionId, 0, bytes, 2); }).success);
  assert(bridge.run([&] { return f.router.finishFileUpload(wrapped.sessionId, slowPath); }).success);
  std::cout << "Streaming bridge -> real Router -> real storage callback lifecycle passed\n";
}
