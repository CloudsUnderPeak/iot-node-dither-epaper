#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$project_dir/tmp/native-tests"
pio_env="${PIO_ENV:-firebeetle2_esp32c6}"
arduinojson_include="$project_dir/.pio/libdeps/$pio_env/ArduinoJson/src"

if [[ ! -d "$arduinojson_include" ]]; then
  echo "ArduinoJson headers are missing; run pio run -e $pio_env first." >&2
  exit 1
fi

mkdir -p "$build_dir"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/tests/native/stubs" \
  -I"$arduinojson_include" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/ApiValidationTest.cpp" \
  "$project_dir/src/api/shared/ApiTypes.cpp" \
  "$project_dir/src/api/shared/ApiResponse.cpp" \
  "$project_dir/src/api/shared/JsonReader.cpp" \
  "$project_dir/src/modules/http/HttpJsonBody.cpp" \
  "$project_dir/src/api/wifi/WifiPayload.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  -o "$build_dir/api-validation-test"

"$build_dir/api-validation-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/tests/native/stubs" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/UserDataPolicyTest.cpp" \
  "$project_dir/src/modules/storage/UserFilePolicy.cpp" \
  "$project_dir/src/modules/storage/UserDataPath.cpp" \
  -o "$build_dir/userdata-policy-test"

"$build_dir/userdata-policy-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/src" \
  "$project_dir/tests/native/EpaperPureLogicTest.cpp" \
  "$project_dir/src/modules/epaper/EpaperImageFormat.cpp" \
  "$project_dir/src/modules/epaper/EpaperFrameSource.cpp" \
  "$project_dir/src/modules/epaper/EpaperCooldown.cpp" \
  -o "$build_dir/epaper-pure-logic-test"

"$build_dir/epaper-pure-logic-test"

g++ -std=c++17 -Wall -Wextra -Werror -pthread \
  -I"$project_dir/tests/native/stubs" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/HardwareResourceTest.cpp" \
  "$project_dir/src/modules/hardware/EpaperHardware.cpp" \
  "$project_dir/src/modules/hardware/PinRegistry.cpp" \
  "$project_dir/src/modules/hardware/SpiBus.cpp" \
  -o "$build_dir/hardware-resource-test"

"$build_dir/hardware-resource-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/src" \
  "$project_dir/tests/native/EpdDriverTest.cpp" \
  "$project_dir/src/modules/epaper/Epd7In3E.cpp" \
  "$project_dir/src/modules/epaper/EpaperFrameSource.cpp" \
  -o "$build_dir/epd-driver-test"

"$build_dir/epd-driver-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/src" \
  "$project_dir/tests/native/EpaperSafetyTest.cpp" \
  "$project_dir/src/modules/epaper/CpuFrequencyGuard.cpp" \
  "$project_dir/src/modules/epaper/EpaperSafetyStore.cpp" \
  "$project_dir/src/modules/epaper/EpaperShutdownCoordinator.cpp" \
  "$project_dir/src/modules/epaper/EpaperPowerProbe.cpp" \
  "$project_dir/src/modules/epaper/EpaperRefreshProbe.cpp" \
  "$project_dir/src/modules/epaper/Epd7In3E.cpp" \
  "$project_dir/src/modules/epaper/EpaperFrameSource.cpp" \
  -o "$build_dir/epaper-safety-test"

"$build_dir/epaper-safety-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/tests/native/stubs" \
  -I"$arduinojson_include" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/ConfigStagingTest.cpp" \
  "$project_dir/src/modules/console/ConfigStaging.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  -o "$build_dir/config-staging-test"

"$build_dir/config-staging-test"

g++ -std=c++17 -Wall -Wextra -Werror -pthread \
  -I"$project_dir/tests/native/stubs" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/ConfigServiceTest.cpp" \
  "$project_dir/src/modules/config/ConfigService.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  -o "$build_dir/config-service-test"

"$build_dir/config-service-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/tests/native/stubs" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/PreferencesConfigStoreTest.cpp" \
  "$project_dir/src/modules/config/storage/PreferencesConfigStore.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  -o "$build_dir/preferences-config-store-test"

"$build_dir/preferences-config-store-test"

g++ -std=c++17 -Wall -Wextra -Werror -pthread \
  -I"$project_dir/tests/native/stubs" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/AuthServiceTest.cpp" \
  "$project_dir/src/modules/auth/AuthService.cpp" \
  "$project_dir/src/modules/config/ConfigService.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  -o "$build_dir/auth-service-test"

