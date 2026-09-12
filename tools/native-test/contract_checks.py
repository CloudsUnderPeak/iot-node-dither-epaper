import pathlib
import re
import sys

project = pathlib.Path(sys.argv[1])
router = (project / "src/api/ApiRouter.cpp").read_text()
server = (project / "src/modules/http/ApiServer.cpp").read_text()
wifi_manager = (project / "src/modules/wifi/WifiManager.cpp").read_text()
wifi_driver = (project / "src/modules/wifi/ArduinoWifiDriver.cpp").read_text()
main = (project / "src/main.cpp").read_text()
platformio = (project / "platformio.ini").read_text()
epaper_hardware = (project / "src/modules/hardware/EpaperHardware.cpp").read_text()
epaper_service = (project / "src/modules/epaper/EpaperService.cpp").read_text()
boot_diagnostics = (
    project / "src/modules/runtime/BootDiagnostics.cpp"
).read_text()
epaper_transport = (project / "src/modules/epaper/EpdSpiTransport.cpp").read_text()
board_profile = (
    project / "src/board/profiles/FireBeetle2Esp32C6Profile.h"
).read_text()
if "while (receivedBytes < begin.contentLength)" not in epaper_service:
    print("Stored image validation must stop at the auto-closing download length", file=sys.stderr)
    raise SystemExit(1)

if "-D ENABLE_EPAPER_PANEL_SELF_TEST=0" not in platformio:
    print("Release configuration must keep the one-shot panel self-test disabled", file=sys.stderr)
    raise SystemExit(1)
if "-D ENABLE_EPAPER_REFRESH_SELF_TEST=0" not in platformio:
    print("Release configuration must keep the one-shot refresh test disabled", file=sys.stderr)
    raise SystemExit(1)
if "-D ENABLE_EPAPER_CONFIRMED_POWER_CYCLE_RECOVERY=0" not in platformio:
    print("Release configuration must keep forced power-cycle recovery disabled", file=sys.stderr)
    raise SystemExit(1)
if "bootDiagnostics.snapshot().resetReason == DeviceResetReason::PowerOn" not in main:
    print("Active e-paper markers must use captured power-on reset evidence", file=sys.stderr)
    raise SystemExit(1)
if "esp_reset_reason()" in main or "esp_reset_reason()" in epaper_service:
    print("Boot diagnostics must be the only reset-reason SDK reader", file=sys.stderr)
    raise SystemExit(1)
if boot_diagnostics.count("esp_reset_reason()") != 1:
    print("Boot diagnostics must capture reset reason exactly once", file=sys.stderr)
    raise SystemExit(1)
if "automatic retry=disabled" not in main:
    print("E-paper hardware tests must not retry after a power-cycle recovery", file=sys.stderr)
    raise SystemExit(1)

if "gpio_set_level" not in epaper_hardware:
    print("ESP32 e-paper outputs must preload their latches before output enable", file=sys.stderr)
    raise SystemExit(1)
if "kSafeResetHigh = true" not in board_profile:
    print("E-paper RST must remain inactive-high outside the short reset pulse", file=sys.stderr)
    raise SystemExit(1)
if "digitalWrite(pins.reset, HIGH)" not in epaper_transport:
    print("E-paper logical quiesce must not hold the HAT power-gating RST low", file=sys.stderr)
    raise SystemExit(1)
if re.search(r"digitalWrite\(epaper\.(?:cs|dc|reset)", epaper_hardware):
    print("Arduino digitalWrite must not be used before e-paper output pinMode", file=sys.stderr)
    raise SystemExit(1)

epaper_start = "startSubsystem(subsystems[kEpaperHardwareSubsystem])"
if epaper_start not in main:
    print("E-paper hardware must be initialized through the subsystem registry", file=sys.stderr)
    raise SystemExit(1)
if not main.index(epaper_start) < main.index("Serial.begin(115200)") < main.index("for (size_t index = kUserdataSubsystem"):
    print("E-paper logical quiesce must precede Serial delays and all other subsystems", file=sys.stderr)
    raise SystemExit(1)
