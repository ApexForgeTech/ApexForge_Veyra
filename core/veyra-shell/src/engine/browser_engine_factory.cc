#include "veyra/engine/browser_engine.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include "veyra/engine/webkitgtk_browser_engine.h"
#elif defined(__ANDROID__)
#include "veyra/engine/android_webview_engine.h"
#endif

#ifdef __ANDROID__
#include <jni.h>

// Global variables for Android JNI context
static JNIEnv* g_android_jni_env = nullptr;
static jobject g_android_main_activity = nullptr;

extern "C" void SetAndroidJniContext(JNIEnv* env, jobject activity) {
    g_android_jni_env = env;
    g_android_main_activity = activity;
}
#endif

namespace veyra {

std::unique_ptr<BrowserEngine> CreateBrowserEngine(std::string* error) {
#if defined(__linux__) && !defined(__ANDROID__)
  return std::make_unique<WebKitGtkBrowserEngine>();
#elif defined(__ANDROID__)
  if (!g_android_jni_env || !g_android_main_activity) {
      if (error) *error = "Android JNI context not initialized";
      return nullptr;
  }
  return std::make_unique<AndroidWebViewEngine>(g_android_jni_env, g_android_main_activity);
#elif defined(_WIN32)
  // TODO: Implement WebView2Engine for Windows
  if (error) *error = "Windows engine is not yet implemented (Phase: WebView2 Setup)";
  return nullptr;
#else
  if (error) *error = "Unsupported platform for Veyra Native Shell";
  return nullptr;
#endif
}

}  // namespace veyra
