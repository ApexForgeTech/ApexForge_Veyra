#ifndef VEYRA_SERIALIZATION_JSON_H_
#define VEYRA_SERIALIZATION_JSON_H_

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace veyra {

class JsonValue {
 public:
  using Object = std::map<std::string, JsonValue>;
  using Array = std::vector<JsonValue>;

  JsonValue();
  JsonValue(std::nullptr_t);
  JsonValue(bool value);
  JsonValue(double value);
  JsonValue(std::string value);
  JsonValue(Object value);
  JsonValue(Array value);

  bool IsNull() const;
  bool IsBool() const;
  bool IsNumber() const;
  bool IsString() const;
  bool IsObject() const;
  bool IsArray() const;

  bool AsBool() const;
  double AsNumber() const;
  const std::string& AsString() const;
  const Object& AsObject() const;
  const Array& AsArray() const;

 private:
  std::variant<std::nullptr_t, bool, double, std::string, Object, Array> value_;
};

struct JsonParseResult {
  JsonValue value;
  std::string error;
};

JsonParseResult ParseJson(const std::string& input);

}  // namespace veyra

#endif  // VEYRA_SERIALIZATION_JSON_H_
