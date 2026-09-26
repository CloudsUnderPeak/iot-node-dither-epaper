#include <cassert>
#include <iostream>
#include "modules/runtime/BasicRestartCoordinator.h"

int main() {
  ESP.restartCalls = 0;
#if IOT_FEATURE_STORAGE
  UserDataStorage storage;
  assert(storage.begin().ok());
  const auto upload = storage.beginUpload("lease.bin", 0);
  assert(upload.result.ok());
  BasicRestartCoordinator restart{&storage};
#else
  BasicRestartCoordinator restart;
#endif
  assert(restart.requestRestart(10) == RestartRequest::Accepted);
  assert(restart.requestRestart(11) == RestartRequest::AlreadyPending);
  restart.pollRestart(20, false);
  assert(restart.restartProgress() == RestartProgress::Draining && ESP.restartCalls == 0);
#if IOT_FEATURE_STORAGE
  restart.pollRestart(21);
  assert(restart.restartProgress() == RestartProgress::Draining && ESP.restartCalls == 0);
  assert(storage.finishUpload(upload.sessionId).result.ok());
#endif
  restart.pollRestart(22);
  assert(restart.restartProgress() == RestartProgress::Ready && ESP.restartCalls == 1);
#if IOT_FEATURE_STORAGE
  assert(storage.operationBusy());
  assert(storage.beginUpload("late.bin", 0).result.status == UserDataFileStatus::Busy);
#endif
  restart.pollRestart(23);
  assert(ESP.restartCalls == 1);

  BasicRestartCoordinator timeout;
  assert(timeout.requestRestart(UINT32_MAX - 100) == RestartRequest::Accepted);
  timeout.pollRestart(149898, false);
  assert(timeout.restartProgress() == RestartProgress::Draining);
  timeout.pollRestart(149899, false);
  assert(timeout.restartProgress() == RestartProgress::Failed && ESP.restartCalls == 1);
  std::cout << "Panel-free restart radio/file ownership and deadline passed\n";
}
