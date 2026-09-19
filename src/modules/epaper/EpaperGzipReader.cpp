#include "EpaperGzipReader.h"

#include <algorithm>

UserDataDownloadBegin EpaperGzipReader::open(const char *name) {
  close();
  auto result = storage_->beginDownload(name, "", true);
  if (!result.result.ok()) return result;
  sessionId_ = result.sessionId;
  storedBytes_ = result.fileSize;
  if (!rewind()) {
    close();
    result.result = {UserDataFileStatus::StorageError, "failed to initialize gzip reader"};
  }
  return result;
}

void EpaperGzipReader::close() {
  decoder_.release();
  if (sessionId_) storage_->finishDownload(sessionId_);
  sessionId_ = 0;
}

bool EpaperGzipReader::rewind() {
  inputOffset_ = inputBytes_ = compressedRead_ = offset_ = 0;
  ioFailed_ = verified_ = false;
  validator_.reset();
  if (!sessionId_ || !decoder_.reset() || !storage_->rewindDownload(sessionId_)) {
    ioFailed_ = true;
    return false;
  }
  return true;
}

EpaperImageFormat::ValidationError EpaperGzipReader::error() const {
  return decoder_.error() != EpaperImageFormat::ValidationError::None
      ? decoder_.error() : validator_.error();
}

bool EpaperGzipReader::pump(uint8_t *output, size_t capacity, size_t &produced) {
  produced = 0;
  if (ioFailed_ || error() != EpaperImageFormat::ValidationError::None) return false;
  if (inputOffset_ == inputBytes_ && compressedRead_ < storedBytes_) {
    auto result = storage_->readDownload(sessionId_, input_, sizeof(input_));
    if (!result.result.ok() || result.bytesRead == 0) { ioFailed_ = true; return false; }
    inputOffset_ = 0;
    inputBytes_ = result.bytesRead;
    compressedRead_ += result.bytesRead;
  }
  size_t consumed = 0;
  if (!decoder_.step(input_ + inputOffset_, inputBytes_ - inputOffset_, consumed,
                     output, capacity, produced, compressedRead_ == storedBytes_)) return false;
  inputOffset_ += consumed;
  if (produced && !validator_.consume(output, produced)) return false;
  offset_ += produced;
  if (!consumed && !produced && !decoder_.done()) return decoder_.finish();
  return true;
}

size_t EpaperGzipReader::read(uint8_t *output, size_t capacity) {
  if (!output || !capacity || verified_ || ioFailed_) return 0;
  size_t total = 0;
  while (total < capacity && offset_ < EpaperImageFormat::kImageBytes) {
    size_t produced = 0;
    if (!pump(output + total, capacity - total, produced)) return 0;
    total += produced;
    if (decoder_.done()) break;
  }
  if (offset_ == EpaperImageFormat::kImageBytes && !complete()) return 0;
  return total;
}

bool EpaperGzipReader::skip(size_t bytes) {
  while (bytes) {
    size_t count = read(discard_, std::min(bytes, sizeof(discard_)));
    if (!count) return false;
    bytes -= count;
  }
  return true;
}

bool EpaperGzipReader::complete() {
  if (verified_) return true;
  if (ioFailed_ || error() != EpaperImageFormat::ValidationError::None) return false;
  while (!decoder_.done() || inputOffset_ < inputBytes_ || compressedRead_ < storedBytes_) {
    size_t produced = 0;
    if (!pump(discard_, sizeof(discard_), produced)) return false;
  }
  verified_ = decoder_.finish() && validator_.finish();
  return verified_;
}

// After syntax failure, distinguish fully readable corrupt content from a
// partial filesystem failure before publishing an invalid metadata snapshot.
bool EpaperGzipReader::drainStored() {
  while (!ioFailed_ && compressedRead_ < storedBytes_) {
    const auto result = storage_->readDownload(sessionId_, input_, sizeof(input_));
    if (!result.result.ok() || !result.bytesRead) { ioFailed_ = true; break; }
    compressedRead_ += result.bytesRead;
  }
  return !ioFailed_;
}
