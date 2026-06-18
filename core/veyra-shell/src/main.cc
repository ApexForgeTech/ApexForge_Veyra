#include "veyra/config/startup_config.h"
#include "veyra/engine/browser_engine.h"
#include "veyra/runtime/browser_window.h"
#include "veyra/runtime/permission_broker.h"
#include "veyra/runtime/profile_manager.h"
#include "veyra/runtime/route_service.h"
#include "veyra/runtime/router_supervisor.h"
#include "veyra/runtime/security_policy_engine.h"
#include "veyra/runtime/session_lifecycle.h"
#include "veyra/runtime/ai_orchestrator_client.h"
#include "veyra/runtime/osint_workspace_client.h"
#include "veyra/runtime/shell_ui_bridge.h"
#include "veyra/runtime/tool_bridge_client.h"
#include "veyra/serialization/json.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct LaunchOptions {
  bool smoke_mode = false;
  bool keep_ephemeral_partitions = false;
  bool list_personas = false;
  bool list_route_profiles = false;
  bool list_resolved_routes = false;
  bool list_tools = false;
  std::string startup_persona_id;
  std::string startup_route_profile_id;
  std::string hot_route_after_start_profile_id;
  std::string startup_url = "veyra:start";
  std::string runtime_root = ".veyra/runtime_sessions";
  std::string route_engine_binary_path;
  std::string artifact_scan_binary_path;
  std::string tool_bridge_binary_path;
  std::string invoke_tool_spec;
  std::string shell_ui_dist_path;
  std::string ai_orchestrator_script_path;
  std::string ai_python_binary = "python3";
  std::string osint_workspace_script_path;
  std::string control_fifo_path;
  std::string extensions_dir = "extensions";
  std::vector<std::string> route_override_specs;
  std::vector<std::string> additional_tab_urls;
};

struct RouteOverrideRecord {
  std::string persona_id;
  std::string route_profile_id;
  std::string source;
};

struct RouteOverrideEvent {
  std::string persona_id;
  std::string previous_route_profile_id;
  std::string next_route_profile_id;
  std::string source;
  veyra::RouteRuntimeState route_state;
};

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string JsonEscape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }
  return escaped;
}

// Scans an extensions directory for enabled extensions and returns their
// content-script JS bodies (to be injected into every browsing tab).
// Each extension is a subdirectory containing manifest.json.
std::vector<std::string> LoadExtensionContentScripts(const std::string& extensions_dir) {
  std::vector<std::string> scripts;
  std::error_code ec;
  if (extensions_dir.empty() || !std::filesystem::is_directory(extensions_dir, ec)) {
    return scripts;
  }

  for (const auto& entry : std::filesystem::directory_iterator(extensions_dir, ec)) {
    if (ec || !entry.is_directory()) {
      continue;
    }
    const std::filesystem::path manifest_path = entry.path() / "manifest.json";
    std::ifstream manifest_in(manifest_path);
    if (!manifest_in.is_open()) {
      continue;
    }
    std::ostringstream manifest_buf;
    manifest_buf << manifest_in.rdbuf();
    const veyra::JsonParseResult parsed = veyra::ParseJson(manifest_buf.str());
    if (!parsed.error.empty() || !parsed.value.IsObject()) {
      std::cerr << "[extensions] skipping malformed manifest: " << manifest_path << "\n";
      continue;
    }
    const auto& obj = parsed.value.AsObject();

    // Honour default_enabled (default true when absent).
    const auto enabled_it = obj.find("default_enabled");
    if (enabled_it != obj.end() && enabled_it->second.IsBool() &&
        !enabled_it->second.AsBool()) {
      continue;
    }

    const auto name_it = obj.find("name");
    const std::string ext_name =
        name_it != obj.end() && name_it->second.IsString() ? name_it->second.AsString()
                                                           : entry.path().filename().string();

    const auto cs_it = obj.find("content_scripts");
    if (cs_it == obj.end() || !cs_it->second.IsArray()) {
      continue;
    }
    std::size_t loaded_files = 0;
    for (const veyra::JsonValue& cs : cs_it->second.AsArray()) {
      if (!cs.IsObject()) continue;
      const auto js_it = cs.AsObject().find("js");
      if (js_it == cs.AsObject().end() || !js_it->second.IsArray()) continue;
      for (const veyra::JsonValue& js_name : js_it->second.AsArray()) {
        if (!js_name.IsString()) continue;
        const std::filesystem::path js_path = entry.path() / js_name.AsString();
        std::ifstream js_in(js_path);
        if (!js_in.is_open()) {
          std::cerr << "[extensions] missing content script: " << js_path << "\n";
          continue;
        }
        std::ostringstream js_buf;
        js_buf << js_in.rdbuf();
        scripts.push_back(js_buf.str());
        ++loaded_files;
      }
    }
    std::cout << "[extensions] loaded '" << ext_name << "' (" << loaded_files
              << " content script(s))\n";
  }
  return scripts;
}

std::string PersonaListString(const veyra::ProfileManager& manager) {
  std::ostringstream output;
  const std::vector<std::string> persona_ids = manager.PersonaIds();
  for (std::size_t index = 0; index < persona_ids.size(); ++index) {
    output << persona_ids[index];
    if (index + 1 < persona_ids.size()) {
      output << ", ";
    }
  }
  return output.str();
}

std::string RouteProfileListString(const veyra::ProfileManager& manager) {
  std::ostringstream output;
  const std::vector<veyra::RouteProfileDefinition>& route_profiles = manager.route_profiles();
  for (std::size_t index = 0; index < route_profiles.size(); ++index) {
    output << route_profiles[index].id << " (" << route_profiles[index].display_name << ")";
    if (index + 1 < route_profiles.size()) {
      output << ", ";
    }
  }
  return output.str();
}

bool ParseRouteOverrideSpec(const std::string& spec,
                            RouteOverrideRecord* record,
                            std::string* error) {
  const std::size_t separator = spec.find(':');
  if (separator == std::string::npos || separator == 0 || separator + 1 >= spec.size()) {
    if (error != nullptr) {
      *error = "Route override must use persona_id:route_profile_id format.";
    }
    return false;
  }

  if (record != nullptr) {
    record->persona_id = spec.substr(0, separator);
    record->route_profile_id = spec.substr(separator + 1);
  }
  return true;
}

