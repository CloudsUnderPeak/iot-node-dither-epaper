#pragma once

enum class ResultCode {
  Ok,
  InvalidInput,
  StorageError,
  NetworkError,
  OutOfSpace,
  Unsupported,
  NotFound,
};

// Small value-type result used across modules so feature code can avoid
// exceptions and still return a stable machine-readable error category.
struct Result {
  ResultCode code;
  const char *message;

  bool ok() const {
    return code == ResultCode::Ok;
  }
};

inline Result okResult() {
  return {ResultCode::Ok, "ok"};
}

inline Result invalidInput(const char *message) {
  return {ResultCode::InvalidInput, message};
}

inline Result storageError(const char *message) {
  return {ResultCode::StorageError, message};
}

inline Result networkError(const char *message) {
  return {ResultCode::NetworkError, message};
}

inline Result outOfSpace(const char *message) {
  return {ResultCode::OutOfSpace, message};
}

inline Result unsupported(const char *message) {
  return {ResultCode::Unsupported, message};
}

inline Result notFound(const char *message) {
  return {ResultCode::NotFound, message};
}
