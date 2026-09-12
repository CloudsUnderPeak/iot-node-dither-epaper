#include <cstdlib>
#include <cstring>
#include <iostream>

#include "modules/wifi/WifiManager.h"
#include "support/WifiDoubles.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

struct Fixture {
  WifiRadio radio;
  FakeWifiDriver driver;
  FakeClock clock;
  WifiManager manager;
  DeviceConfig previous = defaultDeviceConfig();

  Fixture() {
    expect(radio.begin().ok(), "Wi-Fi radio should initialize");
    expect(manager.begin(&radio, &driver, &clock).ok(),
           "Wi-Fi manager should initialize with fake platform dependencies");
  }

  void startManagementAp() {
    previous.wifiMode = WifiMode::Ap;
    WifiStatus status;
    expect(manager.apply(previous, status).ok() &&
               status.apState == WifiApState::Active,
           "management AP should start");
  }

  DeviceConfig candidate(WifiMode mode = WifiMode::Sta) const {
    DeviceConfig config = previous;
    config.wifiMode = mode;
    config.staSecurity = StaSecurity::Wpa;
    strlcpy(config.staSsid, "TestSTA", sizeof(config.staSsid));
    strlcpy(config.staPassword, "Password1", sizeof(config.staPassword));
    return config;
  }

  uint32_t queueAndStart(const DeviceConfig &config) {
    uint32_t testId = 0;
    expect(manager.queueStaTest(config, previous, testId).ok(),
           "safe transition should queue");
    expect(manager.testBlocksScan(),
           "queued transition should expose the radio blocker");
    expect(manager.poll(previous),
           "queued transition should enter testing");
    expect(manager.testStatus().state == WifiTestState::Testing &&
               manager.staConnectionBlocksScan(),
           "testing transition should block scan and general apply scheduling");
    return testId;
  }

  uint32_t reachSucceeded(
      const DeviceConfig &config,
      const IPAddress &ip = IPAddress(10, 0, 0, 10)) {
    const uint32_t testId = queueAndStart(config);
    driver.connected = true;
    driver.staIp = ip;
    driver.staNetmask = IPAddress(255, 255, 255, 0);
    expect(manager.poll(previous),
           "connected candidate should be observed");
    expect(manager.testStatus().state == WifiTestState::Succeeded,
           "candidate with IPv4 should reach pre-commit success");
    return testId;
  }
};

void testConnectionTimeoutAndNoIpv4() {
  {
    Fixture fixture;
    DeviceConfig config = fixture.candidate();
    config.fallbackToAp = true;
    WifiStatus status;
    expect(fixture.manager.apply(config, status).ok(),
           "persisted STA should begin connecting");
    fixture.clock.advance(14999);
    expect(!fixture.manager.poll(config),
           "STA should remain connecting before the deadline");
    fixture.clock.advance(1);
    expect(fixture.manager.poll(config),
           "STA should time out at the deadline");
    status = fixture.manager.status();
    expect(status.staState == WifiLinkState::Failed &&
               status.apState == WifiApState::Active &&
               fixture.driver.disconnectAsyncCount == 1,
           "timeout should stop the driver and activate fallback AP");
  }

  {
    Fixture fixture;
    DeviceConfig config = fixture.candidate();
    WifiStatus status;
    fixture.manager.apply(config, status);
    fixture.driver.connected = true;
    fixture.driver.staIp = IPAddress();
    fixture.clock.advance(1000);
    expect(!fixture.manager.poll(config) &&
               fixture.manager.status().staState == WifiLinkState::Connecting,
           "association without IPv4 must not count as connected");
    fixture.clock.advance(14000);
    fixture.manager.poll(config);
    expect(fixture.manager.status().staState == WifiLinkState::Failed,
           "association without IPv4 should eventually time out");
  }
}

