#include "veyra/runtime/session_lifecycle.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace veyra {
namespace {

std::string SanitizePathToken(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());

  for (const char ch : value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.') {
      sanitized.push_back(ch);
      continue;
    }
    sanitized.push_back('_');
  }

  if (sanitized.empty()) {
    return "unknown";
  }

  return sanitized;
}

std::string BuildEphemeralFolderName(const RuntimeProfile& profile) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
  return SanitizePathToken(profile.persona.id) + "_" + std::to_string(stamp);
}

std::string BuildSessionId(const RuntimeProfile& profile) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
  return SanitizePathToken(profile.persona.id) + ".session." + std::to_string(stamp);
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

std::vector<ValidationIssue> EnsureDirectory(const std::filesystem::path& directory,
                                             const std::string& issue_path_label) {
  std::vector<ValidationIssue> issues;

  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    issues.push_back({issue_path_label,
                      "Failed to create directory: " + directory.string() +
                          " (" + error.message() + ")"});
  }

  return issues;
}

}  // namespace

SessionLifecycleManager::SessionLifecycleManager(SessionLifecycleOptions options)
    : options_(std::move(options)) {}

std::vector<ValidationIssue> SessionLifecycleManager::Bootstrap(const ProfileManager& manager) {
  std::vector<ValidationIssue> issues;

  bootstrapped_ = false;
  allocations_.clear();
  ephemeral_paths_.clear();

  const std::filesystem::path root(options_.runtime_root);
  std::vector<ValidationIssue> root_issues = EnsureDirectory(root, "session_lifecycle:root");
  issues.insert(issues.end(), root_issues.begin(), root_issues.end());
  if (!issues.empty()) {
    return issues;
  }

  const std::vector<ValidationIssue> persist_root_issues =
      EnsureDirectory(root / "persist", "session_lifecycle:persist_root");
  issues.insert(issues.end(), persist_root_issues.begin(), persist_root_issues.end());

  const std::vector<ValidationIssue> memory_root_issues =
      EnsureDirectory(root / "memory", "session_lifecycle:memory_root");
  issues.insert(issues.end(), memory_root_issues.begin(), memory_root_issues.end());
  if (!issues.empty()) {
    return issues;
  }

  if (options_.clean_stale_ephemeral_on_boot && !options_.keep_ephemeral_partitions) {
    const std::vector<ValidationIssue> cleanup_issues = CleanupStaleEphemeralRoots();
    issues.insert(issues.end(), cleanup_issues.begin(), cleanup_issues.end());
    if (!issues.empty()) {
      return issues;
    }
  }

  for (const RuntimeProfile& profile : manager.profiles()) {
    std::vector<ValidationIssue> profile_issues = PrepareProfileSession(profile);
    issues.insert(issues.end(), profile_issues.begin(), profile_issues.end());
  }

  if (issues.empty()) {
    bootstrapped_ = true;
  }

  return issues;
}

const SessionAllocation* SessionLifecycleManager::FindByPersonaId(const std::string& persona_id) const {
  for (const SessionAllocation& allocation : allocations_) {
    if (allocation.persona_id == persona_id) {
      return &allocation;
    }
  }
  return nullptr;
}

const std::vector<SessionAllocation>& SessionLifecycleManager::allocations() const {
  return allocations_;
}

std::size_t SessionLifecycleManager::ephemeral_partition_count() const {
  return ephemeral_paths_.size();
}

std::vector<ValidationIssue> SessionLifecycleManager::CleanupEphemeral() {
  std::vector<ValidationIssue> issues;

  if (!bootstrapped_ || options_.keep_ephemeral_partitions) {
    return issues;
  }

  for (const std::string& path : ephemeral_paths_) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
    if (error) {
      issues.push_back({"session_lifecycle:cleanup",
                        "Failed to remove ephemeral partition path: " + path +
                            " (" + error.message() + ")"});
    }
  }

  return issues;
}

std::vector<ValidationIssue> SessionLifecycleManager::CleanupStaleEphemeralRoots() {
  std::vector<ValidationIssue> issues;

  const std::filesystem::path memory_root =
      std::filesystem::path(options_.runtime_root) / "memory";
  std::error_code iterator_error;
  const auto end = std::filesystem::directory_iterator();
  for (std::filesystem::directory_iterator it(memory_root, iterator_error);
       !iterator_error && it != end;
       it.increment(iterator_error)) {
    const std::filesystem::directory_entry& entry = *it;
    if (!entry.is_directory()) {
      continue;
    }

    std::error_code remove_error;
    std::filesystem::remove_all(entry.path(), remove_error);
    if (remove_error) {
      issues.push_back({"session_lifecycle:stale_cleanup",
                        "Failed to remove stale ephemeral directory: " +
                            entry.path().string() + " (" + remove_error.message() + ")"});
    }
  }

  if (iterator_error) {
    issues.push_back({"session_lifecycle:stale_cleanup",
                      "Failed while scanning stale ephemeral directories: " +
                          iterator_error.message()});
  }

  return issues;
}

