#include "ConsoleConfigCommand.h"

#include <ArduinoJson.h>
#include <cstring>

#include "api/wifi/WifiPayload.h"

namespace {

char *skipSpaces(char *value) {
  while (*value == ' ' || *value == '\t') ++value;
  return value;
}

char *nextToken(char *&cursor) {
  cursor = skipSpaces(cursor);
  if (*cursor == '\0') return nullptr;
  char *token = cursor;
  while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') ++cursor;
  if (*cursor != '\0') *cursor++ = '\0';
  return token;
}

bool noMoreTokens(char *cursor) {
  return *skipSpaces(cursor) == '\0';
}

}  // namespace

Result ConsoleConfigCommand::begin(ConfigService *configService,
                                   ApiRouter *router) {
  if (configService == nullptr || router == nullptr) {
    return invalidInput("missing console config dependencies");
  }
  configService_ = configService;
  router_ = router;
  return okResult();
}

void ConsoleConfigCommand::handle(char *arguments) {
  char *cursor = arguments;
  char *subcommand = nextToken(cursor);
  if (subcommand == nullptr) {
    Serial.println("Usage: config show|get|set|changes|revert|status|commit ...");
  } else if (strcmp(subcommand, "show") == 0) {
    show(cursor);
  } else if (strcmp(subcommand, "get") == 0) {
    get(cursor);
  } else if (strcmp(subcommand, "set") == 0) {
    set(cursor);
  } else if (strcmp(subcommand, "changes") == 0) {
    changes(cursor);
  } else if (strcmp(subcommand, "revert") == 0) {
    revert(cursor);
  } else if (strcmp(subcommand, "status") == 0) {
    status(cursor);
  } else if (strcmp(subcommand, "commit") == 0) {
    commit(cursor);
  } else {
    Serial.println("Config error: unknown subcommand");
  }
}

void ConsoleConfigCommand::show(char *arguments) {
  char *cursor = arguments;
  char *prefix = nextToken(cursor);
  if (!noMoreTokens(cursor)) {
    Serial.println("Usage: config show [prefix]");
    return;
  }
  const DeviceConfig current = configService_->snapshot();
  DeviceConfig working = current;
  staging_.apply(working);
  bool matched = false;
  for (size_t index = 0; index < ConfigStaging::keyCount(); ++index) {
    const ConsoleConfigKeyInfo &info = ConfigStaging::keyInfo(index);
    if (info.readable && ConfigStaging::keyMatchesPrefix(info.key, prefix)) {
      printKeyValue(info.key, working);
      matched = true;
    }
  }
  if (!matched) Serial.println("Config error: unknown prefix");
}

void ConsoleConfigCommand::get(char *arguments) {
  char *cursor = arguments;
  char *name = nextToken(cursor);
  if (name == nullptr || !noMoreTokens(cursor)) {
    Serial.println("Usage: config get <key>");
    return;
  }
  const ConsoleConfigKey key = ConfigStaging::findKey(name);
  if (key == ConsoleConfigKey::Unknown) {
    Serial.println("Config error: unknown key");
    return;
  }
  const ConsoleConfigKeyInfo &info = ConfigStaging::keyInfo(static_cast<size_t>(key));
  if (!info.readable) {
    Serial.println("Config error: sensitive value is not readable");
    return;
  }
  DeviceConfig working = configService_->snapshot();
  staging_.apply(working);
  printKeyValue(key, working);
}

