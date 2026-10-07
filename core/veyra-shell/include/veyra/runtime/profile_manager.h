#ifndef VEYRA_RUNTIME_PROFILE_MANAGER_H_
#define VEYRA_RUNTIME_PROFILE_MANAGER_H_

#include "veyra/config/startup_config.h"
#include "veyra/models/foundation_models.h"
#include "veyra/runtime/profile_registry.h"
#include "veyra/runtime/runtime_policy.h"
#include "veyra/runtime/session_partition.h"

#include <string>
#include <vector>

namespace veyra {

struct RuntimeProfile {
  PersonaDefinition persona;
  SecurityModeDefinition security_mode;
  RouteProfileDefinition route_profile;
  FingerprintProfileDefinition fingerprint_profile;
  ExtensionPolicyDefinition extension_policy;
  AiPolicyDefinition ai_policy;
  OsintPolicyDefinition osint_policy;
  SessionPartition session_partition;
  RuntimePolicy runtime_policy;
};

class ProfileManager {
 public:
  ProfileManager();
  explicit ProfileManager(std::vector<RuntimeProfile> profiles,
                          std::vector<RouteProfileDefinition> route_profiles = {},
                          std::vector<ToolDefinition> tools = {});

  const std::vector<RuntimeProfile>& profiles() const;
  const std::vector<RouteProfileDefinition>& route_profiles() const;
  const std::vector<ToolDefinition>& tools() const;
  const RuntimeProfile* FindProfileById(const std::string& persona_id) const;
  const RouteProfileDefinition* FindRouteProfileById(const std::string& route_profile_id) const;
  const RuntimeProfile* ResolveStartupProfile(const std::string& requested_persona_id) const;
  const RuntimeProfile* DefaultProfile() const;
  std::vector<std::string> PersonaIds() const;
  std::vector<std::string> RouteProfileIds() const;

 private:
  std::vector<RuntimeProfile> profiles_;
  std::vector<RouteProfileDefinition> route_profiles_;
  std::vector<ToolDefinition> tools_;
};

struct ProfileManagerBootstrapResult {
  ProfileManager manager;
  RegistrySummary foundation_summary;
  std::vector<ValidationIssue> issues;
};

ProfileManagerBootstrapResult BootstrapProfileManager(const StartupConfig& config);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_PROFILE_MANAGER_H_
