#pragma once

#define ESP_SNTP_OPMODE_POLL 0
#define SNTP_SYNC_STATUS_COMPLETED 1

void sntp_init() {}
void sntp_stop() {}

inline bool esp_sntp_enabled() { return false; }
inline void esp_sntp_stop() {}
inline void esp_sntp_setoperatingmode(int) {}
inline void esp_sntp_setservername(int, const char*) {}
inline void esp_sntp_init() {}
inline int sntp_get_sync_status() { return SNTP_SYNC_STATUS_COMPLETED; }
