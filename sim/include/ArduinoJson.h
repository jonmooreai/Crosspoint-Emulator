#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "WString.h"

class JsonArray;
class JsonVariant;

namespace arduinojson_compat {
enum class Type { Null, Bool, Number, String, Object, Array };

struct Value {
  Type type = Type::Null;
  bool boolValue = false;
  double numberValue = 0.0;
  std::string stringValue;
  std::map<std::string, Value> objectValue;
  std::vector<Value> arrayValue;
};
}  // namespace arduinojson_compat

class DeserializationError {
 public:
  DeserializationError() = default;
  explicit DeserializationError(const char* msg) : msg_(msg ? msg : "error") {}

  explicit operator bool() const { return !msg_.empty(); }
  const char* c_str() const { return msg_.empty() ? "Ok" : msg_.c_str(); }

 private:
  std::string msg_;
};

namespace DeserializationOption {
struct FilterTag {};
inline FilterTag Filter(const class JsonDocument&) { return {}; }
}  // namespace DeserializationOption

class JsonVariant {
 public:
  JsonVariant() = default;
  explicit JsonVariant(arduinojson_compat::Value* v) : value_(v) {}

  JsonVariant operator[](const char* key);
  JsonVariant operator[](size_t index);
  const JsonVariant operator[](const char* key) const;
  const JsonVariant operator[](size_t index) const;

  template <typename T>
  JsonArray to();

  template <typename T>
  bool is() const;

  template <typename T>
  T as() const;

  size_t size() const;

  JsonVariant& operator=(const char* v);
  JsonVariant& operator=(const std::string& v);
  JsonVariant& operator=(const String& v);
  JsonVariant& operator=(bool v);
  JsonVariant& operator=(int v);
  JsonVariant& operator=(unsigned v);
  JsonVariant& operator=(long v);
  JsonVariant& operator=(unsigned long v);
  JsonVariant& operator=(int64_t v);
  JsonVariant& operator=(size_t v);
  JsonVariant& operator=(float v);
  JsonVariant& operator=(double v);

  bool operator==(const char* rhs) const {
    return value_ && value_->type == arduinojson_compat::Type::String && value_->stringValue == (rhs ? rhs : "");
  }
  bool operator==(const std::string& rhs) const {
    return value_ && value_->type == arduinojson_compat::Type::String && value_->stringValue == rhs;
  }

  arduinojson_compat::Value* raw() { return value_; }
  const arduinojson_compat::Value* raw() const { return value_; }

 private:
  arduinojson_compat::Value* value_ = nullptr;
};

class JsonArray {
 public:
  JsonArray() = default;
  explicit JsonArray(arduinojson_compat::Value* v) : value_(v) {}

  void add(const char* v);
  void add(const std::string& v);
  void add(const String& v);
  void add(int v);
  void add(size_t v);
  void add(bool v);

  size_t size() const;
  JsonVariant operator[](size_t index);
  const JsonVariant operator[](size_t index) const;

 private:
  arduinojson_compat::Value* value_ = nullptr;
};

class JsonDocument {
 public:
  JsonDocument() { clear(); }

  void clear() {
    root_.type = arduinojson_compat::Type::Object;
    root_.objectValue.clear();
    root_.arrayValue.clear();
    root_.stringValue.clear();
    root_.numberValue = 0.0;
    root_.boolValue = false;
  }

  JsonVariant operator[](const char* key) {
    if (root_.type != arduinojson_compat::Type::Object) {
      root_.type = arduinojson_compat::Type::Object;
      root_.objectValue.clear();
    }
    return JsonVariant(&root_.objectValue[std::string(key ? key : "")]);
  }

  const JsonVariant operator[](const char* key) const {
    static arduinojson_compat::Value nullValue;
    if (root_.type != arduinojson_compat::Type::Object) return JsonVariant(const_cast<arduinojson_compat::Value*>(&nullValue));
    auto it = root_.objectValue.find(std::string(key ? key : ""));
    if (it == root_.objectValue.end()) return JsonVariant(const_cast<arduinojson_compat::Value*>(&nullValue));
    return JsonVariant(const_cast<arduinojson_compat::Value*>(&it->second));
  }

