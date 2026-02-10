#pragma once

#include "WString.h"
#include <cstdint>
#include <functional>

enum WStype_t { WStype_TEXT, WStype_BIN, WStype_CONNECTED, WStype_DISCONNECTED };

class WebSocketsServer {
 public:
  explicit WebSocketsServer(uint16_t port) : port_(port) {}
  void begin() {}
  void close() {}
  void onEvent(void (*callback)(uint8_t, WStype_t, uint8_t*, size_t)) { (void)callback; }
  void loop() {}
  void sendTXT(uint8_t num, const char* payload) { (void)num; (void)payload; }
  void sendTXT(uint8_t num, const String& payload) { (void)num; (void)payload; }
  void sendTXT(uint8_t num, const uint8_t* payload, size_t len) { (void)num; (void)payload; (void)len; }

 private:
  uint16_t port_;
};
