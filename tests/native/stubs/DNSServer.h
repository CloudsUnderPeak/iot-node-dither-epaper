#pragma once
#include "IPAddress.h"
class DNSServer {
 public:
  bool start(uint16_t, const char *, IPAddress) { return true; }
  void stop() {}
  void processNextRequest() {}
};
