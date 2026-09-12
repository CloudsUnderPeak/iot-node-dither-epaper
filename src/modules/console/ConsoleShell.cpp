#include "ConsoleShell.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "api/shared/ApiResponse.h"
#include "api/wifi/WifiPayload.h"
#include "modules/storage/UserFilePolicy.h"

namespace {
const char *skipSpaces(const char *value) {
  while (*value == ' ' || *value == '\t') {
    ++value;
  }
  return value;
}

char *skipSpaces(char *value) {
  while (*value == ' ' || *value == '\t') {
    ++value;
  }
  return value;
}

char *nextToken(char *&cursor) {
  cursor = skipSpaces(cursor);
  if (*cursor == '\0') {
    return nullptr;
  }

  char *token = cursor;
  while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
    ++cursor;
  }
  if (*cursor != '\0') {
    *cursor = '\0';
    ++cursor;
  }
  return token;
}

}  // namespace

Result ConsoleShell::begin(ConfigService *configService,
                           WifiManager *wifiManager,
                           WifiScanner *wifiScanner,
                           const FlashStorage *flashStorage,
                           UserDataStorage *userData,
                           AuthService *authService,
                           ApiRouter *router) {
  if (configService == nullptr || wifiManager == nullptr || wifiScanner == nullptr ||
      flashStorage == nullptr || userData == nullptr || authService == nullptr || router == nullptr) {
    return invalidInput("missing console dependencies");
  }

  configService_ = configService;
  wifiManager_ = wifiManager;
  wifiScanner_ = wifiScanner;
  flashStorage_ = flashStorage;
  userData_ = userData;
  authService_ = authService;
  router_ = router;
  return configCommand_.begin(configService, router);
}

void ConsoleShell::clearLine() {
  volatile char *cursor = line_;
  for (size_t index = 0; index < sizeof(line_); ++index) cursor[index] = '\0';
  lineLength_ = 0;
}

void ConsoleShell::poll() {
  if (pendingApi_.id != 0) {
    Api::Response response;
    if (!router_->pollPending(pendingApi_, response)) return;
    pendingApi_ = {};
    printApiResponse(response);
  }
  if (pendingHumanScan_ != 0) {
    WifiScanResult result;
    if (!wifiScanner_->takeResult(pendingHumanScan_, result)) return;
    pendingHumanScan_ = 0;
    printScanResult(result);
  }
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());
    if (incoming == '\r') {
      continue;
    }
    if (incoming == '\n') {
      if (discardingLine_) {
        if (discardedApiLine_) {
          printApiResponse(Api::problem(413, "payload_too_large", "serial API input is too long"));
        } else {
          Serial.println("Console input is too long. Line cleared.");
        }
        discardingLine_ = false;
        discardedApiLine_ = false;
        clearLine();
        continue;
      }
      line_[lineLength_] = '\0';
      handleLine(line_);
      clearLine();
      if (pendingApi_.id != 0 || pendingHumanScan_ != 0) return;
      continue;
    }
    if (discardingLine_) {
      continue;
    }
    if (lineLength_ + 1 >= kInputCapacity) {
      line_[lineLength_] = '\0';
      const char *command = skipSpaces(line_);
      discardedApiLine_ = strncmp(command, "api", 3) == 0 &&
                          (command[3] == '\0' || command[3] == ' ' || command[3] == '\t');
      discardingLine_ = true;
      clearLine();
      continue;
    }
    line_[lineLength_++] = incoming;
  }
}

void ConsoleShell::handleLine(char *line) {
  char *command = skipSpaces(line);
  if (*command == '\0') {
    return;
  }

  if (strncmp(command, "api", 3) == 0 && (command[3] == '\0' || command[3] == ' ' || command[3] == '\t')) {
    handleApiCommand(command + 3);
    return;
  }

  handleHumanCommand(command);
}