  arduinojson_compat::Value& root() { return root_; }
  const arduinojson_compat::Value& root() const { return root_; }

 private:
  arduinojson_compat::Value root_;
};

using JsonObject = JsonVariant;

inline DeserializationError deserializeJson(JsonDocument& doc, const String& input);
inline DeserializationError deserializeJson(JsonDocument& doc, const char* input);
inline DeserializationError deserializeJson(JsonDocument& doc, const char* input, DeserializationOption::FilterTag);

inline size_t serializeJson(const JsonDocument& doc, std::string& out);
inline size_t serializeJson(const JsonDocument& doc, String& out);
inline size_t serializeJson(const JsonDocument& doc, char* output, size_t outputSize);

inline JsonVariant JsonVariant::operator[](const char* key) {
  if (!value_) return JsonVariant();
  if (value_->type != arduinojson_compat::Type::Object) {
    value_->type = arduinojson_compat::Type::Object;
    value_->objectValue.clear();
  }
  return JsonVariant(&value_->objectValue[std::string(key ? key : "")]);
}

inline JsonVariant JsonVariant::operator[](size_t index) {
  if (!value_) return JsonVariant();
  if (value_->type != arduinojson_compat::Type::Array) {
    value_->type = arduinojson_compat::Type::Array;
    value_->arrayValue.clear();
  }
  if (value_->arrayValue.size() <= index) value_->arrayValue.resize(index + 1);
  return JsonVariant(&value_->arrayValue[index]);
}

inline const JsonVariant JsonVariant::operator[](const char* key) const {
  static arduinojson_compat::Value nullValue;
  if (!value_ || value_->type != arduinojson_compat::Type::Object) return JsonVariant(const_cast<arduinojson_compat::Value*>(&nullValue));
  auto it = value_->objectValue.find(std::string(key ? key : ""));
  if (it == value_->objectValue.end()) return JsonVariant(const_cast<arduinojson_compat::Value*>(&nullValue));
  return JsonVariant(const_cast<arduinojson_compat::Value*>(&it->second));
}

inline const JsonVariant JsonVariant::operator[](size_t index) const {
  static arduinojson_compat::Value nullValue;
  if (!value_ || value_->type != arduinojson_compat::Type::Array || index >= value_->arrayValue.size()) {
    return JsonVariant(const_cast<arduinojson_compat::Value*>(&nullValue));
  }
  return JsonVariant(const_cast<arduinojson_compat::Value*>(&value_->arrayValue[index]));
}

template <>
inline JsonArray JsonVariant::to<JsonArray>() {
  if (!value_) return JsonArray();
  if (value_->type != arduinojson_compat::Type::Array) {
    value_->type = arduinojson_compat::Type::Array;
    value_->arrayValue.clear();
  }
  return JsonArray(value_);
}

template <>
inline bool JsonVariant::is<JsonVariant>() const {
  return value_ != nullptr && value_->type != arduinojson_compat::Type::Null;
}

template <>
inline bool JsonVariant::is<JsonArray>() const {
  return value_ != nullptr && value_->type == arduinojson_compat::Type::Array;
}

template <>
inline bool JsonVariant::is<std::string>() const {
  return value_ != nullptr && value_->type == arduinojson_compat::Type::String;
}

template <>
inline std::string JsonVariant::as<std::string>() const {
  if (!value_) return {};
  switch (value_->type) {
    case arduinojson_compat::Type::String:
      return value_->stringValue;
    case arduinojson_compat::Type::Number:
      return std::to_string(value_->numberValue);
    case arduinojson_compat::Type::Bool:
      return value_->boolValue ? "true" : "false";
    default:
      return {};
  }
}

