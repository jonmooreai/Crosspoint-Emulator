#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "esp_err.h"

// ESP-IDF NVS blob API, kept in memory like Preferences.h: a fresh device every run.

#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_INVALID_LENGTH 0x110c

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;

namespace sim_nvs {
inline std::map<std::string, std::vector<uint8_t>>& store() {
  static std::map<std::string, std::vector<uint8_t>> s;
  return s;
}
inline std::vector<std::string>& namespaces() {
  static std::vector<std::string> n;
  return n;
}
inline std::string key(nvs_handle_t h, const char* k) { return namespaces()[h] + "/" + k; }
}  // namespace sim_nvs

inline esp_err_t nvs_open(const char* name, nvs_open_mode_t, nvs_handle_t* out) {
  sim_nvs::namespaces().push_back(name ? name : "");
  *out = static_cast<nvs_handle_t>(sim_nvs::namespaces().size() - 1);
  return ESP_OK;
}

inline void nvs_close(nvs_handle_t) {}

inline esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }

inline esp_err_t nvs_get_blob(nvs_handle_t h, const char* key, void* out, size_t* length) {
  auto& s = sim_nvs::store();
  auto it = s.find(sim_nvs::key(h, key));
  if (it == s.end()) return ESP_ERR_NVS_NOT_FOUND;
  if (!out) {
    *length = it->second.size();
    return ESP_OK;
  }
  if (*length < it->second.size()) return ESP_ERR_NVS_INVALID_LENGTH;
  std::memcpy(out, it->second.data(), it->second.size());
  *length = it->second.size();
  return ESP_OK;
}

inline esp_err_t nvs_set_blob(nvs_handle_t h, const char* key, const void* value, size_t length) {
  const auto* p = static_cast<const uint8_t*>(value);
  sim_nvs::store()[sim_nvs::key(h, key)] = std::vector<uint8_t>(p, p + length);
  return ESP_OK;
}

inline esp_err_t nvs_erase_key(nvs_handle_t h, const char* key) {
  return sim_nvs::store().erase(sim_nvs::key(h, key)) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}
