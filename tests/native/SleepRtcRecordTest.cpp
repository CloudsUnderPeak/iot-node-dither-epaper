#include <cassert>

#include "modules/sleep/SleepRtcRecord.h"
#include "modules/sleep/WakeClassifier.h"

int main() {
  SleepRtcRecord record;
  record.intent = true;
  record.scheduleGeneration = 9;
  record.sleepEnteredClock = 1000;
  record.timerTargetClock = 1400;
  record.plannedDue = 1400;
  record.slotIndex = 2;
  sealSleepRtcRecord(record);
  assert(sleepRtcRecordValid(record));
  assert(classifyWake(true, WakeCause::Timer, record, true, 9) == WakeMode::WakeCycle);
  assert(classifyWake(true, WakeCause::Timer, record, false, 9) == WakeMode::Normal);
  assert(classifyWake(true, WakeCause::Gpio, record, true, 9) == WakeMode::Normal);
  assert(classifyWake(false, WakeCause::Timer, record, true, 9) == WakeMode::Normal);
  assert(classifyWake(true, WakeCause::Timer, record, true, 10) == WakeMode::Normal);
  assert(sleepWakeEvidence(record, true, 1180, 86400).valid);
  assert(sleepWakeEvidence(record, true, 1180, 86400).sleptSeconds == 180);
  assert(!sleepWakeEvidence(record, false, 1180, 86400).valid);
  assert(!sleepWakeEvidence(record, true, 999, 86400).valid);
  assert(!sleepWakeEvidence(record, true, 1000 + 86400 + 301, 86400).valid);
  record.version++;
  assert(!sleepRtcRecordValid(record));
  record.version--;
  record.crc32 ^= 1;
  assert(!sleepRtcRecordValid(record));
  record.crc32 ^= 1;
  record.intent = false;
  assert(!sleepRtcRecordValid(record));
}