void ConsoleShell::handleHumanCommand(char *command) {
  if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
    printHelp();
  } else if (strcmp(command, "status") == 0) {
    printStatus();
  } else if (strcmp(command, "device") == 0) {
    printDevice();
  } else if (strcmp(command, "wifi") == 0) {
    printWifi();
  } else if (strcmp(command, "scan") == 0) {
    printScan();
  } else if (strcmp(command, "storage") == 0) {
    printStorage();
  } else if (strcmp(command, "session") == 0) {
    printSession();
  } else if (strncmp(command, "ls", 2) == 0 &&
             (command[2] == '\0' || command[2] == ' ' || command[2] == '\t')) {
    handleListCommand(command + 2);
  } else if (strncmp(command, "stat", 4) == 0 &&
             (command[4] == '\0' || command[4] == ' ' || command[4] == '\t')) {
    handleStatCommand(command + 4);
  } else if (strncmp(command, "config", 6) == 0 &&
             (command[6] == '\0' || command[6] == ' ' || command[6] == '\t')) {
    configCommand_.handle(command + 6);
  } else {
    Serial.println("Unknown command. Type help.");
  }
}

void ConsoleShell::handleApiCommand(char *command) {
  char *cursor = command;
  char *method = nextToken(cursor);
  char *path = nextToken(cursor);
  if (method == nullptr || path == nullptr) {
    printApiResponse(Api::problem(
        400, "missing_field", "usage: api METHOD PATH [token=<token>] [json]"));
    return;
  }

  const Api::Method apiMethod = Api::methodFromString(method);
  if (apiMethod == Api::Method::Unknown) {
    printApiResponse(Api::problem(400, "invalid_field", "unsupported method", "method"));
    return;
  }

  cursor = skipSpaces(cursor);
  const char *token = "";
  const char *body = cursor;
  if (strncmp(cursor, "token=", 6) == 0) {
    token = cursor + 6;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      ++cursor;
    }
    if (*cursor != '\0') {
      *cursor = '\0';
      ++cursor;
    }
    body = skipSpaces(cursor);
  }

  JsonDocument doc;
  Api::Request request;
  request.method = apiMethod;
  request.path = path;
  request.transport = Api::Transport::Serial;
  request.token = token;
  if (*skipSpaces(body) != '\0') {
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
      printApiResponse(Api::problem(400, "invalid_json", "invalid JSON body"));
      return;
    }
    request.body = doc.as<JsonVariantConst>();
    request.hasJsonBody = true;
    request.hasBody = true;
  }

  const Api::Response response = router_->dispatch(request);
  for (size_t index = 0; index < request.token.length(); ++index) {
    request.token.setCharAt(index, '\0');
  }
  request.token.remove(0);
  if (response.pending.id != 0) pendingApi_ = response.pending;
  else printApiResponse(response);
}

void ConsoleShell::printHelp() const {
  Serial.println("Console commands:");
  Serial.println("  help      Show this command list");
  Serial.println("  status    Show service readiness and Wi-Fi summary");
  Serial.println("  device    Show device identity and memory");
  Serial.println("  wifi      Show Wi-Fi mode, addresses, and saved SSIDs");
  Serial.println("  scan      List nearby Wi-Fi networks");
  Serial.println("  storage   Show flash partitions and available space");
  Serial.println("  session   Show whether an admin session is active");
  Serial.println("  ls [path] [offset]                 List userdata (32 entries per page)");
  Serial.println("  stat <path>                        Inspect a userdata path");
  Serial.println("  config show|get|set|changes|revert|status|commit ...");
  Serial.println("  api METHOD PATH [token=<token>] [json]");
}

void ConsoleShell::handleListCommand(char *arguments) {
  char *cursor = arguments;
  char *pathArgument = nextToken(cursor);
  char *offsetArgument = nextToken(cursor);
  if (nextToken(cursor) != nullptr) {
    Serial.println("Usage: ls [path] [offset]");
    return;
  }
  size_t offset = 0;
  if (offsetArgument != nullptr &&
      !UserFilePolicy::parseSize(offsetArgument, offset)) {
    Serial.println("Usage: ls [path] [offset]");
    return;
  }
  char normalized[UserDataPath::kMaxNormalizedBytes + 1]{};
  const UserDataPath::Error pathError = UserDataPath::normalize(
      pathArgument == nullptr ? "" : pathArgument,
      normalized,
      sizeof(normalized));
  if (pathError != UserDataPath::Error::None) {
    Serial.printf("Path error: %s\n", UserDataPath::message(pathError));
    return;
  }

  Serial.printf("Userdata %s\n", normalized);
  struct ListPrintContext {
    size_t count = 0;
  } context;
  const auto visitor = [](const UserDataInspectionEntry &entry, void *opaque) {
    auto *printContext = static_cast<ListPrintContext *>(opaque);
    if (entry.directory) {
      Serial.printf("  d          %s/\n", entry.name);
    } else {
      Serial.printf("  f %8u %s\n",
                    static_cast<unsigned>(entry.sizeBytes), entry.name);
    }
    ++printContext->count;
  };
  const UserDataInspectionPage page = userData_->inspectList(
      normalized, offset, 32, visitor, &context);
  if (!page.result.ok()) {
    printFilesystemError(page.result);
    return;
  }
  if (context.count == 0) Serial.println("  (empty)");
  if (page.hasMore) {
    Serial.printf("More: ls %s %u\n",
                  normalized, static_cast<unsigned>(page.nextOffset));
  }
}

