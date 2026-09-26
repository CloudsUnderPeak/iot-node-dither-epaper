#pragma once
#include <functional>
class UserDataStorage {
 public:
  bool busy = false;
  bool reserveValue = true;
  bool unmountValue = true;
  bool cancelValue = true;
  bool reserved = false;
  bool mounted = true;
  unsigned reserveCalls = 0;
  unsigned unmountCalls = 0;
  unsigned cancelCalls = 0;
  std::function<void()> onReserve;
  bool operationBusy() const { return busy || reserved; }
  bool reserveSleep() {
    ++reserveCalls;
    if (!reserveValue || busy || reserved) return false;
    reserved = true;
    if (onReserve) onReserve();
    return true;
  }
  bool unmountForSleep() {
    ++unmountCalls;
    if (!reserved || !unmountValue) return false;
    mounted = false;
    return true;
  }
  bool cancelSleep() {
    ++cancelCalls;
    if (!cancelValue) return false;
    mounted = true;
    reserved = false;
    return true;
  }
};
