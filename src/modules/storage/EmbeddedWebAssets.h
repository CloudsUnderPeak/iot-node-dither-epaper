#pragma once

#include <Arduino.h>

#include "../../core/Result.h"

struct EmbeddedWebAsset {
  const char *path = nullptr;
  const char *contentType = nullptr;
  const char *contentEncoding = nullptr;
  const uint8_t *bytes = nullptr;
  size_t size = 0;
};

// Provides raw or precompressed frontend assets compiled into the application image.
class EmbeddedWebAssets {
 public:
  Result begin();
  bool ready() const;
  bool bundled() const;
  const EmbeddedWebAsset *find(const char *path) const;
  size_t count() const;
  size_t payloadBytes() const;
  const char *source() const;
  const char *sha256() const;

 private:
  bool ready_ = false;
};
