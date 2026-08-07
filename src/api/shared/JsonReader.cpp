#include "JsonReader.h"

#include "ApiResponse.h"

JsonReader::JsonReader(JsonObjectConst object, const String &path, JsonDecodeError &error)
    : object_(object), path_(path), error_(&error) {}

const char *JsonReader::requiredString(const char *key, const char *message) {
  JsonVariantConst value = object_[key];
  if (value.isNull()) {
    fail("missing_field", key, message);
    return "";
  }
  if (!value.is<const char *>()) {
    fail("invalid_field", key, "field must be a string");
    return "";
  }
  return value.as<const char *>();
}

const char *JsonReader::optionalString(const char *key,
                                       bool &provided,
                                       const char *typeMessage) {
  JsonVariantConst value = object_[key];
  provided = !value.isNull();
  if (!provided) return "";
  if (!value.is<const char *>()) {
    fail("invalid_field", key, typeMessage);
    return "";
  }
  return value.as<const char *>();
}

bool JsonReader::requiredBool(const char *key, const char *message) {
  JsonVariantConst value = object_[key];
  if (value.isNull()) {
    fail("missing_field", key, message);
    return false;
  }
  if (!value.is<bool>()) {
    fail("invalid_field", key, "field must be a boolean");
    return false;
  }
  return value.as<bool>();
}

uint32_t JsonReader::requiredUint32(const char *key, const char *message) {
  JsonVariantConst value = object_[key];
  if (value.isNull()) {
    fail("missing_field", key, message);
    return 0;
  }
  if (!value.is<uint32_t>()) {
    fail("invalid_field", key, "field must be an unsigned integer");
    return 0;
  }
  return value.as<uint32_t>();
}

JsonArrayConst JsonReader::requiredArray(const char *key, const char *message) {
  JsonVariantConst value = object_[key];
  if (value.isNull()) {
    fail("missing_field", key, message);
    return JsonArrayConst();
  }
  if (!value.is<JsonArrayConst>()) {
    fail("invalid_field", key, "field must be an array");
    return JsonArrayConst();
  }
  return value.as<JsonArrayConst>();
}

JsonReader JsonReader::requiredObject(const char *key, const char *message) {
  JsonVariantConst value = object_[key];
  if (value.isNull()) {
    fail("missing_field", key, message);
    return JsonReader(JsonObjectConst(), fieldPath(key), *error_);
  }
  if (!value.is<JsonObjectConst>()) {
    fail("invalid_field", key, "field must be an object");
    return JsonReader(JsonObjectConst(), fieldPath(key), *error_);
  }
  return JsonReader(value.as<JsonObjectConst>(), fieldPath(key), *error_);
}

void JsonReader::finish(std::initializer_list<const char *> allowedKeys) {
  for (JsonPairConst pair : object_) {
    bool allowed = false;
    for (const char *allowedKey : allowedKeys) {
      if (strcmp(pair.key().c_str(), allowedKey) == 0) {
        allowed = true;
        break;
      }
    }
    if (!allowed) {
      fail("unsupported_field", pair.key().c_str(), "unsupported field");
      return;
    }
  }
}

String JsonReader::fieldPath(const char *key) const {
  if (path_.length() == 0) return String(key);
  String field = path_;
  field += ".";
  field += key;
  return field;
}

void JsonReader::fail(const char *code, const char *key, const char *message) {
  if (!error_->ok()) return;
  error_->code = code;
  error_->field = fieldPath(key);
  error_->message = message;
}

namespace ApiRequest {

Api::Response requireObject(const Api::Request &request, JsonObjectConst &root) {
  if (!request.hasJsonBody) {
    return Api::problem(400, "invalid_json", "JSON body is required");
  }
  root = request.body.as<JsonObjectConst>();
  return root.isNull()
             ? Api::problem(400, "invalid_json", "JSON body must be an object")
             : Api::ok("{}", "ok");
}

bool decodeCredentials(JsonObjectConst root,
                       const char *&username,
                       const char *&password,
                       JsonDecodeError &error) {
  JsonReader reader(root, "", error);
  username = reader.requiredString("username", "username is required");
  password = reader.requiredString("password", "password is required");
  reader.finish({"username", "password"});
  return error.ok();
}

}  // namespace ApiRequest
