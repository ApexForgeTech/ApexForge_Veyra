#include "veyra/config/foundation_loader.h"
#include "veyra/config/schema_validator.h"

#include "veyra/serialization/json.h"

#include <fstream>
#include <functional>
#include <optional>
#include <sstream>

namespace veyra {
namespace {

std::optional<std::string> ReadFile(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

const JsonValue* FindProperty(const JsonValue::Object& object, const std::string& key) {
  const auto found = object.find(key);
  if (found == object.end()) {
    return nullptr;
  }
  return &found->second;
}

std::optional<std::string> RequireString(const JsonValue::Object& object,
                                         const std::string& key,
                                         const std::string& path,
                                         std::vector<ValidationIssue>* issues) {
  const JsonValue* value = FindProperty(object, key);
  if (value == nullptr) {
    issues->push_back({path, "Missing string property '" + key + "'."});
    return std::nullopt;
  }
  if (!value->IsString()) {
    issues->push_back({path, "Property '" + key + "' must be a string."});
    return std::nullopt;
  }
  return value->AsString();
}

std::optional<bool> RequireBool(const JsonValue::Object& object,
                                const std::string& key,
                                const std::string& path,
                                std::vector<ValidationIssue>* issues) {
  const JsonValue* value = FindProperty(object, key);
  if (value == nullptr) {
    issues->push_back({path, "Missing boolean property '" + key + "'."});
    return std::nullopt;
  }
  if (!value->IsBool()) {
    issues->push_back({path, "Property '" + key + "' must be a boolean."});
    return std::nullopt;
  }
  return value->AsBool();
}

std::optional<std::vector<std::string>> RequireStringArray(const JsonValue::Object& object,
                                                           const std::string& key,
                                                           const std::string& path,
                                                           std::vector<ValidationIssue>* issues) {
  const JsonValue* value = FindProperty(object, key);
  if (value == nullptr) {
    issues->push_back({path, "Missing array property '" + key + "'."});
    return std::nullopt;
  }
  if (!value->IsArray()) {
    issues->push_back({path, "Property '" + key + "' must be an array."});
    return std::nullopt;
  }

  std::vector<std::string> strings;
  for (const JsonValue& item : value->AsArray()) {
    if (!item.IsString()) {
      issues->push_back({path, "Property '" + key + "' must contain only strings."});
      return std::nullopt;
    }
    strings.push_back(item.AsString());
  }
  return strings;
}

template <typename Item>
using ParseFunction = std::function<std::optional<Item>(const JsonValue::Object&,
                                                        const std::string&,
                                                        std::vector<ValidationIssue>*)>;

template <typename Item>
std::vector<Item> ParseTopLevelArray(const JsonValue& root,
                                     const std::string& path,
                                     std::vector<ValidationIssue>* issues,
                                     const ParseFunction<Item>& parser) {
  std::vector<Item> items;
  if (!root.IsArray()) {
    issues->push_back({path, "Root document must be a JSON array."});
    return items;
  }

  const JsonValue::Array& array = root.AsArray();
  for (std::size_t index = 0; index < array.size(); ++index) {
    const JsonValue& item = array[index];
    const std::string item_path = path + "[" + std::to_string(index) + "]";
    if (!item.IsObject()) {
      issues->push_back({item_path, "Array entries must be JSON objects."});
      continue;
    }

    std::optional<Item> parsed = parser(item.AsObject(), item_path, issues);
    if (parsed.has_value()) {
      items.push_back(*parsed);
    }
  }

  return items;
}

std::optional<PersonaDefinition> ParsePersona(const JsonValue::Object& object,
                                              const std::string& path,
                                              std::vector<ValidationIssue>* issues) {
  PersonaDefinition persona;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);
  auto storage_partition = RequireString(object, "storage_partition", path, issues);
  auto route_profile_id = RequireString(object, "route_profile_id", path, issues);
  auto fingerprint_profile_id = RequireString(object, "fingerprint_profile_id", path, issues);
  auto timezone_profile = RequireString(object, "timezone_profile", path, issues);
  auto locale_profile = RequireString(object, "locale_profile", path, issues);
  auto extension_policy_id = RequireString(object, "extension_policy_id", path, issues);
  auto ai_memory_scope = RequireString(object, "ai_memory_scope", path, issues);
  auto ai_policy_id = RequireString(object, "ai_policy_id", path, issues);
  auto osint_policy_id = RequireString(object, "osint_policy_id", path, issues);
  auto security_mode = RequireString(object, "security_mode", path, issues);
  auto ephemeral = RequireBool(object, "ephemeral", path, issues);

  if (!id || !display_name || !storage_partition || !route_profile_id ||
      !fingerprint_profile_id || !timezone_profile || !locale_profile ||
      !extension_policy_id || !ai_memory_scope || !ai_policy_id || !osint_policy_id ||
      !security_mode || !ephemeral) {
    return std::nullopt;
  }

  persona.id = *id;
  persona.display_name = *display_name;
  persona.storage_partition = *storage_partition;
  persona.route_profile_id = *route_profile_id;
  persona.fingerprint_profile_id = *fingerprint_profile_id;
  persona.timezone_profile = *timezone_profile;
  persona.locale_profile = *locale_profile;
  persona.extension_policy_id = *extension_policy_id;
  persona.ai_memory_scope = *ai_memory_scope;
  persona.ai_policy_id = *ai_policy_id;
  persona.osint_policy_id = *osint_policy_id;
  persona.security_mode = *security_mode;
  persona.ephemeral = *ephemeral;

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    persona.notes = notes->AsString();
  }

