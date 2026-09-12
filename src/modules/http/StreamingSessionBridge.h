#pragma once

#include <mutex>
#include <utility>

// Serialize callback I/O with disconnect cleanup. ApiRouter and its services
// retain admission, session-id validation and all file policy. Never send a
// response while holding this bridge: send may reenter disconnect callbacks.
class StreamingSessionBridge {
 public:
  template <typename Callback>
  auto run(Callback &&callback) -> decltype(callback()) {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::forward<Callback>(callback)();
  }
 private:
  std::mutex mutex_;
};
