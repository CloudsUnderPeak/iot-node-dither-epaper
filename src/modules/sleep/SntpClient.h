#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>

class SntpClient {
 public:
  bool start();
  void stop();
  bool takeSample(int64_t &epoch);
  void acceptSample(int64_t epoch, uint32_t generation);
  bool running() const { return running_; }

 private:
  volatile bool running_ = false;
  volatile bool sampleReady_ = false;
  int64_t sampleEpoch_ = 0;
  uint32_t generation_ = 0;
  uint32_t sampleGeneration_ = 0;
};
