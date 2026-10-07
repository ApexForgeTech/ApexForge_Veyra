#include "veyra/runtime/runtime_policy.h"

#include <algorithm>

namespace veyra {
namespace {

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::size_t ResolveMaxHistoryEntries(const std::string& history_retention_policy) {
  if (history_retention_policy == "none") {
    return 1;
  }
  if (history_retention_policy == "reduced") {
    return 5;
  }
  return 50;
}

bool HistoryIsEnabled(const std::string& history_retention_policy) {
  return history_retention_policy != "none";
}

bool RequiresDownloadQuarantine(const std::string& download_policy) {
  return download_policy == "quarantine" || download_policy == "airgap_quarantine";
}

bool PersistentStorageAllowed(const std::string& storage_policy) {
  return storage_policy == "persistent" || storage_policy == "scoped_persistent";
}

void ValidatePersonaPartitionConsistency(const PersonaDefinition& persona,
                                         const SessionPartition& session_partition,
                                         std::vector<ValidationIssue>* issues) {
  if (persona.ephemeral && session_partition.is_persistent()) {
    issues->push_back({"persona:" + persona.id,
                       "Ephemeral persona resolved to a persistent session partition."});
  }

  if (!persona.ephemeral && !session_partition.is_persistent()) {
    issues->push_back({"persona:" + persona.id,
                       "Persistent persona resolved to an in-memory session partition."});
  }

  if (persona.ephemeral && StartsWith(persona.storage_partition, "persist:")) {
    issues->push_back({"persona:" + persona.id,
                       "Ephemeral personas must not declare a persistent storage partition."});
  }
}

void ValidateStoragePolicy(const PersonaDefinition& persona,
                           const SecurityModeDefinition& security_mode,
                           const SessionPartition& session_partition,
                           std::vector<ValidationIssue>* issues) {
  if (security_mode.storage_policy == "memory_only" && session_partition.is_persistent()) {
    issues->push_back({"security_mode:" + security_mode.id,
                       "memory_only storage policy cannot run on a persistent session partition."});
  }

  if (security_mode.storage_policy != "memory_only" && !session_partition.is_persistent() &&
      !persona.ephemeral) {
    issues->push_back({"persona:" + persona.id,
                       "Persistent storage mode requires a persistent session partition."});
  }

  if (security_mode.storage_policy != "memory_only" && persona.ephemeral) {
    issues->push_back({"persona:" + persona.id,
                       "Ephemeral personas must use a memory_only storage policy."});
  }
}

void ValidateRoutingPolicy(const SecurityModeDefinition& security_mode,
                           const RouteProfileDefinition& route_profile,
                           std::vector<ValidationIssue>* issues) {
  if (security_mode.routing_requirement == "isolated" && route_profile.dns_policy == "system") {
    issues->push_back({"route_profile:" + route_profile.id,
                       "Isolated routing requirements cannot rely on the system DNS policy."});
  }

  if (security_mode.routing_requirement == "multi_hop_required" && route_profile.hops.size() < 3) {
    issues->push_back({"route_profile:" + route_profile.id,
                       "multi_hop_required routing needs at least three declared route hops."});
  }
}

}  // namespace

RuntimePolicyBuildResult BuildRuntimePolicy(const PersonaDefinition& persona,
                                            const SecurityModeDefinition& security_mode,
                                            const RouteProfileDefinition& route_profile,
                                            const SessionPartition& session_partition) {
  RuntimePolicyBuildResult result;

  ValidatePersonaPartitionConsistency(persona, session_partition, &result.issues);
  ValidateStoragePolicy(persona, security_mode, session_partition, &result.issues);
  ValidateRoutingPolicy(security_mode, route_profile, &result.issues);

  result.policy.javascript_policy = security_mode.javascript_policy;
  result.policy.cookie_policy = security_mode.cookie_policy;
  result.policy.storage_policy = security_mode.storage_policy;
  result.policy.webrtc_policy =
      security_mode.webrtc_policy == "disable" ? "disable" : route_profile.webrtc_policy;
  result.policy.permission_policy = security_mode.permission_policy;
  result.policy.download_policy = security_mode.download_policy;
  result.policy.routing_requirement = security_mode.routing_requirement;
  result.policy.history_retention_policy = security_mode.history_retention_policy;
  result.policy.route_type = route_profile.route_type;
  result.policy.dns_policy = route_profile.dns_policy;
  result.policy.leak_prevention_level = route_profile.leak_prevention_level;
  result.policy.ephemeral_session = persona.ephemeral;
  result.policy.persistent_storage_allowed = PersistentStorageAllowed(security_mode.storage_policy);
  result.policy.requires_download_quarantine =
      RequiresDownloadQuarantine(security_mode.download_policy);
  result.policy.history_enabled = HistoryIsEnabled(security_mode.history_retention_policy);
  result.policy.max_history_entries =
      std::max<std::size_t>(1, ResolveMaxHistoryEntries(security_mode.history_retention_policy));

  return result;
}

}  // namespace veyra