  return persona;
}

std::optional<SecurityModeDefinition> ParseSecurityMode(const JsonValue::Object& object,
                                                        const std::string& path,
                                                        std::vector<ValidationIssue>* issues) {
  SecurityModeDefinition mode;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);
  auto javascript_policy = RequireString(object, "javascript_policy", path, issues);
  auto cookie_policy = RequireString(object, "cookie_policy", path, issues);
  auto storage_policy = RequireString(object, "storage_policy", path, issues);
  auto webrtc_policy = RequireString(object, "webrtc_policy", path, issues);
  auto permission_policy = RequireString(object, "permission_policy", path, issues);
  auto download_policy = RequireString(object, "download_policy", path, issues);
  auto routing_requirement = RequireString(object, "routing_requirement", path, issues);
  auto history_retention_policy = RequireString(object, "history_retention_policy", path, issues);

  if (!id || !display_name || !javascript_policy || !cookie_policy || !storage_policy ||
      !webrtc_policy || !permission_policy || !download_policy || !routing_requirement ||
      !history_retention_policy) {
    return std::nullopt;
  }

  mode.id = *id;
  mode.display_name = *display_name;
  mode.javascript_policy = *javascript_policy;
  mode.cookie_policy = *cookie_policy;
  mode.storage_policy = *storage_policy;
  mode.webrtc_policy = *webrtc_policy;
  mode.permission_policy = *permission_policy;
  mode.download_policy = *download_policy;
  mode.routing_requirement = *routing_requirement;
  mode.history_retention_policy = *history_retention_policy;

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    mode.notes = notes->AsString();
  }

  return mode;
}

