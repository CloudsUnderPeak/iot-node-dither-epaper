#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_dir"
build_dir="$project_dir/tmp/native-tests"
mkdir -p "$build_dir"
common=(-std=c++17 -Wall -Wextra -Werror -pthread -Itests/native/stubs -Isrc)
g++ "${common[@]}" tests/native/EpaperGzipTest.cpp src/modules/epaper/EpaperGzip.cpp \
  src/modules/epaper/EpaperImageFormat.cpp src/modules/epaper/miniz/miniz_tinfl.c \
  -lz -o "$build_dir/epaper-gzip-test"
"$build_dir/epaper-gzip-test"
g++ -Itests/native/status_led_stubs "${common[@]}" \
  tests/native/StatusLedPwmTest.cpp src/modules/status_led/StatusLed.cpp \
  src/modules/status_led/ArduinoStatusLedDriver.cpp -o "$build_dir/status-led-pwm-test"
"$build_dir/status-led-pwm-test"
storage=(src/modules/storage/UserDataStorage.cpp src/modules/storage/UserFilePolicy.cpp)
g++ "${common[@]}" tests/native/UserDataStorageTest.cpp "${storage[@]}" -o "$build_dir/userdata-storage-test"
"$build_dir/userdata-storage-test"
g++ "${common[@]}" tests/native/EpaperServiceTest.cpp "${storage[@]}" \
  src/modules/epaper/EpaperGzip.cpp src/modules/epaper/EpaperGzipReader.cpp \
  src/modules/epaper/miniz/miniz_tinfl.c \
  src/modules/epaper/EpaperService.cpp src/modules/epaper/EpaperCooldown.cpp \
  src/modules/epaper/EpaperImageFormat.cpp src/modules/epaper/EpaperFrameSource.cpp \
  src/modules/epaper/Epd7In3E.cpp src/modules/epaper/EpaperSafetyStore.cpp \
  src/modules/epaper/EpaperShutdownCoordinator.cpp src/modules/epaper/CpuFrequencyGuard.cpp \
  src/modules/runtime/BootDiagnostics.cpp -lz -o "$build_dir/epaper-service-test"
"$build_dir/epaper-service-test"
g++ -Itests/native/sleep_coordinator_stubs "${common[@]}" \
  -include tests/native/stubs/freertos/task.h \
  tests/native/SleepCoordinatorTest.cpp src/modules/sleep/SleepCoordinator.cpp src/modules/status_led/StatusLed.cpp \
  src/modules/sleep/SleepSchedule.cpp src/modules/sleep/SleepStore.cpp \
  src/modules/sleep/SleepRtcRecord.cpp src/modules/sleep/WakeClassifier.cpp \
  -o "$build_dir/sleep-coordinator-test"
"$build_dir/sleep-coordinator-test"
g++ "${common[@]}" -DIOT_FEATURE_SLEEP=0 tests/native/RuntimeActionSchedulerTest.cpp \
  src/modules/runtime/RuntimeActionScheduler.cpp src/modules/config/ConfigService.cpp \
  src/modules/config/model/DeviceConfig.cpp src/modules/config/model/DeviceConfigValidation.cpp \
  src/modules/config/model/StrictIpv4.cpp src/modules/wifi/WifiManager.cpp \
  src/modules/wifi/WifiRadio.cpp src/modules/mdns/MdnsService.cpp \
  src/modules/captive/CaptivePortalDnsService.cpp -o "$build_dir/runtime-action-scheduler-test"
"$build_dir/runtime-action-scheduler-test"
g++ "${common[@]}" -I"${ARDUINOJSON_INCLUDE:-$project_dir/.pio/libdeps/${PIO_ENV:-firebeetle2_esp32c6}/ArduinoJson/src}" \
  tests/native/WifiScannerTest.cpp src/modules/wifi/WifiScanner.cpp \
  src/modules/wifi/WifiManager.cpp src/modules/wifi/WifiRadio.cpp \
  src/modules/config/model/DeviceConfig.cpp src/modules/config/model/DeviceConfigValidation.cpp \
  src/modules/config/model/StrictIpv4.cpp -o "$build_dir/wifi-scanner-test"
"$build_dir/wifi-scanner-test"
