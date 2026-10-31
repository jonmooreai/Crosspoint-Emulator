#pragma once

#include "esp_err.h"

#define ESP_ERR_NOT_FOUND 0x105

// No watchdog in the sim: no task is ever subscribed.
inline void esp_task_wdt_init() {}
inline void esp_task_wdt_add(void*) {}
inline void esp_task_wdt_delete(void*) {}
inline void esp_task_wdt_reset() {}
inline esp_err_t esp_task_wdt_status(void*) { return ESP_ERR_NOT_FOUND; }
