#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../core/Result.h"
#include "WifiRadio.h"
#include "WifiScanDriver.h"
#include "WifiManager.h"

constexpr size_t kWifiScanMaxResults = 20;
constexpr int32_t kWifiScanMinRssi = -75;

struct WifiScanResult {
  Result result = okResult();
  WifiScanNetwork networks[kWifiScanMaxResults];
  size_t count = 0;
  uint32_t retryAfterSeconds = 0;
  uint32_t operationId = 0;
  bool busy = false;
  bool connectBusy = false;
};

// Single owner for scan serialization, cooldown, result selection and
// encryption mapping shared by REST and the human console.
class WifiScanner {
 public:
  Result begin(WifiRadio *radio, WifiScanDriver *driver, WifiManager *manager);
  WifiScanResult start(uint32_t nowMs);
  void poll(uint32_t nowMs, bool restarting = false);
  bool takeResult(uint32_t operationId, WifiScanResult &result);
  void cancelInterest(uint32_t operationId);

 private:
  enum class State : uint8_t { Idle, Starting, Scanning, Stopping, Unavailable };
  static constexpr uint32_t kMinIntervalMs = 10000;
  static constexpr uint32_t kDeadlineMs = 15000;
  static constexpr uint32_t kStopDeadlineMs = 2000;
  SemaphoreHandle_t mutex_ = nullptr;
  WifiRadio *radio_ = nullptr;
  WifiScanDriver *driver_ = nullptr;
  WifiManager *manager_ = nullptr;
  State state_ = State::Idle;
  bool hasRun_ = false;
  bool interested_ = false;
  bool resultReady_ = false;
  bool restarting_ = false;
  uint32_t nextId_ = 1;
  uint32_t activeId_ = 0;
  uint32_t startedMs_ = 0;
  uint32_t stopStartedMs_ = 0;
  uint32_t lastCompletedMs_ = 0;
  WifiScanResult result_;
  void terminalLocked(uint32_t nowMs, bool success);
  void collectLocked(int count);
};
