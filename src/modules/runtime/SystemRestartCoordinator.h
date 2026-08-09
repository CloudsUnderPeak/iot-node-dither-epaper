#pragma once

class SystemRestartCoordinator {
 public:
  virtual ~SystemRestartCoordinator() = default;
  virtual bool restartNow() = 0;
};
