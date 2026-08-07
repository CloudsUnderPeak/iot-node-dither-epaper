#pragma once

#include "Result.h"

using SubsystemStart = Result (*)();
using SubsystemHealth = bool (*)();
using SubsystemReporter = void (*)(const Result &);

struct Subsystem {
  const char *name;
  SubsystemStart start;
  SubsystemHealth healthy = nullptr;
  SubsystemReporter report = nullptr;
  bool ready = false;
};

inline Result startSubsystem(Subsystem &subsystem) {
  if (subsystem.start == nullptr) {
    subsystem.ready = false;
    return invalidInput("subsystem start unavailable");
  }
  const Result result = subsystem.start();
  subsystem.ready =
      result.ok() &&
      (subsystem.healthy == nullptr || subsystem.healthy());
  return result;
}

inline bool subsystemHealthy(const Subsystem &subsystem) {
  return subsystem.ready &&
         (subsystem.healthy == nullptr || subsystem.healthy());
}
