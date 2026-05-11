#ifndef VEYRA_STARTUP_CONFIG_H_
#define VEYRA_STARTUP_CONFIG_H_

#include <string>
#include <vector>

namespace veyra {

struct StartupConfig {
  std::string persona_schema_path;
  std::string security_mode_schema_path;
  std::string route_profile_schema_path;
  std::string seed_personas_path;
  std::string seed_security_modes_path;
  std::string seed_route_profiles_path;
};

struct ValidationIssue {
  std::string path;
  std::string message;
};

StartupConfig DefaultStartupConfig();
std::vector<ValidationIssue> ValidateStartupConfig(const StartupConfig& config);

}  // namespace veyra

#endif  // VEYRA_STARTUP_CONFIG_H_