epaper_test_call = main.rindex("runEpaperPanelSelfTest();")
if not epaper_test_call < main.index("for (size_t index = kUserdataSubsystem"):
    print("E-paper hardware tests must run before storage and Wi-Fi startup", file=sys.stderr)
    raise SystemExit(1)
if "epdDriver.initialize()" in main or "transferAndRefresh" in main:
    print("Hardware-only bring-up must not initialize or refresh the panel", file=sys.stderr)
    raise SystemExit(1)
if 'printHeartbeatField("epaper_busy", epaperBusyLabel())' not in main:
    print("Hardware bring-up must expose the read-only BUSY level in heartbeat", file=sys.stderr)
    raise SystemExit(1)
if "if (Serial && now - lastHeartbeatMs >= 1000U)" not in main:
    print("USB CDC heartbeat must not block the runtime loop without a reader", file=sys.stderr)
    raise SystemExit(1)
epaper_transfer = epaper_service.index("driver_->transferFrame(source)")
epaper_release = epaper_service.index(
    "storage_->finishDownload(storageSessionId)", epaper_transfer
)
if not epaper_transfer < epaper_release < epaper_service.index(
    "driver_->refresh()", epaper_release
):
    print("Stored frame must release userdata before physical refresh", file=sys.stderr)
    raise SystemExit(1)

restart_callers = []
for source_path in (project / "src").rglob("*.cpp"):
    if "ESP.restart()" in source_path.read_text():
        restart_callers.append(source_path.relative_to(project).as_posix())
if restart_callers != ["src/modules/epaper/ArduinoRestartDriver.cpp"]:
    print(
        f"ESP.restart() must exist only behind the e-paper restart coordinator: {restart_callers}",
        file=sys.stderr,
    )
    raise SystemExit(1)
runtime_scheduler = (project / "src/modules/runtime/RuntimeActionScheduler.cpp").read_text()
if "restartCoordinator_->restartNow()" not in runtime_scheduler:
    print("RuntimeActionScheduler must delegate reset to the safety coordinator", file=sys.stderr)
    raise SystemExit(1)

router_header = (project / "src/api/ApiRouter.h").read_text()
if "RouteAccess access = RouteAccess::Protected" not in router_header:
    print("Router entries must default to protected access", file=sys.stderr)
    raise SystemExit(1)
if re.search(r'server_\.on\([^\\n]*"/api(?:/|")', server):
    print("HTTP adapter must not duplicate literal API route registrations", file=sys.stderr)
    raise SystemExit(1)
for required in (
    "ApiRouter::routeCount()",
    "ApiRouter::routeInfo(index, route)",
    "AsyncURIMatcher::exact(route.path)",
    "AsyncURIMatcher::dir(directory)",
    "case ApiRouter::HttpBinding::NoBody:",
    "case ApiRouter::HttpBinding::JsonBody:",
    "case ApiRouter::HttpBinding::Query:",
    "case ApiRouter::HttpBinding::RawUpload:",
    "case ApiRouter::HttpBinding::RawDownload:",
    "case ApiRouter::HttpBinding::EpaperRawUpload:",
    "case ApiRouter::HttpBinding::EpaperRawDownload:",
):
    if required not in server:
        print(f"HTTP adapter is not driven by route metadata: {required}", file=sys.stderr)
        raise SystemExit(1)

for route in (
    '"/api/epaper"',
    '"/api/epaper/status"',
    '"/api/epaper/image"',
    '"/api/epaper/image/download"',
    '"/api/epaper/image/refresh"',
    '"/api/epaper/image/white"',
    '"/api/epaper/image/palette"',
    '"/api/runtime/status"',
):
    if route not in router:
        print(f"E-paper API route is missing: {route}", file=sys.stderr)
        raise SystemExit(1)
if '403, "reserved_file"' not in router:
    print("Generic upload must protect the reserved e-paper image", file=sys.stderr)
    raise SystemExit(1)

print("API route catalogue/HTTP registration contract tests passed")

if "Subsystem subsystems[]" not in main:
    print("main.cpp must declare one ordered subsystem registry", file=sys.stderr)
    raise SystemExit(1)
