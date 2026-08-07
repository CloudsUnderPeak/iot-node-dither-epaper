#include <atomic>
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

  Result load(DeviceConfig &config) override {
    config = value;
    return okResult();
  }

  Result save(const DeviceConfig &config) override {
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

  expect(fixture.config.updateAdminPassword("NewPassword1").ok(),
         "new password should persist");
  fixture.auth.invalidateSession();
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
}  // namespace

int main() {
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
