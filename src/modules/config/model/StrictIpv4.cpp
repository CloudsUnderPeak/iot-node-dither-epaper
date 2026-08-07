#include "StrictIpv4.h"

bool parseStrictIpv4(const char *value, StrictIpv4Address &parsed) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }

  StrictIpv4Address result;
  uint8_t octetIndex = 0;
  uint16_t octetValue = 0;
  uint8_t digitCount = 0;

  for (const char *cursor = value; *cursor != '\0'; ++cursor) {
    const char current = *cursor;
    if (current >= '0' && current <= '9') {
      if (digitCount >= 3) {
        return false;
      }
      octetValue = static_cast<uint16_t>(octetValue * 10U + static_cast<uint16_t>(current - '0'));
      if (octetValue > 255U) {
        return false;
      }
      ++digitCount;
      continue;
    }

    if (current != '.' || digitCount == 0 || octetIndex >= 3) {
      return false;
    }
    result.octets[octetIndex++] = static_cast<uint8_t>(octetValue);
    octetValue = 0;
    digitCount = 0;
  }

  if (octetIndex != 3 || digitCount == 0) {
    return false;
  }
  result.octets[3] = static_cast<uint8_t>(octetValue);
  parsed = result;
  return true;
}
