#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

constexpr uint8_t MSBFIRST = 1;
constexpr uint8_t SPI_MODE0 = 0;

class SPISettings {
 public:
  SPISettings(uint32_t clockHz, uint8_t bitOrder, uint8_t dataMode)
      : clockHz(clockHz), bitOrder(bitOrder), dataMode(dataMode) {}

  uint32_t clockHz;
  uint8_t bitOrder;
  uint8_t dataMode;
};

class SPIClass {
 public:
  void begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {
    ++beginCount;
    this->sck = sck;
    this->miso = miso;
    this->mosi = mosi;
    this->ss = ss;
  }

  void beginTransaction(const SPISettings &settings) {
    ++beginTransactionCount;
    lastSettings = settings;
  }

  void endTransaction() { ++endTransactionCount; }

  void writeBytes(const uint8_t *data, uint32_t length) {
    writes.insert(writes.end(), data, data + length);
  }

  int beginCount = 0;
  int beginTransactionCount = 0;
  int endTransactionCount = 0;
  int8_t sck = -1;
  int8_t miso = -1;
  int8_t mosi = -1;
  int8_t ss = -1;
  SPISettings lastSettings{0, 0, 0};
  std::vector<uint8_t> writes;
};
