#pragma once

#include "Client.h"

// The emulator has no network: every connection attempt fails.
class WiFiClient : public Client {
 public:
  int connect(IPAddress, uint16_t) override { return 0; }
  int connect(const char*, uint16_t) override { return 0; }
  int connect(const char*, uint16_t, int32_t) { return 0; }
  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t*, size_t) override { return 0; }
  size_t write(Stream& stream) {
    size_t n = 0;
    while (stream.available()) {
      int c = stream.read();
      if (c < 0) break;
      n += write(static_cast<uint8_t>(c));
    }
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int read(uint8_t*, size_t) override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  void stop() override {}
  uint8_t connected() override { return 0; }
  operator bool() override { return false; }
  void setTimeout(uint32_t) {}
  void setNoDelay(bool) {}
  void setConnectionTimeout(uint32_t) {}
};
