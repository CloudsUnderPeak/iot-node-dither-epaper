#pragma once

#include <cstddef>

struct StorageCapacity {
  size_t totalBytes = 0;
  size_t usedBytes = 0;
  size_t freeBytes = 0;
};

struct UploadCapacity {
  size_t totalBytes = 0;
  size_t usedBytes = 0;
  size_t availableBytes = 0;
  size_t maxUploadBytes = 0;
  size_t reservedBytes = 0;
  size_t allocationUnitBytes = 0;
};

inline size_t storageRoundDown(size_t value, size_t unit) {
  return unit > 0 ? value - (value % unit) : value;
}

inline UploadCapacity calculateUploadCapacity(const StorageCapacity &raw,
                                              size_t reserveBytes,
                                              size_t allocationUnitBytes) {
  UploadCapacity result;
  result.allocationUnitBytes = allocationUnitBytes;
  result.reservedBytes = reserveBytes;
  if (raw.totalBytes == 0) return result;

  result.reservedBytes = reserveBytes < raw.totalBytes ? reserveBytes : raw.totalBytes;
  result.totalBytes = storageRoundDown(
      raw.totalBytes - result.reservedBytes, allocationUnitBytes);

  const size_t boundedFree = raw.freeBytes < raw.totalBytes
                                 ? raw.freeBytes
                                 : raw.totalBytes;
  const size_t freeAfterReserve = boundedFree > result.reservedBytes
                                      ? boundedFree - result.reservedBytes
                                      : 0;
  result.availableBytes = storageRoundDown(freeAfterReserve, allocationUnitBytes);
  if (result.availableBytes > result.totalBytes) {
    result.availableBytes = result.totalBytes;
  }
  result.maxUploadBytes = result.availableBytes;
  result.usedBytes = result.totalBytes - result.availableBytes;
  return result;
}
