#include "veyra/runtime/shell_ui_bridge.h"

#include "veyra/serialization/json.h"

#include <fstream>
#include <sstream>
#include <string>

namespace veyra {
namespace {

std::string J(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');
  for (const char ch : value) {
    switch (ch) {
      case '"':  escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\n': escaped += "\\n";  break;
      case '\r': escaped += "\\r";  break;
      case '\t': escaped += "\\t";  break;
      default:   escaped.push_back(ch); break;
    }
  }
  escaped.push_back('"');
  return escaped;
}

std::string Jb(bool value) { return value ? "true" : "false"; }
std::string Ji(int value) { return std::to_string(value); }
std::string Jsz(std::size_t value) { return std::to_string(value); }

std::vector<std::string> ReadRecentVaultEvents(const std::string& events_path,
                                               std::size_t max_count) {
  std::vector<std::string> lines;
  std::ifstream input(events_path);
  if (!input.is_open()) {
    return lines;
  }
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    // Each line is embedded verbatim into the dashboard state JSON array.
    // A single corrupted line would break the whole document, so validate
    // that the line parses as a JSON object before trusting it.
    const JsonParseResult parsed = ParseJson(line);
    if (parsed.error.empty() && parsed.value.IsObject()) {
      lines.push_back(line);
    }
  }
  if (lines.size() > max_count) {
    lines.erase(lines.begin(), lines.begin() +
                static_cast<std::ptrdiff_t>(lines.size() - max_count));
  }
  return lines;
}

bool GetJsonString(const JsonValue::Object& obj, const std::string& key, std::string& out) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->second.IsString()) return false;
  out = it->second.AsString();
  return true;
}

}  // namespace

bool ParseDashboardAction(const std::string& json, DashboardAction* action,
                           std::string* error) {
  if (action == nullptr) return false;
  JsonParseResult parsed = ParseJson(json);
  if (!parsed.error.empty()) {
    if (error != nullptr) *error = "JSON parse error: " + parsed.error;
    return false;
  }
  if (!parsed.value.IsObject()) {
    if (error != nullptr) *error = "Dashboard action must be a JSON object.";
    return false;
  }
  const JsonValue::Object& obj = parsed.value.AsObject();

  if (!GetJsonString(obj, "action", action->action)) {
    if (error != nullptr) *error = "Dashboard action missing 'action' field.";
    return false;
  }

  GetJsonString(obj, "persona_id", action->persona_id);
  GetJsonString(obj, "route_profile_id", action->route_profile_id);
  GetJsonString(obj, "tool_id", action->tool_id);
  GetJsonString(obj, "url", action->navigate_url);
  GetJsonString(obj, "payload", action->ai_payload);
  GetJsonString(obj, "target", action->osint_target);

  const auto args_it = obj.find("args");
  if (args_it != obj.end() && args_it->second.IsArray()) {
    for (const JsonValue& item : args_it->second.AsArray()) {
      if (item.IsString()) {
        action->tool_args.push_back(item.AsString());
      }
    }
  }

  return true;
}