void ConsoleShell::handleStatCommand(char *arguments) {
  char *cursor = arguments;
  char *pathArgument = nextToken(cursor);
  if (pathArgument == nullptr || nextToken(cursor) != nullptr) {
    Serial.println("Usage: stat <path>");
    return;
  }
  char normalized[UserDataPath::kMaxNormalizedBytes + 1]{};
  const UserDataPath::Error pathError = UserDataPath::normalize(
      pathArgument, normalized, sizeof(normalized));
  if (pathError != UserDataPath::Error::None) {
    Serial.printf("Path error: %s\n", UserDataPath::message(pathError));
    return;
  }
  UserDataInspectionEntry entry;
  const UserDataFileResult result = userData_->inspectStat(normalized, entry);
  if (!result.ok()) {
    printFilesystemError(result);
    return;
  }
  Serial.printf("Userdata %s\n", normalized);
  Serial.printf("  Type: %s\n", entry.directory ? "directory" : "file");
  if (!entry.directory) {
    Serial.printf("  Size: %u bytes\n", static_cast<unsigned>(entry.sizeBytes));
  }
}

void ConsoleShell::printFilesystemError(const UserDataFileResult &result) const {
  if (result.status == UserDataFileStatus::Busy) {
    Serial.println("Filesystem error: user storage is busy");
  } else if (result.status == UserDataFileStatus::Unavailable) {
    Serial.println("Filesystem error: userdata is not mounted");
  } else {
    Serial.printf("Filesystem error: %s\n", result.message);
  }
}

void ConsoleShell::printStatus() const {
  const DeviceConfig config = configService_->snapshot();
  const WifiStatus status = wifiManager_->status();
  Serial.println("Status");
  Serial.printf("  Wi-Fi mode: %s\n", WifiPayload::wifiModeToApiString(status.mode));
  Serial.printf("  Configured mode: %s\n", WifiPayload::wifiModeToApiString(config.wifiMode));
  Serial.printf("  STA: %s, %s\n", wifiLinkStateToString(status.staState), status.staIp.toString().c_str());
  Serial.printf("  AP: %s\n", status.apIp.toString().c_str());
  Serial.printf("  Flash layout: %s\n", flashStorage_->ready() ? "ready" : "not ready");
  Serial.printf("  Userdata: %s\n", userData_->mounted() ? "mounted" : "not mounted");
  Serial.printf("  Admin session: %s\n", authService_->hasActiveSession() ? "active" : "none");
}

