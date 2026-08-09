#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

#include "modules/epaper/Epd7In3E.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

class FakeTransport final : public EpdTransport {
 public:
  bool ready() const override { return isReady; }

  bool setReset(bool high) override {
    resetLevels.push_back(high);
    return !failReset;
  }

  bool busyHigh() const override {
    if (!refreshIssued) return !busyBeforeRefresh;
    if (refreshNeverAsserts) return true;
    if (refreshStuckLow) return false;
    if (refreshBusyReads++ == 0) return false;
    return true;
  }

  bool writeCommand(uint8_t command) override {
    commands.push_back(command);
    if (command == 0x12) refreshIssued = true;
    return command != failCommand;
  }

  bool writeData(const uint8_t *data, size_t length) override {
    if (data == nullptr || length == 0 || failData) return false;
    dataLengths.push_back(length);
    dataFirstBytes.push_back(data[0]);
    now += advancePerDataMs;
    return true;
  }

  void logicalQuiesce() override { ++quiesceCount; }
  void delayMs(uint32_t durationMs) override { now += durationMs; }
  void yieldCpu() override { ++yieldCount; }
  uint32_t nowMs() const override { return now; }

  bool isReady = true;
  bool failReset = false;
  bool failData = false;
  uint8_t failCommand = 0xFF;
  bool busyBeforeRefresh = false;
  bool refreshNeverAsserts = false;
  bool refreshStuckLow = false;
  uint32_t advancePerDataMs = 0;
  mutable uint32_t now = 0;
  mutable size_t refreshBusyReads = 0;
  mutable bool refreshIssued = false;
  size_t quiesceCount = 0;
  size_t yieldCount = 0;
  std::vector<bool> resetLevels;
  std::vector<uint8_t> commands;
  std::vector<size_t> dataLengths;
  std::vector<uint8_t> dataFirstBytes;
};

class ShortSource final : public EpaperFrameSource {
 public:
  size_t size() const override { return EpaperImageFormat::kFrameBytes; }
  size_t read(size_t offset, uint8_t *output, size_t capacity) const override {
    if (offset >= 8192) return 0;
    const size_t count = std::min<size_t>(capacity, 8192 - offset);
    std::fill(output, output + count, 0x11);
    return count;
  }
};

bool containsOrdered(const std::vector<uint8_t> &actual,
                     const std::vector<uint8_t> &expected) {
  size_t expectedIndex = 0;
  for (uint8_t value : actual) {
    if (expectedIndex < expected.size() && value == expected[expectedIndex]) {
      ++expectedIndex;
    }
  }
  return expectedIndex == expected.size();
}

void testNormalStreamingAndShutdown() {
  FakeTransport transport;
  Epd7In3E driver;
  EpaperWhiteFrameSource source;
  expect(driver.begin(&transport) && driver.initialize(),
         "ready transport should initialize the panel command sequence");
  expect(transport.resetLevels == std::vector<bool>({true, false, true}),
         "panel reset should use the reviewed high-low-high pulse");
  expect(driver.state() == Epd7In3E::State::Powered && driver.panelMayBeActive(),
         "successful power-on should mark the panel active");
  expect(driver.transferAndRefresh(source),
         "complete streaming source should transfer and refresh");
  expect(driver.transferredBytes() == EpaperImageFormat::kFrameBytes &&
             transport.yieldCount ==
                 (EpaperImageFormat::kFrameBytes + Epd7In3E::kTransferChunkBytes - 1) /
                     Epd7In3E::kTransferChunkBytes,
         "driver should send exactly 192000 bytes and yield after every 4 KiB chunk");
  expect(driver.shutdown(), "normal refresh should complete protocol shutdown");
  expect(driver.state() == Epd7In3E::State::Sleeping &&
             !driver.panelMayBeActive() && transport.quiesceCount == 2,
         "deep sleep should finish with logical quiesce and a sleeping state");
  expect(containsOrdered(transport.commands,
                         {0xAA, 0x01, 0x00, 0x03, 0x05, 0x06, 0x08, 0x30,
                          0x50, 0x60, 0x61, 0x84, 0xE3, 0x04, 0x10, 0x04,
                          0x06, 0x12, 0x02, 0x07}),
         "normal command sequence should include init, refresh, power-off, and deep sleep");
  expect(transport.dataFirstBytes.size() >= 2 &&
             transport.dataFirstBytes[transport.dataFirstBytes.size() - 2] == 0x00 &&
             transport.dataFirstBytes.back() == 0xA5,
         "shutdown data should end with Power OFF 0x00 and Deep Sleep 0xA5");
}

