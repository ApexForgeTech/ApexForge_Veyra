#include "veyra/runtime/security_policy_engine.h"

#include <fstream>
#include <map>
#include <utility>

namespace veyra {
namespace {

std::string BuildModeSummary(const RuntimeProfile& profile) {
  return profile.security_mode.display_name + ": js=" + profile.runtime_policy.javascript_policy +
         ", cookies=" + profile.runtime_policy.cookie_policy +
         ", webrtc=" + profile.runtime_policy.webrtc_policy +
         ", dns=" + profile.runtime_policy.dns_policy +
         ", history=" + profile.runtime_policy.history_retention_policy;
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

bool IsStrictCookiePolicy(const std::string& cookie_policy) {
  return cookie_policy == "strict" || cookie_policy == "ephemeral_only";
}

bool IsDnsPrefetchAllowed(const RuntimeProfile& profile) {
  return profile.runtime_policy.dns_policy == "system" &&
         profile.runtime_policy.route_type == "direct";
}

bool IsHyperlinkAuditingAllowed(const RuntimeProfile& profile) {
  return profile.security_mode.id == "casual" &&
         profile.runtime_policy.leak_prevention_level == "standard";
}

}  // namespace

SecurityModeRegistry::SecurityModeRegistry() = default;

SecurityModeRegistry::SecurityModeRegistry(std::vector<SecurityModeDescriptor> descriptors)
    : descriptors_(std::move(descriptors)) {}

const std::vector<SecurityModeDescriptor>& SecurityModeRegistry::descriptors() const {
  return descriptors_;
}

const SecurityModeDescriptor* SecurityModeRegistry::FindById(const std::string& id) const {
  for (const SecurityModeDescriptor& descriptor : descriptors_) {
    if (descriptor.id == id) {
      return &descriptor;
    }
  }
  return nullptr;
}

SecurityModeRegistry BuildSecurityModeRegistry(const ProfileManager& manager) {
  std::map<std::string, SecurityModeDescriptor> descriptors;

  for (const RuntimeProfile& profile : manager.profiles()) {
    descriptors.emplace(profile.security_mode.id,
                        SecurityModeDescriptor{
                            profile.security_mode.id,
                            profile.security_mode.display_name,
                            profile.runtime_policy.javascript_policy,
                            profile.runtime_policy.cookie_policy,
                            profile.runtime_policy.webrtc_policy,
                            profile.runtime_policy.permission_policy,
                            profile.runtime_policy.dns_policy,
                            profile.runtime_policy.history_retention_policy,
                            BuildModeSummary(profile),
                        });
  }

  std::vector<SecurityModeDescriptor> registry_entries;
  registry_entries.reserve(descriptors.size());
  for (auto& [_, descriptor] : descriptors) {
    registry_entries.push_back(std::move(descriptor));
  }

  return SecurityModeRegistry(std::move(registry_entries));
}

std::vector<ValidationIssue> WriteSecurityModeRegistryReport(const SecurityModeRegistry& registry,
                                                             const std::string& report_path) {
  std::vector<ValidationIssue> issues;

  std::ofstream output(report_path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    issues.push_back({"security_registry",
                      "Failed to open security mode registry report for writing: " + report_path});
    return issues;
  }

  output << "{\n"
         << "  \"security_modes\": [\n";

  const std::vector<SecurityModeDescriptor>& descriptors = registry.descriptors();
  for (std::size_t index = 0; index < descriptors.size(); ++index) {
    const SecurityModeDescriptor& descriptor = descriptors[index];
    output << "    {\n"
           << "      \"id\": \"" << JsonEscape(descriptor.id) << "\",\n"
           << "      \"display_name\": \"" << JsonEscape(descriptor.display_name) << "\",\n"
           << "      \"javascript_policy\": \"" << JsonEscape(descriptor.javascript_policy) << "\",\n"
           << "      \"cookie_policy\": \"" << JsonEscape(descriptor.cookie_policy) << "\",\n"
           << "      \"webrtc_policy\": \"" << JsonEscape(descriptor.webrtc_policy) << "\",\n"
           << "      \"permission_policy\": \"" << JsonEscape(descriptor.permission_policy) << "\",\n"
           << "      \"dns_policy\": \"" << JsonEscape(descriptor.dns_policy) << "\",\n"
           << "      \"history_retention_policy\": \""
           << JsonEscape(descriptor.history_retention_policy) << "\",\n"
           << "      \"summary\": \"" << JsonEscape(descriptor.summary) << "\"\n"
           << "    }";
    if (index + 1 < descriptors.size()) {
      output << ",";
    }
    output << "\n";
  }

  output << "  ]\n"
         << "}\n";

  if (!output.good()) {
    issues.push_back({"security_registry",
                      "Failed to flush security mode registry report: " + report_path});
  }

  return issues;
}

EngineSecurityPolicy BuildEngineSecurityPolicy(const RuntimeProfile& profile,
                                               const SessionAllocation& allocation) {
  EngineSecurityPolicy policy;

  policy.mode_id = profile.security_mode.id;
  policy.mode_name = profile.security_mode.display_name;
  policy.summary = BuildModeSummary(profile);
  policy.javascript_policy = profile.runtime_policy.javascript_policy;
  policy.cookie_policy = profile.runtime_policy.cookie_policy;
  policy.webrtc_policy = profile.runtime_policy.webrtc_policy;
  policy.permission_policy = profile.runtime_policy.permission_policy;
  policy.routing_requirement = profile.runtime_policy.routing_requirement;
  policy.route_type = profile.runtime_policy.route_type;
  policy.download_policy = profile.runtime_policy.download_policy;
  policy.dns_policy = profile.runtime_policy.dns_policy;
  policy.history_retention_policy = profile.runtime_policy.history_retention_policy;
  policy.leak_prevention_level = profile.runtime_policy.leak_prevention_level;
  policy.proxy_mode = profile.runtime_policy.dns_policy == "system" ? "system-default" : "no-proxy-hook";
  policy.base_data_directory = allocation.storage_path;
  policy.base_cache_directory = allocation.cache_path;
  policy.cookie_storage_path = allocation.cookies_path + "/cookies.sqlite";
  policy.prompt_decision_store_path = allocation.partition_root + "/permission-decisions.db";
  policy.report_path = allocation.partition_root + "/security-policy.json";
  policy.download_root_directory = allocation.downloads_path;
  policy.quarantine_root_directory = allocation.downloads_path + "/quarantine";
  policy.released_root_directory = allocation.downloads_path + "/released";
  policy.artifact_report_directory = allocation.downloads_path + "/reports";
  policy.artifact_event_log_path = allocation.downloads_path + "/events.jsonl";
  policy.route_proxy_uri.clear();
  policy.route_dns_resolver.clear();
  policy.route_health_status = "unknown";
  policy.route_leak_status = "unknown";
  policy.use_ephemeral_context = !allocation.persistent;

  policy.enable_javascript = profile.runtime_policy.javascript_policy != "high_restrict";
  policy.enable_javascript_markup = profile.runtime_policy.javascript_policy == "allow";
  policy.enable_html5_local_storage = true;
  policy.enable_html5_database = true;
  policy.enable_media_stream = profile.runtime_policy.webrtc_policy != "disable";
  policy.enable_javascript_clipboard = profile.security_mode.id == "casual";
  policy.enable_dns_prefetching = IsDnsPrefetchAllowed(profile);
  policy.enable_hyperlink_auditing = IsHyperlinkAuditingAllowed(profile);
  policy.media_playback_requires_user_gesture = profile.security_mode.id != "casual";
  policy.enable_page_cache = profile.runtime_policy.history_retention_policy == "standard";
  policy.allow_file_access_from_file_urls = false;
  policy.allow_universal_access_from_file_urls = false;
  policy.persistent_credential_storage_enabled =
      allocation.persistent && profile.runtime_policy.persistent_storage_allowed &&
      profile.runtime_policy.cookie_policy != "ephemeral_only";
  policy.enable_itp = IsStrictCookiePolicy(profile.runtime_policy.cookie_policy);
  policy.strict_tls_errors = true;
  policy.enable_sandbox = true;
  policy.block_third_party_cookies = IsStrictCookiePolicy(profile.runtime_policy.cookie_policy);
  policy.allow_ip_literal_navigation = profile.runtime_policy.dns_policy == "system";
  policy.allow_localhost_navigation = profile.runtime_policy.routing_requirement == "none";
  policy.allow_private_network_navigation = profile.runtime_policy.routing_requirement == "none";
  policy.allow_non_web_schemes = false;
  policy.deny_autoplay = profile.security_mode.id != "casual";
  policy.is_i2p_route = profile.runtime_policy.route_type == "i2p";
  policy.fingerprint_profile_id = profile.fingerprint_profile.id;
  {
    const FingerprintPolicy fp_policy =
        BuildFingerprintPolicy(profile.fingerprint_profile, allocation.session_id);
    policy.fingerprint_script = BuildFingerprintScript(fp_policy);
  }
  {
    const ExtensionPolicy ext_policy = BuildExtensionPolicy(profile.extension_policy);
    policy.extension_policy_id = ext_policy.policy_id;
    policy.allow_eval = ext_policy.allow_eval;
    policy.allow_third_party_frames = ext_policy.allow_third_party_frames;
    policy.block_mixed_content = ext_policy.block_mixed_content;
    policy.allow_external_fonts = ext_policy.allow_external_fonts;
    policy.blocked_domains = ext_policy.blocked_domains;
    policy.eval_block_script = ext_policy.eval_block_script;
  }
  policy.max_history_entries = profile.runtime_policy.max_history_entries;

  return policy;
}

std::vector<ValidationIssue> WriteEngineSecurityPolicyReport(
    const RuntimeProfile& profile,
    const SessionAllocation& allocation,
    const EngineSecurityPolicy& policy,
    const std::vector<PermissionEvaluation>& permission_report) {
  std::vector<ValidationIssue> issues;

  std::ofstream output(policy.report_path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    issues.push_back({"security_policy:" + profile.persona.id,
                      "Failed to open security policy report for writing: " + policy.report_path});
    return issues;
  }

  output << "{\n"
         << "  \"persona_id\": \"" << JsonEscape(profile.persona.id) << "\",\n"
         << "  \"persona_name\": \"" << JsonEscape(profile.persona.display_name) << "\",\n"
         << "  \"session_id\": \"" << JsonEscape(allocation.session_id) << "\",\n"
         << "  \"mode_id\": \"" << JsonEscape(policy.mode_id) << "\",\n"
         << "  \"mode_name\": \"" << JsonEscape(policy.mode_name) << "\",\n"
         << "  \"summary\": \"" << JsonEscape(policy.summary) << "\",\n"
         << "  \"javascript_policy\": \"" << JsonEscape(policy.javascript_policy) << "\",\n"
         << "  \"cookie_policy\": \"" << JsonEscape(policy.cookie_policy) << "\",\n"
         << "  \"webrtc_policy\": \"" << JsonEscape(policy.webrtc_policy) << "\",\n"
         << "  \"permission_policy\": \"" << JsonEscape(policy.permission_policy) << "\",\n"
         << "  \"routing_requirement\": \"" << JsonEscape(policy.routing_requirement) << "\",\n"
         << "  \"route_type\": \"" << JsonEscape(policy.route_type) << "\",\n"
         << "  \"download_policy\": \"" << JsonEscape(policy.download_policy) << "\",\n"
         << "  \"dns_policy\": \"" << JsonEscape(policy.dns_policy) << "\",\n"
         << "  \"history_retention_policy\": \"" << JsonEscape(policy.history_retention_policy)
         << "\",\n"
         << "  \"leak_prevention_level\": \"" << JsonEscape(policy.leak_prevention_level) << "\",\n"
         << "  \"proxy_mode\": \"" << JsonEscape(policy.proxy_mode) << "\",\n"
         << "  \"prompt_decision_store_path\": \""
         << JsonEscape(policy.prompt_decision_store_path) << "\",\n"
         << "  \"download_root_directory\": \"" << JsonEscape(policy.download_root_directory)
         << "\",\n"
         << "  \"quarantine_root_directory\": \"" << JsonEscape(policy.quarantine_root_directory)
         << "\",\n"
         << "  \"released_root_directory\": \"" << JsonEscape(policy.released_root_directory)
         << "\",\n"
         << "  \"artifact_report_directory\": \"" << JsonEscape(policy.artifact_report_directory)
         << "\",\n"
         << "  \"artifact_event_log_path\": \"" << JsonEscape(policy.artifact_event_log_path)
         << "\",\n"
         << "  \"route_proxy_uri\": \"" << JsonEscape(policy.route_proxy_uri) << "\",\n"
         << "  \"route_dns_resolver\": \"" << JsonEscape(policy.route_dns_resolver) << "\",\n"
         << "  \"route_health_status\": \"" << JsonEscape(policy.route_health_status) << "\",\n"
         << "  \"route_leak_status\": \"" << JsonEscape(policy.route_leak_status) << "\",\n"
         << "  \"use_ephemeral_context\": " << (policy.use_ephemeral_context ? "true" : "false") << ",\n"
         << "  \"enable_javascript\": " << (policy.enable_javascript ? "true" : "false") << ",\n"
         << "  \"enable_javascript_markup\": "
         << (policy.enable_javascript_markup ? "true" : "false") << ",\n"
         << "  \"enable_html5_local_storage\": "
         << (policy.enable_html5_local_storage ? "true" : "false") << ",\n"
         << "  \"enable_html5_database\": "
         << (policy.enable_html5_database ? "true" : "false") << ",\n"
         << "  \"enable_media_stream\": " << (policy.enable_media_stream ? "true" : "false") << ",\n"
         << "  \"enable_javascript_clipboard\": "
         << (policy.enable_javascript_clipboard ? "true" : "false") << ",\n"
         << "  \"enable_dns_prefetching\": "
         << (policy.enable_dns_prefetching ? "true" : "false") << ",\n"
         << "  \"enable_hyperlink_auditing\": "
         << (policy.enable_hyperlink_auditing ? "true" : "false") << ",\n"
         << "  \"media_playback_requires_user_gesture\": "
         << (policy.media_playback_requires_user_gesture ? "true" : "false") << ",\n"
         << "  \"enable_page_cache\": " << (policy.enable_page_cache ? "true" : "false") << ",\n"
         << "  \"persistent_credential_storage_enabled\": "
         << (policy.persistent_credential_storage_enabled ? "true" : "false") << ",\n"
         << "  \"enable_itp\": " << (policy.enable_itp ? "true" : "false") << ",\n"
         << "  \"strict_tls_errors\": " << (policy.strict_tls_errors ? "true" : "false") << ",\n"
         << "  \"enable_sandbox\": " << (policy.enable_sandbox ? "true" : "false") << ",\n"
         << "  \"block_third_party_cookies\": "
         << (policy.block_third_party_cookies ? "true" : "false") << ",\n"
         << "  \"allow_ip_literal_navigation\": "
         << (policy.allow_ip_literal_navigation ? "true" : "false") << ",\n"
         << "  \"allow_localhost_navigation\": "
         << (policy.allow_localhost_navigation ? "true" : "false") << ",\n"
         << "  \"allow_private_network_navigation\": "
         << (policy.allow_private_network_navigation ? "true" : "false") << ",\n"
         << "  \"deny_autoplay\": " << (policy.deny_autoplay ? "true" : "false") << ",\n"
         << "  \"is_i2p_route\": " << (policy.is_i2p_route ? "true" : "false") << ",\n"
         << "  \"fingerprint_profile_id\": \"" << JsonEscape(policy.fingerprint_profile_id)
         << "\",\n"
         << "  \"fingerprint_script_injected\": "
         << (policy.fingerprint_script.empty() ? "false" : "true") << ",\n"
         << "  \"extension_policy_id\": \"" << JsonEscape(policy.extension_policy_id) << "\",\n"
         << "  \"allow_eval\": " << (policy.allow_eval ? "true" : "false") << ",\n"
         << "  \"allow_third_party_frames\": "
         << (policy.allow_third_party_frames ? "true" : "false") << ",\n"
         << "  \"block_mixed_content\": " << (policy.block_mixed_content ? "true" : "false")
         << ",\n"
         << "  \"allow_external_fonts\": "
         << (policy.allow_external_fonts ? "true" : "false") << ",\n"
         << "  \"blocked_domains_count\": " << policy.blocked_domains.size() << ",\n"
         << "  \"eval_block_script_injected\": "
         << (policy.eval_block_script.empty() ? "false" : "true") << ",\n"
         << "  \"max_history_entries\": " << policy.max_history_entries << ",\n"
         << "  \"permissions\": [\n";

  for (std::size_t index = 0; index < permission_report.size(); ++index) {
    const PermissionEvaluation& evaluation = permission_report[index];
    output << "    {\n"
           << "      \"permission\": \"" << JsonEscape(ToString(evaluation.permission)) << "\",\n"
           << "      \"decision\": \"" << JsonEscape(ToString(evaluation.decision)) << "\",\n"
           << "      \"rationale\": \"" << JsonEscape(evaluation.rationale) << "\"\n"
           << "    }";
    if (index + 1 < permission_report.size()) {
      output << ",";
    }
    output << "\n";
  }

  output << "  ]\n"
         << "}\n";

  if (!output.good()) {
    issues.push_back({"security_policy:" + profile.persona.id,
                      "Failed to flush security policy report: " + policy.report_path});
  }

  return issues;
}

}  // namespace veyra
