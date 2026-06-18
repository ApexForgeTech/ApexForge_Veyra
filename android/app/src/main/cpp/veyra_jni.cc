// Veyra Android JNI bridge.
//
// The security-critical core runs in the SAME C++ engine the desktop shell
// uses: personas, security modes, routes, fingerprint and extension policy are
// loaded from the schemas/seed JSON (extracted to the app's files dir) by the
// shared runtime. Java only renders the mobile UI and drives the WebView; all
// profile/policy/fingerprint decisions come from here so behaviour matches
// desktop exactly.
#include <jni.h>
#include <android/log.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "veyra/engine/browser_engine.h"
#include "veyra/config/startup_config.h"
#include "veyra/models/foundation_models.h"
#include "veyra/runtime/profile_manager.h"
#include "veyra/runtime/security_policy_engine.h"
#include "veyra/runtime/permission_broker.h"
#include "veyra/runtime/fingerprint_engine.h"
#include "veyra/runtime/extension_engine.h"
#include "veyra/runtime/session_lifecycle.h"

#define LOG_TAG "VeyraJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::mutex g_mutex;
std::unique_ptr<veyra::ProfileManager> g_manager;
std::unique_ptr<veyra::SessionLifecycleManager> g_lifecycle;
std::string g_runtime_root;

std::string JniToStd(JNIEnv* env, jstring s) {
  if (s == nullptr) return {};
  const char* c = env->GetStringUTFChars(s, nullptr);
  std::string out(c ? c : "");
  if (c) env->ReleaseStringUTFChars(s, c);
  return out;
}

jstring StdToJni(JNIEnv* env, const std::string& s) {
  return env->NewStringUTF(s.c_str());
}

std::string JsonEscape(const std::string& v) {
  std::string o;
  o.reserve(v.size() + 8);
  for (char c : v) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          o += buf;
        } else {
          o += c;
        }
    }
  }
  return o;
}

std::string Q(const std::string& v) { return "\"" + JsonEscape(v) + "\""; }
std::string B(bool v) { return v ? "true" : "false"; }

// Builds an absolute schema/seed StartupConfig rooted at runtime_root (where
// Java extracted the assets/schemas tree).
veyra::StartupConfig ConfigFor(const std::string& root) {
  auto p = [&](const char* rel) { return root + "/" + rel; };
  veyra::StartupConfig c;
  c.persona_schema_path = p("schemas/persona/persona.schema.json");
  c.security_mode_schema_path = p("schemas/policy/security-mode.schema.json");
  c.route_profile_schema_path = p("schemas/route/route-profile.schema.json");
  c.fingerprint_profile_schema_path = p("schemas/fingerprint/fingerprint-profile.schema.json");
  c.extension_policy_schema_path = p("schemas/extension/extension-policy.schema.json");
  c.ai_policy_schema_path = p("schemas/ai/ai-policy.schema.json");
  c.osint_policy_schema_path = p("schemas/osint/osint-policy.schema.json");
  c.tool_schema_path = p("schemas/integration/tool.schema.json");
  c.seed_personas_path = p("schemas/persona/default-personas.json");
  c.seed_security_modes_path = p("schemas/policy/default-security-modes.json");
  c.seed_route_profiles_path = p("schemas/route/default-route-profiles.json");
  c.seed_fingerprint_profiles_path = p("schemas/fingerprint/default-fingerprint-profiles.json");
  c.seed_extension_policies_path = p("schemas/extension/default-extension-policies.json");
  c.seed_ai_policies_path = p("schemas/ai/default-ai-policies.json");
  c.seed_osint_policies_path = p("schemas/osint/default-osint-policies.json");
  c.seed_tools_path = p("schemas/integration/default-tools.json");
  return c;
}

