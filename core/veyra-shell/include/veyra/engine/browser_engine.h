#ifndef VEYRA_ENGINE_BROWSER_ENGINE_H_
#define VEYRA_ENGINE_BROWSER_ENGINE_H_

#include "veyra/runtime/permission_broker.h"
#include "veyra/runtime/security_policy_engine.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace veyra {

struct BrowserEngineConfig {
  std::string window_title = "ApexForge Veyra | Native Shell";
  int width = 1400;
  int height = 900;
  EngineSecurityPolicy security_policy;
  std::string artifact_scan_binary_path;
  std::string fingerprint_script;

  // Persona this window runs as. Scopes per-profile on-disk state (history,
  // and the cookie store via the security policy) so profiles never mix.
  std::string persona_id;
  // True for ghost/ephemeral personas — history is never written for them.
  bool ephemeral_persona = false;
  // User-Agent strategy: "auto" (branded for casual, blend-in for hardened),
  // "branded" (always show Veyra), or "blendin" (always uniform anti-fingerprint).
  std::string user_agent_mode = "auto";

  // Phase 9: Dashboard panel configuration.
  // shell_ui_dist_path: path to the built React app's dist/ directory.
  // dashboard_state_script: pre-built JS that injects __VEYRA_STATE__ before React boots.
  std::string shell_ui_dist_path;
  std::string dashboard_state_script;

  // Phase: automation / self-test. When set, the shell opens this path as a
  // FIFO and reads line commands (navigate/route/snapshot/url/tab-new/quit),
  // enabling headless-style driving + GDK window snapshots for verification.
  std::string control_fifo_path;

  // Loaded browser-extension content scripts (JS bodies), injected into every
  // browsing tab at document-start. Populated from the extensions/ directory.
  std::vector<std::string> extension_content_scripts;

  std::vector<PermissionEvaluation> permission_report;
  std::function<void(const std::string& tab_id,
                     const std::string& url,
                     const std::string& title)> on_navigation_committed;
  std::function<void(const std::string& tab_id,
                     const std::string& category,
                     const std::string& message)> on_policy_event;
  // Receives JSON action objects from the React dashboard.
  std::function<void(const std::string& action_json)> on_dashboard_action;
};

class BrowserEngine {
 public:
  virtual ~BrowserEngine() = default;

  virtual bool CreateWindow(const BrowserEngineConfig& config, std::string* error) = 0;
  virtual bool CreateTab(const std::string& tab_id, const std::string& initial_url, std::string* error) = 0;
  virtual bool ActivateTab(const std::string& tab_id, std::string* error) = 0;
  virtual bool Navigate(const std::string& tab_id, const std::string& url, std::string* error) = 0;
  virtual bool GoBack(const std::string& tab_id, std::string* error) = 0;
  virtual bool GoForward(const std::string& tab_id, std::string* error) = 0;
  virtual bool ApplySecurityPolicy(const EngineSecurityPolicy& policy,
                                   const std::vector<PermissionEvaluation>& permission_report,
                                   std::string* error) = 0;
  virtual std::string GetCurrentUrl(const std::string& tab_id) const = 0;
  // Optional: runtime persona switch (engines that don't support it no-op).
  virtual void SetActivePersona(const std::string& /*persona_id*/, bool /*ephemeral*/) {}
  virtual void PushDashboardState(const std::string& state_json) const = 0;
  virtual void RequestActiveTabText(
      std::function<void(const std::string& text)> callback) = 0;
  virtual std::string GetActiveTabUrl() const = 0;
  virtual std::string GetActiveTabTitle() const = 0;
  virtual void Run() = 0;
};

std::unique_ptr<BrowserEngine> CreateBrowserEngine(std::string* error);

}  // namespace veyra

#ifdef __ANDROID__
#include <jni.h>
extern "C" void SetAndroidJniContext(JNIEnv* env, jobject activity);
#endif

#endif  // VEYRA_ENGINE_BROWSER_ENGINE_H_
