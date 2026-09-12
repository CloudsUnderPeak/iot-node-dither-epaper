#include "UserDataStorage.h"

#include <cstring>
#include <cerrno>
#include <limits>

namespace {
constexpr const char *kBasePath = "/userdata";
constexpr const char *kPartitionLabel = "userdata";

UserDataFileResult fileOk() {
  return {UserDataFileStatus::Ok, "ok"};
}

UserDataFileResult fileError(UserDataFileStatus status, const char *message) {
  return {status, message};
}
}  // namespace

Result UserDataStorage::begin() {
  mounted_ = false;
  if (operationGate_ == nullptr) {
    operationGate_ = xSemaphoreCreateBinary();
    if (operationGate_ == nullptr) {
      return outOfSpace("failed to create user storage operation gate");
    }
    xSemaphoreGive(operationGate_);
  }

  mounted_ = mountVolume();
  if (!mounted_) return storageError("failed to mount user storage");
  if (!prepareNamespace()) {
    filesystem_.end();
    mounted_ = false;
    return storageError("failed to prepare user storage namespace");
  }
  refreshStableCapacity();
  return okResult();
}

bool UserDataStorage::mountVolume() {
  return filesystem_.begin(false, kBasePath, 4, kPartitionLabel);
}

bool UserDataStorage::prepareNamespace() {
  if (!filesystem_.exists(kFilesPath) && !filesystem_.mkdir(kFilesPath)) {
    return false;
  }
  return removeInternalTemp();
}

Result UserDataStorage::reset() {
  closeActiveFile();
  clearActiveState();
  if (mounted_) {
    filesystem_.end();
  } else if (mountVolume()) {
    filesystem_.end();
  }
  mounted_ = false;
  if (!filesystem_.format()) {
    return storageError("failed to format user storage");
  }
  if (!mountVolume()) {
    return storageError("failed to mount formatted user storage");
  }
  mounted_ = true;
  if (!prepareNamespace()) {
    return storageError("failed to prepare formatted user storage");
  }
  refreshStableCapacity();
  return okResult();
}

bool UserDataStorage::mounted() const {
  return mounted_;
}

StorageCapacity UserDataStorage::rawCapacity() const {
  if (!mounted_) return {};

  StorageCapacity value;
  value.totalBytes = filesystem_.totalBytes();
  value.usedBytes = filesystem_.usedBytes();
  value.freeBytes = value.totalBytes > value.usedBytes
                        ? value.totalBytes - value.usedBytes
                        : 0;
  return value;
}

UploadCapacity UserDataStorage::uploadCapacity() const {
  portENTER_CRITICAL(&capacityMux_);
  const UploadCapacity snapshot = stableCapacity_;
  portEXIT_CRITICAL(&capacityMux_);
  return snapshot;
}

bool UserDataStorage::reserveRestart() {
  // A service that never allocated its gate cannot have an active session.
  if (operationGate_ == nullptr) return !mounted_;
  return tryBeginOperation(ActiveOperation::Synchronous);
}

