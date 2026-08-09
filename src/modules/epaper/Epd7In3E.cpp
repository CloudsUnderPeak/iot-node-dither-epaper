#include "Epd7In3E.h"

namespace {
constexpr uint8_t kRefreshData = 0x00;
}

bool Epd7In3E::fail(EpdDriverError error) {
  lastError_ = error;
  state_ = State::Fault;
  return false;
}

bool Epd7In3E::begin(EpdTransport *transport) {
  if (transport == nullptr || !transport->ready()) {
    return fail(EpdDriverError::TransportFailure);
  }
  transport_ = transport;
  transport_->logicalQuiesce();
  state_ = State::Quiesced;
  lastError_ = EpdDriverError::None;
  operationStarted_ = false;
  panelMayBeActive_ = false;
  transferredBytes_ = 0;
  return true;
}

bool Epd7In3E::withinOperationBudget() {
  if (!operationStarted_) return true;
  if (static_cast<uint32_t>(transport_->nowMs() - operationStartedAtMs_) <
      kOperationWatchdogMs) {
    return true;
  }
  return fail(EpdDriverError::OperationWatchdogTimeout);
}

bool Epd7In3E::sendCommandData(uint8_t command,
                               const uint8_t *data,
                               size_t length) {
  if (!withinOperationBudget()) return false;
  if (!transport_->writeCommand(command) ||
      (length != 0 && !transport_->writeData(data, length))) {
    return fail(EpdDriverError::TransportFailure);
  }
  return true;
}

bool Epd7In3E::waitUntilIdle(uint32_t timeoutMs,
                            EpdDriverError timeoutError,
                            bool enforceOperationBudget) {
  const uint32_t startedAt = transport_->nowMs();
  while (!transport_->busyHigh()) {
    if (enforceOperationBudget && !withinOperationBudget()) return false;
    if (static_cast<uint32_t>(transport_->nowMs() - startedAt) >= timeoutMs) {
      return fail(timeoutError);
    }
    transport_->delayMs(1);
  }
  return true;
}

bool Epd7In3E::waitForBusyCycle() {
  const uint32_t startedAt = transport_->nowMs();
  while (transport_->busyHigh()) {
    if (!withinOperationBudget()) return false;
    if (static_cast<uint32_t>(transport_->nowMs() - startedAt) >=
        kBusyAssertTimeoutMs) {
      return fail(EpdDriverError::BusyNeverAsserted);
    }
    transport_->delayMs(1);
  }
  return waitUntilIdle(kRefreshTimeoutMs, EpdDriverError::BusyTimeout);
}

