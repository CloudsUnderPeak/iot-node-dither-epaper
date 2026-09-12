#include <cassert>
#include <iostream>
#include <vector>
#include <memory>
#include "modules/wifi/WifiScanner.h"
#include "modules/http/PendingResponseSlot.h"
#include "support/WifiDoubles.h"

struct ScanDriver : WifiScanDriver {
  int completionValue = -1;
  unsigned starts = 0, stops = 0, clears = 0;
  bool ack = false;
  bool stopOk = true;
  std::vector<WifiScanNetwork> values;
  int start() override { ++starts; return -1; }
  int completion() override { return completionValue; }
  WifiScanNetwork network(size_t index) override { return values.at(index); }
  bool requestStop() override { ++stops; return stopOk; }
  bool stopConfirmed() const override { return ack; }
  void clearResults() override { ++clears; }
};
struct ScanFixture {
  WifiRadio radio;
  FakeWifiDriver wifiDriver;
  FakeClock clock;
  WifiManager manager;
  ScanDriver driver;
  WifiScanner scanner;
  DeviceConfig config = defaultDeviceConfig();
  ScanFixture() {
    assert(radio.begin().ok());
    assert(manager.begin(&radio, &wifiDriver, &clock).ok());
    WifiStatus status;
    assert(manager.apply(config, status).ok());
    assert(scanner.begin(&radio, &driver, &manager).ok());
  }
};
int main() {
  {
    ScanFixture f;
    auto start = f.scanner.start(0);
    assert(start.operationId != 0 && f.driver.starts == 0);
    assert(f.scanner.start(0).busy);
    auto candidate = f.config;
    candidate.wifiMode = WifiMode::Sta;
    uint32_t id = 0;
    assert(!f.manager.queueStaTest(candidate, f.config, id).ok());
    const unsigned modeCalls = f.wifiDriver.modeCount;
    assert(!f.manager.poll(f.config));
    assert(f.wifiDriver.modeCount == modeCalls);
    f.scanner.poll(0);
    assert(f.driver.starts == 1);
    WifiScanResult result;
    assert(!f.scanner.takeResult(start.operationId, result));
    for (int rssi = -80; rssi < -40; ++rssi) {
      WifiScanNetwork network;
      network.rssi = rssi;
      network.hidden = rssi == -41;
      f.driver.values.push_back(network);
    }
    f.driver.completionValue = f.driver.values.size();
    f.driver.ack = true;
    f.scanner.poll(100);
    assert(f.scanner.takeResult(start.operationId, result));
    assert(result.result.ok() && result.count == 20);
    assert(result.networks[0].rssi == -41 && result.networks[0].hidden);
    assert(result.networks[19].rssi == -60);
    assert(!f.radio.scanReserved() && f.driver.clears == 1);
    assert(f.scanner.start(101).retryAfterSeconds == 10);
    assert(f.scanner.start(10100).operationId != 0);
  }
  for (bool empty : {false, true}) {
    ScanFixture f;
    auto start = f.scanner.start(0);
    f.scanner.poll(0);
    if (!empty) {
      WifiScanNetwork excluded, included;
      excluded.rssi = -75;
      included.rssi = -74;
      included.hidden = true;
      f.driver.values = {excluded, included};
    }
    f.driver.completionValue = f.driver.values.size();
    f.driver.ack = true;
    f.scanner.poll(1);
    WifiScanResult result;
    assert(f.scanner.takeResult(start.operationId, result));
    assert(result.result.ok() && result.count == (empty ? 0 : 1));
    if (!empty) assert(result.networks[0].rssi == -74 && result.networks[0].hidden);
  }
  for (bool ack : {true, false}) {
    ScanFixture f;
    f.driver.stopOk = ack;
    const uint32_t now = UINT32_MAX - 30;
    auto start = f.scanner.start(now);
    f.scanner.poll(now);
    f.scanner.poll(now + 15000);
    WifiScanResult result;
    assert(f.scanner.takeResult(start.operationId, result) && !result.result.ok());
    assert(f.driver.stops == 1 && f.driver.clears == 0 && f.radio.scanReserved());
    f.driver.ack = ack;
    f.scanner.poll(now + 17000);
    assert(f.radio.scanReserved() == !ack);
    if (ack) assert(f.scanner.start(now + 25000).operationId != 0);
    else {
      assert(!f.scanner.start(now + 25000).result.ok());
      f.driver.ack = true;
      f.scanner.poll(now + 25001);
      assert(f.radio.scanReserved()); // late ACK does not silently unlock unavailable
    }
  }
  {
    ScanFixture f;
    auto start = f.scanner.start(0);
    f.scanner.poll(0);
    f.scanner.cancelInterest(start.operationId);
    f.scanner.poll(1);
    assert(f.driver.stops == 1 && f.radio.scanReserved());
    f.driver.ack = true;
    f.scanner.poll(2);
    assert(!f.radio.scanReserved());
    auto candidate = f.config;
    candidate.wifiMode = WifiMode::Sta;
    uint32_t id = 0;
    assert(f.manager.queueStaTest(candidate, f.config, id).ok());
    assert(f.scanner.start(10001).connectBusy);
  }
  {
    ScanFixture f;
    auto start = f.scanner.start(0);
    f.scanner.poll(0, true);
    assert(f.driver.starts == 0 && !f.radio.scanReserved());
    WifiScanResult result;
    assert(f.scanner.takeResult(start.operationId, result) && !result.result.ok());
    assert(f.scanner.start(10000).busy);
  }
  {
    PendingResponseSlot<std::weak_ptr<int>> slot;
    Api::PendingRequest pending;
    pending.id = 1;
    auto request = std::make_shared<int>(1);
    assert(slot.store(request, pending));
    assert(!slot.store(request, pending));
    decltype(slot)::Record detached;
    request.reset();
    assert(slot.snapshot().request.expired());
    assert(slot.take(1, detached));
    pending.id = 2;
    request = std::make_shared<int>(2);
    assert(slot.store(request, pending));
    assert(!slot.take(1, detached)); // late disconnect cannot take next request
    assert(slot.take(2, detached));
    assert(!slot.take(2, detached)); // send/disconnect has one winner
  }
  std::cout << "Async scan admission, timeout/ACK, result and weak-slot lifecycle tests passed\n";
}