void ConsoleConfigCommand::set(char *arguments) {
  char *assignment = skipSpaces(arguments);
  char *equals = strchr(assignment, '=');
  if (equals == nullptr || equals == assignment) {
    Serial.println("Usage: config set <key>=<json-value>");
    return;
  }
  *equals = '\0';
  for (const char *cursor = assignment; *cursor != '\0'; ++cursor) {
    if (*cursor == ' ' || *cursor == '\t') {
      Serial.println("Usage: config set <key>=<json-value>");
      return;
    }
  }
  const ConsoleConfigKey key = ConfigStaging::findKey(assignment);
  if (key == ConsoleConfigKey::Unknown) {
    Serial.println("Config error: unknown key");
    return;
  }

  JsonDocument value;
  const char *jsonValue = equals + 1;
  if (*jsonValue == '\0' || deserializeJson(value, jsonValue)) {
    Serial.println("Config error: value must be one valid JSON value");
    return;
  }
  const ConsoleConfigSetResult result = staging_.set(
      key, value.as<JsonVariantConst>());
  if (!result.success) {
    Serial.printf("Config error: %s\n", result.message);
    return;
  }
  Serial.printf("Config staged: %s\n",
                ConfigStaging::keyInfo(static_cast<size_t>(key)).name);
}

void ConsoleConfigCommand::changes(char *arguments) {
  char *cursor = arguments;
  char *groupText = nextToken(cursor);
  if (!noMoreTokens(cursor)) {
    Serial.println("Usage: config changes [group]");
    return;
  }
  ConsoleConfigGroup group = ConsoleConfigGroup::Unknown;
  if (groupText != nullptr) {
    group = ConfigStaging::groupFromString(groupText);
    if (group == ConsoleConfigGroup::Unknown) {
      Serial.println("Config error: unknown group");
      return;
    }
  }

  const DeviceConfig current = configService_->snapshot();
  DeviceConfig working = current;
  staging_.apply(working);
  bool printed = false;
  for (size_t index = 0; index < ConfigStaging::keyCount(); ++index) {
    const ConsoleConfigKeyInfo &info = ConfigStaging::keyInfo(index);
    if (!staging_.dirty(info.key) ||
        (group != ConsoleConfigGroup::Unknown && info.group != group)) {
      continue;
    }
    if (info.secret) {
      Serial.printf("%s: <updated>\n", info.name);
    } else {
      Serial.printf("%s: %s -> %s\n",
                    info.name,
                    valueLiteral(info.key, current).c_str(),
                    valueLiteral(info.key, working).c_str());
    }
    printed = true;
  }
  if (!printed) Serial.println("No staged changes.");
}

void ConsoleConfigCommand::revert(char *arguments) {
  char *cursor = arguments;
  char *target = nextToken(cursor);
  if (target == nullptr || !noMoreTokens(cursor)) {
    Serial.println("Usage: config revert <group-or-key>");
    return;
  }
  const ConsoleConfigGroup group = ConfigStaging::groupFromString(target);
  if (group != ConsoleConfigGroup::Unknown) {
    staging_.revert(group);
    Serial.printf("Config reverted: %s\n", target);
    return;
  }
  const ConsoleConfigKey key = ConfigStaging::findKey(target);
  if (key == ConsoleConfigKey::Unknown) {
    Serial.println("Config error: unknown group or key");
    return;
  }
  staging_.revert(key);
  Serial.printf("Config reverted: %s\n", target);
}

void ConsoleConfigCommand::status(char *arguments) {
  if (!noMoreTokens(arguments)) {
    Serial.println("Usage: config status");
    return;
  }
  const DeviceConfig current = configService_->snapshot();
  Serial.println("Configuration");
  Serial.printf("  State: %s\n", configStartupStateToString(configService_->startupState()));
  Serial.printf("  Recovery reason: %s\n",
                configRecoveryReasonToString(configService_->recoveryReason()));
  Serial.printf("  Schema: %u\n", static_cast<unsigned>(current.schemaVersion));
  Serial.print("  Staged groups: ");
  bool printed = false;
  for (ConsoleConfigGroup group : {ConsoleConfigGroup::Wifi,
                                   ConsoleConfigGroup::System,
                                   ConsoleConfigGroup::Auth}) {
    if (!staging_.dirty(group)) continue;
    if (printed) Serial.print(", ");
    Serial.print(ConfigStaging::groupName(group));
    printed = true;
  }
  Serial.println(printed ? "" : "none");
}

