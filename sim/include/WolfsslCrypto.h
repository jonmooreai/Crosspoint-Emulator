#pragma once

// Sim stand-in for the SDK's wolfSSL-backed content crypto. The emulator does not
// link wolfSSL, so DRM-protected books cannot be opened; everything else that only
// needs hashing or randomness keeps working.

#include <Crypto.h>

#include <string>

namespace freeink {
namespace content {

class WolfsslCrypto : public Crypto {
 public:
  std::string lastError = "content protection is not available in the emulator";

  int32_t rsaPrivateRaw(const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, size_t) override { return -1; }
  bool aes128CbcDecrypt(const uint8_t[16], const uint8_t[16], const uint8_t*, size_t, uint8_t*) override {
    return false;
  }
  void sha1(const uint8_t* data, size_t len, uint8_t out[20]) override;
  void sha256(const uint8_t* data, size_t len, uint8_t out[32]) override;
  bool rsaGenerate(RsaKeyPairDer*) override { return false; }
  bool rsaPublicEncrypt(const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, size_t, size_t*) override {
    return false;
  }
  bool rsaPrivateSignRaw(const uint8_t*, size_t, const uint8_t[20], uint8_t[128]) override { return false; }
  bool aes128CbcEncrypt(const uint8_t[16], const uint8_t[16], const uint8_t*, size_t, uint8_t*) override {
    return false;
  }
  bool pkcs12Extract(const uint8_t*, size_t, const std::string&, std::vector<uint8_t>*,
                     std::vector<uint8_t>*) override {
    return false;
  }
  void randomBytes(uint8_t* out, size_t len) override;
};

}  // namespace content
}  // namespace freeink
