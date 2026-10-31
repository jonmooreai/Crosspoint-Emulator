#pragma once

#include <sys/time.h>

#define ESP_SNTP_OPMODE_POLL 0
#define SNTP_SYNC_STATUS_RESET 0
#define SNTP_SYNC_STATUS_COMPLETED 1

typedef void (*sntp_sync_time_cb_t)(struct timeval* tv);

// The sim keeps the host clock, which is already synchronised.
inline void sntp_init() {}
inline void sntp_stop() {}
inline bool esp_sntp_enabled() { return false; }
inline void esp_sntp_stop() {}
inline void esp_sntp_setoperatingmode(int) {}
inline void esp_sntp_setservername(int, const char*) {}
inline void esp_sntp_init() {}
inline int sntp_get_sync_status() { return SNTP_SYNC_STATUS_COMPLETED; }
inline void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t) {}