template <>
inline float JsonVariant::as<float>() const {
  if (!value_) return 0.0f;
  if (value_->type == arduinojson_compat::Type::Number) return static_cast<float>(value_->numberValue);
  if (value_->type == arduinojson_compat::Type::String) return static_cast<float>(std::atof(value_->stringValue.c_str()));
  return 0.0f;
}

template <>
inline int JsonVariant::as<int>() const {
  if (!value_) return 0;
  if (value_->type == arduinojson_compat::Type::Number) return static_cast<int>(value_->numberValue);
  if (value_->type == arduinojson_compat::Type::Bool) return value_->boolValue ? 1 : 0;
  if (value_->type == arduinojson_compat::Type::String) return std::atoi(value_->stringValue.c_str());
  return 0;
}

template <>
inline size_t JsonVariant::as<size_t>() const {
  if (!value_) return 0;
  if (value_->type == arduinojson_compat::Type::Number) return static_cast<size_t>(value_->numberValue);
  if (value_->type == arduinojson_compat::Type::String) return static_cast<size_t>(std::strtoull(value_->stringValue.c_str(), nullptr, 10));
  return 0;
}

template <>
inline int64_t JsonVariant::as<int64_t>() const {
  if (!value_) return 0;
  if (value_->type == arduinojson_compat::Type::Number) return static_cast<int64_t>(value_->numberValue);
  if (value_->type == arduinojson_compat::Type::String) return static_cast<int64_t>(std::strtoll(value_->stringValue.c_str(), nullptr, 10));
  return 0;
}

inline size_t JsonVariant::size() const {
  if (!value_) return 0;
  if (value_->type == arduinojson_compat::Type::Array) return value_->arrayValue.size();
  if (value_->type == arduinojson_compat::Type::Object) return value_->objectValue.size();
  return 0;
}

inline JsonVariant& JsonVariant::operator=(const char* v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::String;
  value_->stringValue = v ? v : "";
  return *this;
}
inline JsonVariant& JsonVariant::operator=(const std::string& v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::String;
  value_->stringValue = v;
  return *this;
}
inline JsonVariant& JsonVariant::operator=(const String& v) { return (*this = v.c_str()); }
inline JsonVariant& JsonVariant::operator=(bool v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::Bool;
  value_->boolValue = v;
  return *this;
}
inline JsonVariant& JsonVariant::operator=(int v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::Number;
  value_->numberValue = static_cast<double>(v);
  return *this;
}
inline JsonVariant& JsonVariant::operator=(unsigned v) { return (*this = static_cast<size_t>(v)); }
inline JsonVariant& JsonVariant::operator=(long v) { return (*this = static_cast<int64_t>(v)); }
inline JsonVariant& JsonVariant::operator=(unsigned long v) { return (*this = static_cast<size_t>(v)); }
inline JsonVariant& JsonVariant::operator=(int64_t v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::Number;
  value_->numberValue = static_cast<double>(v);
  return *this;
}
inline JsonVariant& JsonVariant::operator=(size_t v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::Number;
  value_->numberValue = static_cast<double>(v);
  return *this;
}
inline JsonVariant& JsonVariant::operator=(float v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::Number;
  value_->numberValue = static_cast<double>(v);
  return *this;
}
inline JsonVariant& JsonVariant::operator=(double v) {
  if (!value_) return *this;
  value_->type = arduinojson_compat::Type::Number;
  value_->numberValue = v;
  return *this;
}

