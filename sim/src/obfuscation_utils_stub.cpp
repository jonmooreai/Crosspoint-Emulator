#include "ObfuscationUtils.h"

namespace obfuscation {

void xorTransform(std::string&) {}

void xorTransform(std::string&, const uint8_t*, size_t) {}

String obfuscateToBase64(const std::string& plaintext) { return plaintext.c_str(); }

std::string deobfuscateFromBase64(const char* encoded, bool* ok) {
  if (ok) *ok = encoded != nullptr;
  return encoded ? std::string(encoded) : std::string();
}

void selfTest() {}

}  // namespace obfuscation
