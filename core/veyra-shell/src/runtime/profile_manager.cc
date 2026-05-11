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

ProfileManager::ProfileManager(std::vector<RuntimeProfile> profiles) : profiles_(std::move(profiles)) {}

const std::vector<RuntimeProfile>& ProfileManager::profiles() const {
  return profiles_;
}

const RuntimeProfile* ProfileManager::FindProfileById(const std::string& persona_id) const {
  for (const RuntimeProfile& profile : profiles_) {
    if (profile.persona.id == persona_id) {
      return &profile;
    }
  }
  return nullptr;
}

const RuntimeProfile* ProfileManager::DefaultProfile() const {
  if (profiles_.empty()) {
    return nullptr;
  }
  return &profiles_.front();
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

  std::vector<RuntimeProfile> runtime_profiles;
  runtime_profiles.reserve(registry.state.personas.size());

  for (const PersonaDefinition& persona : registry.state.personas) {
    const auto mode_it = security_modes.find(persona.security_mode);
    const auto route_it = route_profiles.find(persona.route_profile_id);
    if (mode_it == security_modes.end() || route_it == route_profiles.end()) {
      result.issues.push_back({"profile_manager:" + persona.id,
                               "Unable to resolve persona dependencies during runtime bootstrap."});
      continue;
    }

    runtime_profiles.push_back(RuntimeProfile{
        persona,
        mode_it->second,
        route_it->second,
        SessionPartition::FromPersona(persona),
    });
  }

  if (result.issues.empty()) {
    result.manager = ProfileManager(std::move(runtime_profiles));
  }

  return result;
}

}  // namespace veyra