UserDataUploadBegin UserDataStorage::beginUpload(const char *name,
                                                 size_t declaredBytes) {
  UserDataUploadBegin begin;
  if (!UserFilePolicy::validPublicName(name)) {
    begin.result = fileError(UserDataFileStatus::InvalidName, "invalid filename");
    return begin;
  }
  if (!tryBeginOperation(ActiveOperation::Upload)) {
    begin.result = fileError(UserDataFileStatus::Busy, "user storage is busy");
    return begin;
  }
  if (!mounted_) {
    begin.result = fileError(UserDataFileStatus::Unavailable, "userdata is not mounted");
    endOperation();
    return begin;
  }
  if (!removeInternalTemp()) {
    begin.result = fileError(UserDataFileStatus::StorageError,
                             "failed to clear stale upload file");
    refreshStableCapacity();
    endOperation();
    return begin;
  }

  const UploadCapacity fresh = calculateUploadCapacity(
      rawCapacity(), kReserveBytes, kAllocationUnitBytes);
  begin.maxUploadBytes = fresh.maxUploadBytes;
  if (declaredBytes > fresh.maxUploadBytes) {
    begin.result = fileError(UserDataFileStatus::PayloadTooLarge,
                             "file exceeds current upload limit");
    refreshStableCapacity();
    endOperation();
    return begin;
  }
  if (!buildTargetPath(name, targetPath_, sizeof(targetPath_))) {
    begin.result = fileError(UserDataFileStatus::InvalidName, "invalid filename");
    endOperation();
    return begin;
  }

  replacingTarget_ = filesystem_.exists(targetPath_);
  activeFile_ = filesystem_.open(kTempPath, FILE_WRITE, true);
  if (!activeFile_) {
    begin.result = fileError(UserDataFileStatus::StorageError,
                             "failed to open temporary upload file");
    refreshStableCapacity();
    endOperation();
    return begin;
  }

  activeSessionId_ = allocateSessionId();
  declaredBytes_ = declaredBytes;
  receivedBytes_ = 0;
  operationLimitBytes_ = fresh.maxUploadBytes;
  begin.sessionId = activeSessionId_;
  begin.result = fileOk();
  return begin;
}

UserDataFileResult UserDataStorage::writeUpload(uint32_t sessionId,
                                                size_t index,
                                                const uint8_t *data,
                                                size_t length) {
  if (!sessionMatches(ActiveOperation::Upload, sessionId)) {
    return fileError(UserDataFileStatus::UploadIncomplete, "upload session is not active");
  }
  if ((data == nullptr && length != 0) || index != receivedBytes_ ||
      length > std::numeric_limits<size_t>::max() - index ||
      index + length > declaredBytes_ || index + length > operationLimitBytes_) {
    abortUpload(sessionId);
    return fileError(UserDataFileStatus::UploadIncomplete,
                     "upload body offsets do not match Content-Length");
  }

  size_t consumed = 0;
  while (consumed < length) {
    const size_t remaining = length - consumed;
    const size_t chunk = remaining < kFileIoChunkBytes ? remaining : kFileIoChunkBytes;
    const size_t written = activeFile_.write(data + consumed, chunk);
    if (written != chunk) {
      abortUpload(sessionId);
      return fileError(UserDataFileStatus::InsufficientStorage,
                       "short write while uploading file");
    }
    consumed += written;
    receivedBytes_ += written;
  }
  return fileOk();
}

UserDataUploadCommit UserDataStorage::finishUpload(uint32_t sessionId) {
  UserDataUploadCommit commit;
  if (!sessionMatches(ActiveOperation::Upload, sessionId)) {
    commit.result = fileError(UserDataFileStatus::UploadIncomplete,
                              "upload session is not active");
    return commit;
  }
  if (receivedBytes_ != declaredBytes_) {
    abortUpload(sessionId);
    commit.result = fileError(UserDataFileStatus::UploadIncomplete,
                              "received bytes do not match Content-Length");
    return commit;
  }

  activeFile_.flush();
  closeActiveFile();
  fs::File verification = filesystem_.open(kTempPath, FILE_READ);
  if (!verification || verification.size() != declaredBytes_) {
    if (verification) verification.close();
    filesystem_.remove(kTempPath);
    refreshStableCapacity();
    endOperation();
    commit.result = fileError(UserDataFileStatus::StorageError,
                              "temporary upload verification failed");
    return commit;
  }
  verification.close();

  const bool created = !replacingTarget_;
  if (!filesystem_.rename(kTempPath, targetPath_)) {
    filesystem_.remove(kTempPath);
    refreshStableCapacity();
    endOperation();
    commit.result = fileError(UserDataFileStatus::StorageError,
                              "atomic upload commit failed");
    return commit;
  }

  commit.created = created;
  commit.sizeBytes = declaredBytes_;
  commit.result = fileOk();
  refreshStableCapacity();
  endOperation();
  return commit;
}

void UserDataStorage::abortUpload(uint32_t sessionId) {
  if (!sessionMatches(ActiveOperation::Upload, sessionId)) return;
  closeActiveFile();
  filesystem_.remove(kTempPath);
  refreshStableCapacity();
  endOperation();
}

