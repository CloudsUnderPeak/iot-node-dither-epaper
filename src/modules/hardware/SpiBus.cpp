#include "SpiBus.h"

namespace {
constexpr const char *kSpiOwner = "spi";
}

Result SpiBus::begin(SPIClass *spi,
                     PinRegistry *pins,
                     const Board::SpiRoute &route) {
  if (spi == nullptr || pins == nullptr || route.sck == Board::kNoPin ||
      route.mosi == Board::kNoPin) {
    return invalidInput("invalid SPI bus dependencies or route");
  }
  if (route.miso == Board::ActiveProfile::kEpaper.busy) {
    return invalidInput("SPI MISO conflicts with e-paper BUSY");
  }

  PinClaim claims[] = {
      {route.sck, PinClaimKind::SharedBus, kSpiOwner, "sck"},
      {route.mosi, PinClaimKind::SharedBus, kSpiOwner, "mosi"},
      {route.miso, PinClaimKind::SharedBus, kSpiOwner, "miso"},
  };
  const size_t claimCount = route.miso == Board::kNoPin ? 2 : 3;

  if (mutex_ == nullptr) mutex_ = xSemaphoreCreateMutex();
  if (mutex_ == nullptr) return outOfSpace("failed to create SPI bus mutex");
  const PinClaimStatus status = pins->claimAll(claims, claimCount);
  if (status != PinClaimStatus::Ok) {
    return invalidInput("SPI route pin claim failed");
  }

  spi_ = spi;
  pins_ = pins;
  route_ = route;
  spi_->begin(route.sck, route.miso, route.mosi, Board::kNoPin);
  initialized_ = true;
  return okResult();
}

bool SpiBus::ready() const {
  return initialized_ && spi_ != nullptr && pins_ != nullptr && mutex_ != nullptr;
}

bool SpiBus::beginTransaction(const SpiDeviceConfig &device,
                              TickType_t timeoutTicks) {
  if (!ready() || device.owner == nullptr || device.clockHz == 0 ||
      !pins_->ownedBy(device.csPin, device.owner) ||
      xSemaphoreTake(mutex_, timeoutTicks) != pdTRUE) {
    return false;
  }
  spi_->beginTransaction(SPISettings(device.clockHz, device.bitOrder, device.dataMode));
  digitalWrite(device.csPin, LOW);
  return true;
}

void SpiBus::endTransaction(const SpiDeviceConfig &device) {
  digitalWrite(device.csPin, HIGH);
  spi_->endTransaction();
  xSemaphoreGive(mutex_);
}

size_t SpiBus::write(const uint8_t *data, size_t length) {
  if (data == nullptr || length == 0) return 0;
  spi_->writeBytes(data, static_cast<uint32_t>(length));
  return length;
}

SpiBusTransaction::SpiBusTransaction(SpiBus *bus,
                                     const SpiDeviceConfig &device,
                                     TickType_t timeoutTicks)
    : bus_(bus), device_(device),
      active_(bus != nullptr && bus->beginTransaction(device, timeoutTicks)) {}

SpiBusTransaction::~SpiBusTransaction() {
  if (active_) bus_->endTransaction(device_);
}

size_t SpiBusTransaction::write(const uint8_t *data, size_t length) {
  return active_ ? bus_->write(data, length) : 0;
}