inline void JsonArray::add(const char* v) {
  if (!value_) return;
  if (value_->type != arduinojson_compat::Type::Array) {
    value_->type = arduinojson_compat::Type::Array;
    value_->arrayValue.clear();
  }
  arduinojson_compat::Value elem;
  elem.type = arduinojson_compat::Type::String;
  elem.stringValue = v ? v : "";
  value_->arrayValue.push_back(std::move(elem));
}
inline void JsonArray::add(const std::string& v) { add(v.c_str()); }
inline void JsonArray::add(const String& v) { add(v.c_str()); }
inline void JsonArray::add(int v) {
  if (!value_) return;
  if (value_->type != arduinojson_compat::Type::Array) {
    value_->type = arduinojson_compat::Type::Array;
    value_->arrayValue.clear();
  }
  arduinojson_compat::Value elem;
  elem.type = arduinojson_compat::Type::Number;
  elem.numberValue = static_cast<double>(v);
  value_->arrayValue.push_back(std::move(elem));
}
inline void JsonArray::add(size_t v) { add(static_cast<int>(v)); }
inline void JsonArray::add(bool v) {
  if (!value_) return;
  if (value_->type != arduinojson_compat::Type::Array) {
    value_->type = arduinojson_compat::Type::Array;
    value_->arrayValue.clear();
  }
  arduinojson_compat::Value elem;
  elem.type = arduinojson_compat::Type::Bool;
  elem.boolValue = v;
  value_->arrayValue.push_back(std::move(elem));
}

inline size_t JsonArray::size() const {
  if (!value_ || value_->type != arduinojson_compat::Type::Array) return 0;
  return value_->arrayValue.size();
}
inline JsonVariant JsonArray::operator[](size_t index) {
  if (!value_ || value_->type != arduinojson_compat::Type::Array || index >= value_->arrayValue.size()) return JsonVariant();
  return JsonVariant(&value_->arrayValue[index]);
}
inline const JsonVariant JsonArray::operator[](size_t index) const {
  if (!value_ || value_->type != arduinojson_compat::Type::Array || index >= value_->arrayValue.size()) return JsonVariant();
  return JsonVariant(const_cast<arduinojson_compat::Value*>(&value_->arrayValue[index]));
}

