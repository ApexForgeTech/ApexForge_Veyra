#ifndef VEYRA_ENGINE_WEBKITGTK_BROWSER_ENGINE_H_
#define VEYRA_ENGINE_WEBKITGTK_BROWSER_ENGINE_H_

#include "veyra/engine/browser_engine.h"

#include <cstdint>
#include <unordered_map>

namespace veyra {

struct ArtifactScanReport;
struct WebKitGtkRuntimeState;

class WebKitGtkBrowserEngine : public BrowserEngine {
 public:
  WebKitGtkBrowserEngine();
  ~WebKitGtkBrowserEngine() override;

  bool CreateWindow(const BrowserEngineConfig& config, std::string* error) override;
  bool CreateTab(const std::string& tab_id, const std::string& initial_url, std::string* error) override;
  bool ActivateTab(const std::string& tab_id, std::string* error) override;
  bool Navigate(const std::string& tab_id, const std::string& url, std::string* error) override;
  bool GoBack(const std::string& tab_id, std::string* error) override;
  bool GoForward(const std::string& tab_id, std::string* error) override;
  bool ApplySecurityPolicy(const EngineSecurityPolicy& policy,
                           const std::vector<PermissionEvaluation>& permission_report,
                           std::string* error) override;
  std::string GetCurrentUrl(const std::string& tab_id) const override;
  void PushDashboardState(const std::string& state_json) const override;
  void RequestActiveTabText(
      std::function<void(const std::string& text)> callback) override;
  std::string GetActiveTabUrl() const override;
  std::string GetActiveTabTitle() const override;
  void Run() override;
  void RecordNavigation(const std::string& tab_id, const std::string& url, const std::string& title);
  const PermissionEvaluation* FindPermissionEvaluation(PermissionKind permission) const;
  std::string EmitArtifactEvent(const std::string& event_type,
                                const std::string& artifact_id,
                                const std::string& tab_id,
                                const std::string& details) const;
  void HandleDownloadStarted(void* download_ptr);
  bool DecideDownloadDestination(void* download_ptr, const std::string& suggested_filename);
  void UpdateDownloadProgress(void* download_ptr, std::uint64_t data_length);
  void MarkDownloadFailed(void* download_ptr, const std::string& error_message);
  void CompleteDownload(void* download_ptr);
  bool ResolvePermissionDecision(const std::string& tab_id,
                                 PermissionKind permission,
                                 const std::string& label,
                                 const std::string& request_details,
                                 std::string* message) const;
  bool HandlePermissionRequest(const std::string& tab_id, void* permission_request, std::string* message) const;
  bool ShouldAllowNavigation(const std::string& tab_id,
                             const std::string& uri,
                             std::string* message) const;
  void EmitPolicyEvent(const std::string& tab_id,
                       const std::string& category,
                       const std::string& message) const;
  void DispatchDashboardAction(const std::string& action_json) const;
  std::string DefaultRouteId() const;
  // Per-site permission override (camera/microphone/geolocation/notifications/
  // clipboard) set from the site-info popover; persists to the decision store.
  void SetSitePermissionDecision(const std::string& origin,
                                 const std::string& permission, bool allow);
  std::string ActiveRouteSummary() const;
  // Manual network proxy from Settings → Network; applies immediately + reloads.
  void SetManualProxy(const std::string& proxy_uri);
  // Switches the active persona at runtime: updates per-profile state (history
  // path, ephemeral, fingerprint/UA via security level) and re-applies to tabs.
  void SetActivePersona(const std::string& persona_id, bool ephemeral) override;
  // Re-applies the security/UA/JS settings to every open tab and reloads them
  // (used when the security level / UA mode changes from Settings).
  void ReapplySecurityToAllTabs();
  bool IsThirdPartyFrameBlocked(const std::string& tab_id,
                                const std::string& dest_uri,
                                std::string* message) const;
  void HandleControlLine(const std::string& line);
  void RunControlScript(const std::string& path);

 private:
  bool ApplyContextSecurityPolicy(std::string* error);
  bool ApplyNetworkProxySettings(const EngineSecurityPolicy& policy, std::string* error);
  bool EnsureDownloadDirectories(std::string* error) const;
  void InjectFingerprintScript(void* web_view) const;
  void InjectExtensionPolicyScript(void* web_view) const;
  void InjectExtensionContentScripts(void* web_view) const;
  bool PromptDownloadRelease(const ArtifactScanReport& report,
                             const std::string& released_path,
                             std::string* error) const;
  void ApplyViewSecurityPolicy(void* web_view);
  std::string ResolvePromptOrigin(const std::string& tab_id) const;
  bool FindCachedPromptDecision(const std::string& origin,
                                PermissionKind permission,
                                bool* allowed) const;
  void CachePromptDecision(const std::string& origin,
                           PermissionKind permission,
                           bool allowed) const;
  bool LoadPromptDecisionStore(std::string* error) const;
  bool FlushPromptDecisionStore(std::string* error) const;
  bool PromptPermissionDialog(const std::string& tab_id,
                              PermissionKind permission,
                              const std::string& label,
                              const std::string& request_details,
                              const std::string& rationale,
                              std::string* message) const;

  BrowserEngineConfig config_;
  WebKitGtkRuntimeState* state_ = nullptr;
  std::unordered_map<std::string, std::string> current_urls_;
};

}  // namespace veyra

#endif  // VEYRA_ENGINE_WEBKITGTK_BROWSER_ENGINE_H_