if main.count("for (const Subsystem &subsystem : subsystems)") != 1:
    print("heartbeat must traverse the subsystem registry exactly once", file=sys.stderr)
    raise SystemExit(1)
remaining_start_loop = "for (size_t index = kUserdataSubsystem; index < kSubsystemCount; ++index)"
if main.count(remaining_start_loop) != 1:
    print("setup must start every non-e-paper subsystem exactly once in registry order", file=sys.stderr)
    raise SystemExit(1)
if main.count(epaper_start) != 1:
    print("setup must start the e-paper hardware subsystem exactly once", file=sys.stderr)
    raise SystemExit(1)
for required in (
    "startSubsystem(subsystem)",
    "subsystemHealthy(subsystem)",
    "subsystem.report(result)",
):
    if required not in main:
        print(f"subsystem registry lifecycle is missing {required}", file=sys.stderr)
        raise SystemExit(1)
for obsolete in (
    "bool configReady",
    "bool assetsReady",
    "bool flashStorageReady",
    "bool userDataReady",
    "bool authReady",
    "bool runtimeReady",
    "bool consoleReady",
):
    if obsolete in main:
        print(f"parallel readiness flag remains outside the registry: {obsolete}", file=sys.stderr)
        raise SystemExit(1)

sta_timeout = wifi_manager[
    wifi_manager.index("if (current.staState == WifiLinkState::Connecting)") :
    wifi_manager.index("const bool changed = current.staState", wifi_manager.index("if (current.staState == WifiLinkState::Connecting)"))
]
if "driver_->disconnectStationAsync(false, false)" not in sta_timeout:
    print("STA timeout must stop the driver connection attempt before publishing failed", file=sys.stderr)
    raise SystemExit(1)
if not sta_timeout.index("driver_->disconnectStationAsync(false, false)") < sta_timeout.index("current.staState = WifiLinkState::Failed"):
    print("STA timeout publishes failed before stopping the driver connection attempt", file=sys.stderr)
    raise SystemExit(1)

if "#include <AsyncJson.h>" in server or "JsonVariant &json" in server:
    print("HTTP API routes still delegate parsing to AsyncJson", file=sys.stderr)
    raise SystemExit(1)
for required in ("payload_too_large", "invalid_json", "HttpJsonBody::parse"):
    if required not in (server + (project / "src/modules/http/HttpJsonBody.cpp").read_text()):
        print(f"HTTP JSON adapter is missing {required}", file=sys.stderr)
        raise SystemExit(1)

for required in (
    "WiFi.softAPConfig(localIp, gateway, netmask, dns, localIp)",
    "WiFi.AP.enableDhcpCaptivePortal()",
    "esp_wifi_set_max_tx_power(",
    "kPlatformMaxQuarterDbm = 84",
):
    if required not in wifi_driver:
        print(f"Arduino Wi-Fi driver contract is missing: {required}", file=sys.stderr)
        raise SystemExit(1)
if "WiFi." in wifi_manager or "millis()" in wifi_manager:
    print("WifiManager must use injected driver and clock seams", file=sys.stderr)
    raise SystemExit(1)
if "request->client()->localIP() == status.apIp" not in server:
    print("Captive redirect must be limited to requests arriving on the AP interface", file=sys.stderr)
    raise SystemExit(1)
embedded_assets = server[
    server.index("bool ApiServer::sendEmbeddedAsset") :
    server.index("void ApiServer::handleCaptivePortal")
]
if 'response->addHeader("Cache-Control", "no-store")' not in embedded_assets or "max-age" in embedded_assets:
    print("Embedded frontend assets must not reuse stale CSS or JavaScript across firmware updates", file=sys.stderr)
    raise SystemExit(1)
for required in (
    "asset->contentEncoding != nullptr",
    'response->addHeader("Content-Encoding", asset->contentEncoding)',
    'response->addHeader("Vary", "Accept-Encoding")',
):
    if required not in embedded_assets:
        print(f"Embedded raw/gzip response handling is missing: {required}", file=sys.stderr)
        raise SystemExit(1)

