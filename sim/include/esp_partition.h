#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

using esp_err_t = int;

#ifndef ESP_OK
#define ESP_OK 0
#endif

#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif

#define ESP_PARTITION_TYPE_APP 0x00
#define ESP_PARTITION_TYPE_DATA 0x01

#define ESP_PARTITION_SUBTYPE_APP_OTA_0 0x10
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
#define ESP_PARTITION_SUBTYPE_DATA_OTA 0x00

typedef struct esp_partition_t {
  uint32_t address;
  uint32_t size;
  uint8_t type;
  uint8_t subtype;
  const char* label;
} esp_partition_t;

inline const esp_partition_t* esp_partition_find_first(uint8_t type, uint8_t subtype, const char*) {
  static const esp_partition_t otaData = {0x9000, 8192, ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                          "otadata"};
  static const esp_partition_t ota0 = {0x20000, 2 * 1024 * 1024, ESP_PARTITION_TYPE_APP,
                                       ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota_0"};
  static const esp_partition_t ota1 = {0x220000, 2 * 1024 * 1024, ESP_PARTITION_TYPE_APP,
                                       ESP_PARTITION_SUBTYPE_APP_OTA_1, "ota_1"};

  if (type == ESP_PARTITION_TYPE_DATA && subtype == ESP_PARTITION_SUBTYPE_DATA_OTA) return &otaData;
  if (type == ESP_PARTITION_TYPE_APP && subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) return &ota0;
  if (type == ESP_PARTITION_TYPE_APP && subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) return &ota1;
  return nullptr;
}

inline esp_err_t esp_partition_read(const esp_partition_t*, size_t, void* dst, size_t size) {
  if (dst && size) std::memset(dst, 0xFF, size);
  return ESP_OK;
}

inline esp_err_t esp_partition_write(const esp_partition_t*, size_t, const void*, size_t) { return ESP_OK; }

inline esp_err_t esp_partition_erase_range(const esp_partition_t*, size_t, size_t) { return ESP_OK; }
