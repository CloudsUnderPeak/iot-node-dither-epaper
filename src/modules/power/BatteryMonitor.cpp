#include "BatteryMonitor.h"

#include "board/BoardProfile.h"

namespace {

struct VoltagePoint {
  uint16_t milliVolts;
  uint8_t percent;
};

// A deliberately coarse single-cell Li-ion/LiPo voltage estimate. Voltage is
// the public ground truth; the percentage is only a UI-friendly approximation
// and must not be interpreted as a fuel-gauge measurement.
constexpr VoltagePoint kEstimateCurve[] = {
    {3300, 0},
    {3400, 5},
    {3550, 25},
    {3700, 50},
    {3870, 75},
    {4200, 100},
};

constexpr uint32_t kMinimumPlausibleBatteryMilliVolts = 2500;
constexpr uint32_t kMaximumPlausibleBatteryMilliVolts = 4400;

}  // namespace

Result BatteryMonitor::begin(BatteryAdc *adc, uint32_t nowMs) {
  constexpr Board::BatterySense sense = Board::ActiveProfile::kBatterySense;
  if (adc == nullptr || sense.pin < 0 || sense.dividerNumerator == 0 ||
      sense.dividerDenominator == 0) {
    return invalidInput("invalid battery monitor configuration");
  }
  if (Board::ActiveProfile::pinDisposition(sense.pin) !=
      Board::PinDisposition::BoardReserved) {
    return invalidInput("battery ADC is not a reserved board function");
  }
  if (!adc->begin(static_cast<uint8_t>(sense.pin))) {
    return invalidInput("battery ADC initialization failed");
  }

  adc_ = adc;
  portENTER_CRITICAL(&snapshotMux_);
  ready_ = true;
  portEXIT_CRITICAL(&snapshotMux_);

  // Publish an initial reading before the API starts. A zero pin voltage is a
  // valid sample when no battery is connected; only its percentage is unknown.
  sample(nowMs);
  return okResult();
}

void BatteryMonitor::poll(uint32_t nowMs) {
  if (!ready()) return;
  if (static_cast<uint32_t>(nowMs - lastAttemptMs_) < kSampleIntervalMs) return;
  sample(nowMs);
}

bool BatteryMonitor::ready() const {
  portENTER_CRITICAL(&snapshotMux_);
  const bool result = ready_;
  portEXIT_CRITICAL(&snapshotMux_);
  return result;
}

BatterySnapshot BatteryMonitor::snapshot(uint32_t nowMs) const {
  BatterySnapshot result;
  portENTER_CRITICAL(&snapshotMux_);
  result.sampleValid = sampleValid_;
  result.voltageMilliVolts = voltageMilliVolts_;
  result.sampleAgeMs = sampleValid_
                           ? static_cast<uint32_t>(nowMs - sampledAtMs_)
                           : 0;
  portEXIT_CRITICAL(&snapshotMux_);
  if (result.sampleValid) {
    result.estimate = estimatePercent(result.voltageMilliVolts);
  }
  return result;
}

BatteryEstimate BatteryMonitor::estimatePercent(uint32_t batteryMilliVolts) {
  BatteryEstimate result;
  if (batteryMilliVolts < kMinimumPlausibleBatteryMilliVolts ||
      batteryMilliVolts > kMaximumPlausibleBatteryMilliVolts) {
    return result;
  }

  result.available = true;
  if (batteryMilliVolts <= kEstimateCurve[0].milliVolts) {
    result.percent = kEstimateCurve[0].percent;
    return result;
  }

  constexpr size_t pointCount = sizeof(kEstimateCurve) / sizeof(kEstimateCurve[0]);
  for (size_t index = 1; index < pointCount; ++index) {
    if (batteryMilliVolts > kEstimateCurve[index].milliVolts) continue;
    const VoltagePoint &lower = kEstimateCurve[index - 1];
    const VoltagePoint &upper = kEstimateCurve[index];
    const uint32_t voltageOffset = batteryMilliVolts - lower.milliVolts;
    const uint32_t voltageSpan = upper.milliVolts - lower.milliVolts;
    const uint32_t percentSpan = upper.percent - lower.percent;
    result.percent = static_cast<uint8_t>(
        lower.percent + (voltageOffset * percentSpan + voltageSpan / 2) /
                            voltageSpan);
    return result;
  }

  result.percent = kEstimateCurve[pointCount - 1].percent;
  return result;
}

uint32_t BatteryMonitor::median(uint32_t *values, size_t count) {
  if (values == nullptr || count == 0) return 0;
  for (size_t index = 1; index < count; ++index) {
    const uint32_t value = values[index];
    size_t insertion = index;
    while (insertion > 0 && values[insertion - 1] > value) {
      values[insertion] = values[insertion - 1];
      --insertion;
    }
    values[insertion] = value;
  }
  return values[count / 2];
}

bool BatteryMonitor::sample(uint32_t nowMs) {
  constexpr Board::BatterySense sense = Board::ActiveProfile::kBatterySense;
  lastAttemptMs_ = nowMs;

  uint32_t pinMilliVolts[kSamplesPerReading]{};
  for (size_t index = 0; index < kSamplesPerReading; ++index) {
    if (!adc_->readMilliVolts(static_cast<uint8_t>(sense.pin),
                              pinMilliVolts[index])) {
      return false;
    }
  }

  const uint32_t filteredPinMilliVolts =
      median(pinMilliVolts, kSamplesPerReading);
  const uint64_t scaled =
      static_cast<uint64_t>(filteredPinMilliVolts) * sense.dividerNumerator;
  const uint32_t batteryMilliVolts = static_cast<uint32_t>(
      (scaled + sense.dividerDenominator / 2U) / sense.dividerDenominator);

  portENTER_CRITICAL(&snapshotMux_);
  voltageMilliVolts_ = batteryMilliVolts;
  sampledAtMs_ = nowMs;
  sampleValid_ = true;
  portEXIT_CRITICAL(&snapshotMux_);
  return true;
}
