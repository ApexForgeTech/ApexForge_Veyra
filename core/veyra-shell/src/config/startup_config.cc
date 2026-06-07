#include "veyra/config/startup_config.h"

#include <filesystem>

namespace veyra {
namespace {

bool FileExists(const std::string& path) {
  return std::filesystem::exists(std::filesystem::path(path));
}

}  // namespace

StartupConfig DefaultStartupConfig() {
  return StartupConfig{
      "schemas/persona/persona.schema.json",
      "schemas/policy/security-mode.schema.json",
      "schemas/route/route-profile.schema.json",
      "schemas/fingerprint/fingerprint-profile.schema.json",
      "schemas/extension/extension-policy.schema.json",
      "schemas/ai/ai-policy.schema.json",
      "schemas/osint/osint-policy.schema.json",
      "schemas/integration/tool.schema.json",
      "schemas/persona/default-personas.json",
      "schemas/policy/default-security-modes.json",
      "schemas/route/default-route-profiles.json",
      "schemas/fingerprint/default-fingerprint-profiles.json",
      "schemas/extension/default-extension-policies.json",
      "schemas/ai/default-ai-policies.json",
      "schemas/osint/default-osint-policies.json",
      "schemas/integration/default-tools.json",
  };
}

std::vector<ValidationIssue> ValidateStartupConfig(const StartupConfig& config) {
  std::vector<ValidationIssue> issues;

  const std::pair<std::string, std::string> required_files[] = {
      {config.persona_schema_path, "Persona schema is missing."},
      {config.security_mode_schema_path, "Security-mode schema is missing."},
      {config.route_profile_schema_path, "Route-profile schema is missing."},
      {config.fingerprint_profile_schema_path, "Fingerprint-profile schema is missing."},
      {config.extension_policy_schema_path, "Extension policy schema is missing."},
      {config.ai_policy_schema_path, "AI policy schema is missing."},
      {config.osint_policy_schema_path, "OSINT policy schema is missing."},
      {config.tool_schema_path, "Tool integration schema is missing."},
      {config.seed_personas_path, "Seed persona definitions are missing."},
      {config.seed_security_modes_path, "Seed security-mode definitions are missing."},
      {config.seed_route_profiles_path, "Seed route-profile definitions are missing."},
      {config.seed_fingerprint_profiles_path, "Seed fingerprint-profile definitions are missing."},
      {config.seed_extension_policies_path, "Seed extension-policy definitions are missing."},
      {config.seed_ai_policies_path, "Seed AI-policy definitions are missing."},
      {config.seed_osint_policies_path, "Seed OSINT-policy definitions are missing."},
      {config.seed_tools_path, "Seed tool definitions are missing."},
  };

  for (const auto& [path, message] : required_files) {
    if (!FileExists(path)) {
      issues.push_back({path, message});
    }
  }

  return issues;
}

}  // namespace veyra
