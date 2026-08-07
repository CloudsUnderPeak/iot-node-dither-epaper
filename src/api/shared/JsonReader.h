#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <initializer_list>

#include "ApiTypes.h"

struct JsonDecodeError {
  const char *code = nullptr;
  String field;
  const char *message = nullptr;

  bool ok() const { return code == nullptr; }
};

class JsonReader {
 public:
  JsonReader(JsonObjectConst object, const String &path, JsonDecodeError &error);

  const char *requiredString(const char *key, const char *message);
  const char *optionalString(const char *key, bool &provided, const char *typeMessage);
  bool requiredBool(const char *key, const char *message);
  uint32_t requiredUint32(const char *key, const char *message);
  JsonArrayConst requiredArray(const char *key, const char *message);
  JsonReader requiredObject(const char *key, const char *message);
  void finish(std::initializer_list<const char *> allowedKeys);

 private:
  JsonObjectConst object_;
  String path_;
  JsonDecodeError *error_;

  String fieldPath(const char *key) const;
  void fail(const char *code, const char *key, const char *message);
};

namespace ApiRequest {

Api::Response requireObject(const Api::Request &request, JsonObjectConst &root);
bool decodeCredentials(JsonObjectConst root,
                       const char *&username,
                       const char *&password,
                       JsonDecodeError &error);

}  // namespace ApiRequest
