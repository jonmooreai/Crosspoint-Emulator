#pragma once

#include "WiFi.h"

#include <cstddef>
#include <cstdint>

class NetworkUDP {
 public:
  bool begin(uint16_t port) {
    (void)port;
    return true;
  }

  void stop() {}

  int parsePacket() { return 0; }

  int read(char* buffer, size_t len) {
    (void)buffer;
    (void)len;
    return 0;
  }

  IPAddress remoteIP() const { return IPAddress(); }

  uint16_t remotePort() const { return 0; }

  int beginPacket(IPAddress ip, uint16_t port) {
    (void)ip;
    (void)port;
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) {
    (void)buffer;
    return size;
  }

  int endPacket() { return 1; }
};