// Resolves a SessionAllocation for a persona (from the lifecycle manager, or a
// reasonable default rooted at runtime_root).
veyra::SessionAllocation AllocationFor(const veyra::RuntimeProfile& profile) {
  if (g_lifecycle) {
    const veyra::SessionAllocation* a = g_lifecycle->FindByPersonaId(profile.persona.id);
    if (a != nullptr) return *a;
  }
  veyra::SessionAllocation a;
  a.persona_id = profile.persona.id;
  a.session_id = profile.persona.id + "-mobile";
  a.partition_id = profile.persona.storage_partition;
  a.persistent = !profile.persona.ephemeral;
  a.security_mode_id = profile.security_mode.id;
  a.route_profile_id = profile.route_profile.id;
  a.partition_root = g_runtime_root + "/sessions/" + profile.persona.id;
  a.cookies_path = a.partition_root + "/cookies.sqlite";
  a.storage_path = a.partition_root + "/storage";
  a.cache_path = a.partition_root + "/cache";
  a.downloads_path = a.partition_root + "/downloads";
  return a;
}

const veyra::RuntimeProfile* ResolveProfile(const std::string& persona_id) {
  if (!g_manager) return nullptr;
  const veyra::RuntimeProfile* p = g_manager->FindProfileById(persona_id);
  return p != nullptr ? p : g_manager->DefaultProfile();
}

std::string PersonaJson(const veyra::RuntimeProfile& p, bool active) {
  std::ostringstream o;
  o << "{"
    << "\"id\":" << Q(p.persona.id) << ","
    << "\"display_name\":" << Q(p.persona.display_name) << ","
    << "\"security_mode\":" << Q(p.security_mode.id) << ","
    << "\"route_profile_id\":" << Q(p.route_profile.id) << ","
    << "\"fingerprint_profile_id\":" << Q(p.fingerprint_profile.id) << ","
    << "\"extension_policy_id\":" << Q(p.extension_policy.id) << ","
    << "\"ephemeral\":" << B(p.persona.ephemeral) << ","
    << "\"active\":" << B(active)
    << "}";
  return o.str();
}

}  // namespace

extern "C" {

// Initialise the shared core from the extracted schema tree. Returns true when
// at least one persona loaded.
JNIEXPORT jboolean JNICALL
Java_com_apexforge_veyra_MainActivity_nativeInit(JNIEnv* env, jobject, jstring runtimeRoot) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_runtime_root = JniToStd(env, runtimeRoot);
  try {
    veyra::ProfileManagerBootstrapResult boot =
        veyra::BootstrapProfileManager(ConfigFor(g_runtime_root));
    g_manager = std::make_unique<veyra::ProfileManager>(std::move(boot.manager));

    veyra::SessionLifecycleOptions opts;
    opts.runtime_root = g_runtime_root + "/sessions";
    opts.keep_ephemeral_partitions = false;
    g_lifecycle = std::make_unique<veyra::SessionLifecycleManager>(opts);
    g_lifecycle->Bootstrap(*g_manager);

    const std::size_t n = g_manager->profiles().size();
    LOGI("Core bootstrapped from %s — %zu personas", g_runtime_root.c_str(), n);
    return n > 0 ? JNI_TRUE : JNI_FALSE;
  } catch (const std::exception& e) {
    LOGE("nativeInit failed: %s", e.what());
    return JNI_FALSE;
  } catch (...) {
    LOGE("nativeInit failed (unknown)");
    return JNI_FALSE;
  }
}

// JSON array of all personas (real, from schemas).
JNIEXPORT jstring JNICALL
Java_com_apexforge_veyra_MainActivity_nativeListPersonas(JNIEnv* env, jobject) {
  std::lock_guard<std::mutex> lock(g_mutex);
  std::ostringstream o;
  o << "[";
  if (g_manager) {
    bool first = true;
    for (const auto& p : g_manager->profiles()) {
      if (!first) o << ",";
      o << PersonaJson(p, false);
      first = false;
    }
  }
  o << "]";
  return StdToJni(env, o.str());
}

