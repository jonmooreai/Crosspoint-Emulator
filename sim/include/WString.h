#pragma once

#include "ArduinoStub.h"
#include <cstring>
#include <string>

class String : public Print {
 public:
  String() : s_(), readPos_(0) {}
  String(const char* c) : s_(c ? c : ""), readPos_(0) {}
  String(const std::string& s) : s_(s), readPos_(0) {}
  String(char c) : s_(1, c), readPos_(0) {}
  String(int n) : s_(std::to_string(n)), readPos_(0) {}
  String(unsigned n) : s_(std::to_string(n)), readPos_(0) {}
  String(long n) : s_(std::to_string(n)), readPos_(0) {}
  String(unsigned long n) : s_(std::to_string(n)), readPos_(0) {}
  String(size_t n) : s_(std::to_string(n)), readPos_(0) {}

  // Stream-like read for ArduinoJson deserializeJson(doc, string)
  int read() {
    if (readPos_ >= s_.size()) return -1;
    return static_cast<unsigned char>(s_[readPos_++]);
  }

  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool empty() const { return s_.empty(); }
  bool isEmpty() const { return s_.empty(); }
  String substring(size_t start, size_t end = std::string::npos) const {
    if (start >= s_.size()) return String("");
    return String(s_.substr(start, end == std::string::npos ? std::string::npos : end - start));
  }
  int indexOf(char c, size_t from = 0) const {
    size_t p = s_.find(c, from);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int indexOf(const char* s, size_t from = 0) const {
    size_t p = s_.find(s ? s : "", from);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int lastIndexOf(char c) const {
    size_t p = s_.rfind(c);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int lastIndexOf(const char* s) const {
    size_t p = s_.rfind(s ? s : "");
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  bool equals(const char* o) const { return o && s_ == o; }
  bool equals(const String& o) const { return s_ == o.s_; }
  long toInt() const {
    if (s_.empty()) return 0;
    return std::stol(s_, nullptr, 0);
  }

  String& operator=(const char* c) {
    s_ = c ? c : "";
    readPos_ = 0;
    return *this;
  }
  String& operator=(const std::string& s) {
    s_ = s;
    readPos_ = 0;
    return *this;
  }
  String& operator+=(const char* c) {
    s_ += (c ? c : "");
    return *this;
  }
  String& operator+=(char c) {
    s_ += c;
    return *this;
  }
  String& operator+=(const String& o) {
    s_ += o.s_;
    return *this;
  }

  bool startsWith(const char* prefix) const {
    if (!prefix) return false;
    size_t len = strlen(prefix);
    return s_.size() >= len && s_.compare(0, len, prefix) == 0;
  }
  bool startsWith(const String& prefix) const { return startsWith(prefix.c_str()); }

  bool endsWith(const char* suffix) const {
    if (!suffix) return false;
    size_t len = strlen(suffix);
    return s_.size() >= len && s_.compare(s_.size() - len, len, suffix) == 0;
  }
  bool endsWith(const String& suffix) const { return endsWith(suffix.c_str()); }

  void toLowerCase() {
    for (char& c : s_)
      if (c >= 'A' && c <= 'Z') c += 32;
  }

  void trim() {
    size_t start = s_.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      s_.clear();
      return;
    }
    size_t end = s_.find_last_not_of(" \t\r\n");
    s_ = s_.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
  }

  std::string& str() { return s_; }
  const std::string& str() const { return s_; }

  size_t write(uint8_t c) override {
    s_ += static_cast<char>(c);
    return 1;
  }
  size_t write(const uint8_t* buf, size_t size) override {
    s_.append(reinterpret_cast<const char*>(buf), size);
    return size;
  }

 private:
  std::string s_;
  mutable size_t readPos_ = 0;
};

inline String operator+(const String& a, const String& b) {
  String r;
  r.str() = a.str() + b.str();
  return r;
}
inline String operator+(const String& a, const char* b) {
  String r;
  r.str() = a.str() + (b ? b : "");
  return r;
}
inline String operator+(const char* a, const String& b) {
  String r;
  r.str() = (a ? a : "") + b.str();
  return r;
}
inline String operator+(const String& a, char b) {
  String r;
  r.str() = a.str() + b;
  return r;
}

inline bool operator==(const String& a, const char* b) {
  return strcmp(a.c_str(), b ? b : "") == 0;
}
inline bool operator==(const char* a, const String& b) {
  return strcmp(a ? a : "", b.c_str()) == 0;
}
inline bool operator==(const String& a, const String& b) {
  return a.str() == b.str();
}
inline bool operator!=(const String& a, const char* b) {
  return !(a == b);
}
inline bool operator!=(const char* a, const String& b) {
  return !(a == b);
}
inline bool operator!=(const String& a, const String& b) {
  return a.str() != b.str();
}
