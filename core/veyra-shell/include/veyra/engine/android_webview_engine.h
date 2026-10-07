#ifndef VEYRA_ENGINE_ANDROID_WEBVIEW_ENGINE_H_
#define VEYRA_ENGINE_ANDROID_WEBVIEW_ENGINE_H_

#include "veyra/engine/browser_engine.h"
#include <jni.h>

namespace veyra {

class AndroidWebViewEngine : public BrowserEngine {
 public:
  AndroidWebViewEngine(JNIEnv* env, jobject main_activity);
  ~AndroidWebViewEngine() override;

  // BrowserEngine implementation
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
  void RequestActiveTabText(std::function<void(const std::string& text)> callback) override;
  std::string GetActiveTabUrl() const override;
  std::string GetActiveTabTitle() const override;
  void Run() override;

 private:
  JavaVM* java_vm_;
  jobject main_activity_ref_;
};

}  // namespace veyra

#endif  // VEYRA_ENGINE_ANDROID_WEBVIEW_ENGINE_H_
