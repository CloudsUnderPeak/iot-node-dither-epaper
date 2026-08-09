#include "RuntimeEndpoints.h"

#include "api/shared/ApiResponse.h"

namespace RuntimeEndpoints {
namespace {

const char *activityFor(EpaperServiceState state) {
  switch (state) {
    case EpaperServiceState::Uploading: return "epaper_upload";
    case EpaperServiceState::Queued:
    case EpaperServiceState::Drawing:
    case EpaperServiceState::Cooldown: return "epaper_draw";
    case EpaperServiceState::Unavailable: return "epaper_recovery";
    case EpaperServiceState::Idle: return "none";
  }
  return "none";
}

void appendBlockedResources(JsonArray resources,
                            const EpaperServiceSnapshot &snapshot) {
  if (snapshot.state == EpaperServiceState::Idle) return;
  resources.add("epaper");
  if (snapshot.state == EpaperServiceState::Uploading ||
      snapshot.phase == EpaperDrawPhase::Transferring) {
    resources.add("userdata");
  }
  if (snapshot.phase == EpaperDrawPhase::Initializing ||
      snapshot.phase == EpaperDrawPhase::Transferring ||
      snapshot.phase == EpaperDrawPhase::PoweringOff ||
      snapshot.phase == EpaperDrawPhase::Quiescing) {
    resources.add("spi");
  }
}

}  // namespace

Api::Response status(const Api::Request &request,
                     const EpaperService &epaper,
                     RuntimeActionScheduler &runtime) {
  if (request.hasBody || request.hasJsonBody || request.queryCount != 0 ||
      request.queryOverflow) {
    return Api::problem(400, "unsupported_field",
                        "runtime status does not accept fields or a body");
  }
  const EpaperServiceSnapshot epaperSnapshot = epaper.snapshot();
  const RuntimeActionSnapshot runtimeSnapshot = runtime.snapshot();
  const bool epaperBusy = epaperSnapshot.state != EpaperServiceState::Idle;
  JsonDocument data;
  data["state"] = runtimeSnapshot.restartPending
                      ? "restarting"
                      : (epaperBusy
                             ? "degraded"
                             : "normal");
  data["busy"] = runtimeSnapshot.restartPending || epaperBusy;
  data["activity"] = runtimeSnapshot.restartPending
                         ? "system_restart"
                         : activityFor(epaperSnapshot.state);
  if (runtimeSnapshot.restartPending) {
    data["phase"] = "epaper_shutdown";
  } else if (epaperSnapshot.phase != EpaperDrawPhase::None) {
    data["phase"] = epaperDrawPhaseToString(epaperSnapshot.phase);
  } else if (epaperSnapshot.state == EpaperServiceState::Cooldown) {
    data["phase"] = "cooldown";
  } else {
    data["phase"] = nullptr;
  }
  data["cpu_mhz"] = epaperSnapshot.cpuMhz;
  data["normal_cpu_mhz"] = 160;
  appendBlockedResources(data["blocked_resources"].to<JsonArray>(),
                         epaperSnapshot);
  data["last_reset_reason"] = epaperSnapshot.lastResetReason;
  data["brownout_detected"] = epaperSnapshot.brownoutDetected;
  data["last_error_code"] = runtimeSnapshot.restartFailed
                                ? "epaper_shutdown_failed"
                                : nullptr;
  return Api::ok(Api::json(data));
}

}  // namespace RuntimeEndpoints
