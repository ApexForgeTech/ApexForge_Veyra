#include "veyra/serialization/json.h"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace veyra {
namespace {

class Parser {
 public:
  explicit Parser(const std::string& input) : input_(input) {}

  JsonParseResult Parse() {
    JsonParseResult result;

    try {
      SkipWhitespace();
      result.value = ParseValue();
      SkipWhitespace();
      if (!IsAtEnd()) {
        throw std::runtime_error("Unexpected trailing characters in JSON document.");
      }
    } catch (const std::runtime_error& error) {
      result.error = error.what();
    }

    return result;
  }

 private:
  JsonValue ParseValue() {
    if (IsAtEnd()) {
      throw std::runtime_error("Unexpected end of input.");
    }

    const char current = input_[index_];
    if (current == '{') {
      return JsonValue(ParseObject());
    }
    if (current == '[') {
      return JsonValue(ParseArray());
    }
    if (current == '"') {
      return JsonValue(ParseString());
    }
    if (current == 't' || current == 'f') {
      return JsonValue(ParseBool());
    }
    if (current == 'n') {
      ParseNull();
      return JsonValue(nullptr);
    }
    if (current == '-' || std::isdigit(static_cast<unsigned char>(current))) {
      return JsonValue(ParseNumber());
    }

    throw std::runtime_error("Unexpected token while parsing JSON.");
  }

  JsonValue::Object ParseObject() {
    Consume('{');
    SkipWhitespace();

    JsonValue::Object object;
    if (TryConsume('}')) {
      return object;
    }

    while (true) {
      SkipWhitespace();
      const std::string key = ParseString();
      SkipWhitespace();
      Consume(':');
      SkipWhitespace();
      object.emplace(key, ParseValue());
      SkipWhitespace();

      if (TryConsume('}')) {
        break;
      }

      Consume(',');
      SkipWhitespace();
    }

    return object;
  }

  JsonValue::Array ParseArray() {
    Consume('[');
    SkipWhitespace();

    JsonValue::Array array;
    if (TryConsume(']')) {
      return array;
    }

    while (true) {
      SkipWhitespace();
      array.push_back(ParseValue());
      SkipWhitespace();

      if (TryConsume(']')) {
        break;
      }

      Consume(',');
      SkipWhitespace();
    }

    return array;
  }

  std::string ParseString() {
    Consume('"');

    std::string value;
    while (!IsAtEnd()) {
      const char current = input_[index_++];
      if (current == '"') {
        return value;
      }

      if (current == '\\') {
        if (IsAtEnd()) {
          throw std::runtime_error("Unexpected end of input in string escape.");
        }

        const char escaped = input_[index_++];
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            value.push_back(escaped);
            break;
          case 'b':
            value.push_back('\b');
            break;
          case 'f':
            value.push_back('\f');
            break;
          case 'n':
            value.push_back('\n');
            break;
          case 'r':
            value.push_back('\r');
            break;
          case 't':
            value.push_back('\t');
            break;
          default:
            throw std::runtime_error("Unsupported string escape sequence.");
        }
        continue;
      }

      value.push_back(current);
    }

    throw std::runtime_error("Unterminated string.");
  }

  bool ParseBool() {
    if (MatchLiteral("true")) {
      return true;
    }
    if (MatchLiteral("false")) {
      return false;
    }
    throw std::runtime_error("Invalid boolean literal.");
  }

  void ParseNull() {
    if (!MatchLiteral("null")) {
      throw std::runtime_error("Invalid null literal.");
    }
  }

  double ParseNumber() {
    const std::size_t start = index_;

    if (input_[index_] == '-') {
      ++index_;
    }

    ConsumeDigits();
    if (!IsAtEnd() && input_[index_] == '.') {
      ++index_;
      ConsumeDigits();
    }

    if (!IsAtEnd() && (input_[index_] == 'e' || input_[index_] == 'E')) {
      ++index_;
      if (!IsAtEnd() && (input_[index_] == '+' || input_[index_] == '-')) {
        ++index_;
      }
      ConsumeDigits();
    }

    return std::stod(input_.substr(start, index_ - start));
  }

  void ConsumeDigits() {
    if (IsAtEnd() || !std::isdigit(static_cast<unsigned char>(input_[index_]))) {
      throw std::runtime_error("Expected digit in number.");
    }

    while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(input_[index_]))) {
      ++index_;
    }
  }

  bool MatchLiteral(const char* literal) {
    std::size_t cursor = index_;
    while (*literal != '\0') {
      if (cursor >= input_.size() || input_[cursor] != *literal) {
        return false;
      }
      ++cursor;
      ++literal;
    }
    index_ = cursor;
    return true;
  }

  void SkipWhitespace() {
    while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(input_[index_]))) {
      ++index_;
    }
  }

  void Consume(char expected) {
    if (IsAtEnd() || input_[index_] != expected) {
      throw std::runtime_error(std::string("Expected '") + expected + "' while parsing JSON.");
    }
    ++index_;
  }

  bool TryConsume(char expected) {
    if (!IsAtEnd() && input_[index_] == expected) {
      ++index_;
      return true;
    }
    return false;
  }

  bool IsAtEnd() const {
    return index_ >= input_.size();
  }

  const std::string& input_;
  std::size_t index_ = 0;
};

}  // namespace

JsonValue::JsonValue() : value_(nullptr) {}
JsonValue::JsonValue(std::nullptr_t value) : value_(value) {}
JsonValue::JsonValue(bool value) : value_(value) {}
JsonValue::JsonValue(double value) : value_(value) {}
JsonValue::JsonValue(std::string value) : value_(std::move(value)) {}
JsonValue::JsonValue(Object value) : value_(std::move(value)) {}
JsonValue::JsonValue(Array value) : value_(std::move(value)) {}

bool JsonValue::IsNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
bool JsonValue::IsBool() const { return std::holds_alternative<bool>(value_); }
bool JsonValue::IsNumber() const { return std::holds_alternative<double>(value_); }
bool JsonValue::IsString() const { return std::holds_alternative<std::string>(value_); }
bool JsonValue::IsObject() const { return std::holds_alternative<Object>(value_); }
bool JsonValue::IsArray() const { return std::holds_alternative<Array>(value_); }

bool JsonValue::AsBool() const { return std::get<bool>(value_); }
double JsonValue::AsNumber() const { return std::get<double>(value_); }
const std::string& JsonValue::AsString() const { return std::get<std::string>(value_); }
const JsonValue::Object& JsonValue::AsObject() const { return std::get<Object>(value_); }
const JsonValue::Array& JsonValue::AsArray() const { return std::get<Array>(value_); }

JsonParseResult ParseJson(const std::string& input) {
  Parser parser(input);
  return parser.Parse();
}

}  // namespace veyra