std::vector<RouteOverrideRecord> LoadRouteOverrides(const std::string& path,
                                                    std::string* error) {
  std::vector<RouteOverrideRecord> overrides;
  if (!std::filesystem::exists(path)) {
    return overrides;
  }

  std::ifstream input(path);
  if (!input.is_open()) {
    if (error != nullptr) {
      *error = "Failed to open route override store: " + path;
    }
    return overrides;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  const veyra::JsonParseResult parsed = veyra::ParseJson(buffer.str());
  if (!parsed.error.empty()) {
    if (error != nullptr) {
      *error = "Failed to parse route override store: " + parsed.error;
    }
    return {};
  }

  if (!parsed.value.IsObject()) {
    if (error != nullptr) {
      *error = "Route override store root must be a JSON object.";
    }
    return {};
  }

  const auto root_it = parsed.value.AsObject().find("overrides");
  if (root_it == parsed.value.AsObject().end()) {
    return overrides;
  }
  if (!root_it->second.IsArray()) {
    if (error != nullptr) {
      *error = "Route override store overrides field must be an array.";
    }
    return {};
  }

  for (const veyra::JsonValue& entry : root_it->second.AsArray()) {
    if (!entry.IsObject()) {
      continue;
    }
    const auto& object = entry.AsObject();
    const auto persona_it = object.find("persona_id");
    const auto route_it = object.find("route_profile_id");
    if (persona_it == object.end() || route_it == object.end() ||
        !persona_it->second.IsString() || !route_it->second.IsString()) {
      continue;
    }

    RouteOverrideRecord record;
    record.persona_id = persona_it->second.AsString();
    record.route_profile_id = route_it->second.AsString();
    const auto source_it = object.find("source");
    record.source = source_it != object.end() && source_it->second.IsString()
                        ? source_it->second.AsString()
                        : "persisted";
    overrides.push_back(std::move(record));
  }

  return overrides;
}

std::vector<veyra::ValidationIssue> WriteRouteOverrideStore(
    const std::string& path,
    const std::map<std::string, RouteOverrideRecord>& overrides) {
  std::vector<veyra::ValidationIssue> issues;
  std::ofstream output(path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    issues.push_back({"route_overrides", "Failed to open route override store for writing: " + path});
    return issues;
  }

  output << "{\n"
         << "  \"overrides\": [\n";
  std::size_t index = 0;
  for (const auto& entry : overrides) {
    output << "    {\n"
           << "      \"persona_id\": \"" << JsonEscape(entry.second.persona_id) << "\",\n"
           << "      \"route_profile_id\": \"" << JsonEscape(entry.second.route_profile_id) << "\",\n"
           << "      \"source\": \"" << JsonEscape(entry.second.source) << "\"\n"
           << "    }";
    if (++index < overrides.size()) {
      output << ",";
    }
    output << "\n";
  }
  output << "  ]\n"
         << "}\n";

  if (!output.good()) {
    issues.push_back({"route_overrides", "Failed to flush route override store: " + path});
  }
  return issues;
}

std::vector<veyra::ValidationIssue> WriteRouteEventLog(
    const std::string& path,
    const std::vector<RouteOverrideEvent>& events) {
  std::vector<veyra::ValidationIssue> issues;
  std::ofstream output(path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    issues.push_back({"route_events", "Failed to open route event log for writing: " + path});
    return issues;
  }

  for (const RouteOverrideEvent& event : events) {
    output << "{"
           << "\"persona_id\":\"" << JsonEscape(event.persona_id) << "\","
           << "\"previous_route_profile_id\":\"" << JsonEscape(event.previous_route_profile_id) << "\","
           << "\"next_route_profile_id\":\"" << JsonEscape(event.next_route_profile_id) << "\","
           << "\"source\":\"" << JsonEscape(event.source) << "\","
           << "\"route_type\":\"" << JsonEscape(event.route_state.route_type) << "\","
           << "\"health_status\":\"" << JsonEscape(event.route_state.health_status) << "\","
           << "\"proxy_uri\":\"" << JsonEscape(event.route_state.proxy_uri) << "\","
           << "\"dns_resolver\":\"" << JsonEscape(event.route_state.dns_resolver) << "\","
           << "\"leak_status\":\"" << JsonEscape(event.route_state.leak_status) << "\""
           << "}\n";
  }

  if (!output.good()) {
    issues.push_back({"route_events", "Failed to flush route event log: " + path});
  }
  return issues;
}

bool BuildEffectiveProfileManager(const veyra::ProfileManager& base_manager,
                                  const std::map<std::string, RouteOverrideRecord>& overrides,
                                  veyra::ProfileManager* effective_manager,
                                  std::vector<veyra::ValidationIssue>* issues) {
  for (const auto& entry : overrides) {
    if (base_manager.FindProfileById(entry.first) == nullptr) {
      issues->push_back({"route_override:" + entry.first,
                         "Unknown persona id in route override store."});
    }
  }

  std::vector<veyra::RuntimeProfile> profiles;
  profiles.reserve(base_manager.profiles().size());

  for (const veyra::RuntimeProfile& base_profile : base_manager.profiles()) {
    veyra::RuntimeProfile profile = base_profile;
    const auto override_it = overrides.find(base_profile.persona.id);
    if (override_it != overrides.end()) {
      const veyra::RouteProfileDefinition* override_route =
          base_manager.FindRouteProfileById(override_it->second.route_profile_id);
      if (override_route == nullptr) {
        issues->push_back({"route_override:" + base_profile.persona.id,
                           "Unknown route profile id: " + override_it->second.route_profile_id});
        continue;
      }

      profile.persona.route_profile_id = override_route->id;
      profile.route_profile = *override_route;
      const veyra::RuntimePolicyBuildResult policy_result =
          veyra::BuildRuntimePolicy(profile.persona, profile.security_mode,
                                    profile.route_profile, profile.session_partition);
      if (!policy_result.issues.empty()) {
        issues->insert(issues->end(),
                       policy_result.issues.begin(),
                       policy_result.issues.end());
        continue;
      }
      profile.runtime_policy = policy_result.policy;
    }

    profiles.push_back(std::move(profile));
  }

  if (!issues->empty()) {
    return false;
  }

  *effective_manager = veyra::ProfileManager(std::move(profiles), base_manager.route_profiles());
  return true;
}

bool BuildProfileWithRouteOverride(const veyra::ProfileManager& manager,
                                   const veyra::RuntimeProfile& base_profile,
                                   const std::string& route_profile_id,
                                   veyra::RuntimeProfile* profile,
                                   std::vector<veyra::ValidationIssue>* issues) {
  const veyra::RouteProfileDefinition* override_route =
      manager.FindRouteProfileById(route_profile_id);
  if (override_route == nullptr) {
    issues->push_back({"route_override:" + base_profile.persona.id,
                       "Unknown route profile id: " + route_profile_id});
    return false;
  }

  *profile = base_profile;
  profile->persona.route_profile_id = override_route->id;
  profile->route_profile = *override_route;
  const veyra::RuntimePolicyBuildResult policy_result =
      veyra::BuildRuntimePolicy(profile->persona, profile->security_mode,
                                profile->route_profile, profile->session_partition);
  if (!policy_result.issues.empty()) {
    issues->insert(issues->end(), policy_result.issues.begin(), policy_result.issues.end());
    return false;
  }
  profile->runtime_policy = policy_result.policy;
  return true;
}

bool ParseArgs(int argc, char** argv, LaunchOptions* options, std::string* error) {
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];

    if (argument == "--smoke") {
      options->smoke_mode = true;
      continue;
    }
    if (argument == "--keep-ephemeral") {
      options->keep_ephemeral_partitions = true;
      continue;
    }
    if (argument == "--list-personas") {
      options->list_personas = true;
      continue;
    }
    if (argument == "--list-route-profiles") {
      options->list_route_profiles = true;
      continue;
    }
    if (argument == "--list-routes") {
      options->list_resolved_routes = true;
      continue;
    }
    if (argument == "--list-tools") {
      options->list_tools = true;
      continue;
    }
    if (StartsWith(argument, "--invoke-tool=")) {
      options->invoke_tool_spec = argument.substr(std::string("--invoke-tool=").size());
      if (options->invoke_tool_spec.empty()) {
        *error = "--invoke-tool flag requires tool_id:arg0:arg1... format.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--tool-bridge-bin=")) {
      options->tool_bridge_binary_path =
          argument.substr(std::string("--tool-bridge-bin=").size());
      if (options->tool_bridge_binary_path.empty()) {
        *error = "--tool-bridge-bin flag requires a non-empty path.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--shell-ui-path=")) {
      options->shell_ui_dist_path =
          argument.substr(std::string("--shell-ui-path=").size());
      if (options->shell_ui_dist_path.empty()) {
        *error = "--shell-ui-path flag requires a non-empty path to the React dist/ folder.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--ai-orchestrator-script=")) {
      options->ai_orchestrator_script_path =
          argument.substr(std::string("--ai-orchestrator-script=").size());
      if (options->ai_orchestrator_script_path.empty()) {
        *error = "--ai-orchestrator-script flag requires a non-empty path to the Python script.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--ai-python=")) {
      options->ai_python_binary =
          argument.substr(std::string("--ai-python=").size());
      if (options->ai_python_binary.empty()) {
        *error = "--ai-python flag requires a non-empty interpreter name or path.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--osint-workspace-script=")) {
      options->osint_workspace_script_path =
          argument.substr(std::string("--osint-workspace-script=").size());
      if (options->osint_workspace_script_path.empty()) {
        *error = "--osint-workspace-script flag requires a non-empty path to the Python script.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--control-fifo=")) {
      options->control_fifo_path =
          argument.substr(std::string("--control-fifo=").size());
      if (options->control_fifo_path.empty()) {
        *error = "--control-fifo flag requires a non-empty FIFO path.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--extensions-dir=")) {
      options->extensions_dir =
          argument.substr(std::string("--extensions-dir=").size());
      continue;
    }
    if (argument == "--no-extensions") {
      options->extensions_dir.clear();
      continue;
    }
    if (StartsWith(argument, "--persona=")) {
      options->startup_persona_id = argument.substr(std::string("--persona=").size());
      if (options->startup_persona_id.empty()) {
        *error = "--persona flag requires a non-empty persona id.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--startup-route=")) {
      options->startup_route_profile_id =
          argument.substr(std::string("--startup-route=").size());
      if (options->startup_route_profile_id.empty()) {
        *error = "--startup-route flag requires a non-empty route profile id.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--hot-route-after-start=")) {
      options->hot_route_after_start_profile_id =
          argument.substr(std::string("--hot-route-after-start=").size());
      if (options->hot_route_after_start_profile_id.empty()) {
        *error = "--hot-route-after-start flag requires a non-empty route profile id.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--url=")) {
      options->startup_url = argument.substr(std::string("--url=").size());
      if (options->startup_url.empty()) {
        *error = "--url flag requires a non-empty URL.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--runtime-root=")) {
      options->runtime_root = argument.substr(std::string("--runtime-root=").size());
      if (options->runtime_root.empty()) {
        *error = "--runtime-root flag requires a non-empty path.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--open-tab=")) {
      const std::string tab_url = argument.substr(std::string("--open-tab=").size());
      if (tab_url.empty()) {
        *error = "--open-tab flag requires a non-empty URL.";
        return false;
      }
      options->additional_tab_urls.push_back(tab_url);
      continue;
    }
    if (StartsWith(argument, "--route-override=")) {
      const std::string spec = argument.substr(std::string("--route-override=").size());
      if (spec.empty()) {
        *error = "--route-override flag requires persona_id:route_profile_id.";
        return false;
      }
      options->route_override_specs.push_back(spec);
      continue;
    }
    if (StartsWith(argument, "--route-engine-bin=")) {
      options->route_engine_binary_path =
          argument.substr(std::string("--route-engine-bin=").size());
      if (options->route_engine_binary_path.empty()) {
        *error = "--route-engine-bin flag requires a non-empty path.";
        return false;
      }
      continue;
    }
    if (StartsWith(argument, "--artifact-scan-bin=")) {
      options->artifact_scan_binary_path =
          argument.substr(std::string("--artifact-scan-bin=").size());
      if (options->artifact_scan_binary_path.empty()) {
        *error = "--artifact-scan-bin flag requires a non-empty path.";
        return false;
      }
      continue;
    }

    *error = "Unknown argument: " + argument;
    return false;
  }

  return true;
}