bool Epd7In3E::initialize() {
  if (state_ != State::Quiesced || transport_ == nullptr) {
    return fail(EpdDriverError::InvalidState);
  }
  state_ = State::Initializing;
  operationStartedAtMs_ = transport_->nowMs();
  operationStarted_ = true;

  if (!transport_->setReset(true)) return fail(EpdDriverError::TransportFailure);
  transport_->delayMs(20);
  if (!transport_->setReset(false)) return fail(EpdDriverError::TransportFailure);
  transport_->delayMs(2);
  if (!transport_->setReset(true)) return fail(EpdDriverError::TransportFailure);
  transport_->delayMs(20);
  if (!waitUntilIdle(kResetTimeoutMs, EpdDriverError::BusyTimeout)) return false;
  transport_->delayMs(30);

  const uint8_t cmdh[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
  const uint8_t powerSetting[] = {0x3F};
  const uint8_t panelSetting[] = {0x5F, 0x69};
  const uint8_t powerOffSequence[] = {0x00, 0x54, 0x00, 0x44};
  const uint8_t boosterSoftStart[] = {0x40, 0x1F, 0x1F, 0x2C};
  const uint8_t boostSetting[] = {0x6F, 0x1F, 0x17, 0x49};
  const uint8_t buckBoostSetting[] = {0x6F, 0x1F, 0x1F, 0x22};
  const uint8_t pll[] = {0x03};
  const uint8_t vcomAndDataInterval[] = {0x3F};
  const uint8_t tcon[] = {0x02, 0x00};
  const uint8_t resolution[] = {0x03, 0x20, 0x01, 0xE0};
  const uint8_t vcomDc[] = {0x01};
  const uint8_t powerSaving[] = {0x2F};

  if (!sendCommandData(0xAA, cmdh, sizeof(cmdh)) ||
      !sendCommandData(0x01, powerSetting, sizeof(powerSetting)) ||
      !sendCommandData(0x00, panelSetting, sizeof(panelSetting)) ||
      !sendCommandData(0x03, powerOffSequence, sizeof(powerOffSequence)) ||
      !sendCommandData(0x05, boosterSoftStart, sizeof(boosterSoftStart)) ||
      !sendCommandData(0x06, boostSetting, sizeof(boostSetting)) ||
      !sendCommandData(0x08, buckBoostSetting, sizeof(buckBoostSetting)) ||
      !sendCommandData(0x30, pll, sizeof(pll)) ||
      !sendCommandData(0x50, vcomAndDataInterval,
                       sizeof(vcomAndDataInterval)) ||
      !sendCommandData(0x60, tcon, sizeof(tcon)) ||
      !sendCommandData(0x61, resolution, sizeof(resolution)) ||
      !sendCommandData(0x84, vcomDc, sizeof(vcomDc)) ||
      !sendCommandData(0xE3, powerSaving, sizeof(powerSaving))) {
    return false;
  }

  if (!transport_->writeCommand(0x04)) return fail(EpdDriverError::TransportFailure);
  panelMayBeActive_ = true;
  if (!waitUntilIdle(kPowerTimeoutMs, EpdDriverError::BusyTimeout)) return false;
  state_ = State::Powered;
  return true;
}

bool Epd7In3E::transferFrame(const EpaperFrameSource &source) {
  if (state_ != State::Powered) return fail(EpdDriverError::InvalidState);
  if (source.size() != EpaperImageFormat::kFrameBytes) {
    return fail(EpdDriverError::FrameSizeMismatch);
  }
  if (!transport_->writeCommand(0x10)) return fail(EpdDriverError::TransportFailure);

  transferredBytes_ = 0;
  while (transferredBytes_ < source.size()) {
    if (!withinOperationBudget()) return false;
    const size_t remaining = source.size() - transferredBytes_;
    const size_t capacity = remaining < sizeof(transferBuffer_) ? remaining
                                                                : sizeof(transferBuffer_);
    const size_t count = source.read(transferredBytes_, transferBuffer_, capacity);
    if (count == 0 || count > capacity) return fail(EpdDriverError::SourceReadFailed);
    if (!transport_->writeData(transferBuffer_, count)) {
      return fail(EpdDriverError::TransportFailure);
    }
    transferredBytes_ += count;
    transport_->yieldCpu();
  }
  if (transferredBytes_ != EpaperImageFormat::kFrameBytes) {
    return fail(EpdDriverError::FrameSizeMismatch);
  }
  state_ = State::FrameTransferred;
  return true;
}

bool Epd7In3E::refresh() {
  if (state_ != State::FrameTransferred) {
    return fail(EpdDriverError::InvalidState);
  }
  if (!transport_->writeCommand(0x04)) return fail(EpdDriverError::TransportFailure);
  panelMayBeActive_ = true;
  if (!waitUntilIdle(kPowerTimeoutMs, EpdDriverError::BusyTimeout)) return false;
  const uint8_t boostSetting[] = {0x6F, 0x1F, 0x17, 0x49};
  if (!sendCommandData(0x06, boostSetting, sizeof(boostSetting)) ||
      !sendCommandData(0x12, &kRefreshData, 1)) {
    return false;
  }
  if (!waitForBusyCycle()) return false;
  state_ = State::Refreshed;
  return true;
}

bool Epd7In3E::transferAndRefresh(const EpaperFrameSource &source) {
  return transferFrame(source) && refresh();
}

bool Epd7In3E::shutdown() {
  if (transport_ == nullptr || !transport_->ready()) {
    return fail(EpdDriverError::TransportFailure);
  }
  if (!panelMayBeActive_) {
    transport_->logicalQuiesce();
    state_ = State::Sleeping;
    lastError_ = EpdDriverError::None;
    operationStarted_ = false;
    return true;
  }

  const uint8_t powerOffData[] = {0x00};
  if (!transport_->writeCommand(0x02) ||
      !transport_->writeData(powerOffData, sizeof(powerOffData)) ||
      !waitUntilIdle(kPowerOffTimeoutMs, EpdDriverError::PowerOffFailed, false)) {
    transport_->logicalQuiesce();
    return fail(EpdDriverError::PowerOffFailed);
  }
  const uint8_t deepSleepKey[] = {0xA5};
  if (!transport_->writeCommand(0x07) ||
      !transport_->writeData(deepSleepKey, sizeof(deepSleepKey))) {
    transport_->logicalQuiesce();
    return fail(EpdDriverError::SleepFailed);
  }
  transport_->delayMs(2000);
  transport_->logicalQuiesce();
  panelMayBeActive_ = false;
  operationStarted_ = false;
  state_ = State::Sleeping;
  lastError_ = EpdDriverError::None;
  return true;
}

void Epd7In3E::logicalQuiesce() {
  if (transport_ != nullptr) transport_->logicalQuiesce();
  if (state_ != State::Sleeping) state_ = State::Fault;
}
