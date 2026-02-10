#pragma once

#include "ArduinoStub.h"
#include <cstddef>
#include <cstdint>

typedef int32_t esp_err_t;
#define ESP_OK 0

typedef struct esp_http_client* esp_http_client_handle_t;
typedef struct esp_http_client_config_t {
  const char* url;
  const char* host;
  int port;
  const char* username;
  const char* password;
  const char* path;
  bool use_global_ca_store;
  void* cert_pem;
  size_t cert_len;
  bool skip_cert_common_name_check;
  int timeout_ms;
  bool disable_auto_redirect;
  int max_redirection_count;
  const char* event_handler;
  void* user_data;
  int buffer_size;
  int buffer_size_tx;
  void* transport_type;
  int crt_bundle_attach;
} esp_http_client_config_t;

typedef struct esp_http_client_event {
  int event_id;
  void* data;
  int data_len;
  void* user_data;
} esp_http_client_event_t;

inline const char* esp_err_to_name(esp_err_t) { return "OK"; }

inline esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*) { return nullptr; }
inline int esp_http_client_perform(esp_http_client_handle_t) { return -1; }
inline void esp_http_client_cleanup(esp_http_client_handle_t) {}
inline esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char*, const char*) { return ESP_OK; }

extern "C" inline esp_err_t esp_crt_bundle_attach(void*) { return ESP_OK; }
