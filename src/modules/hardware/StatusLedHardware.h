#pragma once

namespace StatusLedHardware {
// Preload OFF before configuring output and releasing a retained sleep hold.
bool bootOff();
bool write(bool on);
bool holdOff();
bool releaseHold();
int level();
}  // namespace StatusLedHardware
