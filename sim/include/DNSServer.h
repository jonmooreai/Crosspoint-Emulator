#pragma once

#include "WiFi.h"

class DNSReplyCode { public: static constexpr int NoError = 0; };

class DNSServer {
 public:
  void setErrorReplyCode(int code) { (void)code; }
  bool start(uint16_t port, const char* domainName, const IPAddress& resolvedIP) {
    (void)port; (void)domainName; (void)resolvedIP; return true;
  }
  void stop() {}
  void processNextRequest() {}
};
