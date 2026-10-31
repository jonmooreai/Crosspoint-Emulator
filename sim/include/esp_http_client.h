#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"


typedef enum {
  HTTP_METHOD_GET = 0,
  HTTP_METHOD_POST,
  HTTP_METHOD_PUT,
} esp_http_client_method_t;

typedef enum { HTTP_AUTH_TYPE_NONE = 0, HTTP_AUTH_TYPE_BASIC = 1 } esp_http_client_auth_type_t;

enum { HTTP_EVENT_ON_DATA = 1 };

typedef struct esp_http_client_event {
  int event_id;
  void* data;
  int data_len;
  void* user_data;
} esp_http_client_event_t;

typedef esp_err_t (*esp_http_client_event_cb_t)(esp_http_client_event_t* evt);

struct esp_http_client;
typedef struct esp_http_client* esp_http_client_handle_t;

typedef struct esp_http_client_config_t {
  const char* url = nullptr;
  const char* host = nullptr;
  int port = 0;
  const char* username = nullptr;
  const char* password = nullptr;
  const char* path = nullptr;
  bool use_global_ca_store = false;
  void* cert_pem = nullptr;
  size_t cert_len = 0;
  bool skip_cert_common_name_check = false;
  int timeout_ms = 0;
  bool disable_auto_redirect = false;
  int max_redirection_count = 0;
  esp_http_client_event_cb_t event_handler = nullptr;
  void* user_data = nullptr;
  int buffer_size = 0;
  int buffer_size_tx = 0;
  void* transport_type = nullptr;
  esp_err_t (*crt_bundle_attach)(void*) = nullptr;
  esp_http_client_method_t method = HTTP_METHOD_GET;
  esp_http_client_auth_type_t auth_type = HTTP_AUTH_TYPE_NONE;
} esp_http_client_config_t;

struct esp_http_client {
  esp_http_client_config_t cfg{};
  int status_code = 500;
};


inline esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* cfg) {
  auto* c = new esp_http_client();
  if (cfg) c->cfg = *cfg;
  return c;
}

inline int esp_http_client_perform(esp_http_client_handle_t client) {
  if (!client) return -1;
  client->status_code = 500;
  return -1;
}

inline int esp_http_client_get_status_code(esp_http_client_handle_t client) {
  return client ? client->status_code : 500;
}

inline void esp_http_client_cleanup(esp_http_client_handle_t client) { delete client; }

inline esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char*, const char*) { return ESP_OK; }

inline esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t, const char*, int) { return ESP_OK; }

extern "C" inline esp_err_t esp_crt_bundle_attach(void*) { return ESP_OK; }
