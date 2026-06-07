#include "veyra/runtime/profile_manager.h"

#include <map>
#include <utility>

namespace veyra {
namespace {

template <typename Definition>
std::map<std::string, Definition> IndexById(const std::vector<Definition>& definitions) {
  std::map<std::string, Definition> indexed;
  for (const Definition& definition : definitions) {
    indexed.emplace(definition.id, definition);
  }
  return indexed;
}

}  // namespace

ProfileManager::ProfileManager() = default;

ProfileManager::ProfileManager(std::vector<RuntimeProfile> profiles,
                               std::vector<RouteProfileDefinition> route_profiles,
                               std::vector<ToolDefinition> tools)
    : profiles_(std::move(profiles)),
      route_profiles_(std::move(route_profiles)),
      tools_(std::move(tools)) {}

const std::vector<RuntimeProfile>& ProfileManager::profiles() const {
  return profiles_;
}

const std::vector<RouteProfileDefinition>& ProfileManager::route_profiles() const {
  return route_profiles_;
}

const std::vector<ToolDefinition>& ProfileManager::tools() const {
  return tools_;
}

const RuntimeProfile* ProfileManager::FindProfileById(const std::string& persona_id) const {
  for (const RuntimeProfile& profile : profiles_) {
    if (profile.persona.id == persona_id) {
      return &profile;
    }
  }
  return nullptr;
}

const RouteProfileDefinition* ProfileManager::FindRouteProfileById(
    const std::string& route_profile_id) const {
  for (const RouteProfileDefinition& route_profile : route_profiles_) {
    if (route_profile.id == route_profile_id) {
      return &route_profile;
    }
  }
  return nullptr;
}

const RuntimeProfile* ProfileManager::ResolveStartupProfile(
    const std::string& requested_persona_id) const {
  if (!requested_persona_id.empty()) {
    return FindProfileById(requested_persona_id);
  }
  return DefaultProfile();
}

const RuntimeProfile* ProfileManager::DefaultProfile() const {
  if (profiles_.empty()) {
    return nullptr;
  }
  return &profiles_.front();
}

std::vector<std::string> ProfileManager::PersonaIds() const {
  std::vector<std::string> persona_ids;
  persona_ids.reserve(profiles_.size());

  for (const RuntimeProfile& profile : profiles_) {
    persona_ids.push_back(profile.persona.id);
  }

  return persona_ids;
}

std::vector<std::string> ProfileManager::RouteProfileIds() const {
  std::vector<std::string> route_profile_ids;
  route_profile_ids.reserve(route_profiles_.size());

  for (const RouteProfileDefinition& route_profile : route_profiles_) {
    route_profile_ids.push_back(route_profile.id);
  }

  return route_profile_ids;
}

ProfileManagerBootstrapResult BootstrapProfileManager(const StartupConfig& config) {
  ProfileManagerBootstrapResult result;
  RegistryBootstrapResult registry = BootstrapProfileRegistry(config);

  result.foundation_summary = registry.summary;
  result.issues = registry.issues;
  if (!result.issues.empty()) {
    return result;
  }

  const std::map<std::string, SecurityModeDefinition> security_modes =
      IndexById(registry.state.security_modes);
  const std::map<std::string, RouteProfileDefinition> route_profiles =
      IndexById(registry.state.route_profiles);
  const std::map<std::string, FingerprintProfileDefinition> fingerprint_profiles =
      IndexById(registry.state.fingerprint_profiles);
  const std::map<std::string, ExtensionPolicyDefinition> extension_policies =
      IndexById(registry.state.extension_policies);
  const std::map<std::string, AiPolicyDefinition> ai_policies =
      IndexById(registry.state.ai_policies);
  const std::map<std::string, OsintPolicyDefinition> osint_policies =
      IndexById(registry.state.osint_policies);

  std::vector<RuntimeProfile> runtime_profiles;
  runtime_profiles.reserve(registry.state.personas.size());

  for (const PersonaDefinition& persona : registry.state.personas) {
    const auto mode_it = security_modes.find(persona.security_mode);
    const auto route_it = route_profiles.find(persona.route_profile_id);
    const auto fingerprint_it = fingerprint_profiles.find(persona.fingerprint_profile_id);
    const auto extension_it = extension_policies.find(persona.extension_policy_id);
    const auto ai_it = ai_policies.find(persona.ai_policy_id);
    const auto osint_it = osint_policies.find(persona.osint_policy_id);
    if (mode_it == security_modes.end() || route_it == route_profiles.end() ||
        fingerprint_it == fingerprint_profiles.end() ||
        extension_it == extension_policies.end() ||
        ai_it == ai_policies.end() ||
        osint_it == osint_policies.end()) {
      result.issues.push_back({"profile_manager:" + persona.id,
                               "Unable to resolve persona dependencies during runtime bootstrap."});
      continue;
    }

    SessionPartition session_partition = SessionPartition::FromPersona(persona);
    RuntimePolicyBuildResult policy_result =
        BuildRuntimePolicy(persona, mode_it->second, route_it->second, session_partition);
    if (!policy_result.issues.empty()) {
      result.issues.insert(result.issues.end(), policy_result.issues.begin(), policy_result.issues.end());
      continue;
    }

    runtime_profiles.push_back(RuntimeProfile{
        persona,
        mode_it->second,
        route_it->second,
        fingerprint_it->second,
        extension_it->second,
        ai_it->second,
        osint_it->second,
        std::move(session_partition),
        std::move(policy_result.policy),
    });
  }

  if (result.issues.empty()) {
    result.manager = ProfileManager(std::move(runtime_profiles),
                                    registry.state.route_profiles,
                                    registry.state.tools);
  }

  return result;
}

}  // namespace veyra
