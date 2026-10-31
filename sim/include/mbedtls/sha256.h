#pragma once

#include <cstddef>
#include <cstdint>

typedef struct {
  uint32_t state;
} mbedtls_sha256_context;

inline void mbedtls_sha256_init(mbedtls_sha256_context* ctx) {
  if (ctx) ctx->state = 0x12345678u;
}

inline void mbedtls_sha256_free(mbedtls_sha256_context*) {}

inline int mbedtls_sha256_starts(mbedtls_sha256_context* ctx, int) {
  if (ctx) ctx->state = 0x12345678u;
  return 0;
}

inline int mbedtls_sha256_update(mbedtls_sha256_context* ctx, const uint8_t* data, size_t len) {
  if (!ctx || !data) return 0;
  for (size_t i = 0; i < len; ++i) ctx->state = (ctx->state * 33u) ^ data[i];
  return 0;
}

inline int mbedtls_sha256_finish(mbedtls_sha256_context* ctx, uint8_t out[32]) {
  if (!ctx || !out) return 0;
  for (size_t i = 0; i < 32; ++i) out[i] = static_cast<uint8_t>((ctx->state >> ((i % 4) * 8)) & 0xFF);
  return 0;
}
