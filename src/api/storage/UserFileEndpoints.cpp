#include "UserFileEndpoints.h"

#include <cstring>

#include "api/shared/ApiResponse.h"
#include "modules/storage/UserFilePolicy.h"

namespace UserFileEndpoints {
namespace {

struct ListContext {
  JsonArray files;
};

void appendFile(const UserDataFileEntry &entry, void *opaque) {
  auto *context = static_cast<ListContext *>(opaque);
  JsonObject item = context->files.add<JsonObject>();
  // The entry buffer is stack-owned by the storage iterator and is gone
  // before the completed page is serialized. Force ArduinoJson to own a copy.
  item["name"] = JsonString(entry.name, false);
  item["size_bytes"] = entry.sizeBytes;
  item["media_type"] = UserFilePolicy::mediaPolicyForName(entry.name).contentType;
}

bool decodeListQuery(const Api::Request &request,
                     size_t &limit,
                     size_t &offset,
                     Api::Response &error) {
  limit = UserFilePolicy::kDefaultListLimit;
  offset = 0;
  if (request.queryOverflow) {
    error = Api::problem(400, "unsupported_field", "too many or oversized query fields");
    return false;
  }

  bool limitSeen = false;
  bool cursorSeen = false;
  for (size_t index = 0; index < request.queryCount; ++index) {
    const char *name = request.query[index].name.c_str();
    const char *value = request.query[index].value.c_str();
    if (strcmp(name, "limit") == 0) {
      if (limitSeen || !UserFilePolicy::parseSize(value, limit) || limit == 0 ||
          limit > UserFilePolicy::kMaxListLimit) {
        error = Api::problem(400, "invalid_field", "limit must be an integer from 1 to 100", "limit");
        return false;
      }
      limitSeen = true;
    } else if (strcmp(name, "cursor") == 0) {
      if (cursorSeen || !UserFilePolicy::decodeCursor(value, offset)) {
        error = Api::problem(400, "invalid_field", "invalid file cursor", "cursor");
        return false;
      }
      cursorSeen = true;
    } else {
      error = Api::problem(400, "unsupported_field", "unsupported query field", name);
      return false;
    }
  }
  return true;
}

}  // namespace

Api::Response list(const Api::Request &request, UserDataStorage &storage) {
  if (request.hasBody || request.hasJsonBody) {
    return Api::problem(400, "unsupported_field", "GET file list does not accept a request body");
  }
  size_t limit = 0;
  size_t offset = 0;
  Api::Response queryError;
  if (!decodeListQuery(request, limit, offset, queryError)) return queryError;

  JsonDocument data;
  ListContext context{data["files"].to<JsonArray>()};
  const UserDataFilePage page = storage.listFiles(
      offset, limit, appendFile, &context);
  if (!page.result.ok()) return fromStorageResult(page.result);

  if (page.hasMore) {
    char cursor[32]{};
    if (!UserFilePolicy::encodeCursor(page.nextOffset, cursor, sizeof(cursor))) {
      return Api::problem(500, "storage_error", "failed to encode file cursor");
    }
    data["next_cursor"] = cursor;
  } else {
    data["next_cursor"] = nullptr;
  }
  return Api::ok(Api::json(data));
}

Api::Response remove(const Api::Request &request,
                     const char *name,
                     UserDataStorage &storage) {
  if (request.queryOverflow || request.queryCount != 0) {
    return Api::problem(400, "unsupported_field", "DELETE file does not accept query fields");
  }
  if (request.hasBody || request.hasJsonBody) {
    return Api::problem(400, "unsupported_field", "DELETE file does not accept a request body");
  }
  if ((strcmp(name, "epaper-current.epd") == 0 || strcmp(name, "epaper-current.epd.gz") == 0)) {
    return Api::problem(
        403, "reserved_file", "e-paper image is managed by /api/epaper/image");
  }
  const UserDataFileResult result = storage.deleteFile(name);
  if (!result.ok()) return fromStorageResult(result);

  JsonDocument data;
  data["name"] = name;
  data["deleted"] = true;
  return Api::ok(Api::json(data), "file deleted");
}

Api::Response transportUnsupported() {
  return Api::problem(415,
                      "unsupported_transport",
                      "raw file upload and download require HTTP");
}

Api::Response fromStorageResult(const UserDataFileResult &result,
                                size_t detailBytes) {
  switch (result.status) {
    case UserDataFileStatus::Ok:
      return Api::ok("{}");
    case UserDataFileStatus::InvalidName:
      return Api::problem(400, "invalid_field", result.message, "name");
    case UserDataFileStatus::Busy:
      return Api::problem(409, "storage_busy", result.message);
    case UserDataFileStatus::Unavailable:
      return Api::problem(503, "storage_unavailable", result.message);
    case UserDataFileStatus::NotFound:
      return Api::problem(404, "not_found", result.message);
    case UserDataFileStatus::PayloadTooLarge: {
      JsonDocument data;
      data["code"] = "payload_too_large";
      data["max_upload_bytes"] = detailBytes;
      return Api::error(413, Api::json(data), result.message);
    }
    case UserDataFileStatus::RangeNotSatisfiable:
      return Api::problem(416, "range_not_satisfiable", result.message);
    case UserDataFileStatus::UploadIncomplete:
      return Api::problem(500, "upload_incomplete", result.message);
    case UserDataFileStatus::InsufficientStorage:
      return Api::problem(507, "insufficient_storage", result.message);
    case UserDataFileStatus::StorageError:
      return Api::problem(500, "storage_error", result.message);
  }
  return Api::problem(500, "storage_error", "storage error");
}

Api::Response uploadCommitted(const char *name,
                              const UserDataUploadCommit &commit) {
  if (!commit.result.ok()) return fromStorageResult(commit.result);
  JsonDocument data;
  data["name"] = name;
  data["size_bytes"] = commit.sizeBytes;
  data["created"] = commit.created;
  return commit.created
             ? Api::created(Api::json(data), "file uploaded")
             : Api::ok(Api::json(data), "file replaced");
}

}  // namespace UserFileEndpoints