namespace arduinojson_compat {
inline void skipWs(const std::string& s, size_t& i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

inline bool parseString(const std::string& s, size_t& i, std::string& out) {
  if (i >= s.size() || s[i] != '"') return false;
  ++i;
  out.clear();
  while (i < s.size()) {
    char c = s[i++];
    if (c == '"') return true;
    if (c == '\\' && i < s.size()) {
      char esc = s[i++];
      switch (esc) {
        case '"':
        case '\\':
        case '/':
          out.push_back(esc);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        default:
          out.push_back(esc);
          break;
      }
    } else {
      out.push_back(c);
    }
  }
  return false;
}

inline bool parseValue(const std::string& s, size_t& i, Value& out);

inline bool parseArray(const std::string& s, size_t& i, Value& out) {
  if (s[i] != '[') return false;
  out.type = Type::Array;
  out.arrayValue.clear();
  ++i;
  skipWs(s, i);
  if (i < s.size() && s[i] == ']') {
    ++i;
    return true;
  }
  while (i < s.size()) {
    Value elem;
    if (!parseValue(s, i, elem)) return false;
    out.arrayValue.push_back(std::move(elem));
    skipWs(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      skipWs(s, i);
      continue;
    }
    if (i < s.size() && s[i] == ']') {
      ++i;
      return true;
    }
    return false;
  }
  return false;
}

inline bool parseObject(const std::string& s, size_t& i, Value& out) {
  if (s[i] != '{') return false;
  out.type = Type::Object;
  out.objectValue.clear();
  ++i;
  skipWs(s, i);
  if (i < s.size() && s[i] == '}') {
    ++i;
    return true;
  }
  while (i < s.size()) {
    std::string key;
    if (!parseString(s, i, key)) return false;
    skipWs(s, i);
    if (i >= s.size() || s[i] != ':') return false;
    ++i;
    skipWs(s, i);
    Value val;
    if (!parseValue(s, i, val)) return false;
    out.objectValue[key] = std::move(val);
    skipWs(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      skipWs(s, i);
      continue;
    }
    if (i < s.size() && s[i] == '}') {
      ++i;
      return true;
    }
    return false;
  }
  return false;
}

inline bool parseNumber(const std::string& s, size_t& i, Value& out) {
  const size_t start = i;
  if (s[i] == '-') ++i;
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
  if (i < s.size() && s[i] == '.') {
    ++i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
  }
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    ++i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
  }
  out.type = Type::Number;
  out.numberValue = std::strtod(s.c_str() + start, nullptr);
  return true;
}

inline bool parseValue(const std::string& s, size_t& i, Value& out) {
  skipWs(s, i);
  if (i >= s.size()) return false;
  if (s[i] == '{') return parseObject(s, i, out);
  if (s[i] == '[') return parseArray(s, i, out);
  if (s[i] == '"') {
    out.type = Type::String;
    return parseString(s, i, out.stringValue);
  }
  if (s.compare(i, 4, "true") == 0) {
    out.type = Type::Bool;
    out.boolValue = true;
    i += 4;
    return true;
  }
  if (s.compare(i, 5, "false") == 0) {
    out.type = Type::Bool;
    out.boolValue = false;
    i += 5;
    return true;
  }
  if (s.compare(i, 4, "null") == 0) {
    out.type = Type::Null;
    i += 4;
    return true;
  }
  if (s[i] == '-' || std::isdigit(static_cast<unsigned char>(s[i]))) return parseNumber(s, i, out);
  return false;
}

inline void serializeValue(const Value& v, std::string& out) {
  switch (v.type) {
    case Type::Null:
      out += "null";
      break;
    case Type::Bool:
      out += (v.boolValue ? "true" : "false");
      break;
    case Type::Number:
      out += std::to_string(v.numberValue);
      break;
    case Type::String: {
      out.push_back('"');
      for (char c : v.stringValue) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
      }
      out.push_back('"');
      break;
    }
    case Type::Object: {
      out.push_back('{');
      bool first = true;
      for (const auto& kv : v.objectValue) {
        if (!first) out.push_back(',');
        first = false;
        out.push_back('"');
        out += kv.first;
        out += "\":";
        serializeValue(kv.second, out);
      }
      out.push_back('}');
      break;
    }
    case Type::Array: {
      out.push_back('[');
      for (size_t i = 0; i < v.arrayValue.size(); ++i) {
        if (i) out.push_back(',');
        serializeValue(v.arrayValue[i], out);
      }
      out.push_back(']');
      break;
    }
  }
}
}  // namespace arduinojson_compat

inline DeserializationError deserializeJson(JsonDocument& doc, const String& input) {
  return deserializeJson(doc, input.c_str());
}

inline DeserializationError deserializeJson(JsonDocument& doc, const char* input) {
  if (!input) return DeserializationError("null input");
  std::string s(input);
  size_t i = 0;
  arduinojson_compat::Value parsed;
  if (!arduinojson_compat::parseValue(s, i, parsed)) return DeserializationError("parse error");
  arduinojson_compat::skipWs(s, i);
  if (i != s.size()) return DeserializationError("trailing characters");
  doc.root() = std::move(parsed);
  return DeserializationError();
}

inline DeserializationError deserializeJson(JsonDocument& doc, const char* input, DeserializationOption::FilterTag) {
  return deserializeJson(doc, input);
}

inline size_t serializeJson(const JsonDocument& doc, std::string& out) {
  out.clear();
  arduinojson_compat::serializeValue(doc.root(), out);
  return out.size();
}

inline size_t serializeJson(const JsonDocument& doc, String& out) {
  std::string tmp;
  const size_t n = serializeJson(doc, tmp);
  out = tmp;
  return n;
}

inline size_t serializeJson(const JsonDocument& doc, char* output, size_t outputSize) {
  std::string tmp;
  const size_t n = serializeJson(doc, tmp);
  if (!output || outputSize == 0) return n;
  const size_t copyLen = (n < (outputSize - 1)) ? n : (outputSize - 1);
  if (copyLen > 0) memcpy(output, tmp.data(), copyLen);
  output[copyLen] = '\0';
  return n;
}
