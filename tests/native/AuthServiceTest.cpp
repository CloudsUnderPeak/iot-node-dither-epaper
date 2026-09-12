#include <atomic>
#include <cassert>
#include <future>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "modules/auth/AuthService.h"
#include "modules/config/storage/ConfigStore.h"

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
  bool failSave = false;
  std::function<void()> onSave;

  Result load(DeviceConfig &config) override {
    config = value;
    return okResult();
  }

  Result save(const DeviceConfig &config) override {
    if (onSave) onSave();
    if (failSave) return storageError("injected failure");
    value = config;
    return okResult();
  }
};

struct AuthFixture {
  MemoryConfigStore store;
  ConfigService config;
  AuthService auth;

  AuthFixture() : config(store) {
    expect(config.begin().ok(), "config service should initialize");
    expect(auth.begin(&config).ok(), "auth service should initialize");
  }
};

void testLoginSuccessAndFailure() {
  AuthFixture fixture;
  String token;

  expect(!fixture.auth.credentialsMatch("admin", "wrong"),
         "wrong password must not match");
  expect(!fixture.auth.credentialsMatch("other", "password"),
         "wrong username must not match");
  expect(fixture.auth.login("admin", "wrong", token).code ==
             ResultCode::InvalidInput,
         "wrong credentials must fail login");
  expect(!fixture.auth.hasActiveSession(),
         "failed login must not create a session");

  expect(fixture.auth.login("admin", "password", token).ok(),
         "valid credentials should log in");
  expect(token.length() == 32,
         "opaque token should contain 16 random bytes as hex");
  expect(fixture.auth.tokenValid(token),
         "new token should validate");
  expect(!fixture.auth.tokenValid(String("invalid-token")),
         "unrelated token should not validate");
}

void testNewLoginAndLogoutInvalidateTokens() {
  AuthFixture fixture;
  String first;
  String second;

  expect(fixture.auth.login("admin", "password", first).ok(),
         "first login should succeed");
  expect(fixture.auth.login("admin", "password", second).ok(),
         "second login should succeed");
  expect(!(first == second),
         "consecutive logins should issue distinct tokens");
  expect(!fixture.auth.tokenValid(first),
         "second login must revoke the first token");
  expect(fixture.auth.tokenValid(second),
         "second token should remain active");

  fixture.auth.invalidateSession();
  expect(!fixture.auth.hasActiveSession() && !fixture.auth.tokenValid(second),
         "logout must revoke the active token");
}

void testPasswordChangeAndRebootLifecycle() {
  AuthFixture fixture;
  String oldToken;
  expect(fixture.auth.login("admin", "password", oldToken).ok(),
         "login before password change should succeed");

  bool restartRequired = false;
  expect(fixture.auth.changePassword("NewPassword1", true, restartRequired).ok(),
         "new password should persist");
  expect(!fixture.auth.tokenValid(oldToken),
         "password-change flow must invalidate the previous session");

  String token;
  expect(!fixture.auth.login("admin", "password", token).ok(),
         "old password must stop working after persistence");
  expect(fixture.auth.login("admin", "NewPassword1", token).ok(),
         "new password should authenticate");

  AuthService afterReboot;
  expect(afterReboot.begin(&fixture.config).ok(),
         "rebooted auth service should initialize");
  expect(!afterReboot.hasActiveSession() && !afterReboot.tokenValid(token),
         "runtime token must not survive service initialization");
}