scheduler = (project / "src/modules/runtime/RuntimeActionScheduler.cpp").read_text()
if "xQueue" in scheduler or "command queue is full" in scheduler:
    print("Runtime actions must coalesce instead of using a fallible FIFO", file=sys.stderr)
    raise SystemExit(1)
scheduler_poll = scheduler[
    scheduler.index("void RuntimeActionScheduler::poll") :
    scheduler.index("bool RuntimeActionScheduler::ready")
]
if not scheduler_poll.index("applyPendingSystemReset") < scheduler_poll.index("rollbackFailedWifiTransition") < scheduler_poll.index("commitVerifiedWifiConnection") < scheduler_poll.index("applyPendingWifi"):
    print("A due system restart must run before Wi-Fi transition or apply work", file=sys.stderr)
    raise SystemExit(1)
for required in ("commitVerifiedWifiConnection", "prepareTestCommit", "configService_->updateWifi", "finishTestCommit"):
    if required not in scheduler:
        print(f"Runtime must internally persist verified Wi-Fi credentials: {required}", file=sys.stderr)
        raise SystemExit(1)

wifi_endpoints = (project / "src/api/wifi/WifiEndpoints.cpp").read_text()
wifi_update = wifi_endpoints[ wifi_endpoints.index("Api::Response WifiEndpoints::update") : wifi_endpoints.index("Api::Response WifiEndpoints::reconnect") ]
if not wifi_update.index("runtime_->ready()") < wifi_update.index("configService_->updateWifi") < wifi_update.index("scheduleSystemReset") < wifi_update.index("scheduleWifiApply"):
    print("Wi-Fi update must preflight runtime, commit, then restart for AP protection or apply other changes", file=sys.stderr)
    raise SystemExit(1)

system_endpoints = (project / "src/api/system/SystemEndpoints.cpp").read_text()
system_update = system_endpoints[ system_endpoints.index("Api::Response SystemEndpoints::update") : system_endpoints.index("Api::Response SystemEndpoints::reset") ]
system_reset = system_endpoints[system_endpoints.index("Api::Response SystemEndpoints::reset") :]
if not system_update.index("runtime_->ready()") < system_update.index("configService_->updateSystem") < system_update.index("scheduleWifiApply") < system_update.index("scheduleWifiTxPowerApply"):
    print("System update must preflight runtime before commit and schedule after commit", file=sys.stderr)
    raise SystemExit(1)

auth_endpoints = (project / "src/api/auth/AuthEndpoints.cpp").read_text()
password_update = auth_endpoints[auth_endpoints.index("Api::Response AuthEndpoints::updatePassword") :]
if not password_update.index("runtime_->ready()") < password_update.index("configService_->updateAdminPassword") < password_update.index("invalidateSession") < password_update.index("scheduleSystemReset"):
    print("Password update must preflight AP restart before commit and invalidate after commit", file=sys.stderr)
    raise SystemExit(1)
if not system_reset.index("runtime_->ready()") < system_reset.index("storageLifecycle_->requestReset") < system_reset.index("scheduleSystemReset"):
    print("Reset must guarantee restart before persisting reset intent", file=sys.stderr)
    raise SystemExit(1)

console = (project / "src/modules/console/ConsoleShell.cpp").read_text()
if 'Api::problem(400, "invalid_json", "invalid JSON body")' not in console:
    print("Serial malformed JSON must return invalid_json", file=sys.stderr)
    raise SystemExit(1)

web_tool = (project / "tools/web-build/build_web.py").read_text()
release_tool = (project / "tools/release-build/build_release.py").read_text()
makefile = (project / "Makefile").read_text()
if (project / "tools/demo-build").exists():
    print("Demo frontend output must be owned by tools/web-build", file=sys.stderr)
    raise SystemExit(1)
if "DEMO_TOOL" in makefile:
    print("Makefile must not define a separate demo build tool", file=sys.stderr)
    raise SystemExit(1)
