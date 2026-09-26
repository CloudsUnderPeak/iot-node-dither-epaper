#include "SleepFeatures.h"

#if ENABLE_SLEEP_SCHEDULER
#include "SntpClient.h"

#include <esp_sntp.h>
#include <sys/time.h>

namespace {
portMUX_TYPE sampleMux = portMUX_INITIALIZER_UNLOCKED;
SntpClient *activeClient = nullptr;
uint32_t activeGeneration = 0;
}

// ESP-IDF marks this hook weak. Holding the received sample for the loop owner
// prevents an lwIP callback from changing the clock during a config transaction.
extern "C" void sntp_sync_time(struct timeval *value) {
  if (value == nullptr) return;
  portENTER_CRITICAL(&sampleMux);
  if (activeClient != nullptr)
    activeClient->acceptSample(value->tv_sec, activeGeneration);
  portEXIT_CRITICAL(&sampleMux);
}

bool SntpClient::start() {
  stop();
  portENTER_CRITICAL(&sampleMux);
  if (++generation_ == 0) ++generation_;
  activeClient = this;
  activeGeneration = generation_;
  running_ = true;
  sampleReady_ = false;
  sampleGeneration_ = 0;
  portEXIT_CRITICAL(&sampleMux);
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_setservername(1, "time.google.com");
  esp_sntp_init();
  return true;
}

void SntpClient::stop() {
  portENTER_CRITICAL(&sampleMux);
  const bool wasRunning = running_;
  if (activeClient == this) {
    activeClient = nullptr;
    activeGeneration = 0;
  }
  running_ = false;
  sampleReady_ = false;
  sampleGeneration_ = 0;
  portEXIT_CRITICAL(&sampleMux);
  if (wasRunning) esp_sntp_stop();
}

bool SntpClient::takeSample(int64_t &epoch) {
  portENTER_CRITICAL(&sampleMux);
  const bool available =
      running_ && sampleReady_ && sampleGeneration_ == generation_;
  if (available) {
    epoch = sampleEpoch_;
    sampleReady_ = false;
  }
  portEXIT_CRITICAL(&sampleMux);
  return available;
}

void SntpClient::acceptSample(int64_t epoch, uint32_t generation) {
  // Called only while sampleMux is held by the weak IDF hook. stop() removes
  // the active client before stopping lwIP; the generation also prevents a
  // mailbox value from an earlier start from being consumed after restart.
  if (!running_ || generation == 0 || generation != generation_ ||
      epoch < 1704067200LL || epoch >= 4102444800LL) return;
  sampleEpoch_ = epoch;
  sampleGeneration_ = generation;
  sampleReady_ = true;
}
#endif
