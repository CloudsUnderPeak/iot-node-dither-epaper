#include "EpaperEndpoints.h"

#include <cstdio>

#include "api/shared/ApiResponse.h"
#include "api/shared/JsonReader.h"

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

String calibrationJson(const EpaperCalibrationSnapshot &snapshot) {
  JsonDocument data;
  data["schema_version"] = snapshot.profile.schemaVersion;
  data["source"] = epaperCalibrationSourceToString(snapshot.source);
  data["recovery_reason"] =
      epaperCalibrationRecoveryReasonToString(snapshot.recoveryReason);
  JsonArray colors = data["colors"].to<JsonArray>();
  for (size_t index = 0; index < EpaperCalibration::kColorCount; ++index) {
    const EpaperCalibration::ColorDefinition &definition =
        EpaperCalibration::definition(index);
    const EpaperCalibration::Rgb &display = snapshot.profile.display[index];
    JsonObject color = colors.add<JsonObject>();
    color["id"] = definition.id;
    color["code"] = definition.code;
    JsonObject displayObject = color["display"].to<JsonObject>();
    displayObject["r"] = display.r;
    displayObject["g"] = display.g;
    displayObject["b"] = display.b;
  }
  return Api::json(data);
}

Api::Response calibrationResult(const Result &result,
                                const EpaperCalibrationSnapshot &snapshot,
                                const char *message) {
  if (result.ok()) return Api::ok(calibrationJson(snapshot), message);
  if (result.code == ResultCode::InvalidInput) {
    return Api::problem(400, "invalid_field", result.message, "colors");
  }
  if (result.code == ResultCode::Unsupported) {
    return Api::problem(409, "unsupported_schema", result.message);
  }
  return Api::problem(500, "storage_error", result.message);
}

bool decodeCalibration(const Api::Request &request,
                       EpaperCalibration::Profile &profile,
                       Api::Response &errorResponse) {
  if (request.queryCount != 0 || request.queryOverflow) {
    errorResponse = Api::problem(400, "unsupported_field",
                                 "calibration update does not accept query fields");
    return false;
  }
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) {
    errorResponse = bodyResult;
    return false;
  }

  JsonDecodeError error;
  JsonReader rootReader(root, "", error);
  JsonReader colorsReader = rootReader.requiredObject(
      "colors", "colors object is required");
  rootReader.finish({"colors"});
  profile = EpaperCalibration::defaultProfile();
  for (size_t index = 0; index < EpaperCalibration::kColorCount; ++index) {
    const char *id = EpaperCalibration::definition(index).id;
    JsonReader colorReader = colorsReader.requiredObject(
        id, "color object is required");
    const uint32_t r = colorReader.requiredUint32("r", "r is required");
    const uint32_t g = colorReader.requiredUint32("g", "g is required");
    const uint32_t b = colorReader.requiredUint32("b", "b is required");
    colorReader.finish({"r", "g", "b"});
    if (error.ok() && (r > 255 || g > 255 || b > 255)) {
      error.code = "invalid_field";
      error.field = "colors.";
      error.field += id;
      error.message = "RGB channels must be integers from 0 to 255";
    }
    profile.display[index] = {
        static_cast<uint8_t>(r),
        static_cast<uint8_t>(g),
        static_cast<uint8_t>(b),
    };
  }
  colorsReader.finish({"black", "white", "yellow", "red", "blue", "green"});
  if (!error.ok()) {
    errorResponse = Api::decodeError(error);
    return false;
  }
  const Result validation = EpaperCalibration::validate(profile);
  if (!validation.ok()) {
    errorResponse = Api::problem(400, "invalid_field", validation.message, "colors");
    return false;
  }
  return true;
}

}  // namespace

Api::Response capabilities(const Api::Request &request) {
  Api::Response error;
  if (rejectFields(request, error)) return error;
  JsonDocument data;
  JsonObject panel = data["panel"].to<JsonObject>();
  panel["model"] = EpaperPanelProfile::kModel;
  panel["flip_horizontal"] = EpaperPanelProfile::kFlipHorizontal;
  panel["flip_vertical"] = EpaperPanelProfile::kFlipVertical;
  panel["width"] = EpaperImageFormat::kWidth;
  panel["height"] = EpaperImageFormat::kHeight;
  panel["colors"] = EpaperImageFormat::kPaletteColorCount;
  JsonArray codes = panel["color_codes"].to<JsonArray>();
  for (uint8_t code : EpaperImageFormat::kPaletteCodes) codes.add(code);
  JsonObject image = data["image"].to<JsonObject>();
  image["name"] = EpaperService::kImageName;
  image["format"] = "epdimg";
  image["header_bytes"] = EpaperImageFormat::kHeaderBytes;
  image["frame_bytes"] = EpaperImageFormat::kFrameBytes;
  image["upload_bytes"] = EpaperImageFormat::kImageBytes;
  image["upload_uncompressed_bytes"] = EpaperImageFormat::kImageBytes;
  image["max_compressed_bytes"] = EpaperService::kMaxCompressedBytes;
  image["stored_encoding"] = "gzip";
  image["upload_encodings"].to<JsonArray>().add("gzip");
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
  data["stored_size_bytes"] = stored.storedSizeBytes;
  data["stored_encoding"] = "gzip";
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

Api::Response calibration(const Api::Request &request,
                          const EpaperCalibrationService &service) {
  Api::Response error;
  if (rejectFields(request, error)) return error;
  const EpaperCalibrationSnapshot snapshot = service.snapshot();
  if (!snapshot.ready) {
    return Api::problem(503, "calibration_unavailable",
                        "e-paper calibration service unavailable");
  }
  return Api::ok(calibrationJson(snapshot));
}

Api::Response updateCalibration(const Api::Request &request,
                                EpaperCalibrationService &service) {
  EpaperCalibration::Profile profile;
  Api::Response error;
  if (!decodeCalibration(request, profile, error)) return error;
  EpaperCalibrationSnapshot updated;
  const Result result = service.update(profile, &updated);
  return calibrationResult(result, updated, "e-paper calibration updated");
}

Api::Response resetCalibration(const Api::Request &request,
                               EpaperCalibrationService &service) {
  Api::Response error;
  if (rejectFields(request, error)) return error;
  EpaperCalibrationSnapshot updated;
  const Result result = service.reset(&updated);
  return calibrationResult(result, updated, "e-paper calibration reset");
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
    case EpaperServiceStatusCode::PayloadTooLarge:
      return Api::problem(413, "payload_too_large", result.message);
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
