#pragma once

#include "WiFi.h"

#include <cstdint>
#include <cstring>

class WiFiUDP {
 public:
  bool begin(uint16_t port) { (void)port; return false; }
  void stop() {}
  int parsePacket() { return 0; }
  int read(uint8_t* buf, size_t len) { (void)buf; (void)len; return 0; }
  int read(char* buf, size_t len) { return read(reinterpret_cast<uint8_t*>(buf), len); }
  void beginPacket(IPAddress ip, uint16_t port) { (void)ip; (void)port; }
  size_t write(const uint8_t* buf, size_t len) { (void)buf; (void)len; return 0; }
  void endPacket() {}
  IPAddress remoteIP() const { return IPAddress(0,0,0,0); }
  uint16_t remotePort() const { return 0; }
};
