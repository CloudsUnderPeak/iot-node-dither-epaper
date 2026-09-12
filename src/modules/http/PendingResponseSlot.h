#pragma once
#include <mutex>
#include "api/shared/ApiTypes.h"

// One bounded transport record. Detach before invoking router/send callbacks,
// which can reenter via disconnect. The record never owns a raw request.
template <typename WeakRequest>
class PendingResponseSlot {
 public:
  struct Record { WeakRequest request; Api::PendingRequest pending; };
  bool store(const WeakRequest &request, const Api::PendingRequest &pending) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (record_.pending.id != 0) return false;
    record_ = {request, pending};
    return true;
  }
  Record snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return record_;
  }
  bool take(uint32_t id, Record &record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id == 0 || record_.pending.id != id) return false;
    record = record_;
    record_ = {};
    return true;
  }
 private:
  std::mutex mutex_;
  Record record_;
};