UserDataFilePage UserDataStorage::listFiles(size_t offset,
                                            size_t limit,
                                            UserDataFileVisitor visitor,
                                            void *context) {
  UserDataFilePage page;
  if (limit == 0 || visitor == nullptr) {
    page.result = fileError(UserDataFileStatus::StorageError, "invalid list request");
    return page;
  }
  if (!tryBeginOperation(ActiveOperation::Synchronous)) {
    page.result = fileError(UserDataFileStatus::Busy, "user storage is busy");
    return page;
  }
  if (!mounted_) {
    page.result = fileError(UserDataFileStatus::Unavailable, "userdata is not mounted");
    endOperation();
    return page;
  }

  fs::File directory = filesystem_.open(kFilesPath, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    page.result = fileError(UserDataFileStatus::StorageError,
                            "failed to open user file directory");
    endOperation();
    return page;
  }

  size_t visibleIndex = 0;
  while (true) {
    fs::File file = directory.openNextFile();
    if (!file) break;
    const char *name = baseName(file.name());
    const bool visible = !file.isDirectory() &&
                         strcmp(name, baseName(kTempPath)) != 0 &&
                         UserFilePolicy::validPublicName(name);
    if (visible && visibleIndex++ >= offset) {
      if (page.emitted >= limit) {
        page.hasMore = true;
        file.close();
        break;
      }
      UserDataFileEntry entry;
      strlcpy(entry.name, name, sizeof(entry.name));
      entry.sizeBytes = file.size();
      visitor(entry, context);
      ++page.emitted;
    }
    file.close();
  }
  directory.close();
  if (page.hasMore) page.nextOffset = offset + page.emitted;
  page.result = fileOk();
  endOperation();
  return page;
}

UserDataFileResult UserDataStorage::deleteFile(const char *name) {
  if (!UserFilePolicy::validPublicName(name)) {
    return fileError(UserDataFileStatus::InvalidName, "invalid filename");
  }
  if (!tryBeginOperation(ActiveOperation::Synchronous)) {
    return fileError(UserDataFileStatus::Busy, "user storage is busy");
  }
  if (!mounted_) {
    endOperation();
    return fileError(UserDataFileStatus::Unavailable, "userdata is not mounted");
  }
  char path[sizeof(targetPath_)]{};
  buildTargetPath(name, path, sizeof(path));
  fs::File target = filesystem_.open(path, FILE_READ);
  if (!target || target.isDirectory()) {
    if (target) target.close();
    endOperation();
    return fileError(UserDataFileStatus::NotFound, "file not found");
  }
  target.close();
  if (!filesystem_.remove(path)) {
    endOperation();
    return fileError(UserDataFileStatus::StorageError, "failed to delete file");
  }
  refreshStableCapacity();
  endOperation();
  return fileOk();
}

