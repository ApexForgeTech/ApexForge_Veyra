#include "veyra/config/schema_validator.h"

#include "veyra/serialization/json.h"

#include <cmath>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>

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

std::optional<std::string> ReadSchemaString(const JsonValue::Object& object, const std::string& key) {
  const JsonValue* value = FindProperty(object, key);
  if (value == nullptr || !value->IsString()) {
    return std::nullopt;
  }
  return value->AsString();
}

std::optional<bool> ReadSchemaBool(const JsonValue::Object& object, const std::string& key) {
  const JsonValue* value = FindProperty(object, key);
  if (value == nullptr || !value->IsBool()) {
    return std::nullopt;
  }
  return value->AsBool();
}

std::optional<std::size_t> ReadSchemaSize(const JsonValue::Object& object, const std::string& key) {
  const JsonValue* value = FindProperty(object, key);
  if (value == nullptr || !value->IsNumber()) {
    return std::nullopt;
  }

  const double numeric_value = value->AsNumber();
  if (numeric_value < 0 || std::floor(numeric_value) != numeric_value) {
    return std::nullopt;
  }

  return static_cast<std::size_t>(numeric_value);
}

std::string DescribeType(const JsonValue& value) {
  if (value.IsNull()) {
    return "null";
  }
  if (value.IsBool()) {
    return "boolean";
  }
  if (value.IsNumber()) {
    return "number";
  }
  if (value.IsString()) {
    return "string";
  }
  if (value.IsObject()) {
    return "object";
  }
  return "array";
}

bool MatchesType(const JsonValue& value, const std::string& expected_type) {
  if (expected_type == "string") {
    return value.IsString();
  }
  if (expected_type == "boolean") {
    return value.IsBool();
  }
  if (expected_type == "number") {
    return value.IsNumber();
  }
  if (expected_type == "object") {
    return value.IsObject();
  }
  if (expected_type == "array") {
    return value.IsArray();
  }
  if (expected_type == "null") {
    return value.IsNull();
  }
  return false;
}

void ValidateValueAgainstSchema(const JsonValue& value,
                                const JsonValue::Object& schema,
                                const std::string& path,
                                std::vector<ValidationIssue>* issues);

void ValidateEnumConstraint(const JsonValue& value,
                            const JsonValue::Array& allowed_values,
                            const std::string& path,
                            std::vector<ValidationIssue>* issues) {
  if (value.IsString()) {
    for (const JsonValue& allowed : allowed_values) {
      if (allowed.IsString() && allowed.AsString() == value.AsString()) {
        return;
      }
    }
    issues->push_back({path, "Value '" + value.AsString() + "' is not allowed by the schema enum."});
    return;
  }

  issues->push_back({path,
                     "Schema enum validation is only implemented for string values, got " +
                         DescribeType(value) + "."});
}

void ValidateStringConstraints(const JsonValue& value,
                               const JsonValue::Object& schema,
                               const std::string& path,
                               std::vector<ValidationIssue>* issues) {
  if (!value.IsString()) {
    return;
  }

  const std::string& string_value = value.AsString();

  if (const std::optional<std::size_t> min_length = ReadSchemaSize(schema, "minLength");
      min_length.has_value() && string_value.size() < *min_length) {
    issues->push_back(
        {path, "String must be at least " + std::to_string(*min_length) + " characters long."});
  }

  if (const std::optional<std::string> pattern = ReadSchemaString(schema, "pattern");
      pattern.has_value()) {
    try {
      const std::regex compiled(*pattern);
      if (!std::regex_match(string_value, compiled)) {
        issues->push_back({path, "String does not match required pattern '" + *pattern + "'."});
      }
    } catch (const std::regex_error&) {
      issues->push_back({path, "Schema pattern '" + *pattern + "' is not a valid regex."});
    }
  }
}

void ValidateArrayConstraints(const JsonValue& value,
                              const JsonValue::Object& schema,
                              const std::string& path,
                              std::vector<ValidationIssue>* issues) {
  if (!value.IsArray()) {
    return;
  }

  const JsonValue::Array& array = value.AsArray();
  if (const std::optional<std::size_t> min_items = ReadSchemaSize(schema, "minItems");
      min_items.has_value() && array.size() < *min_items) {
    issues->push_back({path, "Array must contain at least " + std::to_string(*min_items) +
                                 " item(s)."});
  }

  const JsonValue* items_schema = FindProperty(schema, "items");
  if (items_schema == nullptr || !items_schema->IsObject()) {
    return;
  }

  for (std::size_t index = 0; index < array.size(); ++index) {
    ValidateValueAgainstSchema(array[index], items_schema->AsObject(),
                               path + "[" + std::to_string(index) + "]", issues);
  }
}