for required in (
    'choices=("production", "demo")',
    'choices=("auto", "builtin", "user", "none")',
    'choices=("auto", "minify-gzip", "none")',
    '"import-user"',
    '"clean-user"',
    "def import_user_web(",
    "def clean_user_web(",
    "def replace_user_web(",
    "def select_source(",
    "def publish_latest(",
    "user_web_sha256",
    'return "none", None',
):
    if required not in web_tool:
        print(f"Web build tool is missing selected frontend support: {required}", file=sys.stderr)
        raise SystemExit(1)
for forbidden in ("BUILTIN_SOURCE", "USER_SOURCE", "PREVIEW_BLOCK", "def build_web"):
    if forbidden in release_tool:
        print(f"ESP release tool still owns frontend concern: {forbidden}", file=sys.stderr)
        raise SystemExit(1)
for forbidden in ("platformio_core_dir", "def flash_release", "create_firmware_package"):
    if forbidden in web_tool:
        print(f"Web build tool still owns ESP concern: {forbidden}", file=sys.stderr)
        raise SystemExit(1)
for required in (
    "\nbuild:",
    "\nprepare-user-web:",
    "\nweb:",
    "\ndemo:",
    "\nesp:",
    "\nverify-web:",
    "$(WEB_TOOL) build",
    "$(WEB_TOOL) verify",
    "$(WEB_TOOL) import-user",
    "$(WEB_TOOL) clean-user",
    "$(MAKE) -C $(USER_WEB_PROJECT)",
    "$(RELEASE_TOOL) build",
    "WEB=none",
):
    if required not in makefile:
        print(f"Makefile is missing split build orchestration: {required}", file=sys.stderr)
        raise SystemExit(1)
if "WEB ?= user" not in makefile:
    print("Makefile must default full builds to the generated user frontend", file=sys.stderr)
    raise SystemExit(1)
for required in (
    '--target production --web "$(WEB)" --process "$(WEB_PROCESS)"',
    "--target demo --web builtin",
    "make clean all",
):
    if required not in makefile:
        print(f"Makefile is missing direct frontend orchestration: {required}", file=sys.stderr)
        raise SystemExit(1)
if "COMPONENT ?=" in makefile or "COMPONENT=" in makefile:
    print("Makefile still exposes removed COMPONENT compatibility", file=sys.stderr)
    raise SystemExit(1)
if "COMPONENT was removed" not in makefile:
    print("Makefile must reject removed COMPONENT calls instead of ignoring them", file=sys.stderr)
    raise SystemExit(1)
for required in (
    "create_firmware_package(",
    "publish_snapshot(",
    "build/latest/web",
    'PACKAGE_NAMES = ("manifest.json", *IMAGE_NAMES)',
    "SNAPSHOT_PATTERN",
    "gzip.GzipFile(",
    "tarfile.USTAR_FORMAT",
):
    if required not in release_tool:
        print(f"Release builder is missing snapshot packaging: {required}", file=sys.stderr)
        raise SystemExit(1)
if "zipfile" in release_tool or "ZIP_DEFLATED" in release_tool:
    print("Release builder must use the gzip-only compression policy, not ZIP", file=sys.stderr)
    raise SystemExit(1)
for removed_target in (
    "build-web",
    "build-demo",
    "verify-demo",
    "build-esp",
    "verify-esp",
    "verify-build",
    "clean-web",
    "clean-demo",
    "clean-esp",
):
    if f"\n{removed_target}:" in makefile:
        print(f"Makefile still exposes redundant target: {removed_target}", file=sys.stderr)
        raise SystemExit(1)

deploy_start = makefile.index("\ndeploy:")
deploy_end = makefile.index("\nflash:", deploy_start)
deploy_recipe = makefile[deploy_start:deploy_end]
for required in (
    "PORT is required",
    "$(MAKE) --no-print-directory clean",
    "$(MAKE) --no-print-directory build",
    '$(MAKE) --no-print-directory flash PORT="$(PORT)"',
):
    if required not in deploy_recipe:
        print(f"Deploy target is missing required step: {required}", file=sys.stderr)
        raise SystemExit(1)
if not deploy_recipe.index("PORT is required") < deploy_recipe.index(" clean") < deploy_recipe.index(" build") < deploy_recipe.index(" flash"):
    print("Deploy target must validate PORT before clean, build, and flash", file=sys.stderr)
    raise SystemExit(1)

