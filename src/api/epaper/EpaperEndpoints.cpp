#include "EpaperEndpoints.h"

#include <cstdio>

#include "api/shared/ApiResponse.h"

namespace EpaperEndpoints {
namespace {

bool rejectFields(const Api::Request &request, Api::Response &response) {
  if (request.hasBody || request.hasJsonBody || request.queryCount != 0 ||
      request.queryOverflow) {
    response = Api::problem(400, "unsupported_field",
                            "e-paper action does not accept fields or a body");
    return true;
  }
  return false;
}

void appendStored(JsonObject object, const EpaperStoredImageMetadata &stored) {
  object["available"] = stored.present;
  object["valid"] = stored.valid;
  if (stored.present && !stored.valid) {
    object["reason"] = EpaperImageFormat::errorCode(stored.validationError);
  }
}

}  // namespace

Api::Response capabilities(const Api::Request &request) {
  Api::Response error;
  if (rejectFields(request, error)) return error;
  JsonDocument data;
  JsonObject panel = data["panel"].to<JsonObject>();
  panel["model"] = "waveshare-7in3e";
  panel["width"] = EpaperImageFormat::kWidth;
  panel["height"] = EpaperImageFormat::kHeight;
  panel["colors"] = 6;
  JsonArray codes = panel["color_codes"].to<JsonArray>();
  for (uint8_t code : {0, 1, 2, 3, 5, 6}) codes.add(code);
  JsonObject image = data["image"].to<JsonObject>();
  image["name"] = EpaperService::kImageName;
  image["format"] = "epdimg";
  image["header_bytes"] = EpaperImageFormat::kHeaderBytes;
  image["frame_bytes"] = EpaperImageFormat::kFrameBytes;
  image["upload_bytes"] = EpaperImageFormat::kImageBytes;
  JsonObject refresh = data["refresh"].to<JsonObject>();
  refresh["cpu_mhz"] = CpuFrequencyGuard::kEpaperMhz;
  refresh["cooldown_seconds"] = EpaperCooldown::kDurationMs / 1000U;
  refresh["automatic_on_boot"] = false;
  JsonObject capabilities = data["capabilities"].to<JsonObject>();
  capabilities["upload"] = true;
  capabilities["metadata"] = true;
  capabilities["download"] = true;
  capabilities["refresh"] = true;
  capabilities["white"] = true;
  capabilities["palette"] = true;
  return Api::ok(Api::json(data));
}

Api::Response status(const Api::Request &request,
                     const EpaperService &service) {
  Api::Response error;
  if (rejectFields(request, error)) return error;
  const EpaperServiceSnapshot snapshot = service.snapshot();
  JsonDocument data;
  data["state"] = epaperServiceStateToString(snapshot.state);
  if (snapshot.phase == EpaperDrawPhase::None) data["phase"] = nullptr;
  else data["phase"] = epaperDrawPhaseToString(snapshot.phase);
  data["busy"] = snapshot.state != EpaperServiceState::Idle;
  data["can_upload"] = snapshot.canUpload;
  data["can_draw"] = snapshot.canDraw;
  data["can_download"] = snapshot.canDownload;
  if (snapshot.state == EpaperServiceState::Cooldown) {
    data["retry_after_seconds"] = snapshot.retryAfterSeconds;
  } else {
    data["retry_after_seconds"] = nullptr;
  }
  data["cpu_mhz"] = snapshot.cpuMhz;
  data["panel_state"] = epaperPanelStateToString(snapshot.panelState);
  data["shutdown_method"] = snapshot.panelState == EpaperPanelState::Sleeping
                                  ? "power_off_then_deep_sleep"
                                  : (snapshot.panelState == EpaperPanelState::Unknown
                                         ? "logical_only"
                                         : "none");
  data["recovery_required"] = snapshot.recoveryRequired
                                    ? "full_power_cycle"
                                    : nullptr;
  appendStored(data["stored_image"].to<JsonObject>(), snapshot.stored);
  JsonObject last = data["last_operation"].to<JsonObject>();
  last["source"] = snapshot.lastSource;
  last["result"] = snapshot.lastResult;
  last["error_code"] = snapshot.lastErrorCode;
  data["last_reset_reason"] = snapshot.lastResetReason;
  data["brownout_detected"] = snapshot.brownoutDetected;
  data["brownout_during_draw"] = snapshot.brownoutDuringDraw;
  return Api::ok(Api::json(data));
}

Api::Response metadata(const Api::Request &request,
                       const EpaperService &service) {
  Api::Response error;
  if (rejectFields(request, error)) return error;
  const EpaperStoredImageMetadata stored = service.snapshot().stored;
  if (!stored.present) {
    return Api::problem(404, "epaper_image_not_found",
                        "stored e-paper image not found");
  }
  if (!stored.valid) {
    JsonDocument data;
    data["code"] = "invalid_epaper_image";
    data["reason"] = EpaperImageFormat::errorCode(stored.validationError);
    return Api::error(422, Api::json(data), "stored EPDIMG is invalid");
  }
  JsonDocument data;
  data["name"] = EpaperService::kImageName;
  data["format"] = "epdimg";
  data["media_type"] = "application/octet-stream";
  data["size_bytes"] = stored.sizeBytes;
  data["header_bytes"] = stored.header.headerBytes;
  data["frame_bytes"] = stored.header.frameBytes;
  data["width"] = stored.header.width;
  data["height"] = stored.header.height;
  char generation[24]{};
  snprintf(generation, sizeof(generation), "%llu",
           static_cast<unsigned long long>(stored.header.generation));
  char crc[9]{};
  snprintf(crc, sizeof(crc), "%08X",
           static_cast<unsigned>(stored.header.crc32));
  data["generation"] = generation;
  data["crc32"] = crc;
  data["valid"] = true;
  return Api::ok(Api::json(data));
}

Api::Response action(const Api::Request &request,
                     EpaperService &service,
                     EpaperDrawAction action) {
  Api::Response error;
  if (rejectFields(request, error)) return error;
  const EpaperServiceResult result = service.requestDraw(action);
  if (!result.ok()) {
    return fromServiceResult(result, service.snapshot().retryAfterSeconds);
  }
  JsonDocument data;
  data["state"] = "queued";
  return Api::accepted(Api::json(data), "e-paper draw queued");
}

Api::Response fromServiceResult(const EpaperServiceResult &result,
                                uint32_t retryAfterSeconds) {
  switch (result.status) {
    case EpaperServiceStatusCode::Ok:
      return Api::ok("{}");
    case EpaperServiceStatusCode::Busy: {
      JsonDocument data;
      data["code"] = "epaper_busy";
      if (retryAfterSeconds != 0) {
        data["retry_after_seconds"] = retryAfterSeconds;
      }
      return Api::error(409, Api::json(data), result.message);
    }
    case EpaperServiceStatusCode::InvalidImage: {
      JsonDocument data;
      data["code"] = "invalid_epaper_image";
      data["reason"] = result.reason;
      return Api::error(422, Api::json(data), result.message);
    }
    case EpaperServiceStatusCode::ImageNotFound:
      return Api::problem(404, "epaper_image_not_found", result.message);
    case EpaperServiceStatusCode::StorageBusy:
      return Api::problem(409, "storage_busy", result.message);
    case EpaperServiceStatusCode::StorageUnavailable:
      return Api::problem(503, "storage_unavailable", result.message);
    case EpaperServiceStatusCode::Unavailable:
      return Api::problem(503, "epaper_unavailable", result.message);
    case EpaperServiceStatusCode::UploadIncomplete:
      return Api::problem(500, "upload_incomplete", result.message);
    case EpaperServiceStatusCode::StorageError:
      return Api::problem(500, "storage_error", result.message);
  }
  return Api::problem(500, "epaper_unavailable", "e-paper error");
}

Api::Response rawTransportUnsupported() {
  return Api::problem(415, "unsupported_transport",
                      "raw e-paper upload and download require HTTP");
}

}  // namespace EpaperEndpoints
