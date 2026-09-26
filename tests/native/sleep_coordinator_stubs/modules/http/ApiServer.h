#pragma once
class ApiServer {
 public:
  bool running = true;
  unsigned stopCalls = 0;
  unsigned resumeCalls = 0;
  bool started() const { return running; }
  void stopForSleep() { ++stopCalls; running = false; }
  bool resumeAfterSleep() { ++resumeCalls; running = true; return true; }
};
