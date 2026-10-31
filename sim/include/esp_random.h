#pragma once

#include <cstddef>
#include <cstdint>
#include <random>

inline uint32_t esp_random() {
  static std::mt19937 rng{std::random_device{}()};
  return rng();
}

inline void esp_fill_random(void* buf, size_t len) {
  auto* bytes = static_cast<uint8_t*>(buf);
  for (size_t i = 0; i < len; i++) bytes[i] = static_cast<uint8_t>(esp_random());
}
