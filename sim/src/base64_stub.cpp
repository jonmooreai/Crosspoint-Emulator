#include <base64.h>
#include <mbedtls/base64.h>

#include <string>

namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int decodeChar(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

std::string encodeToString(const unsigned char* src, size_t slen) {
  std::string out;
  out.reserve((slen + 2) / 3 * 4);
  for (size_t i = 0; i < slen; i += 3) {
    const uint32_t n = (src[i] << 16) | (i + 1 < slen ? src[i + 1] << 8 : 0) | (i + 2 < slen ? src[i + 2] : 0);
    out += kAlphabet[(n >> 18) & 63];
    out += kAlphabet[(n >> 12) & 63];
    out += i + 1 < slen ? kAlphabet[(n >> 6) & 63] : '=';
    out += i + 2 < slen ? kAlphabet[n & 63] : '=';
  }
  return out;
}

}  // namespace

String base64::encode(const uint8_t* data, size_t length) { return String(encodeToString(data, length).c_str()); }

int mbedtls_base64_encode(unsigned char* dst, size_t dlen, size_t* olen, const unsigned char* src, size_t slen) {
  const std::string encoded = encodeToString(src, slen);
  *olen = encoded.size() + 1;  // mbedTLS counts the terminating NUL
  if (!dst || dlen < encoded.size() + 1) return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
  std::memcpy(dst, encoded.c_str(), encoded.size() + 1);
  *olen = encoded.size();
  return 0;
}

int mbedtls_base64_decode(unsigned char* dst, size_t dlen, size_t* olen, const unsigned char* src, size_t slen) {
  std::string out;
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = 0; i < slen; i++) {
    const unsigned char c = src[i];
    if (c == '=' || c == '\r' || c == '\n') continue;
    const int v = decodeChar(c);
    if (v < 0) return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((acc >> bits) & 0xFF);
    }
  }
  *olen = out.size();
  if (!dst || dlen < out.size()) return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
  std::memcpy(dst, out.data(), out.size());
  return 0;
}
