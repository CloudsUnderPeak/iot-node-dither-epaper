#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t INPUT = 1;
constexpr uint8_t OUTPUT = 3;
constexpr uint8_t INPUT_PULLUP = 5;

struct NativePinRecord {
  uint8_t mode = INPUT;
  uint8_t level = LOW;
  size_t modeWrites = 0;
  size_t levelWrites = 0;
};

inline NativePinRecord nativePins[31];

inline void pinMode(uint8_t pin, uint8_t mode) {
  if (pin >= 31) return;
  nativePins[pin].mode = mode;
  if (mode == INPUT_PULLUP) nativePins[pin].level = HIGH;
  ++nativePins[pin].modeWrites;
}

inline void digitalWrite(uint8_t pin, uint8_t level) {
  if (pin >= 31) return;
  nativePins[pin].level = level;
  ++nativePins[pin].levelWrites;
}

class String {
 public:
  String() = default;
  String(const char *value) : value_(value == nullptr ? "" : value) {}

  size_t length() const {
    return value_.length();
  }

  const char *c_str() const {
    return value_.c_str();
  }

  void reserve(size_t capacity) {
    value_.reserve(capacity);
  }

  size_t write(uint8_t value) {
    value_.push_back(static_cast<char>(value));
    return 1;
  }

  size_t write(const uint8_t *data, size_t size) {
    value_.append(reinterpret_cast<const char *>(data), size);
    return size;
  }

  String &operator=(const char *value) {
    value_ = value == nullptr ? "" : value;
    return *this;
  }

  String &operator+=(const char *value) {
    value_ += value == nullptr ? "" : value;
    return *this;
  }

  String &operator+=(const String &value) {
    value_ += value.value_;
    return *this;
  }

  String &operator+=(char value) {
    value_ += value;
    return *this;
  }

  bool operator==(const char *value) const {
    return value_ == (value == nullptr ? "" : value);
  }

  bool operator==(const String &value) const {
    return value_ == value.value_;
  }

 private:
  std::string value_;
};

inline size_t strlcpy(char *target, const char *source, size_t targetSize) {
  const size_t sourceLength = strlen(source);
  if (targetSize > 0) {
    const size_t copyLength = sourceLength < targetSize - 1 ? sourceLength : targetSize - 1;
    memcpy(target, source, copyLength);
    target[copyLength] = '\0';
  }
  return sourceLength;
}

class NativeEsp {
 public:
  const char *getChipModel() const { return "native-test"; }
  uint32_t getChipRevision() const { return 1; }
  uint32_t getChipCores() const { return 1; }
  uint32_t getFlashChipSize() const { return 4 * 1024 * 1024; }
  uint32_t getHeapSize() const { return 320 * 1024; }
  uint32_t getFreeHeap() const { return 256 * 1024; }
};

inline NativeEsp ESP;

inline uint32_t nativeMillis = 0;

inline uint32_t millis() {
  return nativeMillis;
}

struct NativeSerial {
  template <typename... Args> void printf(const char *, Args...) {}
  void println(const char *) {}
};
inline NativeSerial Serial;
