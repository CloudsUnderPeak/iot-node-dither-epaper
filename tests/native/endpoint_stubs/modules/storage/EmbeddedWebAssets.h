#pragma once

class EmbeddedWebAssets {
 public:
  const char *sourceValue = "builtin";
  const char *sha256Value =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

  const char *source() const { return sourceValue; }
  const char *sha256() const { return sha256Value; }
  bool bundled() const { return sourceValue[0] != 'n'; }
};