std::optional<FingerprintProfileDefinition> ParseFingerprintProfile(const JsonValue::Object& object,
                                                                    const std::string& path,
                                                                    std::vector<ValidationIssue>* issues) {
  FingerprintProfileDefinition profile;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);
  auto canvas_noise = RequireString(object, "canvas_noise", path, issues);
  auto webgl_vendor = RequireString(object, "webgl_vendor", path, issues);
  auto webgl_renderer = RequireString(object, "webgl_renderer", path, issues);
  auto audio_noise = RequireString(object, "audio_noise", path, issues);
  auto platform = RequireString(object, "platform", path, issues);

  if (!id || !display_name || !canvas_noise || !webgl_vendor || !webgl_renderer ||
      !audio_noise || !platform) {
    return std::nullopt;
  }

  profile.id = *id;
  profile.display_name = *display_name;
  profile.canvas_noise = *canvas_noise;
  profile.webgl_vendor = *webgl_vendor;
  profile.webgl_renderer = *webgl_renderer;
  profile.audio_noise = *audio_noise;
  profile.platform = *platform;

  auto read_int_field = [&](const std::string& key, int& out) {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr) {
      issues->push_back({path, "Missing numeric property '" + key + "'."});
      return false;
    }
    if (!v->IsNumber()) {
      issues->push_back({path, "Property '" + key + "' must be a number."});
      return false;
    }
    out = static_cast<int>(v->AsNumber());
    return true;
  };

  auto read_double_field = [&](const std::string& key, double& out) {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr) {
      issues->push_back({path, "Missing numeric property '" + key + "'."});
      return false;
    }
    if (!v->IsNumber()) {
      issues->push_back({path, "Property '" + key + "' must be a number."});
      return false;
    }
    out = v->AsNumber();
    return true;
  };

  if (!read_int_field("hardware_concurrency", profile.hardware_concurrency) ||
      !read_int_field("device_memory", profile.device_memory) ||
      !read_int_field("screen_width", profile.screen_width) ||
      !read_int_field("screen_height", profile.screen_height) ||
      !read_int_field("color_depth", profile.color_depth) ||
      !read_double_field("device_pixel_ratio", profile.device_pixel_ratio)) {
    return std::nullopt;
  }

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    profile.notes = notes->AsString();
  }

  return profile;
}

std::optional<RouteProfileDefinition> ParseRouteProfile(const JsonValue::Object& object,
                                                        const std::string& path,
                                                        std::vector<ValidationIssue>* issues) {
  RouteProfileDefinition route;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);
  auto route_type = RequireString(object, "route_type", path, issues);
  auto dns_policy = RequireString(object, "dns_policy", path, issues);
  auto webrtc_policy = RequireString(object, "webrtc_policy", path, issues);
  auto leak_prevention_level = RequireString(object, "leak_prevention_level", path, issues);
  auto hops = RequireStringArray(object, "hops", path, issues);

  if (!id || !display_name || !route_type || !dns_policy || !webrtc_policy ||
      !leak_prevention_level || !hops) {
    return std::nullopt;
  }

  route.id = *id;
  route.display_name = *display_name;
  route.route_type = *route_type;
  route.dns_policy = *dns_policy;
  route.webrtc_policy = *webrtc_policy;
  route.leak_prevention_level = *leak_prevention_level;
  route.hops = *hops;

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    route.notes = notes->AsString();
  }

  return route;
}

std::optional<ExtensionPolicyDefinition> ParseExtensionPolicy(const JsonValue::Object& object,
                                                                const std::string& path,
                                                                std::vector<ValidationIssue>* issues) {
  ExtensionPolicyDefinition policy;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);

  if (!id || !display_name) {
    return std::nullopt;
  }

  policy.id = *id;
  policy.display_name = *display_name;

  auto load_bool = [&](const std::string& key, bool& out) -> bool {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr) {
      issues->push_back({path, "Missing boolean property '" + key + "'."});
      return false;
    }
    if (!v->IsBool()) {
      issues->push_back({path, "Property '" + key + "' must be a boolean."});
      return false;
    }
    out = v->AsBool();
    return true;
  };

  if (!load_bool("allow_eval", policy.allow_eval) ||
      !load_bool("allow_third_party_frames", policy.allow_third_party_frames) ||
      !load_bool("block_mixed_content", policy.block_mixed_content) ||
      !load_bool("allow_external_fonts", policy.allow_external_fonts)) {
    return std::nullopt;
  }

  if (const JsonValue* v = FindProperty(object, "blocked_domains");
      v != nullptr && v->IsArray()) {
    for (const JsonValue& item : v->AsArray()) {
      if (item.IsString()) {
        policy.blocked_domains.push_back(item.AsString());
      }
    }
  }

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    policy.notes = notes->AsString();
  }

  return policy;
}