"$build_dir/auth-service-test"

g++ -std=c++17 -Wall -Wextra -Werror -pthread \
  -I"$project_dir/tests/native/stubs" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/WifiManagerStateTest.cpp" \
  "$project_dir/src/modules/wifi/WifiManager.cpp" \
  "$project_dir/src/modules/wifi/WifiRadio.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  -o "$build_dir/wifi-manager-state-test"

"$build_dir/wifi-manager-state-test"

g++ -std=c++17 -Wall -Wextra -Werror -pthread \
  -I"$project_dir/tests/native/stubs" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/SemaphoreGuardTest.cpp" \
  -o "$build_dir/semaphore-guard-test"

"$build_dir/semaphore-guard-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/src" \
  "$project_dir/tests/native/SubsystemRegistryTest.cpp" \
  -o "$build_dir/subsystem-registry-test"

"$build_dir/subsystem-registry-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/tests/native/endpoint_stubs" \
  -I"$project_dir/tests/native/stubs" \
  -I"$arduinojson_include" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/UserFileEndpointTest.cpp" \
  "$project_dir/src/api/storage/UserFileEndpoints.cpp" \
  "$project_dir/src/modules/storage/UserFilePolicy.cpp" \
  "$project_dir/src/api/shared/ApiTypes.cpp" \
  "$project_dir/src/api/shared/ApiResponse.cpp" \
  -o "$build_dir/user-file-endpoint-test"

"$build_dir/user-file-endpoint-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/tests/native/endpoint_stubs" \
  -I"$project_dir/tests/native/stubs" \
  -I"$arduinojson_include" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/EndpointFailurePathTest.cpp" \
  "$project_dir/src/api/auth/AuthEndpoints.cpp" \
  "$project_dir/src/api/storage/StorageEndpoints.cpp" \
  "$project_dir/src/api/system/SystemEndpoints.cpp" \
  "$project_dir/src/api/web/WebEndpoints.cpp" \
  "$project_dir/src/api/wifi/WifiEndpoints.cpp" \
  "$project_dir/src/api/wifi/WifiPayload.cpp" \
  "$project_dir/src/api/shared/ApiTypes.cpp" \
  "$project_dir/src/api/shared/ApiResponse.cpp" \
  "$project_dir/src/api/shared/JsonReader.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  -o "$build_dir/endpoint-failure-path-test"

"$build_dir/endpoint-failure-path-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/tests/native/endpoint_stubs" \
  -I"$project_dir/tests/native/stubs" \
  -I"$arduinojson_include" \
  -I"$project_dir/src" \
  "$project_dir/tests/native/ApiRouterAuthTest.cpp" \
  "$project_dir/src/api/ApiRouter.cpp" \
  "$project_dir/src/api/alive/AliveEndpoints.cpp" \
  "$project_dir/src/api/auth/AuthEndpoints.cpp" \
  "$project_dir/src/api/device/DeviceEndpoints.cpp" \
  "$project_dir/src/api/epaper/EpaperEndpoints.cpp" \
  "$project_dir/src/api/runtime/RuntimeEndpoints.cpp" \
  "$project_dir/src/api/storage/StorageEndpoints.cpp" \
  "$project_dir/src/api/storage/UserFileEndpoints.cpp" \
  "$project_dir/src/api/system/SystemEndpoints.cpp" \
  "$project_dir/src/api/web/WebEndpoints.cpp" \
  "$project_dir/src/api/wifi/WifiEndpoints.cpp" \
  "$project_dir/src/api/wifi/WifiPayload.cpp" \
  "$project_dir/src/api/shared/ApiTypes.cpp" \
  "$project_dir/src/api/shared/ApiResponse.cpp" \
  "$project_dir/src/api/shared/JsonReader.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfig.cpp" \
  "$project_dir/src/modules/config/model/DeviceConfigValidation.cpp" \
  "$project_dir/src/modules/config/model/StrictIpv4.cpp" \
  "$project_dir/src/modules/storage/UserFilePolicy.cpp" \
  "$project_dir/src/modules/epaper/EpaperImageFormat.cpp" \
  -o "$build_dir/api-router-auth-test"

"$build_dir/api-router-auth-test"

python3 -m unittest discover \
  -s "$project_dir/tests/tools" \
  -p 'test_*.py' \
  -v

python3 "$project_dir/tools/native-test/contract_checks.py" "$project_dir"
