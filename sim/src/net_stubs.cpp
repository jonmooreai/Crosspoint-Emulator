// Network and crypto stand-ins. The emulator has no network: connections fail the way
// they would on a device with WiFi off, so the firmware's network code runs unchanged
// up to the point where it reports the failure.

#include <SecureClient.h>
#include <WolfsslCrypto.h>
#include <esp_random.h>

#include <cstring>

namespace freeink {

SecureClient::~SecureClient() = default;

void SecureClient::setCACert(const char* rootCA) { _rootCA = rootCA; }
void SecureClient::setInsecure() { _insecure = true; }
void SecureClient::reserveFullRecord() {}
int SecureClient::connect(IPAddress, uint16_t) { return 0; }
int SecureClient::connect(const char*, uint16_t) { return 0; }
size_t SecureClient::write(uint8_t) { return 0; }
size_t SecureClient::write(const uint8_t*, size_t) { return 0; }
int SecureClient::available() { return 0; }
int SecureClient::read() { return -1; }
int SecureClient::read(uint8_t*, size_t) { return -1; }
int SecureClient::peek() { return -1; }
void SecureClient::flush() {}
void SecureClient::stop() {}
uint8_t SecureClient::connected() { return 0; }
bool SecureClient::tls13Available() { return false; }

}  // namespace freeink

namespace freeink {
namespace content {

namespace {

uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

// Pads the message into 64-byte big-endian blocks and feeds each one to `block`.
template <typename BlockFn>
void forEachBlock(const uint8_t* data, size_t len, BlockFn block) {
  const uint64_t bitLen = static_cast<uint64_t>(len) * 8;
  size_t i = 0;
  for (; i + 64 <= len; i += 64) block(data + i);
  uint8_t tail[128] = {};
  const size_t rest = len - i;
  std::memcpy(tail, data + i, rest);
  tail[rest] = 0x80;
  const size_t tailLen = rest + 9 <= 64 ? 64 : 128;
  for (int b = 0; b < 8; b++) tail[tailLen - 1 - b] = static_cast<uint8_t>(bitLen >> (8 * b));
  block(tail);
  if (tailLen == 128) block(tail + 64);
}

uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

void putBe32(uint8_t* p, uint32_t v) {
  for (int b = 0; b < 4; b++) p[b] = static_cast<uint8_t>(v >> (24 - 8 * b));
}

}  // namespace

void WolfsslCrypto::sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  forEachBlock(data, len, [&](const uint8_t* blk) {
    uint32_t w[80];
    for (int t = 0; t < 16; t++) w[t] = be32(blk + 4 * t);
    for (int t = 16; t < 80; t++) w[t] = rotl(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int t = 0; t < 80; t++) {
      uint32_t f, k;
      if (t < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (t < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (t < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      const uint32_t temp = rotl(a, 5) + f + e + k + w[t];
      e = d;
      d = c;
      c = rotl(b, 30);
      b = a;
      a = temp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  });
  for (int i = 0; i < 5; i++) putBe32(out + 4 * i, h[i]);
}

void WolfsslCrypto::sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  static const uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  forEachBlock(data, len, [&](const uint8_t* blk) {
    uint32_t w[64];
    for (int t = 0; t < 16; t++) w[t] = be32(blk + 4 * t);
    for (int t = 16; t < 64; t++) {
      const uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
      const uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
      w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int t = 0; t < 64; t++) {
      const uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[t] + w[t];
      const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  });
  for (int i = 0; i < 8; i++) putBe32(out + 4 * i, h[i]);
}

void WolfsslCrypto::randomBytes(uint8_t* out, size_t len) { esp_fill_random(out, len); }

}  // namespace content
}  // namespace freeink

// lib/Epub/BookKey.cpp wraps DRM content keys with wolfSSL's AES-GCM, which the
// emulator does not link: no protected book ever has a stored key here.
#include <BookKey.h>

namespace bookkey {
bool write(const std::string&, const uint8_t*, size_t, int64_t) { return false; }
size_t read(const std::string&, uint8_t (&)[MAX_KEY_LEN], int64_t*) { return 0; }
}  // namespace bookkey
