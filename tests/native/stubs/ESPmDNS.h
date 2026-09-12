#pragma once
struct NativeMdns {
  bool begin(const char *) { return true; }
  void end() {}
  void addService(const char *, const char *, int) {}
  void addServiceTxt(const char *, const char *, const char *, const char *) {}
};
inline NativeMdns MDNS;
