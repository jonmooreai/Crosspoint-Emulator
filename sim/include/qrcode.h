#pragma once

#include <cstdint>
#include <cstring>

enum {
  MODE_NUMERIC = 0,
  MODE_ALPHANUMERIC = 1,
  MODE_BYTE = 2,
};

enum {
  ECC_LOW = 0,
  ECC_MEDIUM = 1,
  ECC_QUARTILE = 2,
  ECC_HIGH = 3,
};

struct QRCode {
  uint8_t version;
  uint8_t size;
  uint8_t ecc;
  uint8_t mode;
  uint8_t mask;
  uint8_t* modules;
};

inline uint16_t qrcode_getBufferSize(uint8_t version) {
  const uint8_t size = static_cast<uint8_t>(version * 4 + 17);
  return static_cast<uint16_t>(size) * static_cast<uint16_t>(size);
}

inline int8_t qrcode_initText(QRCode* qrcode, uint8_t* buffers, uint8_t version, uint8_t ecc, const char* text) {
  if (!qrcode || !buffers) return -1;
  qrcode->version = version;
  qrcode->size = static_cast<uint8_t>(version * 4 + 17);
  qrcode->ecc = ecc;
  qrcode->mode = MODE_BYTE;
  qrcode->mask = 0;
  qrcode->modules = buffers;

  const size_t total = static_cast<size_t>(qrcode->size) * static_cast<size_t>(qrcode->size);
  std::memset(buffers, 0, total);

  const size_t seedLen = text ? std::strlen(text) : 0;
  for (uint8_t y = 0; y < qrcode->size; ++y) {
    for (uint8_t x = 0; x < qrcode->size; ++x) {
      const bool border = (x < 7 && y < 7) || (x >= qrcode->size - 7 && y < 7) || (x < 7 && y >= qrcode->size - 7);
      const bool pattern = ((x * 3 + y * 5 + seedLen) % 7) < 3;
      buffers[static_cast<size_t>(y) * qrcode->size + x] = static_cast<uint8_t>(border || pattern);
    }
  }

  return 0;
}

inline int8_t qrcode_initBytes(QRCode* qrcode, uint8_t* buffers, uint8_t version, uint8_t ecc, uint8_t* data,
                               uint16_t) {
  return qrcode_initText(qrcode, buffers, version, ecc, reinterpret_cast<const char*>(data));
}

inline bool qrcode_getModule(const QRCode* qrcode, uint8_t x, uint8_t y) {
  if (!qrcode || !qrcode->modules || x >= qrcode->size || y >= qrcode->size) return false;
  return qrcode->modules[static_cast<size_t>(y) * qrcode->size + x] != 0;
}
