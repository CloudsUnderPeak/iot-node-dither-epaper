#include <cassert>
#include <iostream>
#include "modules/storage/UserDataStorage.h"

int main() {
  UserDataStorage storage;
  assert(storage.begin().ok());
  const uint8_t original[] = {1, 2, 3};
  auto upload = storage.beginUpload("image.epd", sizeof(original));
  assert(upload.result.ok());
  assert(storage.beginDownload("image.epd", "").result.status == UserDataFileStatus::Busy);
  assert(storage.writeUpload(upload.sessionId, 0, original, sizeof(original)).ok());
  assert(storage.finishUpload(upload.sessionId).result.ok());
  assert(nativefs::backend.handles == 0);
  for (const char *failure : {"write", "flush", "close", "size", "rename"}) {
    auto next = storage.beginUpload("image.epd", sizeof(original));
    assert(next.result.ok());
    storage.abortUpload(upload.sessionId); // stale callback cannot release next
    assert(storage.beginDownload("image.epd", "").result.status == UserDataFileStatus::Busy);
    nativefs::backend.fail = failure;
    auto written = storage.writeUpload(next.sessionId, 0, original, sizeof(original));
    auto committed = storage.finishUpload(next.sessionId);
    assert(!written.ok() || !committed.result.ok());
    nativefs::backend.fail.clear();
    assert(nativefs::backend.handles == 0);
    assert(nativefs::backend.files.at("/files/image.epd")->bytes == std::vector<uint8_t>(original, original + 3));
    auto download = storage.beginDownload("image.epd", "bytes=1-2");
    assert(download.result.ok() && download.contentLength == 2);
    uint8_t bytes[3]{};
    assert(storage.readDownload(download.sessionId, bytes, 3).bytesRead == 2);
    assert(bytes[0] == 2 && bytes[1] == 3);
    assert(nativefs::backend.handles == 0);
  }
  auto download = storage.beginDownload("image.epd", "");
  assert(download.result.ok());
  nativefs::backend.fail = "read";
  uint8_t bytes[3];
  assert(!storage.readDownload(download.sessionId, bytes, 3).result.ok());
  nativefs::backend.fail.clear();
  assert(nativefs::backend.handles == 0);
  assert(storage.deleteFile("image.epd").ok());
  assert(storage.beginDownload("image.epd", "").result.status == UserDataFileStatus::NotFound);
  auto empty = storage.beginUpload("empty.bin", 0);
  assert(empty.result.ok() && storage.finishUpload(empty.sessionId).result.ok());
  auto zero = storage.beginDownload("empty.bin", "");
  assert(zero.result.ok() && zero.contentLength == 0);
  storage.finishDownload(zero.sessionId);
  assert(storage.beginDownload("empty.bin", "bytes=0-0").result.status == UserDataFileStatus::RangeNotSatisfiable);
  for (bool wrongOffset : {false, true}) {
    auto bad = storage.beginUpload("bad.bin", 3);
    assert(bad.result.ok());
    if (wrongOffset) assert(!storage.writeUpload(bad.sessionId, 1, original, 3).ok());
    else assert(storage.writeUpload(bad.sessionId, 0, original, 2).ok());
    assert(!storage.finishUpload(bad.sessionId).result.ok());
    assert(nativefs::backend.handles == 0);
  }
  const auto capacity = storage.uploadCapacity();
  assert(storage.beginUpload("quota.bin", capacity.maxUploadBytes + 1).result.status == UserDataFileStatus::PayloadTooLarge);
  auto exact = storage.beginUpload("quota.bin", capacity.maxUploadBytes);
  assert(exact.result.ok());
  std::vector<uint8_t> payload(capacity.maxUploadBytes, 42);
  assert(storage.writeUpload(exact.sessionId, 0, payload.data(), payload.size()).ok());
  assert(storage.uploadCapacity().availableBytes == capacity.availableBytes);
  assert(storage.finishUpload(exact.sessionId).result.ok());
  assert(storage.uploadCapacity().availableBytes == 0);
  auto range = storage.beginDownload("quota.bin", "bytes=-1");
  assert(range.result.ok() && range.rangeStart == payload.size() - 1);
  storage.finishDownload(range.sessionId);
  assert(storage.beginDownload("quota.bin", "bytes=99999999-").result.status == UserDataFileStatus::RangeNotSatisfiable);
  nativefs::backend.fail = "seek";
  assert(!storage.beginDownload("quota.bin", "bytes=1-2").result.ok());
  nativefs::backend.fail.clear();
  assert(nativefs::backend.handles == 0);
  assert(storage.deleteFile("quota.bin").ok());
  std::cout << "Real UserDataStorage lifecycle and persistence failure tests passed\n";
}
