#pragma once

#include "EpaperGzip.h"
#include "modules/storage/UserDataStorage.h"

// Owns a retained storage session and one decoder. Reads logical EPDIMG bytes,
// validates both layers, and never releases the storage gate at compressed EOF.
class EpaperGzipReader {
 public:
  explicit EpaperGzipReader(UserDataStorage *storage) : storage_(storage) {}
  ~EpaperGzipReader() { close(); }
  UserDataDownloadBegin open(const char *name);
  void close();
  bool rewind();
  size_t read(uint8_t *output, size_t capacity);
  bool skip(size_t bytes);
  bool complete();
  bool drainStored();
  bool ioFailed() const { return ioFailed_; }
  EpaperImageFormat::ValidationError error() const;
  const EpaperImageFormat::Header &header() const { return validator_.header(); }
  uint32_t sessionId() const { return sessionId_; }
  size_t storedBytes() const { return storedBytes_; }
  size_t offset() const { return offset_; }

 private:
  UserDataStorage *storage_;
  uint32_t sessionId_ = 0;
  size_t storedBytes_ = 0;
  size_t compressedRead_ = 0;
  size_t inputOffset_ = 0;
  size_t inputBytes_ = 0;
  size_t offset_ = 0;
  bool ioFailed_ = false;
  bool verified_ = false;
  EpaperGzip decoder_;
  EpaperImageFormat::StreamingValidator validator_;
  uint8_t input_[UserDataStorage::kFileIoChunkBytes]{};
  // Tail validation may be nested inside a skip/read. Keep their shared
  // scratch on the reader's heap allocation, not the 8 KiB task stack.
  uint8_t discard_[UserDataStorage::kFileIoChunkBytes]{};
  bool pump(uint8_t *output, size_t capacity, size_t &produced);
};
