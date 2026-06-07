#ifndef VEYRA_RUNTIME_SHELL_UI_BRIDGE_H_
#define VEYRA_RUNTIME_SHELL_UI_BRIDGE_H_

#include "veyra/models/foundation_models.h"
#include "veyra/runtime/osint_workspace_client.h"
#include "veyra/runtime/profile_manager.h"
#include "veyra/runtime/route_service.h"
#include "veyra/runtime/security_policy_engine.h"
#include "veyra/runtime/tool_bridge_client.h"

#include <string>
#include <vector>

namespace veyra {

struct DashboardAction {
  std::string action;
  std::string route_profile_id;
  std::string tool_id;
  std::vector<std::string> tool_args;
  std::string navigate_url;
  std::string ai_payload;    // operator-provided text for ai_explain_script
  std::string osint_target;  // domain / ip / username for osint_* actions
};

struct AiResultSnapshot {
  bool has_result = false;
  std::string method;       // summarize | phishing_check | explain_script | ping
  std::string text;         // primary textual output (summary / explanation / detail)
  std::string risk_level;   // phishing_check only
  int risk_score = 0;       // phishing_check only
  std::string source;       // model | offline | heuristic
  bool ok = true;
  std::string error;
};

// The accumulated OSINT investigation case, owned by the shell and serialized
// into the dashboard. Lookups merge their entities/relationships/timeline here.
struct OsintCaseSnapshot {
  bool active = false;
  std::string case_id;
  std::vector<OsintEntity> entities;
  std::vector<OsintRelationship> relationships;
  std::vector<OsintTimelineEvent> timeline;
  // Status of the most recent lookup:
  bool last_ok = true;
  std::string last_summary;
  std::string last_error;
  std::string egress;       // proxy | direct | blocked (from the last lookup/ping)
  std::string route_type;
};

bool ParseDashboardAction(const std::string& json, DashboardAction* action, std::string* error);

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
    const AiResultSnapshot& ai_result = AiResultSnapshot{},
    const OsintCaseSnapshot& osint_case = OsintCaseSnapshot{});

std::string BuildStateInjectionScript(const std::string& state_json);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_SHELL_UI_BRIDGE_H_