void ConsoleConfigCommand::commit(char *arguments) {
  char *cursor = arguments;
  char *groupText = nextToken(cursor);
  char *tokenArgument = nextToken(cursor);
  if (groupText == nullptr || tokenArgument == nullptr ||
      strncmp(tokenArgument, "token=", 6) != 0 || tokenArgument[6] == '\0' ||
      !noMoreTokens(cursor)) {
    Serial.println("Usage: config commit <group> token=<token>");
    return;
  }
  const ConsoleConfigGroup group = ConfigStaging::groupFromString(groupText);
  if (group == ConsoleConfigGroup::Unknown) {
    Serial.println("Config error: unknown group");
    return;
  }
  if (!staging_.dirty(group)) {
    Serial.println("Config error: group has no staged changes");
    return;
  }

  DeviceConfig candidate = configService_->snapshot();
  staging_.applyGroup(group, candidate);
  JsonDocument body = buildPayload(group, candidate);
  Api::Request request;
  request.transport = Api::Transport::Serial;
  request.method = Api::Method::Put;
  request.hasBody = true;
  request.hasJsonBody = true;
  request.body = body.as<JsonVariantConst>();
  request.token = tokenArgument + 6;
  if (group == ConsoleConfigGroup::Wifi) request.path = "/api/wifi";
  else if (group == ConsoleConfigGroup::System) request.path = "/api/system";
  else request.path = "/api/auth/password";

  const Api::Response response = router_->dispatch(request);
  for (size_t index = 0; index < request.token.length(); ++index) {
    request.token.setCharAt(index, '\0');
  }
  request.token.remove(0);
  if (!response.success) {
    printRouterError(response);
    return;
  }
  staging_.revert(group);
  if (group == ConsoleConfigGroup::Wifi && response.statusCode == 202) {
    Serial.println("Config accepted: wifi transition started");
    Serial.println("Check: api GET /api/wifi/connect token=<token>");
  } else {
    Serial.printf("Config committed: %s\n", ConfigStaging::groupName(group));
    if (group == ConsoleConfigGroup::Auth) {
      Serial.println("Admin session invalidated; log in again with the new password.");
    }
  }
}

void ConsoleConfigCommand::printKeyValue(ConsoleConfigKey key,
                                         const DeviceConfig &config) const {
  const ConsoleConfigKeyInfo &info = ConfigStaging::keyInfo(static_cast<size_t>(key));
  Serial.printf("%s=%s\n", info.name, valueLiteral(key, config).c_str());
}

String ConsoleConfigCommand::valueLiteral(ConsoleConfigKey key,
                                          const DeviceConfig &config) const {
  JsonDocument value;
  switch (key) {
    case ConsoleConfigKey::SystemHostname: value.set(config.hostname); break;
    case ConsoleConfigKey::AuthUsername: value.set(config.adminUsername); break;
    case ConsoleConfigKey::AuthPasswordSet: value.set(config.adminPassword[0] != '\0'); break;
    case ConsoleConfigKey::WifiMode: value.set(WifiPayload::wifiModeToApiString(config.wifiMode)); break;
    case ConsoleConfigKey::WifiFallbackToAp: value.set(config.fallbackToAp); break;
    case ConsoleConfigKey::WifiStaSsid: value.set(config.staSsid); break;
    case ConsoleConfigKey::WifiStaSecurity: value.set(staSecurityToString(config.staSecurity)); break;
    case ConsoleConfigKey::WifiStaPasswordSet: value.set(config.staPassword[0] != '\0'); break;
    case ConsoleConfigKey::WifiStaIpMode: value.set(staIpModeToString(config.staIpMode)); break;
    case ConsoleConfigKey::WifiStaIpAddress: value.set(config.staIpAddress); break;
    case ConsoleConfigKey::WifiStaIpGateway: value.set(config.staIpGateway); break;
    case ConsoleConfigKey::WifiStaIpNetmask: value.set(config.staIpNetmask); break;
    case ConsoleConfigKey::WifiStaIpDns: {
      JsonArray dns = value.to<JsonArray>();
      if (config.staDns1[0] != '\0') dns.add(config.staDns1);
      if (config.staDns2[0] != '\0') dns.add(config.staDns2);
      break;
    }
    case ConsoleConfigKey::WifiApSsid: value.set(config.apSsid); break;
    case ConsoleConfigKey::WifiApPasswordEnabled: value.set(config.apPasswordEnabled); break;
    case ConsoleConfigKey::WifiApIpMode: value.set(apIpModeToString(config.apIpMode)); break;
    case ConsoleConfigKey::WifiApIpAddress: value.set(config.apIpAddress); break;
    case ConsoleConfigKey::WifiApIpNetmask: value.set(config.apIpNetmask); break;
    case ConsoleConfigKey::AuthPassword:
    case ConsoleConfigKey::WifiStaPassword:
    case ConsoleConfigKey::Count:
    case ConsoleConfigKey::Unknown:
      value.set(nullptr);
      break;
  }
  String output;
  serializeJson(value, output);
  return output;
}

