#ifndef VEYRA_CONFIG_STARTUP_CONFIG_H_
#define VEYRA_CONFIG_STARTUP_CONFIG_H_

#include <string>
#include <vector>

namespace veyra {

struct StartupConfig {
  std::string persona_schema_path;
  std::string security_mode_schema_path;
  std::string route_profile_schema_path;
  std::string fingerprint_profile_schema_path;
  std::string extension_policy_schema_path;
  std::string ai_policy_schema_path;
  std::string osint_policy_schema_path;
  std::string tool_schema_path;
  std::string seed_personas_path;
  std::string seed_security_modes_path;
  std::string seed_route_profiles_path;
  std::string seed_fingerprint_profiles_path;
  std::string seed_extension_policies_path;
  std::string seed_ai_policies_path;
  std::string seed_osint_policies_path;
  std::string seed_tools_path;
};

struct ValidationIssue {
  std::string path;
  std::string message;
};

StartupConfig DefaultStartupConfig();
std::vector<ValidationIssue> ValidateStartupConfig(const StartupConfig& config);

}  // namespace veyra

#endif  // VEYRA_CONFIG_STARTUP_CONFIG_H_
