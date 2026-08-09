#include <cstdlib>
#include <iostream>
#include <vector>

#include "modules/power/BatteryMonitor.h"

namespace {

int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

class FakeBatteryAdc final : public BatteryAdc {
 public:
  bool beginResult = true;
  bool readResult = true;
  uint8_t beginPin = 255;
  size_t reads = 0;
  std::vector<uint32_t> values;

  bool begin(uint8_t pin) override {
    beginPin = pin;
    return beginResult;
  }

  bool readMilliVolts(uint8_t pin, uint32_t &milliVolts) override {
    if (!readResult || pin != beginPin || values.empty()) return false;
    milliVolts = values[reads % values.size()];
    ++reads;
    return true;
  }
};

void testInitialMedianAndDivider() {
  FakeBatteryAdc adc;
  adc.values = {2000, 1980, 2010, 1990, 2005, 1970, 2100};
  BatteryMonitor monitor;
  expect(monitor.begin(&adc, 0).ok() && monitor.ready() && adc.beginPin == 0,
         "monitor should initialize the dedicated GPIO0 ADC");

  const BatterySnapshot snapshot = monitor.snapshot(25);
  expect(snapshot.sampleValid && snapshot.voltageMilliVolts == 4000 &&
             snapshot.sampleAgeMs == 25,
         "monitor should publish the median calibrated pin voltage times two");
  expect(snapshot.estimate.available && snapshot.estimate.percent == 85,
         "monitor should interpolate the coarse single-cell estimate");
}

void testNoBatteryKeepsVoltageAndOmitsEstimate() {
  FakeBatteryAdc adc;
  adc.values = {0};
  BatteryMonitor monitor;
  expect(monitor.begin(&adc, 0).ok(), "zero-volt ADC should remain a valid sample");
  const BatterySnapshot snapshot = monitor.snapshot(1);
  expect(snapshot.sampleValid && snapshot.voltageMilliVolts == 0 &&
             !snapshot.estimate.available,
         "no connected battery must not fabricate a percentage or presence state");
}

void testPollingCadenceAndFailedReadPreserveSnapshot() {
  FakeBatteryAdc adc;
  adc.values = {1900};
  BatteryMonitor monitor;
  expect(monitor.begin(&adc, 0).ok(), "monitor should begin before cadence test");
  const size_t initialReads = adc.reads;

  monitor.poll(BatteryMonitor::kSampleIntervalMs - 1);
  expect(adc.reads == initialReads,
         "poll should not sample before the fixed interval elapses");

  adc.values = {1950};
  monitor.poll(BatteryMonitor::kSampleIntervalMs);
  expect(adc.reads == initialReads + BatteryMonitor::kSamplesPerReading &&
             monitor.snapshot(BatteryMonitor::kSampleIntervalMs + 10)
                     .voltageMilliVolts == 3900,
         "poll should refresh the cached snapshot at the interval");

  adc.readResult = false;
  monitor.poll(BatteryMonitor::kSampleIntervalMs * 2);
  const BatterySnapshot preserved =
      monitor.snapshot(BatteryMonitor::kSampleIntervalMs * 2 + 20);
  expect(preserved.sampleValid && preserved.voltageMilliVolts == 3900 &&
             preserved.sampleAgeMs == BatteryMonitor::kSampleIntervalMs + 20,
         "a transient ADC failure should retain the last complete sample");
}

void testEstimateBounds() {
  expect(!BatteryMonitor::estimatePercent(0).available &&
             !BatteryMonitor::estimatePercent(4401).available,
         "implausible single-cell voltage should not produce an estimate");
  expect(BatteryMonitor::estimatePercent(2500).available &&
             BatteryMonitor::estimatePercent(2500).percent == 0 &&
             BatteryMonitor::estimatePercent(4200).percent == 100 &&
             BatteryMonitor::estimatePercent(4400).percent == 100,
         "plausible voltage should clamp the estimate to zero through one hundred");
}

}  // namespace

int main() {
  testInitialMedianAndDivider();
  testNoBatteryKeepsVoltageAndOmitsEstimate();
  testPollingCadenceAndFailedReadPreserveSnapshot();
  testEstimateBounds();
  if (failures != 0) {
    std::cerr << failures << " battery monitor test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Battery monitor tests passed\n";
  return EXIT_SUCCESS;
}