void testPersistedStationDisconnectFallsBack() {
  Fixture fixture;
  DeviceConfig config = fixture.candidate();
  config.fallbackToAp = true;
  WifiStatus status;
  fixture.manager.apply(config, status);
  fixture.driver.connected = true;
  fixture.driver.staIp = IPAddress(10, 0, 0, 10);
  expect(fixture.manager.poll(config) &&
             fixture.manager.status().staState == WifiLinkState::Connected,
         "persisted STA should publish connected");

  fixture.driver.connected = false;
  fixture.driver.staIp = IPAddress();
  expect(fixture.manager.poll(config),
         "persisted STA disconnect should change state");
  status = fixture.manager.status();
  expect(status.staState == WifiLinkState::Failed &&
             status.apState == WifiApState::Active,
         "persisted STA disconnect should activate fallback AP");
}

void testCurrentAndFinalApSubnetOverlap() {
  {
    Fixture fixture;
    fixture.startManagementAp();
    DeviceConfig candidate = fixture.candidate();
    fixture.queueAndStart(candidate);
    fixture.driver.connected = true;
    fixture.driver.staIp = IPAddress(192, 168, 4, 20);
    fixture.driver.staNetmask = IPAddress(255, 255, 255, 0);
    fixture.manager.poll(fixture.previous);
    const WifiTestStatus status = fixture.manager.testStatus();
    expect(status.state == WifiTestState::Failed &&
               status.failure == WifiTestFailure::SubnetOverlap,
           "candidate must reject overlap with the current management AP");
  }

  {
    Fixture fixture;
    fixture.startManagementAp();
    DeviceConfig candidate = fixture.candidate(WifiMode::ApSta);
    strlcpy(candidate.apIpAddress, "10.0.0.1", sizeof(candidate.apIpAddress));
    strlcpy(candidate.apIpNetmask, "255.255.255.0",
            sizeof(candidate.apIpNetmask));
    fixture.queueAndStart(candidate);
    fixture.driver.connected = true;
    fixture.driver.staIp = IPAddress(10, 0, 0, 20);
    fixture.driver.staNetmask = IPAddress(255, 255, 255, 0);
    fixture.manager.poll(fixture.previous);
    expect(fixture.manager.testStatus().failure ==
               WifiTestFailure::SubnetOverlap,
           "candidate must reject overlap with the final AP subnet");
  }
}

void testCandidateCommitFailureAndStationDisconnect() {
  {
    Fixture fixture;
    fixture.startManagementAp();
    const DeviceConfig candidate = fixture.candidate();
    const uint32_t testId = fixture.reachSucceeded(candidate);
    DeviceConfig committed;
    expect(fixture.manager.prepareTestCommit(testId, committed).ok(),
           "verified candidate should be ready to persist");
    fixture.manager.finishTestCommit(testId, false);
    expect(fixture.manager.testStatus().state == WifiTestState::Restoring &&
               fixture.manager.testStatus().failure ==
                   WifiTestFailure::StorageError,
           "candidate NVS failure should enter runtime restore");
    fixture.manager.poll(fixture.previous);
    expect(fixture.manager.testStatus().state == WifiTestState::Failed,
           "candidate NVS failure should finish as failed after restore");
  }

  {
    Fixture fixture;
    fixture.startManagementAp();
    const DeviceConfig candidate = fixture.candidate();
    fixture.reachSucceeded(candidate);
    fixture.driver.connected = false;
    fixture.driver.staIp = IPAddress();
    fixture.manager.poll(fixture.previous);
    const WifiTestStatus status = fixture.manager.testStatus();
    expect(status.state == WifiTestState::Failed &&
               status.failure == WifiTestFailure::StationDisconnected,
           "STA disconnect before commit should fail and restore persisted Wi-Fi");
  }
}

void testStaGracePeriodAndFinalize() {
  Fixture fixture;
  fixture.startManagementAp();
  const DeviceConfig candidate = fixture.candidate(WifiMode::Sta);
  const uint32_t testId = fixture.reachSucceeded(candidate);
  DeviceConfig committed;
  fixture.manager.prepareTestCommit(testId, committed);
  fixture.manager.finishTestCommit(testId, true);

  WifiTestStatus test = fixture.manager.testStatus();
  expect(test.state == WifiTestState::Finalizing && test.persisted &&
             test.apShutdownInSeconds == 5,
         "persisted STA transition should publish a five-second AP grace period");
  expect(!fixture.manager.poll(fixture.previous) &&
             fixture.driver.apStopCount == 0,
         "AP should remain active during grace period");
  fixture.clock.advance(5000);
  expect(fixture.manager.poll(fixture.previous),
         "STA transition should finalize at grace deadline");
  test = fixture.manager.testStatus();
  const WifiStatus runtime = fixture.manager.status();
  expect(test.state == WifiTestState::Succeeded && test.persisted &&
             runtime.mode == WifiMode::Sta && !runtime.apEnabled &&
             fixture.driver.apStopCount == 1,
         "grace completion should stop only AP and keep verified STA");
}

