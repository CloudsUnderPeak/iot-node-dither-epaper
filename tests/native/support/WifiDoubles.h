#pragma once
#include "modules/wifi/WifiManager.h"

class FakeClock : public MonotonicClock {
 public:
  uint32_t value = 0;
  uint32_t nowMs() const override { return value; }
  void advance(uint32_t delta) { value += delta; }
};

class FakeWifiDriver : public WifiDriver {
 public:
  WifiDriverMode mode = WifiDriverMode::Off;
  bool modeResult = true;
  bool stationConfigResult = true;
  bool apConfigResult = true;
  bool apStartResult = true;
  bool apStopResult = true;
  bool captivePortalResult = true;
  bool connected = false;
  IPAddress staIp;
  IPAddress staNetmask = IPAddress(255, 255, 255, 0);
  IPAddress currentApIp;
  IPAddress configuredApIp;
  unsigned beginCount = 0;
  unsigned disconnectCount = 0;
  unsigned disconnectAsyncCount = 0;
  unsigned apStartCount = 0;
  unsigned apStopCount = 0;
  unsigned txPowerCount = 0;
  unsigned modeCount = 0;
  uint8_t lastTxDbm = 0;
  bool txPowerResult = true;

  void setPersistent(bool) override {}

  bool setMode(WifiDriverMode requested) override {
    ++modeCount;
    if (!modeResult) return false;
    mode = requested;
    return true;
  }

  bool setTxPower(uint8_t configuredDbm) override {
    ++txPowerCount;
    lastTxDbm = configuredDbm;
    return txPowerResult;
  }

  bool setHostname(const char *) override { return true; }

  bool configureStation(const IPAddress &,
                        const IPAddress &,
                        const IPAddress &,
                        const IPAddress &,
                        const IPAddress &) override {
    return stationConfigResult;
  }

  void beginStation(const char *, const char *) override {
    ++beginCount;
  }

  bool stationConnected() const override { return connected; }
  IPAddress stationIp() const override { return staIp; }
  IPAddress stationNetmask() const override { return staNetmask; }

  void disconnectStation(bool, bool) override {
    ++disconnectCount;
    connected = false;
    staIp = IPAddress();
  }

  void disconnectStationAsync(bool, bool) override {
    ++disconnectAsyncCount;
    connected = false;
    staIp = IPAddress();
  }

  bool configureAp(const IPAddress &localIp,
                   const IPAddress &,
                   const IPAddress &,
                   const IPAddress &) override {
    configuredApIp = localIp;
    return apConfigResult;
  }

  bool startAp(const char *, const char *) override {
    ++apStartCount;
    if (!apStartResult) return false;
    currentApIp = configuredApIp;
    return true;
  }

  bool stopAp(bool) override {
    ++apStopCount;
    if (!apStopResult) return false;
    currentApIp = IPAddress();
    return true;
  }

  IPAddress apIp() const override { return currentApIp; }
  bool enableDhcpCaptivePortal() override { return captivePortalResult; }
};

