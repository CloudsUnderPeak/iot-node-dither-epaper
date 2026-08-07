#include <cstring>
#include <iostream>
#include <thread>

#include "modules/config/ConfigService.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

class MemoryConfigStore : public ConfigStore {
 public:
  DeviceConfig value = defaultDeviceConfig();
  Result loadResult = okResult();
  Result saveResult = okResult();
  unsigned saveCount = 0;

  Result load(DeviceConfig &config) override {
    if (loadResult.ok()) config = value;
    return loadResult;
  }

  Result save(const DeviceConfig &config) override {
    ++saveCount;
    if (saveResult.ok()) value = config;
    return saveResult;
  }
};

void testWifiCommitAndRollbackPreserveNewerSettings() {
  MemoryConfigStore store;
  strlcpy(store.value.hostname, "original-host", sizeof(store.value.hostname));
  strlcpy(store.value.adminPassword, "Original123", sizeof(store.value.adminPassword));
  strlcpy(store.value.apSsid, "OriginalAP", sizeof(store.value.apSsid));

  ConfigService service(store);
  expect(service.begin().ok(), "config service should load the persisted config");

  const DeviceConfig previousWifi = service.snapshot();
  DeviceConfig candidateWifi = previousWifi;
  strlcpy(candidateWifi.apSsid, "CandidateAP", sizeof(candidateWifi.apSsid));

  expect(service.updateHostname("latest-host").ok(),
         "hostname update should be persisted");
  expect(service.updateAdminPassword("Latest123").ok(),
         "admin password update should be persisted");
  expect(service.updateWifi(candidateWifi).ok(),
         "candidate Wi-Fi fields should be persisted");

  DeviceConfig active = service.snapshot();
  expect(strcmp(active.apSsid, "CandidateAP") == 0,
         "candidate Wi-Fi fields should become active");
  expect(strcmp(active.hostname, "latest-host") == 0,
         "candidate commit must preserve a newer hostname");
  expect(strcmp(active.adminPassword, "Latest123") == 0,
         "candidate commit must preserve a newer admin password");

  expect(service.updateHostname("after-candidate").ok(),
         "hostname should remain independently mutable after candidate commit");
  expect(service.updateAdminPassword("After123").ok(),
         "password should remain independently mutable after candidate commit");
  expect(service.updateWifi(previousWifi).ok(),
         "previous Wi-Fi fields should be usable for rollback");

  active = service.snapshot();
  expect(strcmp(active.apSsid, "OriginalAP") == 0,
         "rollback should restore the previous Wi-Fi fields");
  expect(strcmp(active.hostname, "after-candidate") == 0,
         "rollback must preserve the newest hostname");
  expect(strcmp(active.adminPassword, "After123") == 0,
         "rollback must preserve the newest admin password");
}

void testIndependentUpdatesAreSerialized() {
  MemoryConfigStore store;
  ConfigService service(store);
  expect(service.begin().ok(), "config service should initialize for concurrent updates");

  std::thread hostname([&service]() {
    for (int index = 0; index < 100; ++index) {
      service.updateHostname("parallel-host");
    }
  });
  std::thread password([&service]() {
    for (int index = 0; index < 100; ++index) {
      service.updateAdminPassword("Parallel123");
    }
  });
  hostname.join();
  password.join();

  const DeviceConfig active = service.snapshot();
  expect(strcmp(active.hostname, "parallel-host") == 0,
         "serialized updates must retain the hostname result");
  expect(strcmp(active.adminPassword, "Parallel123") == 0,
         "serialized updates must retain the password result");
}

void testFailedSaveDoesNotPublishCandidate() {
  MemoryConfigStore store;
  ConfigService service(store);
  expect(service.begin().ok(), "config service should initialize before a save failure");
  const DeviceConfig before = service.snapshot();

  store.saveResult = storageError("injected save failure");
  DeviceConfig committed = before;
  strlcpy(committed.hostname, "output-sentinel", sizeof(committed.hostname));
  const Result result = service.updateHostname("must-not-publish", &committed);

  expect(result.code == ResultCode::StorageError,
         "injected storage failure should be returned");
  expect(strcmp(service.snapshot().hostname, before.hostname) == 0,
         "failed persistence must not change the active config");
  expect(strcmp(committed.hostname, "output-sentinel") == 0,
         "failed persistence must not publish a committed snapshot");
}

void testTypedUpdatesValidateBeforeTruncation() {
  MemoryConfigStore store;
  ConfigService service(store);
  expect(service.begin().ok(), "config service should initialize before validation tests");
  const DeviceConfig before = service.snapshot();

  expect(service.updateHostname("hostname-that-is-far-beyond-the-supported-limit").code ==
             ResultCode::InvalidInput,
         "hostname update must reject the original overlong value");
  expect(service.updateAdminPassword(
             "Password1234567890123456789012345678901234567890123456789012345678901234").code ==
             ResultCode::InvalidInput,
         "password update must reject the original overlong value");
  expect(store.saveCount == 0,
         "invalid typed updates must not reach persistence");
  expect(strcmp(service.snapshot().hostname, before.hostname) == 0 &&
             strcmp(service.snapshot().adminPassword, before.adminPassword) == 0,
         "invalid typed updates must not mutate active config");
}
}  // namespace

int main() {
  testWifiCommitAndRollbackPreserveNewerSettings();
  testIndependentUpdatesAreSerialized();
  testFailedSaveDoesNotPublishCandidate();
  testTypedUpdatesValidateBeforeTruncation();

  if (failures != 0) {
    std::cerr << failures << " config service test(s) failed\n";
    return 1;
  }
  std::cout << "Config service atomic update tests passed\n";
  return 0;
}
