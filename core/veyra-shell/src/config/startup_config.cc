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
      "schemas/persona/default-personas.json",
      "schemas/policy/default-security-modes.json",
      "schemas/route/default-route-profiles.json",
  };
}

std::vector<ValidationIssue> ValidateStartupConfig(const StartupConfig& config) {
  std::vector<ValidationIssue> issues;

  const std::pair<std::string, std::string> required_files[] = {
      {config.persona_schema_path, "Persona schema is missing."},
      {config.security_mode_schema_path, "Security-mode schema is missing."},
      {config.route_profile_schema_path, "Route-profile schema is missing."},
      {config.seed_personas_path, "Seed persona definitions are missing."},
      {config.seed_security_modes_path, "Seed security-mode definitions are missing."},
      {config.seed_route_profiles_path, "Seed route-profile definitions are missing."},
  };

  for (const auto& [path, message] : required_files) {
    if (!FileExists(path)) {
      issues.push_back({path, message});
    }
  }

  return issues;
}

}  // namespace veyra