std::string SerializeDashboardState(
    const ProfileManager& manager,
    const RuntimeProfile& active_profile,
    const RouteServiceClient& route_service,
    const EngineSecurityPolicy& active_policy,
    const std::vector<PermissionEvaluation>& permissions,
    const std::vector<ToolDefinition>& tools,
    const std::string& persona_id,
    const std::string& runtime_root,
    const std::string& shell_version,
    const AiResultSnapshot& ai_result,
    const OsintCaseSnapshot& osint_case) {
  std::ostringstream out;
  out << "{\n";

  // Active persona
  out << "\"active_persona\":{"
      << "\"id\":" << J(active_profile.persona.id) << ","
      << "\"display_name\":" << J(active_profile.persona.display_name) << ","
      << "\"security_mode\":" << J(active_profile.persona.security_mode) << ","
      << "\"route_profile_id\":" << J(active_profile.persona.route_profile_id) << ","
      << "\"fingerprint_profile_id\":" << J(active_profile.persona.fingerprint_profile_id) << ","
      << "\"extension_policy_id\":" << J(active_profile.persona.extension_policy_id) << ","
      << "\"ephemeral\":" << Jb(active_profile.persona.ephemeral) << ","
      << "\"active\":true"
      << "},\n";

  // All personas
  out << "\"all_personas\":[\n";
  const std::vector<RuntimeProfile>& profiles = manager.profiles();
  for (std::size_t i = 0; i < profiles.size(); ++i) {
    const RuntimeProfile& p = profiles[i];
    if (i > 0) out << ",\n";
    out << "{"
        << "\"id\":" << J(p.persona.id) << ","
        << "\"display_name\":" << J(p.persona.display_name) << ","
        << "\"security_mode\":" << J(p.persona.security_mode) << ","
        << "\"route_profile_id\":" << J(p.persona.route_profile_id) << ","
        << "\"fingerprint_profile_id\":" << J(p.persona.fingerprint_profile_id) << ","
        << "\"extension_policy_id\":" << J(p.persona.extension_policy_id) << ","
        << "\"ephemeral\":" << Jb(p.persona.ephemeral) << ","
        << "\"active\":" << Jb(p.persona.id == persona_id)
        << "}";
  }
  out << "],\n";

  // Active route
  const RouteRuntimeState* route = route_service.FindByPersonaId(persona_id);
  if (route != nullptr) {
    out << "\"active_route\":{"
        << "\"persona_id\":" << J(route->persona_id) << ","
        << "\"route_profile_id\":" << J(route->route_profile_id) << ","
        << "\"route_type\":" << J(route->route_type) << ","
        << "\"health_status\":" << J(route->health_status) << ","
        << "\"proxy_uri\":" << J(route->proxy_uri) << ","
        << "\"dns_resolver\":" << J(route->dns_resolver) << ","
        << "\"leak_status\":" << J(route->leak_status) << ","
        << "\"diagnostic_summary\":" << J(route->diagnostic_summary)
        << "},\n";
  } else {
    out << "\"active_route\":{"
        << "\"persona_id\":" << J(persona_id) << ","
        << "\"route_profile_id\":" << J(active_profile.route_profile.id) << ","
        << "\"route_type\":" << J(active_profile.route_profile.route_type) << ","
        << "\"health_status\":\"unknown\","
        << "\"proxy_uri\":\"\","
        << "\"dns_resolver\":\"system-resolver\","
        << "\"leak_status\":\"unknown\","
        << "\"diagnostic_summary\":\"Route state not yet available.\""
        << "},\n";
  }

  // All route profiles
  out << "\"all_route_profiles\":[\n";
  const std::vector<RouteProfileDefinition>& route_profiles = manager.route_profiles();
  for (std::size_t i = 0; i < route_profiles.size(); ++i) {
    const RouteProfileDefinition& rp = route_profiles[i];
    if (i > 0) out << ",\n";
    out << "{"
        << "\"id\":" << J(rp.id) << ","
        << "\"display_name\":" << J(rp.display_name) << ","
        << "\"route_type\":" << J(rp.route_type) << ","
        << "\"dns_policy\":" << J(rp.dns_policy) << ","
        << "\"leak_prevention_level\":" << J(rp.leak_prevention_level)
        << "}";
  }
  out << "],\n";

  // Security mode
  out << "\"security_mode_id\":" << J(active_policy.mode_id) << ",\n"
      << "\"security_mode_name\":" << J(active_policy.mode_name) << ",\n";

  // Fingerprint / extension
  out << "\"fingerprint_profile_id\":" << J(active_policy.fingerprint_profile_id) << ",\n"
      << "\"extension_policy_id\":" << J(active_policy.extension_policy_id) << ",\n"
      << "\"allow_eval\":" << Jb(active_policy.allow_eval) << ",\n"
      << "\"block_mixed_content\":" << Jb(active_policy.block_mixed_content) << ",\n"
      << "\"blocked_domains_count\":" << Jsz(active_policy.blocked_domains.size()) << ",\n";

  // Permissions
  out << "\"permissions\":[\n";
  for (std::size_t i = 0; i < permissions.size(); ++i) {
    const PermissionEvaluation& perm = permissions[i];
    if (i > 0) out << ",\n";
    out << "{"
        << "\"permission\":" << J(ToString(perm.permission)) << ","
        << "\"decision\":" << J(ToString(perm.decision)) << ","
        << "\"rationale\":" << J(perm.rationale)
        << "}";
  }
  out << "],\n";

  // Recent vault events
  const std::string events_path = active_policy.artifact_event_log_path;
  const std::vector<std::string> event_lines = ReadRecentVaultEvents(events_path, 30);
  out << "\"vault_events\":[\n";
  for (std::size_t i = 0; i < event_lines.size(); ++i) {
    if (i > 0) out << ",\n";
    // Events are already JSONL objects — embed them directly
    out << event_lines[i];
  }
  out << "],\n";

  // Tools with policy check
  out << "\"tools\":[\n";
  ToolBridgeClient bridge("", tools);
  for (std::size_t i = 0; i < tools.size(); ++i) {
    const ToolDefinition& t = tools[i];
    if (i > 0) out << ",\n";

    ToolInvocationRequest check_req;
    check_req.tool_id = t.id;
    check_req.persona_id = active_profile.persona.id;
    check_req.security_mode_id = active_profile.security_mode.id;
    check_req.route_type = active_profile.runtime_policy.route_type;

    std::string denial;
    const bool allowed = bridge.CheckPolicy(check_req, &denial);

    out << "{"
        << "\"id\":" << J(t.id) << ","
        << "\"display_name\":" << J(t.display_name) << ","
        << "\"category\":" << J(t.category) << ","
        << "\"allowed\":" << Jb(allowed) << ","
        << "\"denial_reason\":" << J(denial)
        << "}";
  }
  out << "],\n";

  // AI policy posture for the active persona.
  const AiPolicyDefinition& ai = active_profile.ai_policy;
  out << "\"ai_policy\":{"
      << "\"id\":" << J(ai.id) << ","
      << "\"display_name\":" << J(ai.display_name) << ","
      << "\"enabled\":" << Jb(ai.enabled) << ","
      << "\"model\":" << J(ai.model) << ","
      << "\"endpoint\":" << J(ai.endpoint) << ","
      << "\"allow_page_content\":" << Jb(ai.allow_page_content) << ","
      << "\"allow_script_analysis\":" << Jb(ai.allow_script_analysis) << ","
      << "\"allow_phishing_check\":" << Jb(ai.allow_phishing_check) << ","
      << "\"retain_memory\":" << Jb(ai.retain_memory) << ","
      << "\"max_input_chars\":" << Ji(ai.max_input_chars)
      << "},\n";

  // Most recent AI result (if any).
  out << "\"ai_result\":";
  if (ai_result.has_result) {
    out << "{"
        << "\"method\":" << J(ai_result.method) << ","
        << "\"text\":" << J(ai_result.text) << ","
        << "\"risk_level\":" << J(ai_result.risk_level) << ","
        << "\"risk_score\":" << Ji(ai_result.risk_score) << ","
        << "\"source\":" << J(ai_result.source) << ","
        << "\"ok\":" << Jb(ai_result.ok) << ","
        << "\"error\":" << J(ai_result.error)
        << "}";
  } else {
    out << "null";
  }
  out << ",\n";

  // OSINT policy posture for the active persona.
  const OsintPolicyDefinition& op = active_profile.osint_policy;
  out << "\"osint_policy\":{"
      << "\"id\":" << J(op.id) << ","
      << "\"display_name\":" << J(op.display_name) << ","
      << "\"enabled\":" << Jb(op.enabled) << ","
      << "\"require_route\":" << Jb(op.require_route) << ","
      << "\"allow_whois\":" << Jb(op.allow_whois) << ","
      << "\"allow_dns\":" << Jb(op.allow_dns) << ","
      << "\"allow_archive\":" << Jb(op.allow_archive) << ","
      << "\"allow_username_search\":" << Jb(op.allow_username_search) << ","
      << "\"allow_active_probing\":" << Jb(op.allow_active_probing) << ","
      << "\"max_username_sites\":" << Ji(op.max_username_sites) << ","
      << "\"retain_cases\":" << Jb(op.retain_cases)
      << "},\n";

  // OSINT investigation case.
  out << "\"osint_case\":{"
      << "\"active\":" << Jb(osint_case.active) << ","
      << "\"case_id\":" << J(osint_case.case_id) << ","
      << "\"last_ok\":" << Jb(osint_case.last_ok) << ","
      << "\"last_summary\":" << J(osint_case.last_summary) << ","
      << "\"last_error\":" << J(osint_case.last_error) << ","
      << "\"egress\":" << J(osint_case.egress) << ","
      << "\"route_type\":" << J(osint_case.route_type) << ",\n";

  out << "\"entities\":[";
  for (std::size_t i = 0; i < osint_case.entities.size(); ++i) {
    const OsintEntity& e = osint_case.entities[i];
    if (i > 0) out << ",";
    out << "{"
        << "\"type\":" << J(e.type) << ","
        << "\"value\":" << J(e.value) << ","
        << "\"source\":" << J(e.source) << ","
        << "\"attributes\":" << (e.attributes_json.empty() ? "{}" : e.attributes_json)
        << "}";
  }
  out << "],\n";

  out << "\"relationships\":[";
  for (std::size_t i = 0; i < osint_case.relationships.size(); ++i) {
    const OsintRelationship& r2 = osint_case.relationships[i];
    if (i > 0) out << ",";
    out << "{"
        << "\"from\":" << J(r2.from) << ","
        << "\"to\":" << J(r2.to) << ","
        << "\"kind\":" << J(r2.kind)
        << "}";
  }
  out << "],\n";

  out << "\"timeline\":[";
  for (std::size_t i = 0; i < osint_case.timeline.size(); ++i) {
    const OsintTimelineEvent& t = osint_case.timeline[i];
    if (i > 0) out << ",";
    out << "{"
        << "\"ts_ms\":" << std::to_string(t.ts_ms) << ","
        << "\"event\":" << J(t.event) << ","
        << "\"detail\":" << J(t.detail)
        << "}";
  }
  out << "]"
      << "},\n";

  out << "\"runtime_root\":" << J(runtime_root) << ",\n"
      << "\"shell_version\":" << J(shell_version) << "\n"
      << "}";

  return out.str();
}

std::string BuildStateInjectionScript(const std::string& state_json) {
  // Inject state BEFORE the React app boots. The app reads window.__VEYRA_STATE__
  // on mount, then replaces window.__VEYRA_UPDATE__ with its own setState-backed
  // handler in a useEffect. Until that runs, this bootstrap handler just stores
  // the latest state so a late-mounting React reads the freshest value.
  return
    "(function(){\n"
    "  try{\n"
    "    window.__VEYRA_STATE__=" + state_json + ";\n"
    "  }catch(e){\n"
    "    console.error('[Veyra] Failed to parse dashboard state:',e);\n"
    "  }\n"
    "  if(typeof window.__VEYRA_UPDATE__!=='function'){\n"
    "    window.__VEYRA_UPDATE__=function(s){window.__VEYRA_STATE__=s;};\n"
    "  }\n"
    "})();\n";
}

}  // namespace veyra
