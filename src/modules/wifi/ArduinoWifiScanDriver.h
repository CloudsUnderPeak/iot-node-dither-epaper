#pragma once
#include <atomic>
#include "WifiScanDriver.h"

class ArduinoWifiScanDriver final : public WifiScanDriver {
 public:
  int start() override;
  int completion() override;
  WifiScanNetwork network(size_t index) override;
  bool requestStop() override;
  bool stopConfirmed() const override { return done_.load(); }
  void clearResults() override;
 private:
  bool listening_ = false;
  std::atomic<bool> done_{false};
};
