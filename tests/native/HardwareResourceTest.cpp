#include <cstdlib>
#include <cstring>
#include <iostream>

#include "board/BoardProfile.h"
#include "modules/hardware/EpaperHardware.h"
#include "modules/hardware/PinRegistry.h"
#include "modules/hardware/SpiBus.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void resetNativePins() {
  for (NativePinRecord &pin : nativePins) pin = {};
}

void testBoardProfile() {
  const Board::SpiRoute spi = Board::ActiveProfile::kSpi;
  const Board::EpaperPins epaper = Board::ActiveProfile::kEpaper;
  const Board::BatterySense battery = Board::ActiveProfile::kBatterySense;
  expect(spi.sck == 23 && spi.mosi == 22 && spi.miso == Board::kNoPin,
         "profile should expose the explicit write-only SPI route");
  expect(epaper.cs == 18 && epaper.dc == 1 && epaper.reset == 14 && epaper.busy == 21,
         "profile should preserve the reviewed six-wire mapping");
  expect(Board::ActiveProfile::kEpaperSpiHz == 4000000 &&
             Board::ActiveProfile::kEpaperSpiMode == 0,
         "profile should fix the conservative 4 MHz SPI mode 0 device settings");
  expect(battery.pin == 0 && battery.dividerNumerator == 2 &&
             battery.dividerDenominator == 1,
         "profile should expose the reviewed DFR1075 battery divider route");

  for (int pin : {4, 5, 8, 9, 15}) {
    expect(Board::ActiveProfile::pinDisposition(pin) == Board::PinDisposition::Strapping,
           "ESP32-C6 strapping pins must be rejected");
  }
  for (int pin : {12, 13}) {
    expect(Board::ActiveProfile::pinDisposition(pin) == Board::PinDisposition::UsbJtag,
           "default USB-JTAG pins must be rejected");
  }
  for (int pin = 24; pin <= 30; ++pin) {
    expect(Board::ActiveProfile::pinDisposition(pin) == Board::PinDisposition::Flash,
           "in-package flash pins must be rejected");
  }
  expect(Board::ActiveProfile::pinDisposition(0) == Board::PinDisposition::BoardReserved,
         "battery measurement GPIO0 must remain board-reserved");
  expect(Board::ActiveProfile::pinDisposition(10) == Board::PinDisposition::NotExposed &&
             Board::ActiveProfile::pinDisposition(11) == Board::PinDisposition::NotExposed,
         "ESP32-C6FH4 unbonded GPIO10/11 must be rejected");
}

void testPinRegistryAtomicClaims() {
  PinRegistry pins;
  const PinClaim epaperPins[] = {
      {18, PinClaimKind::Exclusive, "epaper", "cs"},
      {1, PinClaimKind::Exclusive, "epaper", "dc"},
  };
  expect(pins.claimAll(epaperPins, 2) == PinClaimStatus::Ok && pins.count() == 2,
         "valid pin batch should commit atomically");
  expect(pins.claim(epaperPins[0]) == PinClaimStatus::Ok && pins.count() == 2,
         "identical owner/role claim should be idempotent");
  expect(pins.claim({18, PinClaimKind::Exclusive, "storage", "cs"}) ==
             PinClaimStatus::Conflict,
         "exclusive pin reuse should report a conflict");
  expect(pins.claim({8, PinClaimKind::Exclusive, "test", "unsafe"}) ==
             PinClaimStatus::RestrictedPin,
         "registry should reject a strapping pin");

  const size_t before = pins.count();
  const PinClaim conflictingBatch[] = {
      {14, PinClaimKind::Exclusive, "epaper", "reset"},
      {18, PinClaimKind::Exclusive, "other", "collision"},
  };
  expect(pins.claimAll(conflictingBatch, 2) == PinClaimStatus::Conflict &&
             pins.count() == before && pins.find(14) == nullptr,
         "failed batch must not leave a partial claim");
}

