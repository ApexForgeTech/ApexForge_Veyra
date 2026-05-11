#ifndef VEYRA_FOUNDATION_LOADER_H_
#define VEYRA_FOUNDATION_LOADER_H_

#include "veyra/startup_config.h"

#include <string>
#include <vector>

namespace veyra {

struct PersonaDefinition {
  std::string id;
  std::string display_name;
  std::string storage_partition;
  std::string route_profile_id;
  std::string fingerprint_profile_id;
  std::string timezone_profile;
  std::string locale_profile;
  std::string extension_policy_id;
  std::string ai_memory_scope;
  std::string security_mode;
  bool ephemeral = false;
  std::string notes;
};

struct SecurityModeDefinition {
  std::string id;
  std::string display_name;
  std::string javascript_policy;
  std::string cookie_policy;
  std::string storage_policy;
  std::string webrtc_policy;
  std::string permission_policy;
  std::string download_policy;
  std::string routing_requirement;
  std::string history_retention_policy;
  std::string notes;
};

struct RouteProfileDefinition {
  std::string id;
  std::string display_name;
  std::string route_type;
  std::string dns_policy;
  std::string webrtc_policy;
  std::string leak_prevention_level;
  std::vector<std::string> hops;
  std::string notes;
};

struct FoundationState {
  std::vector<PersonaDefinition> personas;
  std::vector<SecurityModeDefinition> security_modes;
  std::vector<RouteProfileDefinition> route_profiles;
};

struct LoadResult {
  FoundationState state;
  std::vector<ValidationIssue> issues;
};

LoadResult LoadFoundation(const StartupConfig& config);

}  // namespace veyra

#endif  // VEYRA_FOUNDATION_LOADER_H_
