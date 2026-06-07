#ifndef VEYRA_RUNTIME_SECURITY_POLICY_ENGINE_H_
#define VEYRA_RUNTIME_SECURITY_POLICY_ENGINE_H_

#include "veyra/config/startup_config.h"
#include "veyra/runtime/extension_engine.h"
#include "veyra/runtime/fingerprint_engine.h"
#include "veyra/runtime/permission_broker.h"
#include "veyra/runtime/profile_manager.h"
#include "veyra/runtime/session_lifecycle.h"

#include <string>
#include <vector>

namespace veyra {

struct SecurityModeDescriptor {
  std::string id;
  std::string display_name;
  std::string javascript_policy;
  std::string cookie_policy;
  std::string webrtc_policy;
  std::string permission_policy;
  std::string dns_policy;
  std::string history_retention_policy;
  std::string summary;
};

class SecurityModeRegistry {
 public:
  SecurityModeRegistry();
  explicit SecurityModeRegistry(std::vector<SecurityModeDescriptor> descriptors);

  const std::vector<SecurityModeDescriptor>& descriptors() const;
  const SecurityModeDescriptor* FindById(const std::string& id) const;

 private:
  std::vector<SecurityModeDescriptor> descriptors_;
};

struct EngineSecurityPolicy {
  std::string mode_id;
  std::string mode_name;
  std::string summary;
  std::string javascript_policy;
  std::string cookie_policy;
  std::string webrtc_policy;
  std::string permission_policy;
  std::string routing_requirement;
  std::string route_type;
  std::string download_policy;
  std::string dns_policy;
  std::string history_retention_policy;
  std::string leak_prevention_level;
  std::string proxy_mode;
  std::string base_data_directory;
  std::string base_cache_directory;
  std::string cookie_storage_path;
  std::string prompt_decision_store_path;
  std::string report_path;
  std::string download_root_directory;
  std::string quarantine_root_directory;
  std::string released_root_directory;
  std::string artifact_report_directory;
  std::string artifact_event_log_path;
  std::string route_proxy_uri;
  std::string route_dns_resolver;
  std::string route_health_status;
  std::string route_leak_status;
  bool use_ephemeral_context = false;
  bool enable_javascript = true;
  bool enable_javascript_markup = true;
  bool enable_html5_local_storage = true;
  bool enable_html5_database = true;
  bool enable_media_stream = true;
  bool enable_javascript_clipboard = false;
  bool enable_dns_prefetching = false;
  bool enable_hyperlink_auditing = false;
  bool media_playback_requires_user_gesture = false;
  bool enable_page_cache = true;
  bool allow_file_access_from_file_urls = false;
  bool allow_universal_access_from_file_urls = false;
  bool persistent_credential_storage_enabled = true;
  bool enable_itp = false;
  bool strict_tls_errors = true;
  bool enable_sandbox = true;
  bool block_third_party_cookies = false;
  bool allow_ip_literal_navigation = true;
  bool allow_localhost_navigation = true;
  bool allow_private_network_navigation = true;
  bool allow_non_web_schemes = false;
  bool deny_autoplay = false;
  bool is_i2p_route = false;
  std::string fingerprint_profile_id;
  std::string fingerprint_script;
  std::string extension_policy_id;
  bool allow_eval = true;
  bool allow_third_party_frames = true;
  bool block_mixed_content = false;
  bool allow_external_fonts = true;
  std::vector<std::string> blocked_domains;
  std::string eval_block_script;
  std::size_t max_history_entries = 1;
};

SecurityModeRegistry BuildSecurityModeRegistry(const ProfileManager& manager);
std::vector<ValidationIssue> WriteSecurityModeRegistryReport(const SecurityModeRegistry& registry,
                                                             const std::string& report_path);
EngineSecurityPolicy BuildEngineSecurityPolicy(const RuntimeProfile& profile,
                                               const SessionAllocation& allocation);
std::vector<ValidationIssue> WriteEngineSecurityPolicyReport(
    const RuntimeProfile& profile,
    const SessionAllocation& allocation,
    const EngineSecurityPolicy& policy,
    const std::vector<PermissionEvaluation>& permission_report);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_SECURITY_POLICY_ENGINE_H_
