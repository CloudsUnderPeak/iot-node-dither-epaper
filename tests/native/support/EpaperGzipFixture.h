#pragma once
#include <cassert>
#include <vector>
#include <zlib.h>
#include "modules/epaper/EpaperImageFormat.h"

inline std::vector<uint8_t> gzipBytes(const std::vector<uint8_t> &raw, int level = 6) {
  z_stream z{};
  assert(deflateInit2(&z, level, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) == Z_OK);
  std::vector<uint8_t> out(deflateBound(&z, raw.size()));
  z.next_in = const_cast<Bytef *>(raw.data()); z.avail_in = raw.size();
  z.next_out = out.data(); z.avail_out = out.size();
  assert(deflate(&z, Z_FINISH) == Z_STREAM_END);
  out.resize(z.total_out);
  deflateEnd(&z);
  return out;
}

inline std::vector<uint8_t> epaperBytes(uint64_t generation = 1) {
  using namespace EpaperImageFormat;
  std::vector<uint8_t> raw(kImageBytes, 0x11);
  Header header{kVersion, kHeaderBytes, kWidth, kHeight, kFrameBytes,
                crc32(raw.data() + kHeaderBytes, kFrameBytes), generation};
  assert(encodeHeader(header, raw.data(), raw.size()));
  return raw;
}