void ValidateObjectConstraints(const JsonValue& value,
                               const JsonValue::Object& schema,
                               const std::string& path,
                               std::vector<ValidationIssue>* issues) {
  if (!value.IsObject()) {
    return;
  }

  const JsonValue::Object& object = value.AsObject();
  const JsonValue* required = FindProperty(schema, "required");
  if (required != nullptr && required->IsArray()) {
    for (const JsonValue& required_entry : required->AsArray()) {
      if (!required_entry.IsString()) {
        continue;
      }

      const std::string& property_name = required_entry.AsString();
      if (object.find(property_name) == object.end()) {
        issues->push_back({path, "Missing required property '" + property_name + "'."});
      }
    }
  }

  const JsonValue* properties = FindProperty(schema, "properties");
  const JsonValue::Object* property_schemas =
      properties != nullptr && properties->IsObject() ? &properties->AsObject() : nullptr;

  if (const std::optional<bool> additional_properties = ReadSchemaBool(schema, "additionalProperties");
      additional_properties.has_value() && !*additional_properties && property_schemas != nullptr) {
    std::set<std::string> allowed_names;
    for (const auto& [property_name, _] : *property_schemas) {
      allowed_names.insert(property_name);
    }

    for (const auto& [property_name, _] : object) {
      if (allowed_names.find(property_name) == allowed_names.end()) {
        issues->push_back({path, "Property '" + property_name +
                                     "' is not allowed by the schema."});
      }
    }
  }

  if (property_schemas == nullptr) {
    return;
  }

  for (const auto& [property_name, property_schema] : *property_schemas) {
    const auto value_it = object.find(property_name);
    if (value_it == object.end() || !property_schema.IsObject()) {
      continue;
    }

    ValidateValueAgainstSchema(value_it->second, property_schema.AsObject(),
                               path + "." + property_name, issues);
  }
}

void ValidateValueAgainstSchema(const JsonValue& value,
                                const JsonValue::Object& schema,
                                const std::string& path,
                                std::vector<ValidationIssue>* issues) {
  if (const std::optional<std::string> expected_type = ReadSchemaString(schema, "type");
      expected_type.has_value() && !MatchesType(value, *expected_type)) {
    issues->push_back(
        {path, "Expected " + *expected_type + ", got " + DescribeType(value) + "."});
    return;
  }

  const JsonValue* allowed_values = FindProperty(schema, "enum");
  if (allowed_values != nullptr && allowed_values->IsArray()) {
    ValidateEnumConstraint(value, allowed_values->AsArray(), path, issues);
  }

  ValidateStringConstraints(value, schema, path, issues);
  ValidateArrayConstraints(value, schema, path, issues);
  ValidateObjectConstraints(value, schema, path, issues);
}

std::optional<JsonValue> LoadJsonDocument(const std::string& path,
                                          std::vector<ValidationIssue>* issues) {
  const std::optional<std::string> content = ReadFile(path);
  if (!content.has_value()) {
    issues->push_back({path, "Unable to read file."});
    return std::nullopt;
  }

  JsonParseResult parsed = ParseJson(*content);
  if (!parsed.error.empty()) {
    issues->push_back({path, parsed.error});
    return std::nullopt;
  }

  return parsed.value;
}

void ValidateSeedFile(const std::string& schema_path,
                      const std::string& data_path,
                      std::vector<ValidationIssue>* issues) {
  const std::optional<JsonValue> schema_document = LoadJsonDocument(schema_path, issues);
  const std::optional<JsonValue> data_document = LoadJsonDocument(data_path, issues);
  if (!schema_document.has_value() || !data_document.has_value()) {
    return;
  }

  if (!schema_document->IsObject()) {
    issues->push_back({schema_path, "Schema document must be a JSON object."});
    return;
  }
  if (!data_document->IsArray()) {
    issues->push_back({data_path, "Seed definition file must be a JSON array."});
    return;
  }

  const JsonValue::Object& schema = schema_document->AsObject();
  const JsonValue::Array& entries = data_document->AsArray();

  for (std::size_t index = 0; index < entries.size(); ++index) {
    ValidateValueAgainstSchema(entries[index], schema, data_path + "[" + std::to_string(index) + "]",
                               issues);
  }
}

}  // namespace

std::vector<ValidationIssue> ValidateSeedDataAgainstSchemas(const StartupConfig& config) {
  std::vector<ValidationIssue> issues;

  ValidateSeedFile(config.persona_schema_path, config.seed_personas_path, &issues);
  ValidateSeedFile(config.security_mode_schema_path, config.seed_security_modes_path, &issues);
  ValidateSeedFile(config.route_profile_schema_path, config.seed_route_profiles_path, &issues);

  return issues;
}

}  // namespace veyra
