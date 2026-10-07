#ifndef VEYRA_RUNTIME_SESSION_LIFECYCLE_H_
#define VEYRA_RUNTIME_SESSION_LIFECYCLE_H_

#include "veyra/config/startup_config.h"
#include "veyra/runtime/profile_manager.h"

#include <cstddef>
#include <string>
#include <vector>

namespace veyra {

struct SessionLifecycleOptions {
  std::string runtime_root = ".veyra/runtime_sessions";
  bool keep_ephemeral_partitions = false;
  bool clean_stale_ephemeral_on_boot = true;
};

struct SessionAllocation {
  std::string session_id;
  std::string persona_id;
  std::string partition_id;
  bool persistent = false;
  std::string security_mode_id;
  std::string route_profile_id;
  std::string partition_root;
  std::string cookies_path;
  std::string storage_path;
  std::string cache_path;
  std::string downloads_path;
  std::string manifest_path;
};

class SessionLifecycleManager {
 public:
  explicit SessionLifecycleManager(SessionLifecycleOptions options);

  std::vector<ValidationIssue> Bootstrap(const ProfileManager& manager);
  const SessionAllocation* FindByPersonaId(const std::string& persona_id) const;
  const std::vector<SessionAllocation>& allocations() const;
  std::size_t ephemeral_partition_count() const;
  std::vector<ValidationIssue> CleanupEphemeral();

 private:
  std::vector<ValidationIssue> CleanupStaleEphemeralRoots();
  std::vector<ValidationIssue> PrepareProfileSession(const RuntimeProfile& profile);
  std::vector<ValidationIssue> WriteSessionManifest(const RuntimeProfile& profile,
                                                    const SessionAllocation& allocation) const;

  SessionLifecycleOptions options_;
  std::vector<SessionAllocation> allocations_;
  std::vector<std::string> ephemeral_paths_;
  bool bootstrapped_ = false;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_SESSION_LIFECYCLE_H_
