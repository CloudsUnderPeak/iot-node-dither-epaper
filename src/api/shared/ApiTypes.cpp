#include "ApiTypes.h"

#include <cstring>

namespace Api {

Method methodFromString(const char *value) {
  if (value == nullptr) return Method::Unknown;
  if (strcmp(value, "GET") == 0) return Method::Get;
  if (strcmp(value, "POST") == 0) return Method::Post;
  if (strcmp(value, "PUT") == 0) return Method::Put;
  if (strcmp(value, "DELETE") == 0) return Method::Delete;
  return Method::Unknown;
}

bool Request::matches(Method expectedMethod, const char *expectedPath) const {
  return method == expectedMethod && path != nullptr && expectedPath != nullptr &&
         strcmp(path, expectedPath) == 0;
}

bool Request::addQueryParameter(const char *name, const char *value) {
  if (name == nullptr || value == nullptr || queryCount >= kMaxQueryParameters ||
      strlen(name) > 32 || strlen(value) > 128) {
    queryOverflow = true;
    return false;
  }
  query[queryCount].name = name;
  query[queryCount].value = value;
  ++queryCount;
  return true;
}

int statusFor(ResultCode code) {
  switch (code) {
    case ResultCode::Ok: return 200;
    case ResultCode::InvalidInput: return 400;
    case ResultCode::NotFound: return 404;
    case ResultCode::OutOfSpace: return 507;
    case ResultCode::Unsupported: return 415;
    case ResultCode::StorageError:
    case ResultCode::NetworkError: return 500;
  }
  return 500;
}

}  // namespace Api