UserDataDownloadBegin UserDataStorage::beginDownload(const char *name,
                                                     const char *rangeHeader) {
  UserDataDownloadBegin begin;
  if (!UserFilePolicy::validPublicName(name)) {
    begin.result = fileError(UserDataFileStatus::InvalidName, "invalid filename");
    return begin;
  }
  if (!tryBeginOperation(ActiveOperation::Download)) {
    begin.result = fileError(UserDataFileStatus::Busy, "user storage is busy");
    return begin;
  }
  if (!mounted_) {
    begin.result = fileError(UserDataFileStatus::Unavailable, "userdata is not mounted");
    endOperation();
    return begin;
  }
  if (!buildTargetPath(name, targetPath_, sizeof(targetPath_))) {
    begin.result = fileError(UserDataFileStatus::InvalidName, "invalid filename");
    endOperation();
    return begin;
  }
  // Pinned Arduino VFS read-open reports missing paths via errno. Only a
  // confirmed ENOENT is absence; allocation/fd/I/O failures retain metadata.
  errno = 0;
  activeFile_ = filesystem_.open(targetPath_, FILE_READ);
  const int openError = errno;
  if (!activeFile_ || activeFile_.isDirectory()) {
    const bool absent = activeFile_ ? activeFile_.isDirectory() : openError == ENOENT;
    closeActiveFile();
    begin.result = absent
        ? fileError(UserDataFileStatus::NotFound, "file not found")
        : fileError(UserDataFileStatus::StorageError, "failed to open user file");
    endOperation();
    return begin;
  }

  UserFilePolicy::ByteRange range;
  const size_t fileSize = activeFile_.size();
  if (!UserFilePolicy::parseRange(rangeHeader, fileSize, range)) {
    closeActiveFile();
    begin.fileSize = fileSize;
    begin.result = fileError(UserDataFileStatus::RangeNotSatisfiable,
                             "requested range is not satisfiable");
    endOperation();
    return begin;
  }
  if (range.start != 0 && !activeFile_.seek(range.start, SeekSet)) {
    closeActiveFile();
    begin.result = fileError(UserDataFileStatus::StorageError,
                             "failed to seek user file");
    endOperation();
    return begin;
  }

  activeSessionId_ = allocateSessionId();
  downloadRemaining_ = range.length;
  begin.sessionId = activeSessionId_;
  begin.fileSize = fileSize;
  begin.rangeStart = range.start;
  begin.contentLength = range.length;
  begin.partial = range.partial;
  begin.result = fileOk();
  return begin;
}

UserDataReadResult UserDataStorage::readDownload(uint32_t sessionId,
                                                 uint8_t *buffer,
                                                 size_t bufferLength) {
  UserDataReadResult read;
  if (!sessionMatches(ActiveOperation::Download, sessionId) || buffer == nullptr) {
    read.result = fileError(UserDataFileStatus::StorageError,
                            "download session is not active");
    return read;
  }
  if (downloadRemaining_ == 0) {
    read.result = fileOk();
    finishDownload(sessionId);
    return read;
  }
  size_t requested = bufferLength;
  if (requested > kFileIoChunkBytes) requested = kFileIoChunkBytes;
  if (requested > downloadRemaining_) requested = downloadRemaining_;
  const size_t received = activeFile_.read(buffer, requested);
  if (received == 0 && requested != 0) {
    finishDownload(sessionId);
    read.result = fileError(UserDataFileStatus::StorageError,
                            "failed while reading user file");
    return read;
  }
  downloadRemaining_ -= received;
  read.bytesRead = received;
  read.result = fileOk();
  if (downloadRemaining_ == 0) finishDownload(sessionId);
  return read;
}

void UserDataStorage::finishDownload(uint32_t sessionId) {
  if (!sessionMatches(ActiveOperation::Download, sessionId)) return;
  closeActiveFile();
  endOperation();
}

UserDataInspectionPage UserDataStorage::inspectList(
    const char *normalizedPath,
    size_t offset,
    size_t limit,
    UserDataInspectionVisitor visitor,
    void *context) {
  UserDataInspectionPage page;
  if (normalizedPath == nullptr || limit == 0 || visitor == nullptr ||
      hiddenInspectionPath(normalizedPath)) {
    page.result = fileError(UserDataFileStatus::NotFound, "path not found");
    return page;
  }
  if (!tryBeginOperation(ActiveOperation::Synchronous)) {
    page.result = fileError(UserDataFileStatus::Busy, "user storage is busy");
    return page;
  }
  if (!mounted_) {
    page.result = fileError(UserDataFileStatus::Unavailable, "userdata is not mounted");
    endOperation();
    return page;
  }
  fs::File directory = filesystem_.open(normalizedPath, FILE_READ);
  if (!directory) {
    page.result = fileError(UserDataFileStatus::NotFound, "path not found");
    endOperation();
    return page;
  }
  if (!directory.isDirectory()) {
    directory.close();
    page.result = fileError(UserDataFileStatus::NotFound, "path is not a directory");
    endOperation();
    return page;
  }

  size_t visibleIndex = 0;
  while (true) {
    fs::File file = directory.openNextFile();
    if (!file) break;
    const char *path = file.path();
    const bool visible = !hiddenInspectionPath(path);
    if (visible && visibleIndex++ >= offset) {
      if (page.emitted >= limit) {
        page.hasMore = true;
        file.close();
        break;
      }
      UserDataInspectionEntry entry;
      strlcpy(entry.name, baseName(file.name()), sizeof(entry.name));
      entry.directory = file.isDirectory();
      entry.sizeBytes = entry.directory ? 0 : file.size();
      visitor(entry, context);
      ++page.emitted;
    }
    file.close();
  }
  directory.close();
  if (page.hasMore) page.nextOffset = offset + page.emitted;
  page.result = fileOk();
  endOperation();
  return page;
}

