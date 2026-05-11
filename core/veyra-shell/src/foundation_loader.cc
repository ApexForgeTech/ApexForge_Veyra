#include "veyra/foundation_loader.h"

#include "veyra/json.h"

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
  for (std::size_t index = 0; index < value->AsArray().size(); ++index) {
    const JsonValue& item = value->AsArray()[index];
    if (!item.IsString()) {
      issues->push_back({path, "Property '" + key + "' must contain only strings."});
      return std::nullopt;
    }
    strings.push_back(item.AsString());
  }
  return strings;
}

template <typename Item>
std::vector<Item> ParseTopLevelArray(const JsonValue& root,
                                     const std::string& path,
                                     std::vector<ValidationIssue>* issues,
                                     const std::function<std::optional<Item>(const JsonValue::Object&,
                                                                              const std::string&,
                                                                              std::vector<ValidationIssue>*)>& parser) {
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
  auto security_mode = RequireString(object, "security_mode", path, issues);
  auto ephemeral = RequireBool(object, "ephemeral", path, issues);

  if (!id || !display_name || !storage_partition || !route_profile_id ||
      !fingerprint_profile_id || !timezone_profile || !locale_profile ||
      !extension_policy_id || !ai_memory_scope || !security_mode || !ephemeral) {
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

template <typename Item>
std::vector<Item> LoadArrayFile(const std::string& path,
                                std::vector<ValidationIssue>* issues,
                                const std::function<std::optional<Item>(const JsonValue::Object&,
                                                                         const std::string&,
                                                                         std::vector<ValidationIssue>*)>& parser) {
  std::vector<Item> items;
  const std::optional<std::string> content = ReadFile(path);
  if (!content.has_value()) {
    issues->push_back({path, "Unable to read file."});
    return items;
  }

  JsonParseResult parsed = ParseJson(*content);
  if (!parsed.error.empty()) {
    issues->push_back({path, parsed.error});
    return items;
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

  result.state.personas = LoadArrayFile<PersonaDefinition>(config.seed_personas_path, &result.issues, ParsePersona);
  result.state.security_modes = LoadArrayFile<SecurityModeDefinition>(
      config.seed_security_modes_path, &result.issues, ParseSecurityMode);
  result.state.route_profiles = LoadArrayFile<RouteProfileDefinition>(
      config.seed_route_profiles_path, &result.issues, ParseRouteProfile);

  return result;
}

}  // namespace veyra
