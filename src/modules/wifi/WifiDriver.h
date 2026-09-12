#pragma once

#include <Arduino.h>
#include <IPAddress.h>

enum class WifiDriverMode : uint8_t {
  Off,
  Sta,
  Ap,
  ApSta,
};

class WifiDriver {
 public:
  virtual ~WifiDriver() = default;
  virtual void setPersistent(bool enabled) = 0;
  virtual bool setMode(WifiDriverMode mode) = 0;
  virtual bool setTxPower(uint8_t configuredDbm) = 0;
  virtual bool setHostname(const char *hostname) = 0;
  virtual bool configureStation(const IPAddress &localIp,
                                const IPAddress &gateway,
                                const IPAddress &netmask,
                                const IPAddress &dns1 = IPAddress(),
                                const IPAddress &dns2 = IPAddress()) = 0;
  virtual void beginStation(const char *ssid, const char *password) = 0;
  virtual bool stationConnected() const = 0;
  virtual IPAddress stationIp() const = 0;
  virtual IPAddress stationNetmask() const = 0;
  virtual void disconnectStation(bool turnOffRadio, bool eraseCredentials) = 0;
  virtual void disconnectStationAsync(bool turnOffRadio,
                                      bool eraseCredentials) = 0;
  virtual bool configureAp(const IPAddress &localIp,
                           const IPAddress &gateway,
                           const IPAddress &netmask,
                           const IPAddress &dns) = 0;
  virtual bool startAp(const char *ssid, const char *password) = 0;
  virtual bool stopAp(bool turnOffRadio) = 0;
  virtual IPAddress apIp() const = 0;
  virtual bool enableDhcpCaptivePortal() = 0;
};