void testSafeBootLevels() {
  resetNativePins();
  PinRegistry pins;
  expect(EpaperHardware::claimAndQuiescePins(&pins).ok() && pins.count() == 4,
         "e-paper device pins should claim as one atomic group");
  const Board::EpaperPins epaper = Board::ActiveProfile::kEpaper;
  expect(nativePins[epaper.cs].mode == OUTPUT && nativePins[epaper.cs].level == HIGH,
         "CS should become an inactive-high output");
  expect(nativePins[epaper.dc].mode == OUTPUT && nativePins[epaper.dc].level == LOW,
         "DC should become a low output during logical quiesce");
  expect(nativePins[epaper.reset].mode == OUTPUT &&
             nativePins[epaper.reset].level == LOW,
         "RST should become a low output during logical quiesce");
  expect(nativePins[epaper.busy].mode == INPUT_PULLUP &&
             nativePins[epaper.busy].level == HIGH,
         "active-low BUSY should use a fail-safe idle-high input pull-up");
}

void testSpiOwnershipAndTransactions() {
  resetNativePins();
  PinRegistry pins;
  expect(EpaperHardware::claimAndQuiescePins(&pins).ok(),
         "device pins should be available before SPI registration");
  SPIClass port;
  SpiBus bus;
  expect(bus.begin(&port, &pins, Board::ActiveProfile::kSpi).ok() && bus.ready(),
         "shared SPI owner should initialize with the reviewed route");
  expect(port.beginCount == 1 && port.sck == 23 && port.mosi == 22 &&
             port.miso == Board::kNoPin && port.ss == Board::kNoPin,
         "SPI.begin should receive explicit SCK/MOSI and no MISO/default SS");
  expect(pins.find(23) != nullptr &&
             pins.find(23)->kind == PinClaimKind::SharedBus &&
             strcmp(pins.find(23)->owner, "spi") == 0,
         "SCK should be owned once by the shared bus");
  expect(pins.claim({23, PinClaimKind::Exclusive, "epaper", "sck"}) ==
             PinClaimStatus::Conflict,
         "device drivers must not claim shared SPI pins directly");

  const SpiDeviceConfig device{"epaper", 18, 4000000, MSBFIRST, SPI_MODE0};
  const uint8_t payload[] = {0xAA, 0x55, 0x00};
  {
    SpiBusTransaction transaction(&bus, device, 0);
    expect(transaction.active(), "registered device should acquire SPI transaction");
    expect(nativePins[18].level == LOW, "transaction should assert CS low");
    expect(transaction.write(payload, sizeof(payload)) == sizeof(payload),
           "active transaction should write the complete byte span");
  }
  expect(nativePins[18].level == HIGH && port.beginTransactionCount == 1 &&
             port.endTransactionCount == 1 && port.lastSettings.clockHz == 4000000 &&
             port.lastSettings.bitOrder == MSBFIRST &&
             port.lastSettings.dataMode == SPI_MODE0 && port.writes.size() == 3,
         "transaction cleanup should deassert CS and close mode-0 4 MHz SPI");

  PinRegistry conflictPins;
  SPIClass conflictPort;
  SpiBus conflictBus;
  Board::SpiRoute unsafeRoute = Board::ActiveProfile::kSpi;
  unsafeRoute.miso = Board::ActiveProfile::kEpaper.busy;
  expect(!conflictBus.begin(&conflictPort, &conflictPins, unsafeRoute).ok() &&
             conflictPins.count() == 0 && conflictPort.beginCount == 0,
         "GPIO21 MISO/BUSY conflict must fail before pin or driver mutation");
}

}  // namespace

int main() {
  testBoardProfile();
  testPinRegistryAtomicClaims();
  testSafeBootLevels();
  testSpiOwnershipAndTransactions();
  if (failures != 0) {
    std::cerr << failures << " hardware resource test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Board profile, pin registry, and SPI ownership tests passed\n";
  return EXIT_SUCCESS;
}