std::vector<ValidationIssue> SessionLifecycleManager::PrepareProfileSession(const RuntimeProfile& profile) {
  std::vector<ValidationIssue> issues;

  const std::filesystem::path root(options_.runtime_root);
  const bool persistent = profile.session_partition.is_persistent();
  const std::string partition_token = SanitizePathToken(profile.session_partition.id());

  std::filesystem::path partition_root;
  if (persistent) {
    partition_root = root / "persist" / partition_token;
  } else {
    partition_root = root / "memory" / BuildEphemeralFolderName(profile);
    ephemeral_paths_.push_back(partition_root.string());
  }

  std::filesystem::path cookies_path = partition_root / "cookies";
  std::filesystem::path storage_path = partition_root / "storage";
  std::filesystem::path cache_path = partition_root / "cache";
  std::filesystem::path downloads_path = partition_root / "downloads";
  std::filesystem::path manifest_path = partition_root / "session.json";

  const std::pair<std::filesystem::path, std::string> required_directories[] = {
      {partition_root, "partition_root"},
      {cookies_path, "cookies"},
      {storage_path, "storage"},
      {cache_path, "cache"},
      {downloads_path, "downloads"},
      {downloads_path / "quarantine", "downloads_quarantine"},
      {downloads_path / "released", "downloads_released"},
      {downloads_path / "reports", "downloads_reports"},
  };

  for (const auto& [directory, label] : required_directories) {
    std::vector<ValidationIssue> directory_issues =
        EnsureDirectory(directory,
                        "session_lifecycle:" + profile.persona.id + ":" + label);
    issues.insert(issues.end(), directory_issues.begin(), directory_issues.end());
  }

  if (!issues.empty()) {
    return issues;
  }

  allocations_.push_back(SessionAllocation{
      BuildSessionId(profile),
      profile.persona.id,
      profile.session_partition.id(),
      persistent,
      profile.security_mode.id,
      profile.route_profile.id,
      partition_root.string(),
      cookies_path.string(),
      storage_path.string(),
      cache_path.string(),
      downloads_path.string(),
      manifest_path.string(),
  });

  const SessionAllocation& allocation = allocations_.back();
  const std::vector<ValidationIssue> manifest_issues =
      WriteSessionManifest(profile, allocation);
  issues.insert(issues.end(), manifest_issues.begin(), manifest_issues.end());

  return issues;
}

std::vector<ValidationIssue> SessionLifecycleManager::WriteSessionManifest(
    const RuntimeProfile& profile, const SessionAllocation& allocation) const {
  std::vector<ValidationIssue> issues;

  std::ofstream output(allocation.manifest_path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    issues.push_back({"session_lifecycle:" + profile.persona.id + ":manifest",
                      "Failed to open session manifest for writing: " +
                          allocation.manifest_path});
    return issues;
  }

  output << "{\n"
         << "  \"session_id\": \"" << JsonEscape(allocation.session_id) << "\",\n"
         << "  \"persona_id\": \"" << JsonEscape(allocation.persona_id) << "\",\n"
         << "  \"persona_name\": \"" << JsonEscape(profile.persona.display_name) << "\",\n"
         << "  \"partition_id\": \"" << JsonEscape(allocation.partition_id) << "\",\n"
         << "  \"persistent\": " << (allocation.persistent ? "true" : "false") << ",\n"
         << "  \"security_mode_id\": \"" << JsonEscape(allocation.security_mode_id) << "\",\n"
         << "  \"route_profile_id\": \"" << JsonEscape(allocation.route_profile_id) << "\",\n"
         << "  \"route_type\": \"" << JsonEscape(profile.runtime_policy.route_type) << "\",\n"
         << "  \"partition_root\": \"" << JsonEscape(allocation.partition_root) << "\",\n"
         << "  \"cookies_path\": \"" << JsonEscape(allocation.cookies_path) << "\",\n"
         << "  \"storage_path\": \"" << JsonEscape(allocation.storage_path) << "\",\n"
         << "  \"cache_path\": \"" << JsonEscape(allocation.cache_path) << "\",\n"
         << "  \"downloads_path\": \"" << JsonEscape(allocation.downloads_path) << "\",\n"
         << "  \"history_retention_policy\": \""
         << JsonEscape(profile.runtime_policy.history_retention_policy) << "\",\n"
         << "  \"webrtc_policy\": \"" << JsonEscape(profile.runtime_policy.webrtc_policy) << "\"\n"
         << "}\n";

  if (!output.good()) {
    issues.push_back({"session_lifecycle:" + profile.persona.id + ":manifest",
                      "Failed to flush session manifest: " + allocation.manifest_path});
  }

  return issues;
}

}  // namespace veyra