UserDataFileResult UserDataStorage::inspectStat(
    const char *normalizedPath,
    UserDataInspectionEntry &entry) {
  if (normalizedPath == nullptr || hiddenInspectionPath(normalizedPath)) {
    return fileError(UserDataFileStatus::NotFound, "path not found");
  }
  if (!tryBeginOperation(ActiveOperation::Synchronous)) {
    return fileError(UserDataFileStatus::Busy, "user storage is busy");
  }
  if (!mounted_) {
    endOperation();
    return fileError(UserDataFileStatus::Unavailable, "userdata is not mounted");
  }
  fs::File file = filesystem_.open(normalizedPath, FILE_READ);
  if (!file) {
    endOperation();
    return fileError(UserDataFileStatus::NotFound, "path not found");
  }
  strlcpy(entry.name, baseName(file.name()), sizeof(entry.name));
  entry.directory = file.isDirectory();
  entry.sizeBytes = entry.directory ? 0 : file.size();
  file.close();
  endOperation();
  return fileOk();
}

bool UserDataStorage::tryBeginOperation(ActiveOperation operation) {
  if (operationGate_ == nullptr || xSemaphoreTake(operationGate_, 0) != pdTRUE) {
    return false;
  }
  activeOperation_ = operation;
  return true;
}

void UserDataStorage::endOperation() {
  closeActiveFile();
  clearActiveState();
  if (operationGate_ != nullptr) xSemaphoreGive(operationGate_);
}

bool UserDataStorage::sessionMatches(ActiveOperation operation,
                                     uint32_t sessionId) const {
  return sessionId != 0 && activeOperation_ == operation &&
         activeSessionId_ == sessionId;
}

uint32_t UserDataStorage::allocateSessionId() {
  uint32_t value = nextSessionId_++;
  if (value == 0) value = nextSessionId_++;
  return value;
}

void UserDataStorage::refreshStableCapacity() {
  const UploadCapacity refreshed = calculateUploadCapacity(
      rawCapacity(), kReserveBytes, kAllocationUnitBytes);
  portENTER_CRITICAL(&capacityMux_);
  stableCapacity_ = refreshed;
  portEXIT_CRITICAL(&capacityMux_);
}

void UserDataStorage::closeActiveFile() {
  if (activeFile_) activeFile_.close();
}

void UserDataStorage::clearActiveState() {
  activeOperation_ = ActiveOperation::None;
  activeSessionId_ = 0;
  declaredBytes_ = 0;
  receivedBytes_ = 0;
  operationLimitBytes_ = 0;
  downloadRemaining_ = 0;
  replacingTarget_ = false;
  memset(targetPath_, 0, sizeof(targetPath_));
}

bool UserDataStorage::buildTargetPath(const char *name,
                                      char *output,
                                      size_t outputSize) const {
  if (!UserFilePolicy::validPublicName(name) || output == nullptr) return false;
  const int written = snprintf(output, outputSize, "%s/%s", kFilesPath, name);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

bool UserDataStorage::removeInternalTemp() {
  return !filesystem_.exists(kTempPath) || filesystem_.remove(kTempPath);
}

const char *UserDataStorage::baseName(const char *path) {
  if (path == nullptr) return "";
  const char *lastSlash = strrchr(path, '/');
  return lastSlash == nullptr ? path : lastSlash + 1;
}

bool UserDataStorage::hiddenInspectionPath(const char *normalizedPath) {
  return normalizedPath != nullptr && strcmp(normalizedPath, kTempPath) == 0;
}
