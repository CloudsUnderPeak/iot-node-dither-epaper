#include <cstdlib>
#include <iostream>

#include "core/SemaphoreGuard.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}
}

int main() {
  SemaphoreGuard missing(nullptr, 0);
  expect(!missing.locked(), "null semaphore must not lock");

  SemaphoreHandle_t semaphore = xSemaphoreCreateMutex();
  {
    SemaphoreGuard guard(semaphore, portMAX_DELAY);
    expect(guard.locked(), "guard must acquire the semaphore");
  }
  expect(xSemaphoreTake(semaphore, 0) == pdTRUE,
         "guard destructor must release the semaphore");
  expect(xSemaphoreTake(semaphore, 0) != pdTRUE, "zero wait must return busy");
  xSemaphoreGive(semaphore);
  vSemaphoreDelete(semaphore);
  auto binary = xSemaphoreCreateBinary();
  expect(xSemaphoreTake(binary, 0) != pdTRUE, "binary gate starts empty");
  xSemaphoreGive(binary);
  expect(xSemaphoreTake(binary, 0) == pdTRUE, "binary gate accepts give");
  vSemaphoreDelete(binary);

  if (failures != 0) {
    std::cerr << failures << " semaphore guard test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Semaphore guard lifecycle tests passed\n";
  return EXIT_SUCCESS;
}
