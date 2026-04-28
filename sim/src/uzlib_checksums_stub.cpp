#include <cstddef>
#include <cstdint>

extern "C" {

uint32_t uzlib_adler32(const void* data, unsigned int length, uint32_t prev_sum) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  uint32_t a = prev_sum & 0xFFFFu;
  uint32_t b = (prev_sum >> 16) & 0xFFFFu;
  if (a == 0) a = 1;
  for (unsigned int i = 0; i < length; ++i) {
    a = (a + bytes[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

uint32_t uzlib_crc32(const void* data, unsigned int length, uint32_t crc) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  crc = ~crc;
  for (unsigned int i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

}  // extern "C"
