#pragma once

#include "esp_partition.h"

inline const esp_partition_t* esp_ota_get_next_update_partition(const void*) {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}

// The emulator has no OTA slots; report OTA_0 as the running image. Its only caller reads the
// image header's chip_id, and the stub esp_partition_read returns zeroed bytes for that.
inline const esp_partition_t* esp_ota_get_running_partition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}
