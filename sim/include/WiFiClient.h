#pragma once

#include "ArduinoStub.h"

class WiFiClient : public Stream {
 public:
  int connect(const char* host, uint16_t port) { (void)host; (void)port; return 0; }
  void stop() {}
  bool connected() { return false; }
  void clear() {}
  size_t write(uint8_t c) override { (void)c; return 1; }
  size_t write(const uint8_t* buf, size_t size) override { (void)buf; return size; }
  size_t write(Stream& stream) {
    size_t n = 0;
    while (stream.available()) {
      int c = stream.read();
      if (c < 0) break;
      n += write(static_cast<uint8_t>(c));
    }
    return n;
  }
};

using NetworkClient = WiFiClient;
