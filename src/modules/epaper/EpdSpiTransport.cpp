#include "EpdSpiTransport.h"

Result EpdSpiTransport::begin(SpiBus *bus, PinRegistry *pins) {
  if (bus == nullptr || pins == nullptr || !bus->ready()) {
    return invalidInput("ready SPI bus and pin registry are required");
  }
  if (!pins->ownedBy(Board::ActiveProfile::kEpaper.cs, EpaperHardware::kOwner) ||
      !pins->ownedBy(Board::ActiveProfile::kEpaper.dc, EpaperHardware::kOwner) ||
      !pins->ownedBy(Board::ActiveProfile::kEpaper.reset, EpaperHardware::kOwner) ||
      !pins->ownedBy(Board::ActiveProfile::kEpaper.busy, EpaperHardware::kOwner)) {
    return invalidInput("e-paper device pins are not claimed");
  }
  bus_ = bus;
  device_ = {EpaperHardware::kOwner,
             Board::ActiveProfile::kEpaper.cs,
             Board::ActiveProfile::kEpaperSpiHz,
             MSBFIRST,
             SPI_MODE0};
  return okResult();
}

bool EpdSpiTransport::ready() const {
  return bus_ != nullptr && bus_->ready();
}

bool EpdSpiTransport::setReset(bool high) {
  if (!ready()) return false;
  digitalWrite(Board::ActiveProfile::kEpaper.reset, high ? HIGH : LOW);
  return true;
}

bool EpdSpiTransport::busyHigh() const {
  return ready() && digitalRead(Board::ActiveProfile::kEpaper.busy) == HIGH;
}

bool EpdSpiTransport::writeCommand(uint8_t command) {
  if (!ready()) return false;
  digitalWrite(Board::ActiveProfile::kEpaper.dc, LOW);
  SpiBusTransaction transaction(bus_, device_, pdMS_TO_TICKS(1000));
  return transaction.active() && transaction.write(&command, 1) == 1;
}

bool EpdSpiTransport::writeData(const uint8_t *data, size_t length) {
  if (!ready() || data == nullptr || length == 0) return false;
  digitalWrite(Board::ActiveProfile::kEpaper.dc, HIGH);
  SpiBusTransaction transaction(bus_, device_, pdMS_TO_TICKS(1000));
  return transaction.active() && transaction.write(data, length) == length;
}

void EpdSpiTransport::logicalQuiesce() {
  const Board::EpaperPins pins = Board::ActiveProfile::kEpaper;
  digitalWrite(pins.cs, HIGH);
  digitalWrite(pins.dc, LOW);
  // RST low is an active reset and also controls the HAT's power switch. A
  // long low interval can power the driver off, so quiesce with reset high;
  // Epd7In3E::initialize() still supplies the official 2 ms low pulse.
  digitalWrite(pins.reset, HIGH);
}

void EpdSpiTransport::delayMs(uint32_t durationMs) {
  delay(durationMs);
}

void EpdSpiTransport::yieldCpu() {
  yield();
}

uint32_t EpdSpiTransport::nowMs() const {
  return millis();
}
