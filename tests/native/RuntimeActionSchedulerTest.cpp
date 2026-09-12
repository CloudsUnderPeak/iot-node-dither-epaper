#include <cassert>
#include <iostream>
#include "modules/runtime/RuntimeActionScheduler.h"

class MemoryConfig final : public ConfigStore {
 public:
  Result load(DeviceConfig &) override { return storageError("empty"); }
  Result save(const DeviceConfig &) override { return okResult(); }
};
class Restart final : public SystemRestartCoordinator {
 public:
  size_t calls = 0;
  RestartProgress progress = RestartProgress::Idle;
  RestartRequest requestRestart(uint32_t) override {
    ++calls; progress = RestartProgress::Draining; return RestartRequest::Accepted;
  }
  void pollRestart(uint32_t, bool) override {}
  RestartProgress restartProgress() const override { return progress; }
};
int main() {
  MemoryConfig persistence;
  ConfigService config(persistence);
  WifiManager wifi;
  MdnsService mdns;
  CaptivePortalDnsService dns;
  Restart restart;
  RuntimeActionScheduler scheduler;
  assert(scheduler.begin(&config, &wifi, &mdns, &dns, &restart).ok());
  nativeMillis = UINT32_MAX - 20;
  scheduler.scheduleSystemReset(30);
  scheduler.scheduleSystemReset(100);
  nativeMillis = 8;
  scheduler.poll();
  assert(restart.calls == 0 && scheduler.snapshot().restartPending);
  nativeMillis = 9;
  scheduler.poll();
  assert(restart.calls == 1 && scheduler.snapshot().restartPending);
  scheduler.scheduleSystemReset(100);
  scheduler.poll();
  assert(restart.calls == 1 && scheduler.snapshot().restartPending);
  restart.progress = RestartProgress::Failed;
  scheduler.poll();
  assert(scheduler.snapshot().restartFailed && !scheduler.snapshot().restartPending);
  scheduler.poll();
  assert(restart.calls == 1);
  std::cout << "Real RuntimeActionScheduler deadline and failure tests passed\n";
}