void testTransferAndRefreshCanBeSeparated() {
  FakeTransport transport;
  Epd7In3E driver;
  EpaperWhiteFrameSource source;
  expect(driver.begin(&transport) && driver.initialize() &&
             driver.transferFrame(source),
         "frame transfer should complete independently of physical refresh");
  expect(driver.state() == Epd7In3E::State::FrameTransferred &&
             !transport.refreshIssued,
         "separate transfer must stop before issuing the refresh command");
  expect(driver.refresh() && transport.refreshIssued && driver.shutdown(),
         "physical refresh and shutdown should remain independently callable");
}

void testSourceAndTransportFailuresRemainShutdownEligible() {
  {
    FakeTransport transport;
    Epd7In3E driver;
    ShortSource source;
    expect(driver.begin(&transport) && driver.initialize(),
           "source-failure fixture should initialize");
    expect(!driver.transferAndRefresh(source) &&
               driver.lastError() == EpdDriverError::SourceReadFailed &&
               driver.panelMayBeActive(),
           "short source should fail while preserving the need for shutdown");
    expect(driver.shutdown(),
           "source failure should still allow graceful Power OFF and Deep Sleep");
  }

  {
    FakeTransport transport;
    Epd7In3E driver;
    EpaperWhiteFrameSource source;
    expect(driver.begin(&transport) && driver.initialize(),
           "shutdown-failure fixture should initialize");
    expect(driver.transferAndRefresh(source),
           "shutdown-failure fixture should reach refreshed state");
    transport.failCommand = 0x02;
    expect(!driver.shutdown() && driver.lastError() == EpdDriverError::PowerOffFailed &&
               driver.state() == Epd7In3E::State::Fault &&
               driver.panelMayBeActive() && transport.quiesceCount == 2,
           "Power OFF failure should logical-quiesce but retain unknown active state");
  }
}

void testBusyAndOperationTimeouts() {
  {
    FakeTransport transport;
    transport.refreshStuckLow = true;
    Epd7In3E driver;
    EpaperWhiteFrameSource source;
    expect(driver.begin(&transport) && driver.initialize(),
           "BUSY timeout fixture should initialize");
    expect(!driver.transferAndRefresh(source) &&
               driver.lastError() == EpdDriverError::BusyTimeout &&
               driver.panelMayBeActive(),
           "BUSY stuck low after refresh should fail at the bounded timeout");
    expect(!driver.shutdown() &&
               driver.lastError() == EpdDriverError::PowerOffFailed &&
               driver.panelMayBeActive() && driver.state() == Epd7In3E::State::Fault,
           "BUSY stuck low must make shutdown unconfirmed and remain fail closed");
  }

  {
    FakeTransport transport;
    transport.advancePerDataMs = 2000;
    Epd7In3E driver;
    EpaperWhiteFrameSource source;
    expect(driver.begin(&transport) && driver.initialize(),
           "watchdog fixture should initialize");
    expect(!driver.transferAndRefresh(source) &&
               driver.lastError() == EpdDriverError::OperationWatchdogTimeout &&
               driver.panelMayBeActive(),
           "90-second operation watchdog should abort a stalled frame transfer");
    transport.advancePerDataMs = 0;
    expect(driver.shutdown(),
           "operation watchdog expiry must not suppress protocol shutdown cleanup");
  }

  {
    FakeTransport transport;
    transport.refreshNeverAsserts = true;
    Epd7In3E driver;
    EpaperWhiteFrameSource source;
    expect(driver.begin(&transport) && driver.initialize(),
           "BUSY assertion fixture should initialize");
    expect(!driver.transferAndRefresh(source) &&
               driver.lastError() == EpdDriverError::BusyNeverAsserted,
           "refresh BUSY that never asserts should have a distinct timeout");
    expect(driver.shutdown(), "BUSY assertion failure should remain shutdown eligible");
  }
}

}  // namespace

int main() {
  testNormalStreamingAndShutdown();
  testTransferAndRefreshCanBeSeparated();
  testSourceAndTransportFailuresRemainShutdownEligible();
  testBusyAndOperationTimeouts();
  if (failures != 0) {
    std::cerr << failures << " e-paper driver test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "E-paper streaming driver and shutdown tests passed\n";
  return EXIT_SUCCESS;
}