std::optional<AiPolicyDefinition> ParseAiPolicy(const JsonValue::Object& object,
                                                const std::string& path,
                                                std::vector<ValidationIssue>* issues) {
  AiPolicyDefinition policy;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);
  auto model = RequireString(object, "model", path, issues);
  auto endpoint = RequireString(object, "endpoint", path, issues);

  if (!id || !display_name || !model || !endpoint) {
    return std::nullopt;
  }

  policy.id = *id;
  policy.display_name = *display_name;
  policy.model = *model;
  policy.endpoint = *endpoint;

  auto load_bool = [&](const std::string& key, bool& out) -> bool {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr) {
      issues->push_back({path, "Missing boolean property '" + key + "'."});
      return false;
    }
    if (!v->IsBool()) {
      issues->push_back({path, "Property '" + key + "' must be a boolean."});
      return false;
    }
    out = v->AsBool();
    return true;
  };

  if (!load_bool("enabled", policy.enabled) ||
      !load_bool("allow_page_content", policy.allow_page_content) ||
      !load_bool("allow_script_analysis", policy.allow_script_analysis) ||
      !load_bool("allow_phishing_check", policy.allow_phishing_check) ||
      !load_bool("retain_memory", policy.retain_memory)) {
    return std::nullopt;
  }

  if (const JsonValue* v = FindProperty(object, "max_input_chars");
      v != nullptr && v->IsNumber()) {
    policy.max_input_chars = static_cast<int>(v->AsNumber());
  } else {
    issues->push_back({path, "Missing numeric property 'max_input_chars'."});
    return std::nullopt;
  }

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    policy.notes = notes->AsString();
  }

  return policy;
}

std::optional<OsintPolicyDefinition> ParseOsintPolicy(const JsonValue::Object& object,
                                                      const std::string& path,
                                                      std::vector<ValidationIssue>* issues) {
  OsintPolicyDefinition policy;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);
  if (!id || !display_name) {
    return std::nullopt;
  }
  policy.id = *id;
  policy.display_name = *display_name;

  auto load_bool = [&](const std::string& key, bool& out) -> bool {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr || !v->IsBool()) {
      issues->push_back({path, "Missing or non-boolean property '" + key + "'."});
      return false;
    }
    out = v->AsBool();
    return true;
  };
  auto load_int = [&](const std::string& key, int& out) -> bool {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr || !v->IsNumber()) {
      issues->push_back({path, "Missing or non-numeric property '" + key + "'."});
      return false;
    }
    out = static_cast<int>(v->AsNumber());
    return true;
  };

  if (!load_bool("enabled", policy.enabled) ||
      !load_bool("require_route", policy.require_route) ||
      !load_bool("allow_whois", policy.allow_whois) ||
      !load_bool("allow_dns", policy.allow_dns) ||
      !load_bool("allow_archive", policy.allow_archive) ||
      !load_bool("allow_username_search", policy.allow_username_search) ||
      !load_bool("allow_active_probing", policy.allow_active_probing) ||
      !load_bool("retain_cases", policy.retain_cases) ||
      !load_int("max_targets_per_case", policy.max_targets_per_case) ||
      !load_int("max_username_sites", policy.max_username_sites)) {
    return std::nullopt;
  }

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    policy.notes = notes->AsString();
  }

  return policy;
}