uint32_t reachFinalApFailure(Fixture &fixture) {
  fixture.startManagementAp();
  DeviceConfig candidate = fixture.candidate(WifiMode::ApSta);
  strlcpy(candidate.apSsid, "ChangedAP", sizeof(candidate.apSsid));
  const uint32_t testId = fixture.reachSucceeded(candidate);
  DeviceConfig committed;
  fixture.manager.prepareTestCommit(testId, committed);
  fixture.manager.finishTestCommit(testId, true);
  fixture.driver.apStopResult = false;
  fixture.manager.poll(fixture.previous);
  expect(fixture.manager.testStatus().state ==
             WifiTestState::RollbackPending,
         "final AP apply failure should request persisted rollback");
  return testId;
}

void testFinalApFailureRollbackSuccessAndPersistenceFailure() {
  {
    Fixture fixture;
    const uint32_t testId = reachFinalApFailure(fixture);
    DeviceConfig previous;
    expect(fixture.manager.prepareTestRollback(testId, previous).ok(),
           "final AP failure should expose previous Wi-Fi config");
    fixture.manager.finishTestRollback(testId, true);
    fixture.driver.apStopResult = true;
    fixture.manager.poll(fixture.previous);
    const WifiTestStatus status = fixture.manager.testStatus();
    expect(status.state == WifiTestState::Failed &&
               status.failure == WifiTestFailure::ApConfigurationFailed &&
               fixture.manager.status().apState == WifiApState::Active,
           "successful persisted rollback should restore management AP");
  }

  {
    Fixture fixture;
    const uint32_t testId = reachFinalApFailure(fixture);
    DeviceConfig previous;
    fixture.manager.prepareTestRollback(testId, previous);
    fixture.manager.finishTestRollback(testId, false);
    fixture.driver.apStopResult = true;
    fixture.manager.poll(fixture.previous);
    const WifiTestStatus status = fixture.manager.testStatus();
    expect(status.state == WifiTestState::Failed &&
               status.failure == WifiTestFailure::StorageError,
           "rollback persistence failure should remain a storage failure");
  }
}

void testTxPowerPolicyUsesActiveModesAndDedicatedApply() {
  Fixture fixture;
  DeviceConfig config = defaultDeviceConfig();
  config.wifiTxDbm = 20;
  WifiStatus status;
  expect(fixture.manager.apply(config, status).ok() &&
             fixture.driver.lastTxDbm == 20 && fixture.driver.txPowerCount == 1,
         "active mode startup should apply configured TX power");

  const unsigned modeCount = fixture.driver.modeCount;
  const unsigned disconnectCount = fixture.driver.disconnectCount;
  config.wifiTxDbm = 9;
  expect(fixture.manager.applyTxPower(config).ok() &&
             fixture.driver.lastTxDbm == 9 &&
             fixture.driver.modeCount == modeCount &&
             fixture.driver.disconnectCount == disconnectCount,
         "dedicated TX apply must not change mode or disconnect Wi-Fi");

  fixture.driver.txPowerResult = false;
  expect(!fixture.manager.applyTxPower(config).ok(),
         "dedicated TX apply should report driver failure");
}
}  // namespace

int main() {
  testConnectionTimeoutAndNoIpv4();
  testPersistedStationDisconnectFallsBack();
  testCurrentAndFinalApSubnetOverlap();
  testCandidateCommitFailureAndStationDisconnect();
  testStaGracePeriodAndFinalize();
  testFinalApFailureRollbackSuccessAndPersistenceFailure();
  testTxPowerPolicyUsesActiveModesAndDedicatedApply();
  if (failures != 0) {
    std::cerr << failures << " Wi-Fi manager state test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Wi-Fi manager state-machine tests passed\n";
  return EXIT_SUCCESS;
}