// Full VeyraState JSON for the React control panel, built from the real
// profile + engine-computed security policy + permission report.
JNIEXPORT jstring JNICALL
Java_com_apexforge_veyra_MainActivity_nativePersonaState(JNIEnv* env, jobject, jstring personaId) {
  std::lock_guard<std::mutex> lock(g_mutex);
  const std::string id = JniToStd(env, personaId);
  const veyra::RuntimeProfile* profile = ResolveProfile(id);
  if (g_manager == nullptr || profile == nullptr) {
    return StdToJni(env, "{}");
  }

  const veyra::SessionAllocation alloc = AllocationFor(*profile);
  const veyra::EngineSecurityPolicy policy =
      veyra::BuildEngineSecurityPolicy(*profile, alloc);
  const std::vector<veyra::PermissionEvaluation> perms =
      veyra::BuildPermissionReport(*profile);

  std::ostringstream o;
  o << "{";
  o << "\"active_persona\":" << PersonaJson(*profile, true) << ",";

  o << "\"all_personas\":[";
  { bool f = true; for (const auto& p : g_manager->profiles()) {
      if (!f) o << ","; o << PersonaJson(p, p.persona.id == profile->persona.id); f = false; } }
  o << "],";

  o << "\"active_route\":{"
    << "\"persona_id\":" << Q(profile->persona.id) << ","
    << "\"route_profile_id\":" << Q(profile->route_profile.id) << ","
    << "\"route_type\":" << Q(profile->route_profile.route_type) << ","
    << "\"health_status\":" << Q(policy.route_health_status.empty() ? "unknown" : policy.route_health_status) << ","
    << "\"proxy_uri\":" << Q(policy.route_proxy_uri) << ","
    << "\"dns_resolver\":" << Q(policy.route_dns_resolver) << ","
    << "\"leak_status\":" << Q(policy.route_leak_status.empty() ? "open" : policy.route_leak_status) << ","
    << "\"diagnostic_summary\":" << Q("Mobile session - " + profile->route_profile.route_type + " route")
    << "},";

  o << "\"all_route_profiles\":[";
  { bool f = true; for (const auto& r : g_manager->route_profiles()) {
      if (!f) o << ",";
      o << "{" << "\"id\":" << Q(r.id) << ",\"display_name\":" << Q(r.display_name)
        << ",\"route_type\":" << Q(r.route_type) << ",\"dns_policy\":" << Q(r.dns_policy)
        << ",\"leak_prevention_level\":" << Q(r.leak_prevention_level) << "}";
      f = false; } }
  o << "],";

  o << "\"security_mode_id\":" << Q(policy.mode_id) << ",";
  o << "\"security_mode_name\":" << Q(policy.mode_name) << ",";
  o << "\"fingerprint_profile_id\":" << Q(profile->fingerprint_profile.id) << ",";
  o << "\"extension_policy_id\":" << Q(profile->extension_policy.id) << ",";
  o << "\"allow_eval\":" << B(profile->extension_policy.allow_eval) << ",";
  o << "\"block_mixed_content\":" << B(profile->extension_policy.block_mixed_content) << ",";
  o << "\"blocked_domains_count\":" << profile->extension_policy.blocked_domains.size() << ",";

  o << "\"permissions\":[";
  { bool f = true; for (const auto& pe : perms) {
      if (!f) o << ",";
      o << "{\"permission\":" << Q(veyra::ToString(pe.permission))
        << ",\"decision\":" << Q(veyra::ToString(pe.decision))
        << ",\"rationale\":" << Q(pe.rationale) << "}";
      f = false; } }
  o << "],";

  o << "\"vault_events\":[],\"tools\":[],";

  const auto& ai = profile->ai_policy;
  o << "\"ai_policy\":{"
    << "\"id\":" << Q(ai.id) << ",\"display_name\":" << Q(ai.display_name)
    << ",\"enabled\":" << B(ai.enabled) << ",\"model\":" << Q(ai.model)
    << ",\"endpoint\":" << Q(ai.endpoint) << ",\"allow_page_content\":" << B(ai.allow_page_content)
    << ",\"allow_script_analysis\":" << B(ai.allow_script_analysis)
    << ",\"allow_phishing_check\":" << B(ai.allow_phishing_check)
    << ",\"retain_memory\":" << B(ai.retain_memory) << ",\"max_input_chars\":8000},";
  o << "\"ai_result\":null,";

  const auto& os = profile->osint_policy;
  o << "\"osint_policy\":{"
    << "\"id\":" << Q(os.id) << ",\"display_name\":" << Q(os.display_name)
    << ",\"enabled\":" << B(os.enabled) << ",\"require_route\":" << B(os.require_route)
    << ",\"allow_whois\":" << B(os.allow_whois) << ",\"allow_dns\":" << B(os.allow_dns)
    << ",\"allow_archive\":" << B(os.allow_archive)
    << ",\"allow_username_search\":" << B(os.allow_username_search)
    << ",\"allow_active_probing\":" << B(os.allow_active_probing)
    << ",\"max_username_sites\":30,\"retain_cases\":" << B(os.retain_cases) << "},";
  o << "\"osint_case\":{\"active\":false,\"case_id\":\"\",\"last_ok\":true,\"last_summary\":\"\","
    << "\"last_error\":\"\",\"egress\":" << Q(profile->route_profile.route_type)
    << ",\"route_type\":" << Q(profile->route_profile.route_type)
    << ",\"entities\":[],\"relationships\":[],\"timeline\":[]},";

  o << "\"runtime_root\":" << Q(g_runtime_root) << ",";
  o << "\"shell_version\":\"1.0-mobile\"";
  o << "}";
  return StdToJni(env, o.str());
}

