#pragma once

#include "ArduinoStub.h"
#include "WString.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// Stub MD5Builder for emulator: provides API, hash is not implemented (returns zeros).
class MD5Builder {
 public:
  void begin() { memset(hash_, 0, sizeof(hash_)); }
  void add(const uint8_t* data, size_t len) { (void)data; (void)len; }
  void add(const char* data) { (void)data; }
  void add(const String& s) { (void)s; }
  void add(const uint8_t* data) { (void)data; }
  bool addStream(Stream& stream, const size_t maxLen = 0) {
    (void)stream;
    (void)maxLen;
    return true;
  }
  void calculate() {}
  void getBytes(uint8_t* output) const { memset(output, 0, 16); }
  void getChars(char* output) const {
    for (int i = 0; i < 16; i++) snprintf(output + i * 2, 3, "%02x", hash_[i]);
  }
  String toString() const { return String(""); }

 private:
  uint8_t hash_[16] = {0};
};
