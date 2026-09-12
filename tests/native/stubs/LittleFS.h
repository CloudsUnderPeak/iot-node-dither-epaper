#pragma once
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define FILE_READ "r"
#define FILE_WRITE "w"
enum SeekMode { SeekSet };
namespace nativefs {
struct Node { bool directory = false; std::vector<uint8_t> bytes; };
struct Backend {
  std::map<std::string, std::shared_ptr<Node>> files;
  std::string fail;
  size_t handles = 0;
  size_t closes = 0;
  size_t total = 1024 * 1024;
  size_t calls = 0;
  std::function<void(const char *)> before;
  bool operation(const char *name) {
    ++calls;
    if (before) before(name);
    return fail != name;
  }
};
inline Backend backend;
struct Handle {
  std::shared_ptr<Node> node;
  std::string path;
  size_t offset = 0;
  size_t entry = 0;
  bool closed = false;
  ~Handle() { close(); }
  void close() { if (!closed) { closed = true; --backend.handles; ++backend.closes; } }
};
}
namespace fs {
class File {
  std::shared_ptr<nativefs::Handle> handle_;
 public:
  File() = default;
  File(const std::string &path, std::shared_ptr<nativefs::Node> node) {
    handle_ = std::make_shared<nativefs::Handle>();
    handle_->path = path;
    handle_->node = std::move(node);
    ++nativefs::backend.handles;
  }
  explicit operator bool() const { return handle_ && !handle_->closed; }
  bool isDirectory() const { return *this && handle_->node->directory; }
  const char *path() const { return handle_->path.c_str(); }
  const char *name() const { return path(); }
  size_t size() const {
    return *this && nativefs::backend.operation("size") ? handle_->node->bytes.size() : 0;
  }
  size_t read(uint8_t *data, size_t count) {
    if (!*this || !nativefs::backend.operation("read")) return 0;
    auto &bytes = handle_->node->bytes;
    count = std::min(count, bytes.size() - handle_->offset);
    if (count) std::memcpy(data, bytes.data() + handle_->offset, count);
    handle_->offset += count;
    return count;
  }
  size_t write(const uint8_t *data, size_t count) {
    if (!*this || !nativefs::backend.operation("write")) return 0;
    auto &bytes = handle_->node->bytes;
    bytes.resize(handle_->offset + count);
    if (count) std::memcpy(bytes.data() + handle_->offset, data, count);
    handle_->offset += count;
    return count;
  }
  bool seek(size_t offset, SeekMode) {
    if (!*this || !nativefs::backend.operation("seek") || offset > size()) return false;
    handle_->offset = offset;
    return true;
  }
  void flush() {
    // Arduino exposes void flush/close; simulate failed persistence through
    // the read-back verification, without inventing a production return value.
    if (*this && !nativefs::backend.operation("flush")) handle_->node->bytes.clear();
  }
  void close() {
    if (!*this) return;
    if (!nativefs::backend.operation("close")) handle_->node->bytes.clear();
    handle_->close();
  }
  File openNextFile() {
    if (!isDirectory() || !nativefs::backend.operation("list")) return {};
    size_t index = 0;
    const std::string prefix = handle_->path + "/";
    for (const auto &item : nativefs::backend.files) {
      if (item.first.compare(0, prefix.size(), prefix) != 0 ||
          item.first.find('/', prefix.size()) != std::string::npos) continue;
      if (index++ == handle_->entry) { ++handle_->entry; return File(item.first, item.second); }
    }
    return {};
  }
};
class LittleFSFS {
 public:
  bool begin(bool, const char *, size_t, const char *) { return nativefs::backend.operation("mount"); }
  void end() {}
  bool format() {
    if (!nativefs::backend.operation("format")) return false;
    nativefs::backend.files.clear();
    return true;
  }
  bool exists(const char *path) { return nativefs::backend.files.count(path) != 0; }
  bool mkdir(const char *path) {
    if (!nativefs::backend.operation("mkdir")) return false;
    auto node = std::make_shared<nativefs::Node>();
    node->directory = true;
    nativefs::backend.files[path] = node;
    return true;
  }
  File open(const char *path, const char *mode = FILE_READ, bool = false) {
    if (!nativefs::backend.operation("open")) { errno = EIO; return {}; }
    if (std::strcmp(mode, FILE_WRITE) == 0) nativefs::backend.files[path] = std::make_shared<nativefs::Node>();
    auto found = nativefs::backend.files.find(path);
    if (found == nativefs::backend.files.end()) { errno = ENOENT; return {}; }
    return File(path, found->second);
  }
  bool remove(const char *path) {
    return nativefs::backend.operation("remove") && nativefs::backend.files.erase(path) != 0;
  }
  bool rename(const char *from, const char *to) {
    if (!nativefs::backend.operation("rename")) return false;
    auto found = nativefs::backend.files.find(from);
    if (found == nativefs::backend.files.end()) return false;
    nativefs::backend.files[to] = found->second;
    nativefs::backend.files.erase(found);
    return true;
  }
  size_t totalBytes() { return nativefs::backend.operation("capacity") ? nativefs::backend.total : 0; }
  size_t usedBytes() {
    size_t bytes = 0;
    for (const auto &item : nativefs::backend.files) bytes += item.second->bytes.size();
    return bytes;
  }
};
}
