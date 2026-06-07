#ifndef VEYRA_MODELS_FOUNDATION_MODELS_H_
#define VEYRA_MODELS_FOUNDATION_MODELS_H_

#include <string>
#include <vector>

namespace veyra {

struct PersonaDefinition {
  std::string id;
  std::string display_name;
  std::string storage_partition;
  std::string route_profile_id;
  std::string fingerprint_profile_id;
  std::string timezone_profile;
  std::string locale_profile;
  std::string extension_policy_id;
  std::string ai_memory_scope;
  std::string ai_policy_id;
  std::string osint_policy_id;
  std::string security_mode;
  bool ephemeral = false;
  std::string notes;
};

struct SecurityModeDefinition {
  std::string id;
  std::string display_name;
  std::string javascript_policy;
  std::string cookie_policy;
  std::string storage_policy;
  std::string webrtc_policy;
  std::string permission_policy;
  std::string download_policy;
  std::string routing_requirement;
  std::string history_retention_policy;
  std::string notes;
};

struct RouteProfileDefinition {
  std::string id;
  std::string display_name;
  std::string route_type;
  std::string dns_policy;
  std::string webrtc_policy;
  std::string leak_prevention_level;
  std::vector<std::string> hops;
  std::string notes;
};

struct FingerprintProfileDefinition {
  std::string id;
  std::string display_name;
  std::string canvas_noise;
  std::string webgl_vendor;
  std::string webgl_renderer;
  std::string audio_noise;
  int hardware_concurrency = 0;
  int device_memory = 0;
  std::string platform;
  int screen_width = 0;
  int screen_height = 0;
  int color_depth = 24;
  double device_pixel_ratio = 0.0;
  std::string notes;
};

struct AiPolicyDefinition {
  std::string id;
  std::string display_name;
  bool enabled = false;
  std::string model;
  std::string endpoint;
  bool allow_page_content = false;
  bool allow_script_analysis = false;
  bool allow_phishing_check = false;
  bool retain_memory = false;
  int max_input_chars = 0;
  std::string notes;
};

struct OsintPolicyDefinition {
  std::string id;
  std::string display_name;
  bool enabled = false;
  bool require_route = true;
  bool allow_whois = false;
  bool allow_dns = false;
  bool allow_archive = false;
  bool allow_username_search = false;
  bool allow_active_probing = false;
  int max_targets_per_case = 0;
  int max_username_sites = 0;
  bool retain_cases = false;
  std::string notes;
};

struct ExtensionPolicyDefinition {
  std::string id;
  std::string display_name;
  bool allow_eval = true;
  bool allow_third_party_frames = true;
  bool block_mixed_content = false;
  bool allow_external_fonts = true;
  std::vector<std::string> blocked_domains;
  std::string notes;
};

struct ToolDefinition {
  std::string id;
  std::string display_name;
  std::string category;
  std::string binary_path;
  std::vector<std::string> allowed_personas;
  std::vector<std::string> denied_personas;
  std::vector<std::string> denied_security_modes;
  int max_runtime_seconds = 60;
  bool output_to_quarantine = false;
  bool requires_network = false;
  std::string audit_level;
  std::string notes;
};

struct FoundationState {
  std::vector<PersonaDefinition> personas;
  std::vector<SecurityModeDefinition> security_modes;
  std::vector<RouteProfileDefinition> route_profiles;
  std::vector<FingerprintProfileDefinition> fingerprint_profiles;
  std::vector<ExtensionPolicyDefinition> extension_policies;
  std::vector<AiPolicyDefinition> ai_policies;
  std::vector<OsintPolicyDefinition> osint_policies;
  std::vector<ToolDefinition> tools;
};

}  // namespace veyra

#endif  // VEYRA_MODELS_FOUNDATION_MODELS_H_
