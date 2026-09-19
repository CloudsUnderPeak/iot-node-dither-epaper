#include <algorithm>
#include <cassert>
#include <iostream>
#include "modules/epaper/EpaperGzip.h"
#include "support/EpaperGzipFixture.h"

using EpaperImageFormat::ValidationError;

bool decode(EpaperGzip &decoder, const std::vector<uint8_t> &gzip, size_t chunk,
            std::vector<uint8_t> &output, bool validate = true) {
  assert(decoder.reset());
  EpaperImageFormat::StreamingValidator validator;
  output.clear();
  size_t offset = 0;
  uint8_t buffer[4096];
  while (offset < gzip.size()) {
    const size_t end = std::min(gzip.size(), offset + chunk);
    while (true) {
      size_t used = 0, produced = 0;
      if (!decoder.step(gzip.data() + offset, end - offset, used, buffer, sizeof(buffer), produced,
                        end == gzip.size())) return false;
      offset += used;
      output.insert(output.end(), buffer, buffer + produced);
      if (validate && !validator.consume(buffer, produced)) return false;
      if (!used && !produced) break;
    }
  }
  return decoder.finish() && (!validate || validator.finish());
}

int main() {
  EpaperGzip decoder;
  auto raw = epaperBytes();
  auto gzip = gzipBytes(raw);
  std::vector<uint8_t> result;
  for (size_t chunk : {1U, 2U, 3U, 7U, 8U, 9U, 10U, 11U, 31U, 4096U, 999999U}) {
    assert(decode(decoder, gzip, chunk, result)); assert(result == raw);
  }
  // Exercise a realistic high-entropy six-color frame, not only a solid
  // repeated byte. This catches dictionary/window corruption as palette data.
  auto varied = epaperBytes(2);
  constexpr uint8_t codes[] = {0, 1, 2, 3, 5, 6};
  uint32_t state = 0x12345678;
  for (size_t index = EpaperImageFormat::kHeaderBytes; index < varied.size(); ++index) {
    state = state * 1664525U + 1013904223U;
    const uint8_t left = codes[(state >> 8) % 6];
    const uint8_t right = codes[(state >> 20) % 6];
    varied[index] = static_cast<uint8_t>((left << 4) | right);
  }
  EpaperImageFormat::Header variedHeader{EpaperImageFormat::kVersion,
      EpaperImageFormat::kHeaderBytes, EpaperImageFormat::kWidth,
      EpaperImageFormat::kHeight, EpaperImageFormat::kFrameBytes,
      EpaperImageFormat::crc32(varied.data() + EpaperImageFormat::kHeaderBytes,
                               EpaperImageFormat::kFrameBytes), 2};
  assert(EpaperImageFormat::encodeHeader(variedHeader, varied.data(), varied.size()));
  const auto variedGzip = gzipBytes(varied);
  for (size_t chunk : {1U, 7U, 1460U, 4096U, 16384U}) {
    assert(decode(decoder, variedGzip, chunk, result)); assert(result == varied);
  }
  // Stored, fixed and dynamic DEFLATE plus dictionary wrap and backreferences.
  for (int level : {0, 1, 9}) {
    auto input = gzipBytes(raw, level);
    assert(decode(decoder, input, 1, result)); assert(result == raw);
  }
  size_t caseNumber = 0;
  auto reject = [&](std::vector<uint8_t> bytes, ValidationError error = ValidationError::None) {
    ++caseNumber;
    if (decode(decoder, bytes, 7, result)) { std::cerr << "unexpected valid case " << caseNumber << "\n"; std::abort(); }
    if (error != ValidationError::None) assert(decoder.error() == error);
    assert(decode(decoder, gzip, 1, result)); // reusable after every failure
  };
  for (size_t count : {0U, 1U, 9U, 10U, 20U}) reject({gzip.begin(), gzip.begin() + count});
  for (size_t tail = 1; tail <= 9; ++tail) reject({gzip.begin(), gzip.end() - tail});
  for (size_t index : {0U, 1U, 2U}) { auto bad = gzip; bad[index] ^= 0x80; reject(bad, ValidationError::InvalidGzip); }
  { auto bad = gzip; bad[3] = 0x20; reject(bad, ValidationError::InvalidGzip); }
  { auto bad = gzip; bad[gzip.size() - 8] ^= 1; reject(bad, ValidationError::BadGzipCrc); }
  { auto bad = gzip; bad[gzip.size() - 4] ^= 1; reject(bad, ValidationError::BadGzipSize); }
  { auto bad = gzip; bad.insert(bad.end(), gzip.begin(), gzip.end()); reject(bad, ValidationError::InvalidGzip); }
  { auto bad = gzip; bad.push_back(0); reject(bad, ValidationError::InvalidGzip); }
  { auto bad = raw; bad.pop_back(); reject(gzipBytes(bad), ValidationError::BadGzipSize); }
  { auto bad = raw; bad.push_back(0x11); reject(gzipBytes(bad), ValidationError::BadGzipSize); }
  for (size_t index : {0U, 8U, 12U, 16U, 20U, 24U, 28U, 40U}) {
    auto bad = raw; bad[index] ^= 0xFF; reject(gzipBytes(bad));
  }
  { auto bad = raw; std::fill(bad.begin() + 32, bad.begin() + 40, 0); reject(gzipBytes(bad)); }
  // All optional fields, including FHCRC; each byte is independently chunked.
  auto optional = gzip;
  optional[3] = 0x1E;
  std::vector<uint8_t> fields{3,0,1,2,3,'f',0,'c',0};
  optional.insert(optional.begin() + 10, fields.begin(), fields.end());
  auto crc = EpaperImageFormat::crc32(optional.data(), 10 + fields.size());
  optional.insert(optional.begin() + 10 + fields.size(), {static_cast<uint8_t>(crc), static_cast<uint8_t>(crc >> 8)});
  assert(decode(decoder, optional, 1, result) && result == raw);
  for (size_t count = 10; count < 21; ++count) reject({optional.begin(), optional.begin() + count});
  optional[19] ^= 1; reject(optional, ValidationError::InvalidGzip);
  { auto bad = gzip; bad[3] = 8; bad.insert(bad.begin() + 10, 1025, 'x'); reject(bad, ValidationError::InvalidGzip); }
  { auto bad = gzip; bad[3] = 4; bad.insert(bad.begin() + 10, {255,255}); reject(bad, ValidationError::InvalidGzip); }
  { auto bad = gzip; bad[10] = 7; reject(bad, ValidationError::InvalidGzip); }
  // Hand-built fixed blocks exercise DEFLATE errors independently of the
  // EPDIMG validator, including after the sliding dictionary has wrapped.
  for (bool filledDictionary : {false, true}) {
    for (unsigned invalidSymbol : {0U, 286U, 287U, 30U, 31U}) {
      if (filledDictionary && invalidSymbol == 0) continue;
      std::vector<uint8_t> bad(gzip.begin(), gzip.begin() + 10);
      if (filledDictionary) {
        bad.insert(bad.end(), {0, 0, 128, 255, 127}); // non-final stored 32768 bytes
        bad.insert(bad.end(), 32768, 0);
      }
      unsigned bitOffset = 0;
      auto bit = [&](unsigned value) {
        if (bitOffset == 0) bad.push_back(0);
        bad.back() |= (value & 1) << bitOffset;
        bitOffset = (bitOffset + 1) % 8;
      };
      auto huffman = [&](unsigned code, unsigned count) {
        while (count) bit(code >> --count);
      };
      bit(1); bit(1); bit(0); // final fixed-Huffman block
      if (invalidSymbol >= 286) huffman(192 + invalidSymbol - 280, 8);
      else {
        huffman(1, 7); // length 3; no literal history for invalidSymbol == 0
        huffman(invalidSymbol, 5);
      }
      huffman(0, 7); // end of block
      bad.insert(bad.end(), 8, 0); // unused trailer
      assert(!decode(decoder, bad, 1, result, false));
      assert(decoder.error() == ValidationError::InvalidGzip);
      assert(decode(decoder, gzip, 1, result));
    }
  }
  std::cout << "Gzip boundaries, CRC, ISIZE, optional headers, limits and reuse passed\n";
}