network_view = (project / "builtin-web/assets/js/pages/network.js").read_text()
for state in ("connecting", "connected", "starting", "active", "failed", "disabled"):
    if f"'{state}'" not in network_view:
        print(f"Network view is missing runtime state {state}", file=sys.stderr)
        raise SystemExit(1)
if "sta.state === 'connected'" not in network_view or "ap.state === 'active'" not in network_view:
    print("Active interface count must use terminal runtime states", file=sys.stderr)
    raise SystemExit(1)

auth_module = (project / "builtin-web/assets/js/modules/auth.js").read_text()
login_handler = auth_module[auth_module.index("async function login(") : auth_module.index("async function changePassword")]
if "error.code === 'unauthorized'" not in auth_module or "t('invalidAdminPassword')" not in auth_module:
    print("Login unauthorized errors must use the localized credential message", file=sys.stderr)
    raise SystemExit(1)
if "setNotice(error.message" in login_handler:
    print("Login handler must not display the raw API credential message", file=sys.stderr)
    raise SystemExit(1)

admin_password_pattern = r"^[A-Za-z0-9!@#$%^&*()_+=.,:?-]{8,63}$"
if f"const ADMIN_PASSWORD_PATTERN = /{admin_password_pattern}/;" not in auth_module:
    print("Admin password form is missing the frontend symbol allowlist", file=sys.stderr)
    raise SystemExit(1)
compiled_admin_password = re.compile(admin_password_pattern)
for allowed_password in ("Admin123", "A1!@#$%^", "Z9&*()-_", "Safe1=+.,:?", "a" * 63):
    if not compiled_admin_password.fullmatch(allowed_password):
        print(f"Admin password allowlist rejects allowed input: {allowed_password}", file=sys.stderr)
        raise SystemExit(1)
for forbidden_password in ("Admin 123", r"Admin\123", 'Admin"123', "Admin/123", "Admin`123", "短密碼A123"):
    if compiled_admin_password.fullmatch(forbidden_password):
        print(f"Admin password allowlist accepts forbidden input: {forbidden_password}", file=sys.stderr)
        raise SystemExit(1)

settings_page = (project / "builtin-web/assets/js/pages/settings.js").read_text()
if "pattern: ADMIN_PATTERN" not in settings_page or settings_page.count("adminPasswordField(") != 3:
    print("Both admin password inputs must use the HTML allowlist", file=sys.stderr)
    raise SystemExit(1)

dom_helpers = (project / "builtin-web/assets/js/core/dom.js").read_text()
for required in (
    "function setTransientNotice(",
    "window.clearTimeout(globalNoticeTimer)",
    "generation === globalNoticeGeneration",
):
    if required not in dom_helpers:
        print(f"Global transient notice protection is missing: {required}", file=sys.stderr)
        raise SystemExit(1)

hardware_page = (project / "builtin-web/assets/js/pages/hardware.js").read_text()
for required in (
    "state.storage.app",
    "firmware_image_bytes",
    "frontend_payload_bytes",
    "fixed_regions",
    "bootloader_reserved_bytes",
    "partition_table_bytes",
    "state.storage.user",
    "max_upload_bytes",
    "user_nvs",
    "reserved_bytes",
    "flash.partitions",
    "firmwareAvailableSpace",
):
    if required not in hardware_page:
        print(f"Hardware storage view is missing the flash/storage contract: {required}", file=sys.stderr)
        raise SystemExit(1)
for legacy in ("storage.areas", "storageArea(", "system_assets"):
    if legacy in hardware_page:
        print(f"Hardware storage view still reads the standalone-assets contract: {legacy}", file=sys.stderr)
        raise SystemExit(1)
for disclaimer in ("systemStorageDescription", "not an upload quota", "不代表檔案上傳配額"):
    if disclaimer in hardware_page:
        print(f"Hardware storage view must not show a capacity disclaimer: {disclaimer}", file=sys.stderr)
        raise SystemExit(1)

