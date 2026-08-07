#pragma once

#include <cstdint>
#include <cstring>

#include "modules/storage/StorageCapacity.h"

enum class UserDataFileStatus : uint8_t {
  Ok,
  InvalidName,
  Busy,
  Unavailable,
  NotFound,
  PayloadTooLarge,
  RangeNotSatisfiable,
  UploadIncomplete,
  InsufficientStorage,
  StorageError,
};

struct UserDataFileResult {
  UserDataFileStatus status = UserDataFileStatus::StorageError;
  const char *message = "storage error";
  bool ok() const { return status == UserDataFileStatus::Ok; }
};

struct UserDataUploadBegin {
  UserDataFileResult result;
  uint32_t sessionId = 0;
  size_t maxUploadBytes = 0;
};

struct UserDataUploadCommit {
  UserDataFileResult result;
  bool created = false;
  size_t sizeBytes = 0;
};

struct UserDataDownloadBegin {
  UserDataFileResult result;
  uint32_t sessionId = 0;
  size_t fileSize = 0;
  size_t rangeStart = 0;
  size_t contentLength = 0;
  bool partial = false;
};

struct UserDataReadResult {
  UserDataFileResult result;
  size_t bytesRead = 0;
};

struct UserDataFileEntry {
  char name[65]{};
  size_t sizeBytes = 0;
};

using UserDataFileVisitor = void (*)(const UserDataFileEntry &, void *);

struct UserDataFilePage {
  UserDataFileResult result;
  size_t emitted = 0;
  size_t nextOffset = 0;
  bool hasMore = false;
};

class UserDataStorage {
 public:
  static constexpr size_t kAllocationUnitBytes = 4096;
  static constexpr size_t kReserveBytes = 64 * 1024;

  bool mountedValue = true;
  size_t totalValue = 1998848;
  size_t usedValue = 327680;
  UserDataFileResult listResult{UserDataFileStatus::Ok, "ok"};
  UserDataFileResult deleteResult{UserDataFileStatus::Ok, "ok"};
  UserDataFileEntry entries[2] = {{{}, 12}, {{}, 34}};
  size_t entryCount = 2;
  size_t lastOffset = 0;
  size_t lastLimit = 0;
  unsigned uploadBeginCount = 0;
  unsigned downloadBeginCount = 0;

  UserDataStorage() {
    strlcpy(entries[0].name, "a.txt", sizeof(entries[0].name));
    strlcpy(entries[1].name, "b.bin", sizeof(entries[1].name));
  }

  bool mounted() const {
    return mountedValue;
  }

  StorageCapacity rawCapacity() const {
    if (!mountedValue) return {};
    return {
        totalValue,
        usedValue,
        totalValue > usedValue ? totalValue - usedValue : 0,
    };
  }

  UploadCapacity uploadCapacity() const {
    return calculateUploadCapacity(
        rawCapacity(), kReserveBytes, kAllocationUnitBytes);
  }

  UserDataFilePage listFiles(size_t offset,
                             size_t limit,
                             UserDataFileVisitor visitor,
                             void *context) {
    lastOffset = offset;
    lastLimit = limit;
    UserDataFilePage page;
    page.result = listResult;
    if (!listResult.ok()) return page;
    for (size_t index = offset; index < entryCount && page.emitted < limit; ++index) {
      UserDataFileEntry transient = entries[index];
      visitor(transient, context);
      for (char &value : transient.name) value = static_cast<char>(0xa5);
      transient.sizeBytes = static_cast<size_t>(-1);
      ++page.emitted;
    }
    page.hasMore = offset + page.emitted < entryCount;
    if (page.hasMore) page.nextOffset = offset + page.emitted;
    return page;
  }

  UserDataFileResult deleteFile(const char *) {
    return deleteResult;
  }

  UserDataUploadBegin beginUpload(const char *, size_t) {
    ++uploadBeginCount;
    return {{UserDataFileStatus::Ok, "ok"}, 41, uploadCapacity().maxUploadBytes};
  }

  UserDataFileResult writeUpload(uint32_t, size_t, const uint8_t *, size_t) {
    return {UserDataFileStatus::Ok, "ok"};
  }

  UserDataUploadCommit finishUpload(uint32_t) {
    return {{UserDataFileStatus::Ok, "ok"}, true, 12};
  }

  void abortUpload(uint32_t) {}

  UserDataDownloadBegin beginDownload(const char *, const char *) {
    ++downloadBeginCount;
    return {{UserDataFileStatus::Ok, "ok"}, 42, 12, 0, 12, false};
  }

  UserDataReadResult readDownload(uint32_t, uint8_t *, size_t) {
    return {{UserDataFileStatus::Ok, "ok"}, 0};
  }

  void finishDownload(uint32_t) {}
};
