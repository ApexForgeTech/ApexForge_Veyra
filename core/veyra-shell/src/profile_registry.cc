#include "veyra/profile_registry.h"

#include <set>

namespace veyra {
namespace {

template <typename Item>
void ValidateUniqueIds(const std::vector<Item>& items,
                       const std::string& item_kind,
                       std::vector<ValidationIssue>* issues) {
  std::set<std::string> seen_ids;
  for (const Item& item : items) {
    if (!seen_ids.insert(item.id).second) {
      issues->push_back({item_kind + ":" + item.id, "Duplicate id detected."});
    }
  }
}

template <typename Item>
std::set<std::string> CollectIds(const std::vector<Item>& items) {
  std::set<std::string> ids;
  for (const Item& item : items) {
    ids.insert(item.id);
  }
  return ids;
}

void ValidatePersonaReferences(const FoundationState& state, std::vector<ValidationIssue>* issues) {
  const std::set<std::string> route_ids = CollectIds(state.route_profiles);
  const std::set<std::string> mode_ids = CollectIds(state.security_modes);

  for (const PersonaDefinition& persona : state.personas) {
    if (route_ids.find(persona.route_profile_id) == route_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown route_profile_id '" + persona.route_profile_id + "'."});
    }
    if (mode_ids.find(persona.security_mode) == mode_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown security_mode '" + persona.security_mode + "'."});
    }
  }
}

void ValidateNonEmptyFoundation(const FoundationState& state, std::vector<ValidationIssue>* issues) {
  if (state.personas.empty()) {
    issues->push_back({"foundation:personas", "At least one persona must be defined."});
  }
  if (state.security_modes.empty()) {
    issues->push_back({"foundation:security_modes", "At least one security mode must be defined."});
  }
  if (state.route_profiles.empty()) {
    issues->push_back({"foundation:route_profiles", "At least one route profile must be defined."});
  }
}

}  // namespace

RegistryBootstrapResult BootstrapProfileRegistry(const StartupConfig& config) {
  RegistryBootstrapResult result;
  LoadResult load_result = LoadFoundation(config);

  result.state = std::move(load_result.state);
  result.issues = std::move(load_result.issues);

  if (result.issues.empty()) {
    ValidateNonEmptyFoundation(result.state, &result.issues);
    ValidateUniqueIds(result.state.personas, "persona", &result.issues);
    ValidateUniqueIds(result.state.security_modes, "security_mode", &result.issues);
    ValidateUniqueIds(result.state.route_profiles, "route_profile", &result.issues);
    ValidatePersonaReferences(result.state, &result.issues);
  }

  result.summary.persona_count = result.state.personas.size();
  result.summary.security_mode_count = result.state.security_modes.size();
  result.summary.route_profile_count = result.state.route_profiles.size();

  return result;
}

}  // namespace veyra