wifi_module = (project / "builtin-web/assets/js/modules/wifi/controller.js").read_text()
scan_handler = wifi_module[wifi_module.index("async function scanWifi") : wifi_module.index("function selectScannedNetwork")]
if "setTransientNotice(t('scanComplete'))" not in scan_handler:
    print("Successful Wi-Fi scan notice must auto-dismiss", file=sys.stderr)
    raise SystemExit(1)
if "error.code === 'wifi_scan_busy' || error.code === 'wifi_connect_busy'" not in scan_handler:
    print("Retryable Wi-Fi scan busy errors must use localized frontend feedback", file=sys.stderr)
    raise SystemExit(1)
if "setTransientNotice(t('wifiConnectionReady'))" not in wifi_module or "setNotice(t('wifiApplyFailed'), true)" not in wifi_module:
    print("Wi-Fi connection success/error notices have incorrect behavior", file=sys.stderr)
    raise SystemExit(1)

wifi_resources = (project / "builtin-web/assets/js/core/api/resources.js").read_text()
if "update: (body) => api('/api/wifi', { method: 'PUT', body })" not in wifi_resources:
    print("Frontend Wi-Fi update resource is missing", file=sys.stderr)
    raise SystemExit(1)
save_handler = wifi_module[wifi_module.index("async function saveWifi") : wifi_module.index("function openScanDialog")]
for required in ("resources.wifi.update(payload)", "retryWifiStatus(payload.mode)"):
    if required not in save_handler:
        print(f"Frontend Wi-Fi update flow is missing: {required}", file=sys.stderr)
        raise SystemExit(1)
if not save_handler.index("resources.wifi.update(payload)") < save_handler.index("retryWifiStatus(payload.mode)"):
    print("Frontend must persist Wi-Fi before polling runtime status", file=sys.stderr)
    raise SystemExit(1)
if "/api/wifi/test" in wifi_resources or "commitTest" in wifi_resources:
    print("Frontend must not expose Wi-Fi test/commit resources", file=sys.stderr)
    raise SystemExit(1)

mock_api = (project / "builtin-web/assets/js/preview/mockApi.js").read_text()
for required in (
    "frontend_bundled: true",
    "firmware_image_bytes: 1257440",
    "frontend_payload_bytes: 39189",
    "bootloader_reserved_bytes: 32768",
    "partition_table_bytes: 4096",
    "id: 'userdata'",
    "id: 'user_nvs'",
    "max_upload_bytes: 1605632",
    "reserved_bytes: 65536",
    "allocation_unit_bytes: 4096",
):
    if required not in mock_api:
        print(f"Preview storage mock is missing the bundled storage contract: {required}", file=sys.stderr)
        raise SystemExit(1)

partition_table = (project / "partitions.csv").read_text()
for required in (
    "app0,     app,  ota_0,   0x10000, 0x1F0000,",
    "userdata, data, spiffs,  0x200000,0x1E8000,",
    "user_nvs, data, nvs,     0x3E8000,0x8000,",
):
    if required not in partition_table:
        print(f"Partition table is missing the bundled-app storage layout: {required}", file=sys.stderr)
        raise SystemExit(1)
if "spiffs,   data, spiffs" in partition_table:
    print("Partition table still reserves a standalone frontend filesystem", file=sys.stderr)
    raise SystemExit(1)
platformio = (project / "platformio.ini").read_text()
for required in (
    "board_build.partitions = partitions.csv",
    "-I build/.work/esp-generated",
):
    if required not in platformio:
        print(f"PlatformIO storage layout is missing: {required}", file=sys.stderr)
        raise SystemExit(1)

release_builder = (project / "tools/release-build/build_release.py").read_text()
for required in (
    "generate_embedded_web_header(web_manifest)",
    '"frontend_delivery"',
    '"none" if web_manifest["web"] == "none" else "embedded"',
    '"app_available_size"',
    "APP_SIZE = 0x1F0000",
    "USER_DATA_SIZE = 0x1E8000",
    "USER_NVS_SIZE = 0x8000",
    'find_named_partition(partitions, "userdata")',
    'find_named_partition(partitions, "user_nvs")',
    '"user_data_reserve_bytes": 64 * 1024',
    "release image would overwrite user data",
    "release image would overwrite user settings",
    "release must not contain a standalone frontend filesystem",
    "WEB=none snapshot must contain an empty unprocessed web output",
):
    if required not in release_builder:
        print(f"Release builder does not protect user data: {required}", file=sys.stderr)
        raise SystemExit(1)

