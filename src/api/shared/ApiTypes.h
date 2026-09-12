#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "core/Result.h"

namespace Api {

enum class Method : uint8_t {
  Get,
  Post,
  Put,
  Delete,
  Unknown,
};

enum class Transport : uint8_t {
  Unknown,
  Http,
  Serial,
};

struct QueryParameter {
  String name;
  String value;
};

struct Request {
  Method method = Method::Unknown;
  const char *path = "";
  JsonVariantConst body;
  bool hasJsonBody = false;
  bool hasBody = false;
  Transport transport = Transport::Unknown;
  String token;
  static constexpr size_t kMaxQueryParameters = 3;
  QueryParameter query[kMaxQueryParameters];
  size_t queryCount = 0;
  bool queryOverflow = false;

  bool matches(Method expectedMethod, const char *expectedPath) const;
  bool addQueryParameter(const char *name, const char *value);
};

// Internal continuation handle. Never serialized into the REST envelope.
struct PendingRequest {
  uint32_t id = 0;
  Method method = Method::Unknown;
  String path;
  String principal;
};

struct Response {
  int statusCode = 500;
  bool success = false;
  String data = "{}";
  String message = "internal error";
  PendingRequest pending{};
};

Method methodFromString(const char *value);
int statusFor(ResultCode code);

}  // namespace Api
