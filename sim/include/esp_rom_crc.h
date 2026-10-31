#pragma once

#include <cstddef>
#include <cstdint>

inline uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t* data, uint32_t len) {
  uint32_t h = crc;
  for (uint32_t i = 0; i < len; ++i) {
    h = (h * 16777619u) ^ data[i];
  }
  return h;
}
