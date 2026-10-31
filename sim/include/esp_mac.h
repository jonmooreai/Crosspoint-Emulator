#pragma once

#include <cstdint>
#include <cstring>

#include "esp_err.h"

typedef enum { ESP_MAC_WIFI_STA, ESP_MAC_WIFI_SOFTAP, ESP_MAC_BT, ESP_MAC_ETH } esp_mac_type_t;

// A fixed, locally administered MAC so device-derived IDs stay stable between runs.
inline esp_err_t esp_read_mac(uint8_t* mac, esp_mac_type_t) {
  static const uint8_t kMac[6] = {0x02, 0x00, 0x00, 0xC0, 0xFF, 0xEE};
  std::memcpy(mac, kMac, sizeof(kMac));
  return ESP_OK;
}

inline esp_err_t esp_efuse_mac_get_default(uint8_t* mac) { return esp_read_mac(mac, ESP_MAC_WIFI_STA); }
