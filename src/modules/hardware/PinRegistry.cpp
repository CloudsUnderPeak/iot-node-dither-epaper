#include "PinRegistry.h"

#include <cstring>

bool PinRegistry::sameText(const char *left, const char *right) {
  return left != nullptr && right != nullptr && strcmp(left, right) == 0;
}

const PinClaim *PinRegistry::find(int pin) const {
  for (size_t index = 0; index < count_; ++index) {
    if (claims_[index].pin == pin) return &claims_[index];
  }
  return nullptr;
}

bool PinRegistry::ownedBy(int pin, const char *owner) const {
  const PinClaim *claim = find(pin);
  return claim != nullptr && sameText(claim->owner, owner);
}

PinClaimStatus PinRegistry::validate(const PinClaim &request,
                                     const PinClaim *pending,
                                     size_t pendingCount) const {
  if (request.owner == nullptr || request.owner[0] == '\0' ||
      request.role == nullptr || request.role[0] == '\0') {
    return PinClaimStatus::InvalidRequest;
  }
  if (!Board::ActiveProfile::usable(request.pin)) {
    return PinClaimStatus::RestrictedPin;
  }

  const PinClaim *existing = find(request.pin);
  if (existing != nullptr) {
    return existing->kind == request.kind && sameText(existing->owner, request.owner) &&
                   sameText(existing->role, request.role)
               ? PinClaimStatus::Ok
               : PinClaimStatus::Conflict;
  }
  for (size_t index = 0; index < pendingCount; ++index) {
    if (pending[index].pin != request.pin) continue;
    return pending[index].kind == request.kind &&
                   sameText(pending[index].owner, request.owner) &&
                   sameText(pending[index].role, request.role)
               ? PinClaimStatus::Ok
               : PinClaimStatus::Conflict;
  }
  return PinClaimStatus::Ok;
}

PinClaimStatus PinRegistry::claim(const PinClaim &request) {
  return claimAll(&request, 1);
}

PinClaimStatus PinRegistry::claimAll(const PinClaim *requests, size_t count) {
  if (requests == nullptr || count == 0) return PinClaimStatus::InvalidRequest;
  size_t newClaims = 0;
  for (size_t index = 0; index < count; ++index) {
    const PinClaimStatus status = validate(requests[index], requests, index);
    if (status != PinClaimStatus::Ok) return status;
    if (find(requests[index].pin) == nullptr) {
      bool duplicatePending = false;
      for (size_t earlier = 0; earlier < index; ++earlier) {
        if (requests[earlier].pin == requests[index].pin) duplicatePending = true;
      }
      if (!duplicatePending) ++newClaims;
    }
  }
  if (newClaims > kMaxClaims - count_) return PinClaimStatus::CapacityExceeded;

  for (size_t index = 0; index < count; ++index) {
    if (find(requests[index].pin) != nullptr) continue;
    bool alreadyAdded = false;
    for (size_t earlier = 0; earlier < index; ++earlier) {
      if (requests[earlier].pin == requests[index].pin) alreadyAdded = true;
    }
    if (!alreadyAdded) claims_[count_++] = requests[index];
  }
  return PinClaimStatus::Ok;
}
