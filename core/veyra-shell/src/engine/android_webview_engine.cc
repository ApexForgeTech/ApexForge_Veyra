#include "veyra/engine/android_webview_engine.h"
#include <android/log.h>

#define LOG_TAG "VeyraAndroidEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace veyra {

AndroidWebViewEngine::AndroidWebViewEngine(JNIEnv* env, jobject main_activity) {
    env->GetJavaVM(&java_vm_);
    main_activity_ref_ = env->NewGlobalRef(main_activity);
    LOGI("AndroidWebViewEngine created and linked to MainActivity JNI.");
}

AndroidWebViewEngine::~AndroidWebViewEngine() {
    JNIEnv* env;
    if (java_vm_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
        env->DeleteGlobalRef(main_activity_ref_);
    }
}

bool AndroidWebViewEngine::CreateWindow(const BrowserEngineConfig& /*config*/, std::string* /*error*/) {
    LOGI("AndroidWebViewEngine: CreateWindow");
    return true;
}

bool AndroidWebViewEngine::CreateTab(const std::string& /*tab_id*/, const std::string& /*initial_url*/, std::string* /*error*/) {
    LOGI("AndroidWebViewEngine: CreateTab");
    return true;
}

bool AndroidWebViewEngine::ActivateTab(const std::string& /*tab_id*/, std::string* /*error*/) {
    LOGI("AndroidWebViewEngine: ActivateTab");
    return true;
}

bool AndroidWebViewEngine::Navigate(const std::string& /*tab_id*/, const std::string& url, std::string* /*error*/) {
    LOGI("Backend requesting navigation to: %s", url.c_str());
    
    JNIEnv* env;
    bool attached = false;
    int res = java_vm_->GetEnv((void**)&env, JNI_VERSION_1_6);
    
    if (res == JNI_EDETACHED) {
        if (java_vm_->AttachCurrentThread(&env, NULL) != 0) {
            LOGE("Failed to attach thread for navigation.");
            return false;
        }
        attached = true;
    }

    jclass mainActivityClass = env->FindClass("com/apexforge/veyra/MainActivity");
    if (mainActivityClass) {
        jmethodID navigateId = env->GetStaticMethodID(mainActivityClass, "navigateToUrl", "(Ljava/lang/String;)V");
        if (navigateId) {
            jstring jurl = env->NewStringUTF(url.c_str());
            env->CallStaticVoidMethod(mainActivityClass, navigateId, jurl);
            env->DeleteLocalRef(jurl);
        }
    }

    if (attached) java_vm_->DetachCurrentThread();
    return true;
}

bool AndroidWebViewEngine::GoBack(const std::string& /*tab_id*/, std::string* /*error*/) {
    return true;
}

bool AndroidWebViewEngine::GoForward(const std::string& /*tab_id*/, std::string* /*error*/) {
    return true;
}

bool AndroidWebViewEngine::ApplySecurityPolicy(const EngineSecurityPolicy& /*policy*/,
                                               const std::vector<PermissionEvaluation>& /*permission_report*/,
                                               std::string* /*error*/) {
    LOGI("AndroidWebViewEngine: ApplySecurityPolicy");
    return true;
}

std::string AndroidWebViewEngine::GetCurrentUrl(const std::string& /*tab_id*/) const {
    return "android://current";
}

void AndroidWebViewEngine::PushDashboardState(const std::string& /*state_json*/) const {
    LOGI("AndroidWebViewEngine: PushDashboardState");
}

void AndroidWebViewEngine::RequestActiveTabText(std::function<void(const std::string& text)> callback) {
    callback("Sample page text from Android");
}

std::string AndroidWebViewEngine::GetActiveTabUrl() const {
    return "android://local_session";
}

std::string AndroidWebViewEngine::GetActiveTabTitle() const {
    return "Veyra Mobile";
}

void AndroidWebViewEngine::Run() {
    LOGI("Veyra Mobile Engine Loop Started.");
}

}  // namespace veyra
