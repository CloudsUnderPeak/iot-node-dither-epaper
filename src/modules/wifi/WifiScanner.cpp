#include "WifiScanner.h"
#include "core/SemaphoreGuard.h"

Result WifiScanner::begin(WifiRadio *radio, WifiScanDriver *driver, WifiManager *manager) {
  if (!radio || !driver || !manager) return invalidInput("missing scan dependencies");
  radio_ = radio;
  driver_ = driver;
  manager_ = manager;
  mutex_ = xSemaphoreCreateMutex();
  return mutex_ ? okResult() : outOfSpace("failed to create scan mutex");
}

WifiScanResult WifiScanner::start(uint32_t nowMs) {
  WifiScanResult outcome;
  SemaphoreGuard lock(mutex_, 0);
  if (!lock.locked()) { outcome.busy = true; return outcome; }
  if (state_ == State::Unavailable) {
    outcome.result = networkError("scan unavailable after unconfirmed stop");
    return outcome;
  }
  if (restarting_ || state_ != State::Idle || resultReady_) {
    outcome.busy = true;
    return outcome;
  }
  // Same lock order as queueStaTest: radio -> manager state. Reservation and
  // connect admission cannot both succeed in the check-to-start gap.
  WifiRadioGuard radioLock(radio_, 0);
  if (!radioLock.locked()) { outcome.busy = true; return outcome; }
  if (manager_->testBlocksScan()) { outcome.connectBusy = true; return outcome; }
  if (manager_->staConnectionBlocksScan()) { outcome.busy = true; return outcome; }
  if (hasRun_ && nowMs - lastCompletedMs_ < kMinIntervalMs) {
    outcome.retryAfterSeconds = (kMinIntervalMs - (nowMs - lastCompletedMs_) + 999U) / 1000U;
    return outcome;
  }
  radio_->reserveScanLocked();
  activeId_ = nextId_++;
  if (activeId_ == 0) activeId_ = nextId_++;
  result_ = {};
  interested_ = true;
  startedMs_ = nowMs;
  state_ = State::Starting;
  outcome.operationId = activeId_;
  return outcome;
}

void WifiScanner::terminalLocked(uint32_t nowMs, bool success) {
  result_.result = success ? okResult() : networkError("wifi scan failed");
  result_.operationId = activeId_;
  resultReady_ = interested_;
  hasRun_ = true;
  lastCompletedMs_ = nowMs;
}

void WifiScanner::collectLocked(int count) {
  for (int index = 0; index < count; ++index) {
    const auto network = driver_->network(static_cast<size_t>(index));
    if (network.rssi <= kWifiScanMinRssi) continue;
    size_t position = 0;
    while (position < result_.count && result_.networks[position].rssi >= network.rssi) ++position;
    if (position >= kWifiScanMaxResults) continue;
    const size_t newCount = result_.count < kWifiScanMaxResults ? result_.count + 1 : result_.count;
    for (size_t cursor = newCount - 1; cursor > position; --cursor) {
      result_.networks[cursor] = result_.networks[cursor - 1];
    }
    result_.networks[position] = network;
    result_.count = newCount;
  }
}

void WifiScanner::poll(uint32_t nowMs, bool restarting) {
  SemaphoreGuard lock(mutex_, 0);
  if (!lock.locked()) return;
  restarting_ = restarting;
  if (state_ == State::Idle || state_ == State::Unavailable) return;
  if (!radio_->lockScan(0)) return;
  if (state_ == State::Starting) {
    if (!interested_ || restarting || nowMs - startedMs_ >= kDeadlineMs) {
      terminalLocked(nowMs, false);
      state_ = State::Idle;
      radio_->releaseScanLocked();
    } else {
      const int started = driver_->start();
      if (started == -2) {
        terminalLocked(nowMs, false);
        state_ = State::Idle;
        radio_->releaseScanLocked();
      } else {
        state_ = State::Scanning;
      }
    }
  } else if (state_ == State::Scanning) {
    const int count = driver_->completion();
    if (count >= 0 && driver_->stopConfirmed()) {
      const bool timely = nowMs - startedMs_ < kDeadlineMs && !restarting;
      if (interested_ && timely) collectLocked(count);
      driver_->clearResults();
      terminalLocked(nowMs, timely);
      state_ = State::Idle;
      radio_->releaseScanLocked();
    } else if (count == -2 || !interested_ || restarting || nowMs - startedMs_ >= kDeadlineMs) {
      // The client gets a terminal result now; ownership remains until a real
      // stop/completion ACK. scanDelete alone is never treated as stopping.
      terminalLocked(nowMs, false);
      driver_->requestStop();
      stopStartedMs_ = nowMs;
      state_ = State::Stopping;
    }
  } else if (state_ == State::Stopping) {
    if (driver_->stopConfirmed()) {
      driver_->clearResults();
      state_ = State::Idle;
      radio_->releaseScanLocked();
    } else if (nowMs - stopStartedMs_ >= kStopDeadlineMs) {
      state_ = State::Unavailable; // fail closed, retain radio reservation
    }
  }
  radio_->unlock();
}

bool WifiScanner::takeResult(uint32_t operationId, WifiScanResult &result) {
  SemaphoreGuard lock(mutex_, 0);
  if (!lock.locked()) return false;
  if (operationId == 0 || operationId != activeId_) {
    result.result = networkError("scan operation is no longer active");
    return true;
  }
  if (!resultReady_) return false;
  result = result_;
  resultReady_ = false;
  interested_ = false;
  result_ = {};
  return true;
}

void WifiScanner::cancelInterest(uint32_t operationId) {
  SemaphoreGuard lock(mutex_, portMAX_DELAY);
  if (!lock.locked() || operationId != activeId_) return;
  interested_ = false;
  resultReady_ = false;
}
