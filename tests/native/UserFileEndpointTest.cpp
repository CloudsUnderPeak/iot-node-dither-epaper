#include <ArduinoJson.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "api/storage/UserFileEndpoints.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

const char *code(const Api::Response &response, JsonDocument &document) {
  document.clear();
  if (deserializeJson(document, response.data.c_str())) return "parse_error";
  return document["code"] | "";
}

void testListQueries() {
  UserDataStorage storage;
  Api::Request request;
  Api::Response response = UserFileEndpoints::list(request, storage);
  expect(response.success && storage.lastOffset == 0 && storage.lastLimit == 50,
         "file list should use default pagination");
  JsonDocument data;
  expect(!deserializeJson(data, response.data.c_str()) && data["files"].size() == 2 &&
             std::string(data["files"][0]["name"].as<const char *>()) == "a.txt" &&
             std::string(data["files"][1]["name"].as<const char *>()) == "b.bin" &&
             data["next_cursor"].isNull(),
         "file list should own entry names and serialize a terminal cursor");

  request = Api::Request();
  request.addQueryParameter("limit", "1");
  response = UserFileEndpoints::list(request, storage);
  data.clear();
  expect(response.success && storage.lastLimit == 1 &&
             !deserializeJson(data, response.data.c_str()) &&
             std::string(data["next_cursor"].as<const char *>()) == "v1:1",
         "file list should emit an opaque next cursor");

  request = Api::Request();
  request.addQueryParameter("limit", "100");
  request.addQueryParameter("cursor", "v1:1");
  response = UserFileEndpoints::list(request, storage);
  expect(response.success && storage.lastLimit == 100 && storage.lastOffset == 1,
         "file list should decode maximum limit and cursor");

  for (const char *invalid : {"0", "101", "-1", " 1"}) {
    request = Api::Request();
    request.addQueryParameter("limit", invalid);
    response = UserFileEndpoints::list(request, storage);
    JsonDocument error;
    expect(response.statusCode == 400 && std::string(code(response, error)) == "invalid_field",
           "invalid list limit should be rejected");
  }

  request = Api::Request();
  request.addQueryParameter("cursor", "bad");
  response = UserFileEndpoints::list(request, storage);
  JsonDocument error;
  expect(response.statusCode == 400 && std::string(code(response, error)) == "invalid_field",
         "invalid cursor should be rejected");

  request = Api::Request();
  request.addQueryParameter("sort", "name");
  response = UserFileEndpoints::list(request, storage);
  expect(response.statusCode == 400 && std::string(code(response, error)) == "unsupported_field",
         "unknown list query should be rejected");

  request = Api::Request();
  request.hasBody = true;
  response = UserFileEndpoints::list(request, storage);
  expect(response.statusCode == 400 && std::string(code(response, error)) == "unsupported_field",
         "list body should be rejected");
}

void testDeleteAndResponseMapping() {
  UserDataStorage storage;
  Api::Request request;
  Api::Response response = UserFileEndpoints::remove(request, "a.txt", storage);
  expect(response.success && response.statusCode == 200,
         "delete should return a successful envelope");

  request.hasBody = true;
  response = UserFileEndpoints::remove(request, "a.txt", storage);
  JsonDocument error;
  expect(response.statusCode == 400 && std::string(code(response, error)) == "unsupported_field",
         "delete body should be rejected");

  const struct {
    UserDataFileStatus status;
    int http;
    const char *code;
  } cases[] = {
      {UserDataFileStatus::InvalidName, 400, "invalid_field"},
      {UserDataFileStatus::Busy, 409, "storage_busy"},
      {UserDataFileStatus::Unavailable, 503, "storage_unavailable"},
      {UserDataFileStatus::NotFound, 404, "not_found"},
      {UserDataFileStatus::RangeNotSatisfiable, 416, "range_not_satisfiable"},
      {UserDataFileStatus::UploadIncomplete, 500, "upload_incomplete"},
      {UserDataFileStatus::InsufficientStorage, 507, "insufficient_storage"},
      {UserDataFileStatus::StorageError, 500, "storage_error"},
  };
  for (const auto &item : cases) {
    response = UserFileEndpoints::fromStorageResult({item.status, "test failure"});
    expect(response.statusCode == item.http && std::string(code(response, error)) == item.code,
           "storage status should map to the stable HTTP error contract");
  }
  response = UserFileEndpoints::fromStorageResult(
      {UserDataFileStatus::PayloadTooLarge, "too large"}, 4096);
  error.clear();
  expect(response.statusCode == 413 && !deserializeJson(error, response.data.c_str()) &&
             std::string(error["code"].as<const char *>()) == "payload_too_large" &&
             error["max_upload_bytes"].as<size_t>() == 4096,
         "oversized upload should include the fresh limit");

  response = UserFileEndpoints::transportUnsupported();
  expect(response.statusCode == 415 && std::string(code(response, error)) == "unsupported_transport",
         "raw serial transport should be explicitly unsupported");
}

void testUploadSuccessShape() {
  Api::Response response = UserFileEndpoints::uploadCommitted(
      "a.txt", {{UserDataFileStatus::Ok, "ok"}, true, 12});
  expect(response.success && response.statusCode == 201,
         "new upload should return created");
  response = UserFileEndpoints::uploadCommitted(
      "a.txt", {{UserDataFileStatus::Ok, "ok"}, false, 15});
  expect(response.success && response.statusCode == 200,
         "replacement upload should return ok");
}
}  // namespace

int main() {
  testListQueries();
  testDeleteAndResponseMapping();
  testUploadSuccessShape();
  if (failures != 0) {
    std::cerr << failures << " user-file endpoint test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All user-file endpoint tests passed\n";
  return EXIT_SUCCESS;
}
