#pragma once

typedef void* esp_https_ota_handle_t;
esp_https_ota_handle_t esp_https_ota_begin(const void* config) { return nullptr; }
int esp_https_ota_perform(esp_https_ota_handle_t) { return -1; }
int esp_https_ota_finish(esp_https_ota_handle_t) { return -1; }
void esp_https_ota_abort(esp_https_ota_handle_t) {}
