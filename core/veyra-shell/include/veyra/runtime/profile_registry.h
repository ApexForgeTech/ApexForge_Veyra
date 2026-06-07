#ifndef VEYRA_RUNTIME_PROFILE_REGISTRY_H_
#define VEYRA_RUNTIME_PROFILE_REGISTRY_H_

#include "veyra/config/foundation_loader.h"
#include "veyra/config/startup_config.h"

#include <cstddef>
#include <string>
#include <vector>

namespace veyra {

struct RegistrySummary {
  std::size_t persona_count = 0;
  std::size_t security_mode_count = 0;
  std::size_t route_profile_count = 0;
  std::size_t fingerprint_profile_count = 0;
  std::size_t extension_policy_count = 0;
  std::size_t ai_policy_count = 0;
  std::size_t osint_policy_count = 0;
  std::size_t tool_count = 0;
};

struct RegistryBootstrapResult {
  FoundationState state;
  RegistrySummary summary;
  std::vector<ValidationIssue> issues;
};

RegistryBootstrapResult BootstrapProfileRegistry(const StartupConfig& config);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_PROFILE_REGISTRY_H_