void ConsoleShell::printDevice() const {
  const DeviceConfig config = configService_->snapshot();
  Serial.println("Device");
  Serial.printf("  Chip: %s rev %u\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("  Cores: %u\n", ESP.getChipCores());
  Serial.printf("  Flash: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("  Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("  MAC: %s\n", WiFi.macAddress().c_str());
  Serial.printf("  Hostname: %s\n", config.hostname);
}

void ConsoleShell::printWifi() const {
  const DeviceConfig config = configService_->snapshot();
  const WifiStatus status = wifiManager_->status();
  Serial.println("Wi-Fi");
  Serial.printf("  Current mode: %s\n", WifiPayload::wifiModeToApiString(status.mode));
  Serial.printf("  Saved mode: %s\n", WifiPayload::wifiModeToApiString(config.wifiMode));
  Serial.printf("  Fallback to setup network: %s\n", config.fallbackToAp ? "on" : "off");
  Serial.printf("  Saved home network: %s\n", config.staSsid[0] == '\0' ? "(not set)" : config.staSsid);
  Serial.printf("  Saved home security: %s\n", staSecurityToString(config.staSecurity));
  Serial.printf("  Saved home IPv4: %s", staIpModeToString(config.staIpMode));
  if (config.staIpMode == StaIpMode::Static) {
    Serial.printf(", %s, gateway %s, netmask %s", config.staIpAddress, config.staIpGateway, config.staIpNetmask);
  }
  Serial.println();
  Serial.printf("  Home connection: %s, %s\n", wifiLinkStateToString(status.staState), status.staIp.toString().c_str());
  Serial.printf("  Setup network: %s, password %s, runtime %s, configured %s/%s\n",
                config.apSsid,
                config.apPasswordEnabled ? "on" : "off",
                status.apIp.toString().c_str(),
                config.apIpAddress,
                config.apIpNetmask);
}

void ConsoleShell::printScan() {
  if (wifiManager_->testBlocksScan()) {
    Serial.println("Scan unavailable while a STA connection is active.");
    return;
  }
  if (wifiManager_->staConnectionBlocksScan()) {
    Serial.println("Scan unavailable while STA is connecting. Try again shortly.");
    return;
  }
  Serial.println("Scanning nearby Wi-Fi networks...");
  const WifiScanResult scanResult = wifiScanner_->start(millis());
  if (scanResult.operationId != 0) { pendingHumanScan_ = scanResult.operationId; return; }
  printScanResult(scanResult);
}

void ConsoleShell::printScanResult(const WifiScanResult &scanResult) {
  if (scanResult.busy || scanResult.connectBusy) {
    Serial.println("Scan is busy. Try again shortly.");
    return;
  }
  if (scanResult.retryAfterSeconds > 0) {
    Serial.printf("Scan is cooling down. Try again in %u seconds.\n",
                  static_cast<unsigned>(scanResult.retryAfterSeconds));
    return;
  }
  if (!scanResult.result.ok()) {
    Serial.println("Scan failed.");
    return;
  }

  if (scanResult.count == 0) {
    Serial.println("No networks found.");
  } else {
    for (size_t index = 0; index < scanResult.count; ++index) {
      const WifiScanNetwork &network = scanResult.networks[index];
      Serial.printf("  %2u. %s, %d dBm, channel %d, %s%s\n",
                    static_cast<unsigned>(index + 1),
                    network.hidden ? "(hidden)" : network.ssid.c_str(),
                    network.rssi,
                    network.channel,
                    network.encryption,
                    network.hidden ? ", hidden" : "");
    }
  }
}

void ConsoleShell::printStorage() const {
  const FlashStorageSnapshot flash = flashStorage_->snapshot();
  const UploadCapacity user = userData_->uploadCapacity();
  Serial.println("Storage");
  Serial.printf("  Flash: %s, total=%u bytes\n",
                flashStorage_->ready() ? "ready" : "not ready",
                static_cast<unsigned>(flash.totalBytes));
  for (size_t index = 0; index < flash.partitionCount; ++index) {
    const FlashPartitionCapacity &partition = flash.partitions[index];
    Serial.printf("  Partition %s: type=%s, subtype=%s, offset=0x%06x, size=%u bytes\n",
                  partition.id,
                  partition.type,
                  partition.subtype,
                  static_cast<unsigned>(partition.offsetBytes),
                  static_cast<unsigned>(partition.sizeBytes));
  }
  Serial.printf("  App: partition=%s, capacity=%u, firmware_image=%u, frontend=%u, available=%u bytes\n",
                flash.app.partitionId,
                static_cast<unsigned>(flash.app.totalBytes),
                static_cast<unsigned>(flash.app.imageBytes),
                static_cast<unsigned>(flash.app.frontendBytes),
                static_cast<unsigned>(flash.app.availableBytes));
  Serial.printf("  User: %s, capacity=%u, used=%u, available=%u, max_upload=%u bytes\n",
                userData_->mounted() ? "mounted" : "not mounted",
                static_cast<unsigned>(user.totalBytes),
                static_cast<unsigned>(user.usedBytes),
                static_cast<unsigned>(user.availableBytes),
                static_cast<unsigned>(user.maxUploadBytes));
}

void ConsoleShell::printSession() const {
  Serial.println("Admin Session");
  Serial.printf("  Active: %s\n", authService_->hasActiveSession() ? "yes" : "no");
}

void ConsoleShell::printApiResponse(const Api::Response &response) const {
  Serial.println(Api::serialize(response));
}
