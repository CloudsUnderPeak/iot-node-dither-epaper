#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../core/Result.h"
#include "StorageCapacity.h"
#include "UserDataPath.h"
#include "UserFilePolicy.h"

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
  char name[UserFilePolicy::kMaxFilenameBytes + 1]{};
  size_t sizeBytes = 0;
};

using UserDataFileVisitor = void (*)(const UserDataFileEntry &entry, void *context);

struct UserDataFilePage {
  UserDataFileResult result;
  size_t emitted = 0;
  size_t nextOffset = 0;
  bool hasMore = false;
};

struct UserDataInspectionEntry {
  char name[UserDataPath::kMaxNormalizedBytes + 1]{};
  size_t sizeBytes = 0;
  bool directory = false;
};

using UserDataInspectionVisitor = void (*)(const UserDataInspectionEntry &entry,
                                           void *context);

struct UserDataInspectionPage {
  UserDataFileResult result;
  size_t emitted = 0;
  size_t nextOffset = 0;
  bool hasMore = false;
};

// Owns the persistent LittleFS volume and every open userdata file handle.
// A single non-blocking gate serializes HTTP and console file operations.
class UserDataStorage {
 public:
  static constexpr size_t kAllocationUnitBytes = 4096;
  static constexpr size_t kReserveBytes = 64 * 1024;
  static constexpr size_t kFileIoChunkBytes = 4096;

  Result begin();
  Result reset();
  bool mounted() const;
  StorageCapacity rawCapacity() const;
  UploadCapacity uploadCapacity() const;

  // Final restart ACK: no I/O, never closes another session. On success the
  // gate remains held through reboot, including a returning test restart.
  bool reserveRestart();

  UserDataUploadBegin beginUpload(const char *name, size_t declaredBytes);
  UserDataFileResult writeUpload(uint32_t sessionId,
                                 size_t index,
                                 const uint8_t *data,
                                 size_t length);
  UserDataUploadCommit finishUpload(uint32_t sessionId);
  void abortUpload(uint32_t sessionId);

  UserDataFilePage listFiles(size_t offset,
                             size_t limit,
                             UserDataFileVisitor visitor,
                             void *context);
  UserDataFileResult deleteFile(const char *name);

  UserDataDownloadBegin beginDownload(const char *name, const char *rangeHeader,
                                      bool retainAtEof = false);
  bool rewindDownload(uint32_t sessionId);
  UserDataReadResult readDownload(uint32_t sessionId,
                                  uint8_t *buffer,
                                  size_t bufferLength);
  void finishDownload(uint32_t sessionId);

  UserDataInspectionPage inspectList(const char *normalizedPath,
                                     size_t offset,
                                     size_t limit,
                                     UserDataInspectionVisitor visitor,
                                     void *context);
  UserDataFileResult inspectStat(const char *normalizedPath,
                                 UserDataInspectionEntry &entry);

 private:
  enum class ActiveOperation : uint8_t {
    None,
    Upload,
    Download,
    Synchronous,
  };

  static constexpr const char *kFilesPath = "/files";
  static constexpr const char *kTempPath = "/files/.upload.tmp";

  // Arduino's capacity accessors are not const even though they only query
  // filesystem state, so the handle remains mutable for logical-const reads.
  mutable fs::LittleFSFS filesystem_;
  mutable portMUX_TYPE capacityMux_ = portMUX_INITIALIZER_UNLOCKED;
  SemaphoreHandle_t operationGate_ = nullptr;
  bool mounted_ = false;
  UploadCapacity stableCapacity_{};
  ActiveOperation activeOperation_ = ActiveOperation::None;
  fs::File activeFile_;
  uint32_t activeSessionId_ = 0;
  uint32_t nextSessionId_ = 1;
  size_t declaredBytes_ = 0;
  size_t receivedBytes_ = 0;
  size_t operationLimitBytes_ = 0;
  size_t downloadRemaining_ = 0;
  bool retainDownloadAtEof_ = false;
  bool replacingTarget_ = false;
  char targetPath_[sizeof("/files/") + UserFilePolicy::kMaxFilenameBytes]{};

  bool mountVolume();
  bool prepareNamespace();
  bool tryBeginOperation(ActiveOperation operation);
  void endOperation();
  bool sessionMatches(ActiveOperation operation, uint32_t sessionId) const;
  uint32_t allocateSessionId();
  void refreshStableCapacity();
  void closeActiveFile();
  void clearActiveState();
  bool buildTargetPath(const char *name, char *output, size_t outputSize) const;
  bool removeInternalTemp();
  static const char *baseName(const char *path);
  static bool hiddenInspectionPath(const char *normalizedPath);
};