if '"frontend not bundled"' not in server or "assets_->bundled()" not in server:
    print("HTTP static adapter does not distinguish firmware without a frontend", file=sys.stderr)
    raise SystemExit(1)

user_storage_header = (project / "src/modules/storage/UserDataStorage.h").read_text()
for required in (
    "kAllocationUnitBytes = 4096",
    "kReserveBytes = 64 * 1024",
):
    if required not in user_storage_header:
        print(f"User storage policy drifted from the API/release contract: {required}", file=sys.stderr)
        raise SystemExit(1)

preferences_backend = (project / "src/modules/config/storage/ArduinoPreferencesBackend.cpp").read_text()
if 'constexpr const char *kPartitionLabel = "user_nvs";' not in preferences_backend or \
        preferences_backend.count("kPartitionLabel") < 3:
    print("User settings are not isolated in the user_nvs partition", file=sys.stderr)
    raise SystemExit(1)
for required in (
    "nvs_flash_init_partition(kPartitionLabel)",
    "ESP_ERR_NVS_NOT_FOUND",
    "NamespaceState::Missing",
):
    if required not in preferences_backend:
        print(f"Blank user_nvs is not distinguished from a storage failure: {required}", file=sys.stderr)
        raise SystemExit(1)

storage_lifecycle = (project / "src/modules/storage/StorageLifecycle.cpp").read_text()
for required in (
    'constexpr const char *kSystemNvsPartition = "nvs";',
    'constexpr const char *kUserNvsInitializedKey = "user_nvs_init";',
    'constexpr const char *kUserDataInitializedKey = "userdata_init";',
    'constexpr const char *kResetPendingKey = "reset_pending";',
    'nvs_flash_erase_partition(kUserNvsPartition)',
):
    if required not in storage_lifecycle:
        print(f"Storage lifecycle metadata/reset behavior is missing: {required}", file=sys.stderr)
        raise SystemExit(1)
for route in ("POST /api/wifi/connect", "GET /api/wifi/connect"):
    if route not in mock_api:
        print(f"Preview mock is missing Wi-Fi connect route: {route}", file=sys.stderr)
        raise SystemExit(1)
for route in (
    "POST /api/system/reset",
    "POST /api/system/reset/settings",
    "POST /api/system/reset/data",
):
    if route not in mock_api:
        print(f"Preview mock is missing reset route: {route}", file=sys.stderr)
        raise SystemExit(1)
if "/api/wifi/test" in mock_api or "/commit" in mock_api:
    print("Preview mock must not expose Wi-Fi test/commit routes", file=sys.stderr)
    raise SystemExit(1)
if "code: 'wifi_scan_busy', retry_after_seconds: 1" not in mock_api:
    print("Preview scan must mirror the retryable persisted STA connecting response", file=sys.stderr)
    raise SystemExit(1)

system_module = (project / "builtin-web/assets/js/modules/system.js").read_text()
if "setTransientNotice(t('systemSaved'))" not in system_module or "setNotice(t('restarting'))" not in system_module:
    print("System success and restart notices have incorrect dismissal behavior", file=sys.stderr)
    raise SystemExit(1)

main_entry = (project / "builtin-web/assets/js/main.js").read_text()
if "setTransientNotice(t('ready'))" not in main_entry or "refreshNoticeTimer" in main_entry:
    print("Refresh completion must use the shared transient notice helper", file=sys.stderr)
    raise SystemExit(1)

zh_messages = (project / "builtin-web/assets/js/i18n/zh-Hant.js").read_text()
if "invalidAdminPassword: '管理員密碼不正確，請再試一次。'" not in zh_messages:
    print("Traditional Chinese login credential translation is missing", file=sys.stderr)
    raise SystemExit(1)

print("Release blocker source-contract checks passed")
