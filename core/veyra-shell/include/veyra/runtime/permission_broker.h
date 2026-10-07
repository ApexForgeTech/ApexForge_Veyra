#ifndef VEYRA_RUNTIME_PERMISSION_BROKER_H_
#define VEYRA_RUNTIME_PERMISSION_BROKER_H_

#include "veyra/runtime/profile_manager.h"

#include <string>
#include <vector>

namespace veyra {

enum class PermissionKind {
  kNotifications,
  kFullscreen,
  kClipboard,
  kGeolocation,
  kCamera,
  kMicrophone,
  kUsb,
  kFilesystem,
};

enum class PermissionDecision {
  kAllow,
  kPrompt,
  kDeny,
};

struct PermissionEvaluation {
  PermissionKind permission;
  PermissionDecision decision;
  std::string rationale;
};

const char* ToString(PermissionKind permission);
const char* ToString(PermissionDecision decision);

PermissionEvaluation EvaluatePermission(const RuntimeProfile& profile, PermissionKind permission);
std::vector<PermissionEvaluation> BuildPermissionReport(const RuntimeProfile& profile);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_PERMISSION_BROKER_H_
