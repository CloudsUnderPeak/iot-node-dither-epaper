#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../board/BoardProfile.h"
#include "../../core/Result.h"
#include "PinRegistry.h"

struct SpiDeviceConfig {
  const char *owner = nullptr;
  int8_t csPin = Board::kNoPin;
  uint32_t clockHz = 0;
  uint8_t bitOrder = MSBFIRST;
  uint8_t dataMode = SPI_MODE0;
};

class SpiBus {
 public:
  Result begin(SPIClass *spi, PinRegistry *pins, const Board::SpiRoute &route);
  bool ready() const;
  const Board::SpiRoute &route() const { return route_; }

 private:
  friend class SpiBusTransaction;

  bool beginTransaction(const SpiDeviceConfig &device, TickType_t timeoutTicks);
  void endTransaction(const SpiDeviceConfig &device);
  size_t write(const uint8_t *data, size_t length);

  SPIClass *spi_ = nullptr;
  PinRegistry *pins_ = nullptr;
  SemaphoreHandle_t mutex_ = nullptr;
  Board::SpiRoute route_{};
  bool initialized_ = false;
};

class SpiBusTransaction {
 public:
  SpiBusTransaction(SpiBus *bus,
                    const SpiDeviceConfig &device,
                    TickType_t timeoutTicks);
  ~SpiBusTransaction();

  SpiBusTransaction(const SpiBusTransaction &) = delete;
  SpiBusTransaction &operator=(const SpiBusTransaction &) = delete;

  bool active() const { return active_; }
  size_t write(const uint8_t *data, size_t length);

 private:
  SpiBus *bus_ = nullptr;
  SpiDeviceConfig device_{};
  bool active_ = false;
};