int RunBootstrap(const LaunchOptions& launch_options) {
  const veyra::StartupConfig config = veyra::DefaultStartupConfig();
  const veyra::ProfileManagerBootstrapResult profile_manager_bootstrap =
      veyra::BootstrapProfileManager(config);
  const veyra::ProfileManager& base_manager = profile_manager_bootstrap.manager;

  std::cout << "Veyra Shell Bootstrap\n";
  std::cout << "Mode: core-foundation\n";
  std::cout << "Browser shell language: C++\n";
  std::cout << "Routing services language: Rust\n";
  std::cout << "Control-plane UI language: TypeScript + React\n";
  if (launch_options.smoke_mode) {
    std::cout << "Run mode: smoke\n";
  }
  std::cout << "\n";

  if (!profile_manager_bootstrap.issues.empty()) {
    std::cerr << "Startup validation failed.\n";
    for (const auto& issue : profile_manager_bootstrap.issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    return 1;
  }

  const veyra::RuntimeProfile* requested_startup_profile =
      base_manager.ResolveStartupProfile(launch_options.startup_persona_id);
  if (launch_options.list_personas) {
    std::cout << "Available personas: " << PersonaListString(base_manager) << "\n";
  }
  if (launch_options.list_route_profiles) {
    std::cout << "Available route profiles: " << RouteProfileListString(base_manager) << "\n";
  }
  if (launch_options.list_tools) {
    std::cout << "Available tools:\n";
    for (const veyra::ToolDefinition& tool : profile_manager_bootstrap.manager.tools()) {
      std::cout << " - " << tool.id << " (" << tool.display_name << ") ["
                << tool.category << "]\n";
    }
  }
  if (requested_startup_profile == nullptr) {
    std::cerr << "Startup validation failed.\n";
    std::cerr << " - profile_manager: requested startup persona not found: "
              << launch_options.startup_persona_id << "\n";
    std::cerr << " - available personas: " << PersonaListString(base_manager) << "\n";
    return 1;
  }

  if (!std::filesystem::create_directories(launch_options.runtime_root) &&
      !std::filesystem::exists(launch_options.runtime_root)) {
    std::cerr << "Runtime bootstrap failed.\n";
    std::cerr << " - runtime_root: failed to create runtime root: "
              << launch_options.runtime_root << "\n";
    return 1;
  }

  const std::string route_override_store_path =
      (std::filesystem::path(launch_options.runtime_root) / "route-overrides.json").string();
  std::string route_override_error;
  std::vector<RouteOverrideRecord> persisted_route_overrides =
      LoadRouteOverrides(route_override_store_path, &route_override_error);
  if (!route_override_error.empty()) {
    std::cerr << "Route override bootstrap failed.\n";
    std::cerr << " - route_overrides: " << route_override_error << "\n";
    return 1;
  }

  std::map<std::string, RouteOverrideRecord> route_overrides_by_persona;
  for (const RouteOverrideRecord& persisted_override : persisted_route_overrides) {
    route_overrides_by_persona[persisted_override.persona_id] = persisted_override;
  }
  for (const std::string& override_spec : launch_options.route_override_specs) {
    RouteOverrideRecord override_record;
    if (!ParseRouteOverrideSpec(override_spec, &override_record, &route_override_error)) {
      std::cerr << "Route override bootstrap failed.\n";
      std::cerr << " - route_overrides: " << route_override_error << "\n";
      return 1;
    }
    override_record.source = "cli";
    route_overrides_by_persona[override_record.persona_id] = override_record;
  }
  if (!launch_options.startup_route_profile_id.empty()) {
    route_overrides_by_persona[requested_startup_profile->persona.id] = RouteOverrideRecord{
        requested_startup_profile->persona.id,
        launch_options.startup_route_profile_id,
        "startup-cli",
    };
  }

  std::vector<veyra::ValidationIssue> route_override_issues;
  veyra::ProfileManager effective_manager;
  if (!BuildEffectiveProfileManager(base_manager,
                                    route_overrides_by_persona,
                                    &effective_manager,
                                    &route_override_issues)) {
    std::cerr << "Route override bootstrap failed.\n";
    for (const auto& issue : route_override_issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    return 1;
  }

  const std::vector<veyra::ValidationIssue> route_override_store_issues =
      WriteRouteOverrideStore(route_override_store_path, route_overrides_by_persona);
  if (!route_override_store_issues.empty()) {
    std::cerr << "Route override bootstrap failed.\n";
    for (const auto& issue : route_override_store_issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    return 1;
  }

  const veyra::RuntimeProfile* startup_profile =
      effective_manager.ResolveStartupProfile(launch_options.startup_persona_id);
  if (startup_profile == nullptr) {
    std::cerr << "Startup validation failed.\n";
    std::cerr << " - profile_manager: effective startup persona could not be resolved.\n";
    return 1;
  }

  veyra::SessionLifecycleOptions lifecycle_options;
  lifecycle_options.keep_ephemeral_partitions = launch_options.keep_ephemeral_partitions;
  lifecycle_options.runtime_root = launch_options.runtime_root;

  veyra::SessionLifecycleManager session_lifecycle(lifecycle_options);
  const std::vector<veyra::ValidationIssue> lifecycle_issues =
      session_lifecycle.Bootstrap(effective_manager);
  if (!lifecycle_issues.empty()) {
    std::cerr << "Session lifecycle bootstrap failed.\n";
    for (const auto& issue : lifecycle_issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    return 1;
  }

  const veyra::SessionAllocation* startup_session =
      session_lifecycle.FindByPersonaId(startup_profile->persona.id);
  auto cleanup_ephemeral = [&session_lifecycle]() {
    const std::vector<veyra::ValidationIssue> cleanup_issues =
        session_lifecycle.CleanupEphemeral();
    for (const auto& issue : cleanup_issues) {
      std::cerr << "Session cleanup warning: " << issue.path << ": " << issue.message << "\n";
    }
  };

  if (startup_session == nullptr) {
    std::cerr << "Session lifecycle bootstrap failed.\n";
    std::cerr << " - session_lifecycle: missing allocation for startup persona '"
              << startup_profile->persona.id << "'.\n";
    cleanup_ephemeral();
    return 1;
  }

  const veyra::SecurityModeRegistry security_registry =
      veyra::BuildSecurityModeRegistry(effective_manager);
  const std::string security_registry_report_path =
      (std::filesystem::path(launch_options.runtime_root) / "security-mode-registry.json").string();
  const std::vector<veyra::ValidationIssue> security_registry_issues =
      veyra::WriteSecurityModeRegistryReport(security_registry, security_registry_report_path);
  if (!security_registry_issues.empty()) {
    std::cerr << "Security policy bootstrap failed.\n";
    for (const auto& issue : security_registry_issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    cleanup_ephemeral();
    return 1;
  }

  std::map<std::string, veyra::EngineSecurityPolicy> engine_policies_by_persona;
  std::map<std::string, std::vector<veyra::PermissionEvaluation>> permission_reports_by_persona;
  for (const veyra::RuntimeProfile& profile : effective_manager.profiles()) {
    const veyra::SessionAllocation* allocation =
        session_lifecycle.FindByPersonaId(profile.persona.id);
    if (allocation == nullptr) {
      std::cerr << "Security policy bootstrap failed.\n";
      std::cerr << " - security_policy:" << profile.persona.id
                << ": missing session allocation for persona.\n";
      cleanup_ephemeral();
      return 1;
    }

    permission_reports_by_persona.emplace(profile.persona.id,
                                          veyra::BuildPermissionReport(profile));
    engine_policies_by_persona.emplace(profile.persona.id,
                                       veyra::BuildEngineSecurityPolicy(profile, *allocation));
  }

  // Bundled-router supervisor: launches Veyra's own tor/i2pd (shipped with the
  // app, Tor-Browser style) so anonymity works with no system install.
  veyra::RouterSupervisor router_supervisor(launch_options.runtime_root);
  {
    std::string router_status;
    router_supervisor.Ensure(startup_profile->route_profile.route_type, &router_status);
    std::cout << " - Router: " << router_status << "\n";
  }

  veyra::RouteServiceClient route_service(launch_options.route_engine_binary_path);
  std::string route_error;
  if (!route_service.Bootstrap(base_manager.profiles(), launch_options.runtime_root, &route_error)) {
    std::cerr << "Route service bootstrap failed.\n";
    std::cerr << " - route_service: " << route_error << "\n";
    cleanup_ephemeral();
    return 1;
  }

  std::vector<RouteOverrideEvent> route_override_events;
  for (const auto& entry : route_overrides_by_persona) {
    const veyra::RuntimeProfile* effective_profile = effective_manager.FindProfileById(entry.first);
    const veyra::RuntimeProfile* base_profile = base_manager.FindProfileById(entry.first);
    if (effective_profile == nullptr || base_profile == nullptr ||
        effective_profile->route_profile.id == base_profile->route_profile.id) {
      continue;
    }

    veyra::RouteRuntimeState updated_route;
    if (!route_service.SwitchRoute(*effective_profile, launch_options.runtime_root,
                                   &updated_route, &route_error)) {
      std::cerr << "Route service switch failed.\n";
      std::cerr << " - route_service: " << route_error << "\n";
      cleanup_ephemeral();
      return 1;
    }

    route_override_events.push_back(RouteOverrideEvent{
        effective_profile->persona.id,
        base_profile->route_profile.id,
        effective_profile->route_profile.id,
        entry.second.source,
        updated_route,
    });
  }

  veyra::RouteRuntimeState startup_route;
  if (!route_service.RequestStatus(startup_profile->persona.id, &startup_route, &route_error)) {
    std::cerr << "Route service bootstrap failed.\n";
    std::cerr << " - route_service: " << route_error << "\n";
    cleanup_ephemeral();
    return 1;
  }

  const std::string route_state_report_path =
      (std::filesystem::path(launch_options.runtime_root) / "route-state.json").string();
  const std::vector<veyra::ValidationIssue> route_report_issues =
      route_service.WriteRouteStateReport(route_state_report_path);
  if (!route_report_issues.empty()) {
    std::cerr << "Route service bootstrap failed.\n";
    for (const auto& issue : route_report_issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    cleanup_ephemeral();
    return 1;
  }

  const std::string route_events_path =
      (std::filesystem::path(launch_options.runtime_root) / "route-events.jsonl").string();
  const std::vector<veyra::ValidationIssue> route_event_issues =
      WriteRouteEventLog(route_events_path, route_override_events);
  if (!route_event_issues.empty()) {
    std::cerr << "Route service bootstrap failed.\n";
    for (const auto& issue : route_event_issues) {
      std::cerr << " - " << issue.path << ": " << issue.message << "\n";
    }
    cleanup_ephemeral();
    return 1;
  }

  for (const veyra::RouteRuntimeState& route_state : route_service.routes()) {
    auto policy_it = engine_policies_by_persona.find(route_state.persona_id);
    if (policy_it == engine_policies_by_persona.end()) {
      continue;
    }

    policy_it->second.route_proxy_uri = route_state.proxy_uri;
    policy_it->second.route_dns_resolver = route_state.dns_resolver;
    policy_it->second.route_health_status = route_state.health_status;
    policy_it->second.route_leak_status = route_state.leak_status;
    if (!route_state.proxy_uri.empty()) {
      policy_it->second.proxy_mode = "custom-proxy";
    } else if (route_state.route_type == "direct") {
      policy_it->second.proxy_mode = "system-default";
    } else {
      policy_it->second.proxy_mode = "no-proxy-hook";
    }
  }

  for (const veyra::RuntimeProfile& profile : effective_manager.profiles()) {
    const veyra::SessionAllocation* allocation =
        session_lifecycle.FindByPersonaId(profile.persona.id);
    const auto policy_it = engine_policies_by_persona.find(profile.persona.id);
    const auto permission_it = permission_reports_by_persona.find(profile.persona.id);
    if (allocation == nullptr || policy_it == engine_policies_by_persona.end() ||
        permission_it == permission_reports_by_persona.end()) {
      std::cerr << "Security policy bootstrap failed.\n";
      std::cerr << " - security_policy:" << profile.persona.id
                << ": missing finalized policy material.\n";
      cleanup_ephemeral();
      return 1;
    }

    const std::vector<veyra::ValidationIssue> security_report_issues =
        veyra::WriteEngineSecurityPolicyReport(profile, *allocation, policy_it->second,
                                               permission_it->second);
    if (!security_report_issues.empty()) {
      std::cerr << "Security policy bootstrap failed.\n";
      for (const auto& issue : security_report_issues) {
        std::cerr << " - " << issue.path << ": " << issue.message << "\n";
      }
      cleanup_ephemeral();
      return 1;
    }
  }

  const auto startup_policy_it = engine_policies_by_persona.find(startup_profile->persona.id);
  const auto startup_permissions_it = permission_reports_by_persona.find(startup_profile->persona.id);
  if (startup_policy_it == engine_policies_by_persona.end() ||
      startup_permissions_it == permission_reports_by_persona.end()) {
    std::cerr << "Security policy bootstrap failed.\n";
    std::cerr << " - security_policy: startup persona policy report was not generated.\n";
    cleanup_ephemeral();
    return 1;
  }

  veyra::BrowserWindow primary_window =
      veyra::BrowserWindow::CreatePrimaryWindow(*startup_profile, startup_route,
                                                launch_options.startup_url);
  for (std::size_t index = 0; index < launch_options.additional_tab_urls.size(); ++index) {
    primary_window.OpenTab(*startup_profile,
                           startup_route,
                           launch_options.additional_tab_urls[index],
                           "Tab " + std::to_string(index + 2));
  }

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
  std::cout << " - Fingerprint profiles loaded: "
            << profile_manager_bootstrap.foundation_summary.fingerprint_profile_count << "\n";
  std::cout << " - Extension policies loaded: "
            << profile_manager_bootstrap.foundation_summary.extension_policy_count << "\n";
  std::cout << " - Tool definitions loaded: "
            << profile_manager_bootstrap.foundation_summary.tool_count << "\n";
  std::cout << "\n";

  std::cout << "Runtime bootstrap summary.\n";
  std::cout << " - Runtime root: " << launch_options.runtime_root << "\n";
  std::cout << " - Route engine binary: " << launch_options.route_engine_binary_path << "\n";
  std::cout << " - Artifact scan binary: " << launch_options.artifact_scan_binary_path << "\n";
  std::cout << " - Tool bridge binary: " << launch_options.tool_bridge_binary_path << "\n";
  if (!launch_options.shell_ui_dist_path.empty()) {
    std::cout << " - Dashboard UI: " << launch_options.shell_ui_dist_path << "\n";
  }
  if (!launch_options.ai_orchestrator_script_path.empty()) {
    std::cout << " - AI orchestrator: " << launch_options.ai_orchestrator_script_path
              << " (" << launch_options.ai_python_binary << ")\n";
  }
  if (!launch_options.osint_workspace_script_path.empty()) {
    std::cout << " - OSINT workspace: " << launch_options.osint_workspace_script_path
              << " (" << launch_options.ai_python_binary << ")\n";
  }
  std::cout << " - Route override store: " << route_override_store_path << "\n";
  std::cout << " - Route event log: " << route_events_path << "\n";
  std::cout << " - Route state report: " << route_state_report_path << "\n";
  std::cout << " - Security mode registry report: " << security_registry_report_path << "\n";
  std::cout << " - Startup persona: " << startup_profile->persona.display_name << " ("
            << startup_profile->persona.id << ")\n";
  std::cout << " - Session partition: " << startup_profile->session_partition.id() << "\n";
  std::cout << " - Session persistent: "
            << (startup_profile->session_partition.is_persistent() ? "yes" : "no") << "\n";
  std::cout << " - Partition root: " << startup_session->partition_root << "\n";
  std::cout << " - Route profile: " << startup_profile->route_profile.display_name << "\n";
  std::cout << " - Route engine health: " << startup_route.health_status << "\n";
  std::cout << " - Route leak status: " << startup_route.leak_status << "\n";
  std::cout << " - Route DNS resolver: " << startup_route.dns_resolver << "\n";
  std::cout << " - Route proxy endpoint: "
            << (startup_route.proxy_uri.empty() ? std::string("none") : startup_route.proxy_uri)
            << "\n";
  std::cout << " - Security mode: " << startup_profile->security_mode.display_name << "\n";
  std::cout << " - Effective route type: " << startup_profile->runtime_policy.route_type << "\n";
  std::cout << " - DNS policy: " << startup_profile->runtime_policy.dns_policy << "\n";
  std::cout << " - WebRTC policy: " << startup_profile->runtime_policy.webrtc_policy << "\n";
  std::cout << " - Download handling: " << startup_profile->runtime_policy.download_policy << "\n";
  std::cout << " - History retention: "
            << startup_profile->runtime_policy.history_retention_policy << " (max "
            << startup_profile->runtime_policy.max_history_entries << " entries)\n";
  std::cout << " - Security policy report: " << startup_policy_it->second.report_path << "\n";
  std::cout << " - Fingerprint profile: " << startup_policy_it->second.fingerprint_profile_id
            << "\n";
  std::cout << " - Fingerprint injection: "
            << (startup_policy_it->second.fingerprint_script.empty() ? "none" : "active")
            << "\n";
  std::cout << " - Extension policy: " << startup_policy_it->second.extension_policy_id << "\n";
  std::cout << " - Eval allowed: "
            << (startup_policy_it->second.allow_eval ? "yes" : "blocked") << "\n";
  std::cout << " - Mixed content: "
            << (startup_policy_it->second.block_mixed_content ? "blocked" : "allowed") << "\n";
  std::cout << " - Blocked domains: "
            << startup_policy_it->second.blocked_domains.size() << "\n";
  std::cout << " - JavaScript engine mode: "
            << (startup_policy_it->second.enable_javascript ? "enabled" : "blocked") << "\n";
  std::cout << " - Media capture: "
            << (startup_policy_it->second.enable_media_stream ? "available" : "disabled") << "\n";
  std::cout << " - Localhost navigation: "
            << (startup_policy_it->second.allow_localhost_navigation ? "allowed" : "blocked") << "\n";
  std::cout << " - Private-network navigation: "
            << (startup_policy_it->second.allow_private_network_navigation ? "allowed" : "blocked")
            << "\n";
  std::cout << " - Ephemeral partitions staged: "
            << session_lifecycle.ephemeral_partition_count() << "\n";
  std::cout << " - Session allocations prepared: "
            << session_lifecycle.allocations().size() << "\n";
  std::cout << " - Active route overrides: " << route_overrides_by_persona.size() << "\n";
  std::cout << " - Route switch events recorded: " << route_override_events.size() << "\n";
  std::cout << " - Primary window id: " << primary_window.id() << "\n";
  std::cout << " - Open tabs: " << primary_window.tabs().size() << "\n";
  if (active_tab != nullptr) {
    std::cout << " - Active tab url: " << active_tab->current_url() << "\n";
    std::cout << " - Active tab route profile: " << active_tab->route_profile_id() << "\n";
    std::cout << " - Active tab route type: " << active_tab->route_type() << "\n";
    std::cout << " - Active tab route health: " << active_tab->route_health_status() << "\n";
    std::cout << " - Active tab history mode: "
              << (active_tab->history_enabled() ? "tracked" : "disabled") << "\n";
  }
  if (launch_options.list_resolved_routes) {
    std::cout << "Resolved routes:\n";
    for (const veyra::RouteRuntimeState& route_state : route_service.routes()) {
      std::cout << " - " << route_state.persona_id << ": "
                << route_state.route_profile_id << " -> "
                << route_state.route_type << " ["
                << route_state.health_status << "]\n";
    }
  }
  std::cout << "\n";

  std::cout << "Permission broker preview.\n";
  for (const veyra::PermissionEvaluation& evaluation : startup_permissions_it->second) {
    std::cout << " - " << veyra::ToString(evaluation.permission) << ": "
              << veyra::ToString(evaluation.decision) << "\n";
  }
  std::cout << "\n";

  std::string engine_error;
  std::unique_ptr<veyra::BrowserEngine> engine = veyra::CreateBrowserEngine(&engine_error);
  if (engine == nullptr) {
    std::cerr << "Engine initialization failed.\n";
    std::cerr << " - engine: " << engine_error << "\n";
    cleanup_ephemeral();
    return 1;
  }

  veyra::BrowserEngineConfig engine_config;
  engine_config.window_title = "ApexForge Veyra | Native Shell";
  engine_config.security_policy = startup_policy_it->second;
  engine_config.persona_id = startup_profile->persona.id;
  engine_config.ephemeral_persona = startup_policy_it->second.use_ephemeral_context;
  engine_config.artifact_scan_binary_path = launch_options.artifact_scan_binary_path;
  engine_config.fingerprint_script = startup_policy_it->second.fingerprint_script;
  engine_config.shell_ui_dist_path = launch_options.shell_ui_dist_path;
  engine_config.control_fifo_path = launch_options.control_fifo_path;
  engine_config.extension_content_scripts =
      LoadExtensionContentScripts(launch_options.extensions_dir);

  if (!launch_options.shell_ui_dist_path.empty()) {
    const std::string dashboard_state_json = veyra::SerializeDashboardState(
        effective_manager,
        *startup_profile,
        route_service,
        startup_policy_it->second,
        startup_permissions_it->second,
        effective_manager.tools(),
        startup_profile->persona.id,
        launch_options.runtime_root,
        "0.10");
    engine_config.dashboard_state_script =
        veyra::BuildStateInjectionScript(dashboard_state_json);
  }
  engine_config.permission_report = startup_permissions_it->second;
  engine_config.on_navigation_committed =
      [&primary_window](const std::string& tab_id, const std::string& url, const std::string& title) {
        primary_window.SyncTabFromEngine(tab_id, url, title);
      };
  engine_config.on_policy_event =
      [](const std::string& tab_id, const std::string& category, const std::string& message) {
        std::cerr << "[policy][" << category << "][" << tab_id << "] " << message << "\n";
      };

  // The accumulated OSINT case lives in RunBootstrap scope so it persists across
  // dashboard actions and can be captured by reference in the push helper.
  veyra::OsintCaseSnapshot osint_case;

  // Serializes the current dashboard state (optionally with an AI result) and
  // pushes it to the React panel. Defined in RunBootstrap scope so it remains
  // valid when captured by asynchronous AI callbacks that fire on the main loop.
  auto push_dashboard_with_ai =
      [&](const veyra::AiResultSnapshot& ai_snapshot) {
        if (launch_options.shell_ui_dist_path.empty()) {
          return;
        }
        const std::string state_json = veyra::SerializeDashboardState(
            effective_manager,
            *startup_profile,
            route_service,
            startup_policy_it->second,
            startup_permissions_it->second,
            effective_manager.tools(),
            startup_profile->persona.id,
            launch_options.runtime_root,
            "0.11",
            ai_snapshot,
            osint_case);
        engine->PushDashboardState(state_json);
      };

  // Dashboard action handler — called when the React UI posts a message.
  engine_config.on_dashboard_action =
      [&](const std::string& action_json) {
        veyra::DashboardAction action;
        std::string parse_error;
        if (!veyra::ParseDashboardAction(action_json, &action, &parse_error)) {
          std::cerr << "[dashboard] Failed to parse action: " << parse_error << "\n";
          return;
        }

        if (action.action == "switch_route" && !action.route_profile_id.empty()) {
          veyra::RuntimeProfile hot_profile = *startup_profile;
          std::vector<veyra::ValidationIssue> hot_issues;
          if (!BuildProfileWithRouteOverride(
                  effective_manager, *startup_profile, action.route_profile_id,
                  &hot_profile, &hot_issues)) {
            for (const auto& issue : hot_issues) {
              std::cerr << "[dashboard] Route switch failed: " << issue.message << "\n";
            }
            return;
          }

          veyra::RouteRuntimeState hot_route;
          std::string route_error;
          if (!route_service.SwitchRoute(hot_profile, launch_options.runtime_root,
                                         &hot_route, &route_error)) {
            std::cerr << "[dashboard] Route switch failed: " << route_error << "\n";
            return;
          }

          // Bring up the bundled router (tor/i2pd) for this route if needed.
          std::string router_status;
          router_supervisor.Ensure(hot_route.route_type, &router_status);
          std::cout << "[router] " << router_status << "\n";

          veyra::EngineSecurityPolicy hot_policy =
              veyra::BuildEngineSecurityPolicy(hot_profile, *startup_session);
          hot_policy.route_proxy_uri = hot_route.proxy_uri;
          hot_policy.route_dns_resolver = hot_route.dns_resolver;
          hot_policy.route_health_status = hot_route.health_status;
          hot_policy.route_leak_status = hot_route.leak_status;
          hot_policy.proxy_mode = hot_route.proxy_uri.empty()
              ? (hot_route.route_type == "direct" ? "system-default" : "no-proxy-hook")
              : "custom-proxy";

          const std::vector<veyra::PermissionEvaluation> hot_perms =
              veyra::BuildPermissionReport(hot_profile);
          std::string apply_error;
          if (!engine->ApplySecurityPolicy(hot_policy, hot_perms, &apply_error)) {
            std::cerr << "[dashboard] Policy apply failed: " << apply_error << "\n";
            return;
          }

          primary_window.ApplyRouteStateToProfileTabs(startup_profile->persona.id, hot_route);
          std::cout << "[dashboard] Route switched to " << action.route_profile_id << "\n";

          if (!launch_options.shell_ui_dist_path.empty()) {
            const std::string new_state_json = veyra::SerializeDashboardState(
                effective_manager,
                hot_profile,
                route_service,
                hot_policy,
                hot_perms,
                effective_manager.tools(),
                startup_profile->persona.id,
                launch_options.runtime_root,
                "0.10");
            engine->PushDashboardState(new_state_json);
          }

        } else if (action.action == "switch_persona" && !action.persona_id.empty()) {
          // Full runtime persona switch: resolve the new profile, switch its
          // route, apply its precomputed security policy + permissions, and
          // update the engine's active-persona state (history/fingerprint/UA/
          // ephemeral). This is the real per-profile switch (not just a refresh).
          const veyra::RuntimeProfile* next_profile =
              effective_manager.FindProfileById(action.persona_id);
          const auto next_policy_it = engine_policies_by_persona.find(action.persona_id);
          const auto next_perms_it = permission_reports_by_persona.find(action.persona_id);
          if (next_profile == nullptr || next_policy_it == engine_policies_by_persona.end() ||
              next_perms_it == permission_reports_by_persona.end()) {
            std::cerr << "[dashboard] Unknown persona: " << action.persona_id << "\n";
            return;
          }

          veyra::EngineSecurityPolicy next_policy = next_policy_it->second;
          veyra::RouteRuntimeState next_route;
          std::string route_error;
          if (route_service.SwitchRoute(*next_profile, launch_options.runtime_root,
                                        &next_route, &route_error)) {
            std::string router_status;
            router_supervisor.Ensure(next_route.route_type, &router_status);
            std::cout << "[router] " << router_status << "\n";
            next_policy.route_proxy_uri = next_route.proxy_uri;
            next_policy.route_dns_resolver = next_route.dns_resolver;
            next_policy.route_health_status = next_route.health_status;
            next_policy.route_leak_status = next_route.leak_status;
            next_policy.proxy_mode = next_route.proxy_uri.empty()
                ? (next_route.route_type == "direct" ? "system-default" : "no-proxy-hook")
                : "custom-proxy";
          }

          std::string apply_error;
          if (!engine->ApplySecurityPolicy(next_policy, next_perms_it->second, &apply_error)) {
            std::cerr << "[dashboard] Persona policy apply failed: " << apply_error << "\n";
            return;
          }
          engine->SetActivePersona(next_profile->persona.id, next_profile->persona.ephemeral);
          startup_profile = next_profile;  // subsequent actions use the new persona
          std::cout << "[dashboard] Persona switched to " << action.persona_id << "\n";

          if (!launch_options.shell_ui_dist_path.empty()) {
            const std::string new_state_json = veyra::SerializeDashboardState(
                effective_manager, *next_profile, route_service, next_policy,
                next_perms_it->second, effective_manager.tools(),
                next_profile->persona.id, launch_options.runtime_root, "0.11");
            engine->PushDashboardState(new_state_json);
          }

        } else if (action.action == "invoke_tool" && !action.tool_id.empty()) {
          veyra::ToolBridgeClient tool_bridge(
              launch_options.tool_bridge_binary_path,
              effective_manager.tools());

          veyra::ToolInvocationRequest tool_req;
          tool_req.tool_id = action.tool_id;
          tool_req.args = action.tool_args;
          tool_req.persona_id = startup_profile->persona.id;
          tool_req.security_mode_id = startup_profile->security_mode.id;
          tool_req.route_type = startup_profile->runtime_policy.route_type;
          tool_req.quarantine_root = startup_policy_it->second.quarantine_root_directory;
          tool_req.artifact_event_log_path = startup_policy_it->second.artifact_event_log_path;

          veyra::ToolInvocationResult tool_result;
          std::string tool_error;
          tool_bridge.Invoke(tool_req, &tool_result, &tool_error);
          std::cout << "[dashboard] Tool " << action.tool_id
                    << " exit=" << tool_result.exit_code << "\n";

        } else if (action.action == "open_tab") {
          const std::string& url = action.navigate_url.empty()
              ? std::string("veyra:start") : action.navigate_url;
          const std::size_t next_index = primary_window.tabs().size();
          primary_window.OpenTab(*startup_profile,
                                 route_service.routes().empty()
                                     ? veyra::RouteRuntimeState{}
                                     : route_service.routes().front(),
                                 url, "Tab " + std::to_string(next_index + 1));
          std::string eng_error;
          const veyra::TabModel* new_tab = primary_window.ActiveTab();
          if (new_tab != nullptr) {
            engine->CreateTab(new_tab->id(), url, &eng_error);
          }

        } else if (action.action == "request_state_refresh") {
          push_dashboard_with_ai(veyra::AiResultSnapshot{});

        } else if (action.action == "ai_summarize" ||
                   action.action == "ai_phishing_check" ||
                   action.action == "ai_explain_script") {
          if (launch_options.ai_orchestrator_script_path.empty()) {
            std::cerr << "[dashboard] AI request ignored: no --ai-orchestrator-script configured.\n";
            veyra::AiResultSnapshot snap;
            snap.has_result = true;
            snap.method = action.action;
            snap.ok = false;
            snap.error = "AI orchestrator is not configured for this session.";
            push_dashboard_with_ai(snap);
            return;
          }

          const std::string py = launch_options.ai_python_binary;
          const std::string script = launch_options.ai_orchestrator_script_path;
          const veyra::AiPolicyDefinition ai_policy = startup_profile->ai_policy;

          if (action.action == "ai_explain_script") {
            veyra::AiOrchestratorClient ai(py, script, ai_policy);
            veyra::AiRequestResult res;
            std::string err;
            ai.ExplainScript(action.ai_payload, &res, &err);

            veyra::AiResultSnapshot snap;
            snap.has_result = true;
            snap.method = "explain_script";
            snap.ok = res.ok;
            snap.text = res.ok ? res.explanation
                               : (res.error.empty() ? err : res.error);
            snap.source = res.source;
            snap.error = res.ok ? "" : (res.error.empty() ? err : res.error);
            push_dashboard_with_ai(snap);

          } else if (action.action == "ai_phishing_check") {
            const std::string url = engine->GetActiveTabUrl();
            const std::string title = engine->GetActiveTabTitle();
            veyra::AiOrchestratorClient ai(py, script, ai_policy);
            veyra::AiRequestResult res;
            std::string err;
            ai.PhishingCheck(url, title, &res, &err);

            veyra::AiResultSnapshot snap;
            snap.has_result = true;
            snap.method = "phishing_check";
            snap.ok = res.ok;
            snap.text = res.ok ? res.explanation
                               : (res.error.empty() ? err : res.error);
            snap.risk_level = res.risk_level;
            snap.risk_score = res.risk_score;
            snap.source = res.source;
            snap.error = res.ok ? "" : (res.error.empty() ? err : res.error);
            push_dashboard_with_ai(snap);

          } else {  // ai_summarize — needs async page-text extraction.
            // Capture only RunBootstrap-scoped references (which outlive the
            // GTK main loop) plus the AI config by value. Never capture the
            // handler-local 'action'.
            engine->RequestActiveTabText(
                [&push_dashboard_with_ai, py, script, ai_policy](const std::string& text) {
                  veyra::AiOrchestratorClient ai(py, script, ai_policy);
                  veyra::AiRequestResult res;
                  std::string err;
                  ai.Summarize(text, &res, &err);

                  veyra::AiResultSnapshot snap;
                  snap.has_result = true;
                  snap.method = "summarize";
                  snap.ok = res.ok;
                  snap.text = res.ok ? res.summary
                                     : (res.error.empty() ? err : res.error);
                  snap.source = res.source;
                  snap.error = res.ok ? "" : (res.error.empty() ? err : res.error);
                  push_dashboard_with_ai(snap);
                });
          }

        } else if (action.action == "osint_clear") {
          osint_case = veyra::OsintCaseSnapshot{};
          push_dashboard_with_ai(veyra::AiResultSnapshot{});

        } else if (action.action == "osint_whois" ||
                   action.action == "osint_dns" ||
                   action.action == "osint_archive" ||
                   action.action == "osint_username") {
          if (launch_options.osint_workspace_script_path.empty()) {
            osint_case.active = true;
            osint_case.last_ok = false;
            osint_case.last_error = "OSINT workspace is not configured for this session.";
            push_dashboard_with_ai(veyra::AiResultSnapshot{});
            return;
          }
          if (action.osint_target.empty()) {
            osint_case.active = true;
            osint_case.last_ok = false;
            osint_case.last_error = "No OSINT target provided.";
            push_dashboard_with_ai(veyra::AiResultSnapshot{});
            return;
          }

          // Build the live route context so every lookup is forced through the
          // persona's active proxy (leak prevention is enforced shell-side and
          // again inside the Python workspace).
          veyra::OsintRouteContext rc;
          const veyra::RouteRuntimeState* rs =
              route_service.FindByPersonaId(startup_profile->persona.id);
          if (rs != nullptr) {
            rc.proxy_uri = rs->proxy_uri;
            rc.route_type = rs->route_type;
          } else {
            rc.route_type = startup_profile->runtime_policy.route_type;
          }

          veyra::OsintWorkspaceClient osint(
              launch_options.ai_python_binary,
              launch_options.osint_workspace_script_path,
              startup_profile->osint_policy);

          veyra::OsintLookupResult res;
          std::string err;
          if (action.action == "osint_whois") {
            osint.Whois(action.osint_target, rc, &res, &err);
          } else if (action.action == "osint_dns") {
            osint.Dns(action.osint_target, rc, &res, &err);
          } else if (action.action == "osint_archive") {
            osint.Archive(action.osint_target, rc, &res, &err);
          } else {
            osint.UsernameSearch(action.osint_target, rc, &res, &err);
          }

          osint_case.active = true;
          osint_case.route_type = rc.route_type;
          osint_case.last_ok = res.ok;
          osint_case.last_summary = res.ok ? res.summary : "";
          osint_case.last_error = res.ok ? "" : (res.error.empty() ? err : res.error);
          if (!res.egress.empty()) {
            osint_case.egress = res.egress;
          }

          if (res.ok) {
            // Merge entities (dedup by type+value).
            for (const veyra::OsintEntity& e : res.entities) {
              bool exists = false;
              for (const veyra::OsintEntity& have : osint_case.entities) {
                if (have.type == e.type && have.value == e.value) { exists = true; break; }
              }
              if (!exists) osint_case.entities.push_back(e);
            }
            // Merge relationships (dedup by from+to+kind).
            for (const veyra::OsintRelationship& r2 : res.relationships) {
              bool exists = false;
              for (const veyra::OsintRelationship& have : osint_case.relationships) {
                if (have.from == r2.from && have.to == r2.to && have.kind == r2.kind) {
                  exists = true; break;
                }
              }
              if (!exists) osint_case.relationships.push_back(r2);
            }
            // Append timeline events.
            for (const veyra::OsintTimelineEvent& t : res.timeline) {
              osint_case.timeline.push_back(t);
            }
          }

          std::cout << "[dashboard] osint " << action.action << " target="
                    << action.osint_target << " ok=" << res.ok
                    << " entities=" << osint_case.entities.size() << "\n";
          push_dashboard_with_ai(veyra::AiResultSnapshot{});
        }
      };

  if (!launch_options.invoke_tool_spec.empty()) {
    const std::size_t first_colon = launch_options.invoke_tool_spec.find(':');
    const std::string tool_id = first_colon == std::string::npos
                                    ? launch_options.invoke_tool_spec
                                    : launch_options.invoke_tool_spec.substr(0, first_colon);

    std::vector<std::string> tool_args;
    if (first_colon != std::string::npos && first_colon + 1 < launch_options.invoke_tool_spec.size()) {
      std::istringstream arg_stream(launch_options.invoke_tool_spec.substr(first_colon + 1));
      std::string arg;
      while (std::getline(arg_stream, arg, ':')) {
        if (!arg.empty()) {
          tool_args.push_back(arg);
        }
      }
    }

    veyra::ToolBridgeClient tool_bridge(
        launch_options.tool_bridge_binary_path,
        profile_manager_bootstrap.manager.tools());

    veyra::ToolInvocationRequest tool_request;
    tool_request.tool_id = tool_id;
    tool_request.args = tool_args;
    tool_request.persona_id = startup_profile->persona.id;
    tool_request.security_mode_id = startup_profile->security_mode.id;
    tool_request.route_type = startup_profile->runtime_policy.route_type;
    tool_request.quarantine_root = startup_policy_it->second.quarantine_root_directory;
    tool_request.artifact_event_log_path = startup_policy_it->second.artifact_event_log_path;

    std::cout << "Tool invocation: " << tool_id << "\n";
    for (const std::string& arg : tool_args) {
      std::cout << " - arg: " << arg << "\n";
    }
    std::cout << "\n";

    veyra::ToolInvocationResult tool_result;
    std::string tool_error;
    if (!tool_bridge.Invoke(tool_request, &tool_result, &tool_error)) {
      if (tool_result.policy_denied) {
        std::cerr << "Tool invocation denied.\n";
        std::cerr << " - reason: " << tool_result.denial_reason << "\n";
      } else if (tool_result.timed_out) {
        std::cerr << "Tool invocation timed out.\n";
        std::cerr << " - detail: " << tool_result.failure_reason << "\n";
      } else {
        std::cerr << "Tool invocation failed.\n";
        std::cerr << " - error: " << tool_error << "\n";
        if (!tool_result.failure_reason.empty()) {
          std::cerr << " - detail: " << tool_result.failure_reason << "\n";
        }
      }
      cleanup_ephemeral();
      return 1;
    }

    std::cout << "Tool invocation result.\n";
    std::cout << " - invocation_id: " << tool_result.invocation_id << "\n";
    std::cout << " - exit_code: " << tool_result.exit_code << "\n";
    std::cout << " - timed_out: " << (tool_result.timed_out ? "yes" : "no") << "\n";
    std::cout << " - output lines: " << tool_result.output.size() << "\n";
    if (!tool_result.quarantine_path.empty()) {
      std::cout << " - quarantined to: " << tool_result.quarantine_path << "\n";
    }
    for (const veyra::ToolOutputLine& line : tool_result.output) {
      std::cout << " [" << line.stream << "] " << line.content << "\n";
    }
    std::cout << "\n";

    if (launch_options.smoke_mode) {
      cleanup_ephemeral();
      return 0;
    }
  }

  const bool has_display = std::getenv("DISPLAY") != nullptr;
  const bool has_wayland = std::getenv("WAYLAND_DISPLAY") != nullptr;
  if (launch_options.smoke_mode && !has_display && !has_wayland) {
    std::cout << "Smoke mode without DISPLAY detected.\n";
    std::cout << " - Engine window checks skipped.\n";
    std::cout << " - Foundation/bootstrap/session lifecycle checks passed.\n";
    cleanup_ephemeral();
    return 0;
  }

  if (!engine->CreateWindow(engine_config, &engine_error)) {
    std::cerr << "Engine window bootstrap failed.\n";
    std::cerr << " - engine: " << engine_error << "\n";
    cleanup_ephemeral();
    return 1;
  }

  for (const veyra::TabModel& tab : primary_window.tabs()) {
    if (!engine->CreateTab(tab.id(), tab.current_url(), &engine_error)) {
      std::cerr << "Engine tab bootstrap failed.\n";
      std::cerr << " - engine: " << engine_error << "\n";
      cleanup_ephemeral();
      return 1;
    }
  }

  const std::string* active_tab_id = primary_window.ActiveTabId();
  if (active_tab_id == nullptr || active_tab == nullptr) {
    std::cerr << "Engine tab bootstrap failed.\n";
    std::cerr << " - runtime: no active tab available for engine launch.\n";
    cleanup_ephemeral();
    return 1;
  }

  if (!engine->ActivateTab(*active_tab_id, &engine_error)) {
    std::cerr << "Engine tab activation failed.\n";
    std::cerr << " - engine: " << engine_error << "\n";
    cleanup_ephemeral();
    return 1;
  }

  if (!launch_options.hot_route_after_start_profile_id.empty()) {
    veyra::RuntimeProfile hot_profile = *startup_profile;
    std::vector<veyra::ValidationIssue> hot_route_issues;
    if (!BuildProfileWithRouteOverride(
            effective_manager, *startup_profile, launch_options.hot_route_after_start_profile_id,
            &hot_profile, &hot_route_issues)) {
      std::cerr << "Hot route reconfigure failed.\n";
      for (const auto& issue : hot_route_issues) {
        std::cerr << " - " << issue.path << ": " << issue.message << "\n";
      }
      cleanup_ephemeral();
      return 1;
    }

    veyra::RouteRuntimeState hot_route;
    if (!route_service.SwitchRoute(hot_profile, launch_options.runtime_root, &hot_route, &route_error)) {
      std::cerr << "Hot route reconfigure failed.\n";
      std::cerr << " - route_service: " << route_error << "\n";
      cleanup_ephemeral();
      return 1;
    }

    veyra::EngineSecurityPolicy hot_policy =
        veyra::BuildEngineSecurityPolicy(hot_profile, *startup_session);
    hot_policy.route_proxy_uri = hot_route.proxy_uri;
    hot_policy.route_dns_resolver = hot_route.dns_resolver;
    hot_policy.route_health_status = hot_route.health_status;
    hot_policy.route_leak_status = hot_route.leak_status;
    if (!hot_route.proxy_uri.empty()) {
      hot_policy.proxy_mode = "custom-proxy";
    } else if (hot_route.route_type == "direct") {
      hot_policy.proxy_mode = "system-default";
    } else {
      hot_policy.proxy_mode = "no-proxy-hook";
    }

    const std::vector<veyra::PermissionEvaluation> hot_permission_report =
        veyra::BuildPermissionReport(hot_profile);
    if (!engine->ApplySecurityPolicy(hot_policy, hot_permission_report, &engine_error)) {
      std::cerr << "Hot route reconfigure failed.\n";
      std::cerr << " - engine: " << engine_error << "\n";
      cleanup_ephemeral();
      return 1;
    }

    route_overrides_by_persona[startup_profile->persona.id] = RouteOverrideRecord{
        startup_profile->persona.id,
        hot_profile.route_profile.id,
        "hot-cli",
    };
    route_override_events.push_back(RouteOverrideEvent{
        startup_profile->persona.id,
        startup_profile->route_profile.id,
        hot_profile.route_profile.id,
        "hot-cli",
        hot_route,
    });

    primary_window.ApplyRouteStateToProfileTabs(startup_profile->persona.id, hot_route);

    const std::vector<veyra::ValidationIssue> hot_override_store_issues =
        WriteRouteOverrideStore(route_override_store_path, route_overrides_by_persona);
    const std::vector<veyra::ValidationIssue> hot_route_report_issues =
        route_service.WriteRouteStateReport(route_state_report_path);
    const std::vector<veyra::ValidationIssue> hot_event_issues =
        WriteRouteEventLog(route_events_path, route_override_events);
    const std::vector<veyra::ValidationIssue> hot_policy_report_issues =
        veyra::WriteEngineSecurityPolicyReport(hot_profile, *startup_session, hot_policy,
                                               hot_permission_report);

    auto print_hot_issues = [](const std::vector<veyra::ValidationIssue>& issues) {
      for (const auto& issue : issues) {
        std::cerr << " - " << issue.path << ": " << issue.message << "\n";
      }
    };
    if (!hot_override_store_issues.empty() || !hot_route_report_issues.empty() ||
        !hot_event_issues.empty() || !hot_policy_report_issues.empty()) {
      std::cerr << "Hot route reconfigure failed.\n";
      print_hot_issues(hot_override_store_issues);
      print_hot_issues(hot_route_report_issues);
      print_hot_issues(hot_event_issues);
      print_hot_issues(hot_policy_report_issues);
      cleanup_ephemeral();
      return 1;
    }

    std::cout << "Hot route reconfigure applied.\n";
    std::cout << " - Persona: " << startup_profile->persona.id << "\n";
    std::cout << " - New route profile: " << hot_profile.route_profile.id << "\n";
    std::cout << " - New route type: " << hot_route.route_type << "\n";
    std::cout << " - New proxy endpoint: "
              << (hot_route.proxy_uri.empty() ? std::string("none") : hot_route.proxy_uri) << "\n";
    std::cout << " - New DNS resolver: " << hot_route.dns_resolver << "\n";
    std::cout << "\n";
  }

  std::cout << "Native browser shell launched.\n";
  std::cout << " - Engine runtime: WebKitGTK\n";
  std::cout << " - Platform scope: Linux-first\n";
  std::cout << " - Active tab id: " << *active_tab_id << "\n";
  std::cout << " - Active tab url: " << engine->GetCurrentUrl(*active_tab_id) << "\n";
  std::cout << "\n";

  if (launch_options.smoke_mode) {
    std::cout << "Smoke check complete.\n";
    std::cout << " - Engine window bootstrap: pass\n";
    std::cout << " - Tab bootstrap count: pass (" << primary_window.tabs().size() << ")\n";
    std::cout << " - Default/startup tab bootstrap: pass\n";
    std::cout << " - Route override persistence: pass (" << route_overrides_by_persona.size()
              << " active override(s))\n";
    cleanup_ephemeral();
    return 0;
  }

  std::cout << "Veyra shell is now running. Close the native window to exit.\n";

  engine->Run();
  cleanup_ephemeral();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  LaunchOptions launch_options;
  launch_options.route_engine_binary_path =
      (std::filesystem::path(argv[0]).parent_path() / "route_engine").string();
  launch_options.artifact_scan_binary_path =
      (std::filesystem::path(argv[0]).parent_path() / "artifact_scan").string();
  launch_options.tool_bridge_binary_path =
      (std::filesystem::path(argv[0]).parent_path() / "tool_bridge").string();
  std::string parse_error;
  if (!ParseArgs(argc, argv, &launch_options, &parse_error)) {
    std::cerr << "Argument parsing failed.\n";
    std::cerr << " - error: " << parse_error << "\n";
    std::cerr << "Supported flags: --smoke --keep-ephemeral --list-personas "
                 "--list-route-profiles --list-routes --persona=<id> "
                 "--startup-route=<route_profile_id> --hot-route-after-start=<route_profile_id> "
                 "--url=<url> --runtime-root=<path> "
                 "--open-tab=<url> --route-override=<persona_id:route_profile_id> "
                 "--route-engine-bin=<path> --artifact-scan-bin=<path>\n";
    return 1;
  }

  return RunBootstrap(launch_options);
}
