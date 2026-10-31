#pragma once

#include <cstdint>

enum { ECC_LOW };

struct QRCode {
  uint8_t size;
};

inline int qrcode_getBufferSize(int version) {
  (void)version;
  return 200;
}

inline int8_t qrcode_initText(struct QRCode* qrcode, uint8_t* buffers, int version, int ecc, const char* text) {
  (void)buffers;
  (void)version;
  (void)ecc;
  (void)text;
  if (!qrcode) return -1;
  qrcode->size = 21;
  return 0;
}

inline bool qrcode_getModule(const struct QRCode* qrcode, int x, int y) {
  (void)qrcode;
  // Simple deterministic checker pattern for emulator visualization.
  return ((x + y) & 1) == 0;
}
