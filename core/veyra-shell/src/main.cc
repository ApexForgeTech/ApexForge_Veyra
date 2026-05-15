#include "veyra/config/startup_config.h"
#include "veyra/runtime/permission_broker.h"
#include "veyra/runtime/browser_window.h"
#include "veyra/runtime/profile_manager.h"

#include <iostream>

namespace {

int RunBootstrap() {
  const veyra::StartupConfig config = veyra::DefaultStartupConfig();
  const veyra::ProfileManagerBootstrapResult profile_manager_bootstrap =
      veyra::BootstrapProfileManager(config);

  std::cout << "Veyra Shell Bootstrap\n";
  std::cout << "Mode: core-foundation\n";
  std::cout << "Browser shell language: C++\n";
  std::cout << "Routing services language: Rust\n";
  std::cout << "Control-plane UI language: TypeScript + React\n";
  std::cout << "\n";

  if (!profile_manager_bootstrap.issues.empty()) {
    std::cerr << "Startup validation failed.\n";
    for (const auto& issue : profile_manager_bootstrap.issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    return 1;
  }

  const veyra::RuntimeProfile* default_profile =
      profile_manager_bootstrap.manager.DefaultProfile();
  if (default_profile == nullptr) {
    std::cerr << "Startup validation failed.\n";
    std::cerr << " - profile_manager: no runtime profiles are available.\n";
    return 1;
  }

  const veyra::BrowserWindow primary_window =
      veyra::BrowserWindow::CreatePrimaryWindow(*default_profile);
  const veyra::TabModel* active_tab = primary_window.ActiveTab();

  std::cout << "Foundation assets loaded.\n";
  std::cout << " - Persona schema: " << config.persona_schema_path << "\n";
  std::cout << " - Security mode schema: " << config.security_mode_schema_path << "\n";
  std::cout << " - Route profile schema: " << config.route_profile_schema_path << "\n";
  std::cout << " - Personas loaded: " << profile_manager_bootstrap.foundation_summary.persona_count
            << "\n";
  std::cout << " - Security modes loaded: "
            << profile_manager_bootstrap.foundation_summary.security_mode_count << "\n";
  std::cout << " - Route profiles loaded: "
            << profile_manager_bootstrap.foundation_summary.route_profile_count << "\n";
  std::cout << "\n";
  std::cout << "Runtime bootstrap summary.\n";
  std::cout << " - Default persona: " << default_profile->persona.display_name << "\n";
  std::cout << " - Session partition: " << default_profile->session_partition.id() << "\n";
  std::cout << " - Route profile: " << default_profile->route_profile.display_name << "\n";
  std::cout << " - Security mode: " << default_profile->security_mode.display_name << "\n";
  std::cout << " - Effective route type: " << default_profile->runtime_policy.route_type << "\n";
  std::cout << " - DNS policy: " << default_profile->runtime_policy.dns_policy << "\n";
  std::cout << " - WebRTC policy: " << default_profile->runtime_policy.webrtc_policy << "\n";
  std::cout << " - Download handling: " << default_profile->runtime_policy.download_policy << "\n";
  std::cout << " - History retention: "
            << default_profile->runtime_policy.history_retention_policy << " (max "
            << default_profile->runtime_policy.max_history_entries << " entries)\n";
  std::cout << " - Primary window id: " << primary_window.id() << "\n";
  std::cout << " - Open tabs: " << primary_window.tabs().size() << "\n";
  if (active_tab != nullptr) {
    std::cout << " - Active tab url: " << active_tab->current_url() << "\n";
    std::cout << " - Active tab history mode: "
              << (active_tab->history_enabled() ? "tracked" : "disabled") << "\n";
  }
  std::cout << "\n";
  std::cout << "Permission broker preview.\n";
  for (const veyra::PermissionEvaluation& evaluation :
       veyra::BuildPermissionReport(*default_profile)) {
    std::cout << " - " << veyra::ToString(evaluation.permission) << ": "
              << veyra::ToString(evaluation.decision) << "\n";
  }
  std::cout << "\n";
  std::cout << "Next milestone: replace bootstrap shell with the real Veyra browser-core integration.\n";

  return 0;
}

}  // namespace

int main() {
  return RunBootstrap();
}
