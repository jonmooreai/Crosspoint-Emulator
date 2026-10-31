#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "WString.h"

// Arduino-ESP32 NVS key/value store, kept in memory: it resets every time the
// emulator starts, as a freshly flashed device would.
class Preferences {
 public:
  bool begin(const char* name, bool readOnly = false) {
    ns_ = name ? name : "";
    readOnly_ = readOnly;
    return true;
  }
  void end() {}
  bool clear() {
    store()[ns_].clear();
    return true;
  }
  bool remove(const char* key) { return store()[ns_].erase(key) > 0; }
  bool isKey(const char* key) { return store()[ns_].count(key) > 0; }

  size_t putBool(const char* key, bool v) { return putRaw(key, &v, sizeof(v)); }
  size_t putUChar(const char* key, uint8_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putUShort(const char* key, uint16_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putShort(const char* key, int16_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putUInt(const char* key, uint32_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putInt(const char* key, int32_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putULong(const char* key, uint32_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putLong(const char* key, int32_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putULong64(const char* key, uint64_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putLong64(const char* key, int64_t v) { return putRaw(key, &v, sizeof(v)); }
  size_t putString(const char* key, const char* v) { return putRaw(key, v, std::strlen(v) + 1); }
  size_t putString(const char* key, const String& v) { return putString(key, v.c_str()); }
  size_t putBytes(const char* key, const void* v, size_t len) { return putRaw(key, v, len); }

  bool getBool(const char* key, bool def = false) { return get(key, def); }
  uint8_t getUChar(const char* key, uint8_t def = 0) { return get(key, def); }
  uint16_t getUShort(const char* key, uint16_t def = 0) { return get(key, def); }
  int16_t getShort(const char* key, int16_t def = 0) { return get(key, def); }
  uint32_t getUInt(const char* key, uint32_t def = 0) { return get(key, def); }
  int32_t getInt(const char* key, int32_t def = 0) { return get(key, def); }
  uint32_t getULong(const char* key, uint32_t def = 0) { return get(key, def); }
  int32_t getLong(const char* key, int32_t def = 0) { return get(key, def); }
  uint64_t getULong64(const char* key, uint64_t def = 0) { return get(key, def); }
  int64_t getLong64(const char* key, int64_t def = 0) { return get(key, def); }
  String getString(const char* key, const String& def = String()) {
    auto* v = find(key);
    return v ? String(reinterpret_cast<const char*>(v->data())) : def;
  }
  size_t getBytesLength(const char* key) {
    auto* v = find(key);
    return v ? v->size() : 0;
  }
  size_t getBytes(const char* key, void* buf, size_t maxLen) {
    auto* v = find(key);
    if (!v || v->size() > maxLen) return 0;
    std::memcpy(buf, v->data(), v->size());
    return v->size();
  }

 private:
  using Bytes = std::vector<uint8_t>;
  static std::map<std::string, std::map<std::string, Bytes>>& store() {
    static std::map<std::string, std::map<std::string, Bytes>> s;
    return s;
  }
  size_t putRaw(const char* key, const void* v, size_t len) {
    if (readOnly_) return 0;
    const auto* p = static_cast<const uint8_t*>(v);
    store()[ns_][key] = Bytes(p, p + len);
    return len;
  }
  Bytes* find(const char* key) {
    auto& ns = store()[ns_];
    auto it = ns.find(key);
    return it == ns.end() ? nullptr : &it->second;
  }
  template <typename T>
  T get(const char* key, T def) {
    auto* v = find(key);
    if (!v || v->size() != sizeof(T)) return def;
    T out;
    std::memcpy(&out, v->data(), sizeof(T));
    return out;
  }

  std::string ns_;
  bool readOnly_ = false;
};
