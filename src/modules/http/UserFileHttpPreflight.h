#pragma once

#include <strings.h>

#include "api/ApiRouter.h"
#include "api/shared/ApiResponse.h"
#include "modules/storage/UserFilePolicy.h"

// HTTP transport shape gate. Router authorizes the matched dynamic route
// before any body/query/media checks; neither path opens a storage session.
class UserFileHttpPreflight {
 public:
  static Api::Response upload(ApiRouter &router, const String &token,
                              const char *path, bool hasQuery,
                              bool hasTransferEncoding, const char *lengthHeader,
                              size_t contentLength, size_t callbackTotal,
                              const String &contentType) {
    const Api::Response access = router.checkStreamingFileAccess(Api::Method::Put, token, path);
    if (!access.success) return access;
    if (hasQuery) return Api::problem(400, "unsupported_field", "file upload does not accept query fields");
    if (hasTransferEncoding || lengthHeader == nullptr) {
      return Api::problem(411, "content_length_required", "file upload requires Content-Length");
    }
    size_t declaredBytes = 0;
    if (!UserFilePolicy::parseSize(lengthHeader, declaredBytes) ||
        declaredBytes != contentLength || (callbackTotal != 0 && callbackTotal != declaredBytes)) {
      return Api::problem(411, "content_length_required", "invalid Content-Length");
    }
    const char *media = contentType.c_str();
    if (strncasecmp(media, "multipart/form-data", 19) == 0 ||
        strncasecmp(media, "application/x-www-form-urlencoded", 33) == 0 ||
        strncasecmp(media, "text/plain", 10) == 0) {
      return Api::problem(415, "unsupported_media_type",
                          "raw file upload requires application/octet-stream or no Content-Type");
    }
    return Api::ok("{}");
  }

  static Api::Response download(ApiRouter &router, const String &token,
                                const char *path, bool hasBody, bool hasQuery) {
    const Api::Response access = router.checkStreamingFileAccess(Api::Method::Get, token, path);
    if (!access.success) return access;
    if (hasBody) return Api::problem(400, "unsupported_field", "file download does not accept a request body");
    if (hasQuery) return Api::problem(400, "unsupported_field", "file download does not accept query fields");
    return Api::ok("{}");
  }
};
