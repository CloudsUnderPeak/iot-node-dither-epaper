#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_dir"
build_dir="$project_dir/tmp/native-tests"
mkdir -p "$build_dir"
arduinojson_include="${ARDUINOJSON_INCLUDE:-$project_dir/.pio/libdeps/${PIO_ENV:-firebeetle2_esp32c6}/ArduinoJson/src}"
sources=(src/api/ApiRouter.cpp src/api/features/FeaturesEndpoints.cpp src/api/alive/AliveEndpoints.cpp
  src/api/device/DeviceEndpoints.cpp src/api/runtime/RuntimeEndpoints.cpp src/api/storage/StorageEndpoints.cpp
  src/api/system/SystemEndpoints.cpp src/api/web/WebEndpoints.cpp src/api/wifi/WifiEndpoints.cpp src/api/wifi/WifiPayload.cpp
  src/api/shared/ApiTypes.cpp src/api/shared/ApiResponse.cpp src/api/shared/JsonReader.cpp
  src/modules/config/model/DeviceConfig.cpp src/modules/config/model/DeviceConfigValidation.cpp
  src/modules/config/model/StrictIpv4.cpp src/modules/storage/UserFilePolicy.cpp src/modules/runtime/BootDiagnostics.cpp)
for profile in auth-off minimal panel-only; do
  flags=(-DIOT_FEATURE_SLEEP=0 -DIOT_FEATURE_AUTH=0)
  optional=(src/api/storage/UserFileEndpoints.cpp)
  if [ "$profile" = minimal ]; then
    flags+=(-DIOT_FEATURE_EPAPER=0 -DIOT_FEATURE_STORAGE=0 -DIOT_FEATURE_USER_FILES=0 -DIOT_FEATURE_BATTERY=0)
    optional=()
  else
    optional+=(src/api/epaper/EpaperEndpoints.cpp src/modules/epaper/EpaperImageFormat.cpp
      src/modules/epaper/calibration/EpaperCalibration.cpp)
  fi
  if [ "$profile" = panel-only ]; then flags+=(-DIOT_FEATURE_USER_FILES=0); fi
  g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter "${flags[@]}" \
    -Itests/native/endpoint_stubs -Itests/native/stubs -I"$arduinojson_include" -Isrc \
    tests/native/FeatureRouterTest.cpp "${sources[@]}" "${optional[@]}" -o "$build_dir/feature-router-$profile"
  "$build_dir/feature-router-$profile"
done

for storage in 0 1; do
  flags=(-DIOT_FEATURE_EPAPER=0 -DIOT_FEATURE_MDNS=0 -DIOT_FEATURE_USER_FILES=0 -DIOT_FEATURE_STORAGE="$storage")
  g++ -std=c++17 -Wall -Wextra -Werror -pthread "${flags[@]}" \
    -Itests/native/sleep_coordinator_stubs -Itests/native/stubs -Isrc \
    -include tests/native/stubs/freertos/task.h \
    tests/native/SleepCoordinatorTest.cpp src/modules/sleep/SleepCoordinator.cpp \
    src/modules/sleep/SleepSchedule.cpp src/modules/sleep/SleepStore.cpp \
    src/modules/sleep/SleepRtcRecord.cpp src/modules/sleep/WakeClassifier.cpp \
    -o "$build_dir/timer-only-storage-$storage"
  "$build_dir/timer-only-storage-$storage"
  optional=()
  if [ "$storage" = 1 ]; then
    optional=(src/modules/storage/UserDataStorage.cpp src/modules/storage/UserFilePolicy.cpp)
  fi
  g++ -std=c++17 -Wall -Wextra -Werror -pthread "${flags[@]}" -Itests/native/stubs -Isrc \
    tests/native/BasicRestartCoordinatorTest.cpp "${optional[@]}" -o "$build_dir/basic-restart-storage-$storage"
  "$build_dir/basic-restart-storage-$storage"
done
