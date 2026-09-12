#pragma once

#include "MonotonicClock.h"
#include "WifiDriver.h"

class ArduinoWifiDriver : public WifiDriver {
 public:
  void setPersistent(bool enabled) override;
  bool setMode(WifiDriverMode mode) override;
  bool setTxPower(uint8_t configuredDbm) override;
  bool setHostname(const char *hostname) override;
  bool configureStation(const IPAddress &localIp,
                        const IPAddress &gateway,
                        const IPAddress &netmask,
                        const IPAddress &dns1,
                        const IPAddress &dns2) override;
  void beginStation(const char *ssid, const char *password) override;
  bool stationConnected() const override;
  IPAddress stationIp() const override;
  IPAddress stationNetmask() const override;
  void disconnectStation(bool turnOffRadio, bool eraseCredentials) override;
  void disconnectStationAsync(bool turnOffRadio,
                              bool eraseCredentials) override;
  bool configureAp(const IPAddress &localIp,
                   const IPAddress &gateway,
                   const IPAddress &netmask,
                   const IPAddress &dns) override;
  bool startAp(const char *ssid, const char *password) override;
  bool stopAp(bool turnOffRadio) override;
  IPAddress apIp() const override;
  bool enableDhcpCaptivePortal() override;
};

class ArduinoMonotonicClock : public MonotonicClock {
 public:
  uint32_t nowMs() const override;
};
