#include "veyra/startup_config.h"

#include <iostream>

namespace {

int RunBootstrap() {
  const veyra::StartupConfig config = veyra::DefaultStartupConfig();
  const std::vector<veyra::ValidationIssue> issues = veyra::ValidateStartupConfig(config);

  std::cout << "Veyra Shell Bootstrap\n";
  std::cout << "Mode: core-foundation\n";
  std::cout << "Browser shell language: C++\n";
  std::cout << "Routing services language: Rust\n";
  std::cout << "Control-plane UI language: TypeScript + React\n";
  std::cout << "\n";

  if (!issues.empty()) {
    std::cerr << "Startup validation failed.\n";
    for (const auto& issue : issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    return 1;
  }

  std::cout << "Foundation assets detected.\n";
  std::cout << " - Persona schema: " << config.persona_schema_path << "\n";
  std::cout << " - Security mode schema: " << config.security_mode_schema_path << "\n";
  std::cout << " - Route profile schema: " << config.route_profile_schema_path << "\n";
  std::cout << "\n";
  std::cout << "Next milestone: replace bootstrap shell with the real Veyra browser-core integration.\n";

  return 0;
}

}  // namespace

int main() {
  return RunBootstrap();
}
