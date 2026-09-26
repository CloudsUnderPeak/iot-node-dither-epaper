#pragma once

#include <cstdint>

enum class TimeOrigin : uint8_t { None, Client, Ntp, Carried };

struct TimeSnapshot {
  int64_t epoch = 0;
  TimeOrigin origin = TimeOrigin::None;
  uint32_t revision = 0;
  bool synced() const { return origin != TimeOrigin::None; }
};

inline const char *timeOriginToString(TimeOrigin origin) {
  switch (origin) {
    case TimeOrigin::None: return "none";
    case TimeOrigin::Client: return "client";
    case TimeOrigin::Ntp: return "ntp";
    case TimeOrigin::Carried: return "carried";
  }
  return "none";
}

class TimeSource {
 public:
  virtual ~TimeSource() = default;
  virtual TimeSnapshot snapshot() const = 0;
  virtual bool set(int64_t epoch, TimeOrigin origin) = 0;
  virtual void clear() = 0;
  virtual void restoreCarried(bool evidenceValid) {
    (void)evidenceValid;
    clear();
  }
};
