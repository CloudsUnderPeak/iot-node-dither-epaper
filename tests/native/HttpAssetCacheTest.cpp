#include <cassert>
#include <iostream>
#include <string>

#include "modules/http/HttpAssetCache.h"

int main() {
  using HttpAssetCache::matches;
  const char *etag = "\"current-gzip\"";
  assert(matches(etag, etag));
  assert(matches(" W/\"current-gzip\"\t", etag));
  assert(matches("\"old\", W/\"current-gzip\", \"other\"", etag));
  assert(matches(", ,\"current-gzip\",\t,", etag));
  assert(matches("\"has,a,comma\", \"current-gzip\"", etag));
  assert(matches("*", etag));
  assert(matches(" \t* \t", etag));
  assert(matches("\"\"", "\"\""));
  for (const char *header : {"", " ", "\"old\"", "\"CURRENT-gzip\"",
       "\"current-identity\"", "current-gzip", "w/\"current-gzip\"",
       "W/ \"current-gzip\"", "\"current-gzip", "\"current-gzip\" junk",
       "\"current-gzip\", invalid", "\"current-gzip\", *", "*, \"current-gzip\"",
       "\"current-gzip\" \"other\"", "\"contains space\",\"current-gzip\"",
       "\"control\r\",\"current-gzip\"", ",,", "W/"}) {
    assert(!matches(header, etag));
  }
  assert(!matches(nullptr, etag));
  assert(!matches(etag, nullptr));
  assert(!matches(etag, ""));
  const std::string boundary = std::string(HttpAssetCache::kMaxIfNoneMatchBytes - 14, ' ') + etag;
  assert(boundary.size() == HttpAssetCache::kMaxIfNoneMatchBytes);
  assert(matches(boundary.c_str(), etag));
  assert(!matches((boundary + ' ').c_str(), etag));
  // Content changes invalidate even when the URL and byte length are unchanged.
  assert(!matches("\"oldhash-gzip\"", "\"newhash-gzip\""));
  std::cout << "HTTP asset ETag validation passed\n";
}
