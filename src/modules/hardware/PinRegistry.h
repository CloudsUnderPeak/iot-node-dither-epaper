#pragma once

#include <cstddef>
#include <cstdint>

#include "../../board/BoardProfile.h"

enum class PinClaimKind : uint8_t {
  Exclusive,
  SharedBus,
};

enum class PinClaimStatus : uint8_t {
  Ok,
  InvalidRequest,
  RestrictedPin,
  Conflict,
  CapacityExceeded,
};

struct PinClaim {
  int8_t pin = Board::kNoPin;
  PinClaimKind kind = PinClaimKind::Exclusive;
  const char *owner = nullptr;
  const char *role = nullptr;
};

class PinRegistry {
 public:
  static constexpr size_t kMaxClaims = 24;

  PinClaimStatus claim(const PinClaim &request);
  PinClaimStatus claimAll(const PinClaim *requests, size_t count);

  size_t count() const { return count_; }
  const PinClaim *find(int pin) const;
  bool ownedBy(int pin, const char *owner) const;

 private:
  PinClaim claims_[kMaxClaims]{};
  size_t count_ = 0;

  PinClaimStatus validate(const PinClaim &request,
                          const PinClaim *pending,
                          size_t pendingCount) const;
  static bool sameText(const char *left, const char *right);
};
