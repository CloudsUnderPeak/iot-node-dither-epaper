#pragma once
#include "core/ProjectFeatures.h"
#if !IOT_FEATURE_EPAPER
#include "SystemRestartCoordinator.h"
#include "modules/storage/UserDataStorage.h"
#include <Arduino.h>

// The scheduler supplies radio cleanup readiness. File callbacks keep ownership
// until this coordinator acquires the same gate used by the panel coordinator.
class BasicRestartCoordinator : public SystemRestartCoordinator {
 public:
  explicit BasicRestartCoordinator(UserDataStorage *storage = nullptr) : storage_(storage) {}
  RestartRequest requestRestart(uint32_t nowMs) override {
    if (progress_ == RestartProgress::Draining || progress_ == RestartProgress::Ready)
      return RestartRequest::AlreadyPending;
    startedMs_ = nowMs;
    progress_ = RestartProgress::Draining;
    return RestartRequest::Accepted;
  }
  void pollRestart(uint32_t nowMs, bool allowRestart = true) override {
    if (progress_ != RestartProgress::Draining) return;
    if (nowMs - startedMs_ >= 150000U) { progress_ = RestartProgress::Failed; return; }
    if (!allowRestart) return;
#if IOT_FEATURE_STORAGE
    if (!storage_ || !storage_->reserveRestart()) return;
#endif
    progress_ = RestartProgress::Ready;
    ESP.restart();
  }
  RestartProgress restartProgress() const override { return progress_; }
 private:
  UserDataStorage *storage_;
  uint32_t startedMs_ = 0;
  RestartProgress progress_ = RestartProgress::Idle;
};
#endif
