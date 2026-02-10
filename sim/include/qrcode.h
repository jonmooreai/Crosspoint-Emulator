#pragma once

#include <cstdint>

enum { ECC_LOW };

struct QRCode {
  uint8_t size;
};

int qrcode_getBufferSize(int version) { (void)version; return 200; }
void qrcode_initText(struct QRCode* qrcode, uint8_t* buffers, int version, int ecc, const char* text) {
  (void)qrcode; (void)buffers; (void)version; (void)ecc; (void)text;
  qrcode->size = 0;
}
bool qrcode_getModule(const struct QRCode* qrcode, int x, int y) {
  (void)qrcode; (void)x; (void)y; return false;
}
