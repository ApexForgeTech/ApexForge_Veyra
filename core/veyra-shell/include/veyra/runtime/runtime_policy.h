#ifndef VEYRA_RUNTIME_RUNTIME_POLICY_H_
#define VEYRA_RUNTIME_RUNTIME_POLICY_H_

#include "veyra/config/startup_config.h"
#include "veyra/models/foundation_models.h"
#include "veyra/runtime/session_partition.h"

#include <string>
#include <vector>

namespace veyra {

struct RuntimePolicy {
  std::string javascript_policy;
  std::string cookie_policy;
  std::string storage_policy;
  std::string webrtc_policy;
  std::string permission_policy;
  std::string download_policy;
  std::string routing_requirement;
  std::string history_retention_policy;
  std::string route_type;
  std::string dns_policy;
  std::string leak_prevention_level;
  bool ephemeral_session = false;
  bool persistent_storage_allowed = false;
  bool requires_download_quarantine = false;
  bool history_enabled = true;
  std::size_t max_history_entries = 1;
};

struct RuntimePolicyBuildResult {
  RuntimePolicy policy;
  std::vector<ValidationIssue> issues;
};

RuntimePolicyBuildResult BuildRuntimePolicy(const PersonaDefinition& persona,
                                            const SecurityModeDefinition& security_mode,
                                            const RouteProfileDefinition& route_profile,
                                            const SessionPartition& session_partition);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_RUNTIME_POLICY_H_
