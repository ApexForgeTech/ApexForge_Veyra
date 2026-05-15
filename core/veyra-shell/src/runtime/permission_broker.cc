#include "veyra/runtime/permission_broker.h"

#include <array>

namespace veyra {
namespace {

PermissionDecision DecideByPermissionPolicy(const std::string& permission_policy,
                                            PermissionKind permission) {
  if (permission_policy == "prompt") {
    switch (permission) {
      case PermissionKind::kNotifications:
      case PermissionKind::kFullscreen:
        return PermissionDecision::kAllow;
      case PermissionKind::kUsb:
      case PermissionKind::kFilesystem:
        return PermissionDecision::kDeny;
      default:
        return PermissionDecision::kPrompt;
    }
  }

  if (permission_policy == "strict_prompt") {
    switch (permission) {
      case PermissionKind::kFullscreen:
      case PermissionKind::kNotifications:
      case PermissionKind::kClipboard:
      case PermissionKind::kGeolocation:
        return PermissionDecision::kPrompt;
      default:
        return PermissionDecision::kDeny;
    }
  }

  if (permission == PermissionKind::kFullscreen) {
    return PermissionDecision::kPrompt;
  }
  return PermissionDecision::kDeny;
}

const char* PermissionRationale(PermissionDecision decision) {
  switch (decision) {
    case PermissionDecision::kAllow:
      return "Allowed by the active permission posture.";
    case PermissionDecision::kPrompt:
      return "Requires an explicit Veyra policy prompt.";
    case PermissionDecision::kDeny:
      return "Denied by the active security posture.";
  }
  return "Unknown decision.";
}

bool IsWebrtcBoundPermission(PermissionKind permission) {
  return permission == PermissionKind::kCamera || permission == PermissionKind::kMicrophone;
}

std::array<PermissionKind, 8> AllPermissions() {
  return {PermissionKind::kNotifications, PermissionKind::kFullscreen, PermissionKind::kClipboard,
          PermissionKind::kGeolocation, PermissionKind::kCamera, PermissionKind::kMicrophone,
          PermissionKind::kUsb, PermissionKind::kFilesystem};
}

}  // namespace

const char* ToString(PermissionKind permission) {
  switch (permission) {
    case PermissionKind::kNotifications:
      return "notifications";
    case PermissionKind::kFullscreen:
      return "fullscreen";
    case PermissionKind::kClipboard:
      return "clipboard";
    case PermissionKind::kGeolocation:
      return "geolocation";
    case PermissionKind::kCamera:
      return "camera";
    case PermissionKind::kMicrophone:
      return "microphone";
    case PermissionKind::kUsb:
      return "usb";
    case PermissionKind::kFilesystem:
      return "filesystem";
  }
  return "unknown";
}

const char* ToString(PermissionDecision decision) {
  switch (decision) {
    case PermissionDecision::kAllow:
      return "allow";
    case PermissionDecision::kPrompt:
      return "prompt";
    case PermissionDecision::kDeny:
      return "deny";
  }
  return "unknown";
}

PermissionEvaluation EvaluatePermission(const RuntimeProfile& profile, PermissionKind permission) {
  if (IsWebrtcBoundPermission(permission) && profile.runtime_policy.webrtc_policy == "disable") {
    return PermissionEvaluation{
        permission,
        PermissionDecision::kDeny,
        "Denied because WebRTC-bound capture is disabled for this persona.",
    };
  }

  if (permission == PermissionKind::kFilesystem && profile.runtime_policy.ephemeral_session) {
    return PermissionEvaluation{
        permission,
        PermissionDecision::kDeny,
        "Denied because ephemeral sessions cannot expose direct filesystem access.",
    };
  }

  const PermissionDecision decision =
      DecideByPermissionPolicy(profile.runtime_policy.permission_policy, permission);
  return PermissionEvaluation{permission, decision, PermissionRationale(decision)};
}

std::vector<PermissionEvaluation> BuildPermissionReport(const RuntimeProfile& profile) {
  std::vector<PermissionEvaluation> report;
  report.reserve(AllPermissions().size());

  for (const PermissionKind permission : AllPermissions()) {
    report.push_back(EvaluatePermission(profile, permission));
  }

  return report;
}

}  // namespace veyra