void testConcurrentLoginVerifyAndInvalidate() {
  AuthFixture fixture;
  std::atomic<unsigned> operationFailures{0};
  std::vector<String> issued(100);

  std::thread login([&]() {
    for (String &token : issued) {
      if (!fixture.auth.login("admin", "password", token).ok() ||
          token.length() != 32) {
        ++operationFailures;
      }
    }
  });
  std::thread verify([&]() {
    for (int index = 0; index < 200; ++index) {
      if (!fixture.auth.credentialsMatch("admin", "password")) {
        ++operationFailures;
      }
      fixture.auth.hasActiveSession();
      fixture.auth.tokenValid(String("invalid-token"));
    }
  });
  std::thread invalidate([&]() {
    for (int index = 0; index < 100; ++index) {
      fixture.auth.invalidateSession();
    }
  });

  login.join();
  verify.join();
  invalidate.join();
  expect(operationFailures.load() == 0,
         "concurrent auth operations should complete without corrupting results");

  String finalToken;
  expect(fixture.auth.login("admin", "password", finalToken).ok() &&
             fixture.auth.tokenValid(finalToken),
         "auth service should remain usable after concurrent operations");
  fixture.auth.invalidateSession();
  expect(!fixture.auth.tokenValid(finalToken),
         "final invalidation should deterministically revoke the token");
}
void testCredentialBarrierAndFailures() {
  AuthFixture f;
  std::promise<void> validated, contended, releaseLogin;
  auto release = releaseLogin.get_future();
  String oldToken;
  std::thread login([&] {
    bool paused = false;
    nativeAfterSemaphoreGive = [&] {
      if (paused) return;
      paused = true; // config snapshot is read, credential mutex still held
      validated.set_value();
      release.wait();
    };
    assert(f.auth.login("admin", "password", oldToken).ok());
    nativeAfterSemaphoreGive = {};
  });
  validated.get_future().wait();
  std::thread change([&] {
    nativeSemaphoreBlocked = [&] { contended.set_value(); };
    bool restart = false;
    assert(f.auth.changePassword("NewPassword1", true, restart).ok());
    nativeSemaphoreBlocked = {};
  });
  // Observe actual contention, not a semaphore allocation number or sleep.
  assert(contended.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
  releaseLogin.set_value();
  login.join();
  change.join();
  assert(!f.auth.tokenValid(oldToken));
  String current;
  assert(f.auth.login("admin", "NewPassword1", current).ok());
  bool restart = false;
  f.store.failSave = true;
  assert(!f.auth.changePassword("OtherPassword1", true, restart).ok());
  assert(f.auth.tokenValid(current));
  assert(f.auth.credentialsMatch("admin", "NewPassword1"));
  f.store.failSave = false;
  auto config = f.config.snapshot();
  config.apPasswordEnabled = true;
  assert(f.config.updateWifi(config).ok());
  assert(f.auth.changePassword("OtherPassword1", false, restart).code == ResultCode::Unsupported);
  assert(f.auth.tokenValid(current));
  assert(f.auth.changePassword("OtherPassword1", true, restart).ok() && restart);
  assert(!f.auth.tokenValid(current));
}
void testLatestApSettingBarrier() {
  AuthFixture f;
  String token;
  assert(f.auth.login("admin", "password", token).ok());
  auto updated = f.config.snapshot();
  updated.apPasswordEnabled = true;
  std::promise<void> saving, blocked, releaseSave;
  auto release = releaseSave.get_future();
  f.store.onSave = [&] { saving.set_value(); release.wait(); };
  std::thread wifi([&] { assert(f.config.updateWifi(updated).ok()); });
  saving.get_future().wait();
  std::thread password([&] {
    nativeSemaphoreBlocked = [&] { blocked.set_value(); };
    bool restart = false;
    assert(f.auth.changePassword("NewPassword1", false, restart).code == ResultCode::Unsupported);
    nativeSemaphoreBlocked = {};
  });
  assert(blocked.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
  releaseSave.set_value();
  wifi.join();
  password.join();
  assert(f.auth.tokenValid(token));
  assert(f.auth.credentialsMatch("admin", "password"));
}
}  // namespace

int main() {
  testLatestApSettingBarrier();
  testCredentialBarrierAndFailures();
  testLoginSuccessAndFailure();
  testNewLoginAndLogoutInvalidateTokens();
  testPasswordChangeAndRebootLifecycle();
  testConcurrentLoginVerifyAndInvalidate();
  if (failures != 0) {
    std::cerr << failures << " auth service test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Auth service lifecycle and concurrency tests passed\n";
  return EXIT_SUCCESS;
}