std::optional<ToolDefinition> ParseTool(const JsonValue::Object& object,
                                        const std::string& path,
                                        std::vector<ValidationIssue>* issues) {
  ToolDefinition tool;

  auto id = RequireString(object, "id", path, issues);
  auto display_name = RequireString(object, "display_name", path, issues);
  auto category = RequireString(object, "category", path, issues);
  auto binary_path = RequireString(object, "binary_path", path, issues);
  auto audit_level = RequireString(object, "audit_level", path, issues);

  if (!id || !display_name || !category || !binary_path || !audit_level) {
    return std::nullopt;
  }

  tool.id = *id;
  tool.display_name = *display_name;
  tool.category = *category;
  tool.binary_path = *binary_path;
  tool.audit_level = *audit_level;

  auto load_string_array = [&](const std::string& key, std::vector<std::string>& out) {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr || !v->IsArray()) {
      return;
    }
    for (const JsonValue& item : v->AsArray()) {
      if (item.IsString()) {
        out.push_back(item.AsString());
      }
    }
  };

  load_string_array("allowed_personas", tool.allowed_personas);
  load_string_array("denied_personas", tool.denied_personas);
  load_string_array("denied_security_modes", tool.denied_security_modes);

  if (const JsonValue* v = FindProperty(object, "max_runtime_seconds"); v != nullptr && v->IsNumber()) {
    tool.max_runtime_seconds = static_cast<int>(v->AsNumber());
  } else {
    issues->push_back({path, "Missing numeric property 'max_runtime_seconds'."});
    return std::nullopt;
  }

  auto load_bool = [&](const std::string& key, bool& out) -> bool {
    const JsonValue* v = FindProperty(object, key);
    if (v == nullptr) {
      issues->push_back({path, "Missing boolean property '" + key + "'."});
      return false;
    }
    if (!v->IsBool()) {
      issues->push_back({path, "Property '" + key + "' must be a boolean."});
      return false;
    }
    out = v->AsBool();
    return true;
  };

  if (!load_bool("output_to_quarantine", tool.output_to_quarantine) ||
      !load_bool("requires_network", tool.requires_network)) {
    return std::nullopt;
  }

  if (const JsonValue* notes = FindProperty(object, "notes"); notes != nullptr && notes->IsString()) {
    tool.notes = notes->AsString();
  }

  return tool;
}

template <typename Item>
std::vector<Item> LoadArrayFile(const std::string& path,
                                std::vector<ValidationIssue>* issues,
                                const ParseFunction<Item>& parser) {
  const std::optional<std::string> content = ReadFile(path);
  if (!content.has_value()) {
    issues->push_back({path, "Unable to read file."});
    return {};
  }

  JsonParseResult parsed = ParseJson(*content);
  if (!parsed.error.empty()) {
    issues->push_back({path, parsed.error});
    return {};
  }

  return ParseTopLevelArray<Item>(parsed.value, path, issues, parser);
}

}  // namespace

LoadResult LoadFoundation(const StartupConfig& config) {
  LoadResult result;
  result.issues = ValidateStartupConfig(config);
  if (!result.issues.empty()) {
    return result;
  }

  result.issues = ValidateSeedDataAgainstSchemas(config);
  if (!result.issues.empty()) {
    return result;
  }

  result.state.personas =
      LoadArrayFile<PersonaDefinition>(config.seed_personas_path, &result.issues, ParsePersona);
  result.state.security_modes = LoadArrayFile<SecurityModeDefinition>(
      config.seed_security_modes_path, &result.issues, ParseSecurityMode);
  result.state.route_profiles = LoadArrayFile<RouteProfileDefinition>(
      config.seed_route_profiles_path, &result.issues, ParseRouteProfile);
  result.state.fingerprint_profiles = LoadArrayFile<FingerprintProfileDefinition>(
      config.seed_fingerprint_profiles_path, &result.issues, ParseFingerprintProfile);
  result.state.extension_policies = LoadArrayFile<ExtensionPolicyDefinition>(
      config.seed_extension_policies_path, &result.issues, ParseExtensionPolicy);
  result.state.ai_policies = LoadArrayFile<AiPolicyDefinition>(
      config.seed_ai_policies_path, &result.issues, ParseAiPolicy);
  result.state.osint_policies = LoadArrayFile<OsintPolicyDefinition>(
      config.seed_osint_policies_path, &result.issues, ParseOsintPolicy);
  result.state.tools =
      LoadArrayFile<ToolDefinition>(config.seed_tools_path, &result.issues, ParseTool);

  return result;
}

}  // namespace veyra