// Per-persona fingerprint defence script (real, from the fingerprint engine).
JNIEXPORT jstring JNICALL
Java_com_apexforge_veyra_MainActivity_nativeFingerprintScript(JNIEnv* env, jobject, jstring personaId) {
  std::lock_guard<std::mutex> lock(g_mutex);
  const veyra::RuntimeProfile* profile = ResolveProfile(JniToStd(env, personaId));
  if (profile == nullptr) return StdToJni(env, "");
  const veyra::FingerprintPolicy fp =
      veyra::BuildFingerprintPolicy(profile->fingerprint_profile, profile->persona.id + "-mobile");
  return StdToJni(env, veyra::BuildFingerprintScript(fp));
}

// eval()/Function() blocker when the persona's extension policy forbids eval.
JNIEXPORT jstring JNICALL
Java_com_apexforge_veyra_MainActivity_nativeExtensionEvalBlock(JNIEnv* env, jobject, jstring personaId) {
  std::lock_guard<std::mutex> lock(g_mutex);
  const veyra::RuntimeProfile* profile = ResolveProfile(JniToStd(env, personaId));
  if (profile == nullptr || profile->extension_policy.allow_eval) return StdToJni(env, "");
  return StdToJni(env, veyra::BuildEvalBlockScript());
}

// Compact security flags Java applies directly to the WebView.
JNIEXPORT jstring JNICALL
Java_com_apexforge_veyra_MainActivity_nativeSecurityFlags(JNIEnv* env, jobject, jstring personaId) {
  std::lock_guard<std::mutex> lock(g_mutex);
  const veyra::RuntimeProfile* profile = ResolveProfile(JniToStd(env, personaId));
  if (profile == nullptr) return StdToJni(env, "{}");
  const veyra::SessionAllocation alloc = AllocationFor(*profile);
  const veyra::EngineSecurityPolicy policy = veyra::BuildEngineSecurityPolicy(*profile, alloc);
  std::ostringstream o;
  o << "{"
    << "\"ephemeral\":" << B(profile->persona.ephemeral) << ","
    << "\"javascript_policy\":" << Q(policy.javascript_policy) << ","
    << "\"cookie_policy\":" << Q(policy.cookie_policy) << ","
    << "\"block_mixed_content\":" << B(profile->extension_policy.block_mixed_content) << ","
    << "\"allow_eval\":" << B(profile->extension_policy.allow_eval) << ","
    << "\"allow_third_party_frames\":" << B(profile->extension_policy.allow_third_party_frames) << ","
    << "\"route_type\":" << Q(profile->route_profile.route_type) << ","
    << "\"routing_requirement\":" << Q(policy.routing_requirement)
    << "}";
  return StdToJni(env, o.str());
}

// Legacy entry point retained for compatibility (engine factory smoke check).
JNIEXPORT void JNICALL
Java_com_apexforge_veyra_MainActivity_initNativeBackend(JNIEnv* env, jobject activity) {
  SetAndroidJniContext(env, activity);
  std::string error;
  auto engine = veyra::CreateBrowserEngine(&error);
  if (!engine) {
    LOGE("CreateBrowserEngine: %s", error.c_str());
    return;
  }
  LOGI("Veyra native engine available on Android.");
}

}  // extern "C"