JsonDocument ConsoleConfigCommand::buildPayload(
    ConsoleConfigGroup group,
    const DeviceConfig &config) const {
  JsonDocument body;
  if (group == ConsoleConfigGroup::System) {
    body["hostname"] = config.hostname;
    return body;
  }
  if (group == ConsoleConfigGroup::Auth) {
    body["password"] = config.adminPassword;
    return body;
  }

  body["mode"] = WifiPayload::wifiModeToApiString(config.wifiMode);
  body["fallback_to_ap"] = config.fallbackToAp;
  JsonObject interfaces = body["interfaces"].to<JsonObject>();
  JsonObject sta = interfaces["sta"].to<JsonObject>();
  sta["ssid"] = config.staSsid;
  sta["security"] = staSecurityToString(config.staSecurity);
  if (staging_.dirty(ConsoleConfigKey::WifiStaPassword)) {
    sta["password"] = config.staPassword;
  } else if (config.staSecurity == StaSecurity::Wpa && config.staPassword[0] != '\0') {
    sta["password"] = "********";
  }
  JsonObject staIp = sta["ip_config"].to<JsonObject>();
  staIp["mode"] = staIpModeToString(config.staIpMode);
  staIp["address"] = config.staIpAddress;
  staIp["gateway"] = config.staIpGateway;
  staIp["netmask"] = config.staIpNetmask;
  JsonArray dns = staIp["dns"].to<JsonArray>();
  if (config.staDns1[0] != '\0') dns.add(config.staDns1);
  if (config.staDns2[0] != '\0') dns.add(config.staDns2);

  JsonObject ap = interfaces["ap"].to<JsonObject>();
  ap["ssid"] = config.apSsid;
  ap["password_enabled"] = config.apPasswordEnabled;
  JsonObject apIp = ap["ip_config"].to<JsonObject>();
  apIp["mode"] = apIpModeToString(config.apIpMode);
  if (config.apIpMode == ApIpMode::Static) {
    apIp["address"] = config.apIpAddress;
    apIp["netmask"] = config.apIpNetmask;
  }
  return body;
}

void ConsoleConfigCommand::printRouterError(const Api::Response &response) const {
  JsonDocument data;
  const DeserializationError parseError = deserializeJson(data, response.data);
  const char *code = parseError ? "request_failed" : data["code"] | "request_failed";
  const char *field = "";
  if (!parseError && data["fields"].is<JsonArrayConst>() &&
      data["fields"].as<JsonArrayConst>().size() > 0) {
    field = data["fields"][0] | "";
  }
  if (field[0] != '\0') {
    Serial.printf("Config error [%s] %s: %s\n", code, field, response.message.c_str());
  } else {
    Serial.printf("Config error [%s]: %s\n", code, response.message.c_str());
  }
}
