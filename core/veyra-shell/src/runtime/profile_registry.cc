#include "veyra/runtime/profile_registry.h"

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
  const std::set<std::string> fingerprint_ids = CollectIds(state.fingerprint_profiles);
  const std::set<std::string> extension_ids = CollectIds(state.extension_policies);
  const std::set<std::string> ai_ids = CollectIds(state.ai_policies);
  const std::set<std::string> osint_ids = CollectIds(state.osint_policies);

  for (const PersonaDefinition& persona : state.personas) {
    if (route_ids.find(persona.route_profile_id) == route_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown route_profile_id '" + persona.route_profile_id + "'."});
    }
    if (mode_ids.find(persona.security_mode) == mode_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown security_mode '" + persona.security_mode + "'."});
    }
    if (fingerprint_ids.find(persona.fingerprint_profile_id) == fingerprint_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown fingerprint_profile_id '" + persona.fingerprint_profile_id +
                             "'."});
    }
    if (extension_ids.find(persona.extension_policy_id) == extension_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown extension_policy_id '" + persona.extension_policy_id + "'."});
    }
    if (ai_ids.find(persona.ai_policy_id) == ai_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown ai_policy_id '" + persona.ai_policy_id + "'."});
    }
    if (osint_ids.find(persona.osint_policy_id) == osint_ids.end()) {
      issues->push_back({"persona:" + persona.id,
                         "Unknown osint_policy_id '" + persona.osint_policy_id + "'."});
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
  if (state.fingerprint_profiles.empty()) {
    issues->push_back({"foundation:fingerprint_profiles",
                       "At least one fingerprint profile must be defined."});
  }
  if (state.ai_policies.empty()) {
    issues->push_back({"foundation:ai_policies",
                       "At least one AI policy must be defined."});
  }
  if (state.osint_policies.empty()) {
    issues->push_back({"foundation:osint_policies",
                       "At least one OSINT policy must be defined."});
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
    ValidateUniqueIds(result.state.fingerprint_profiles, "fingerprint_profile", &result.issues);
    ValidateUniqueIds(result.state.extension_policies, "extension_policy", &result.issues);
    ValidateUniqueIds(result.state.ai_policies, "ai_policy", &result.issues);
    ValidateUniqueIds(result.state.osint_policies, "osint_policy", &result.issues);
    ValidateUniqueIds(result.state.tools, "tool", &result.issues);
    ValidatePersonaReferences(result.state, &result.issues);
  }

  result.summary.persona_count = result.state.personas.size();
  result.summary.security_mode_count = result.state.security_modes.size();
  result.summary.route_profile_count = result.state.route_profiles.size();
  result.summary.fingerprint_profile_count = result.state.fingerprint_profiles.size();
  result.summary.extension_policy_count = result.state.extension_policies.size();
  result.summary.ai_policy_count = result.state.ai_policies.size();
  result.summary.osint_policy_count = result.state.osint_policies.size();
  result.summary.tool_count = result.state.tools.size();

  return result;
}

}  // namespace veyra
