#pragma once

#include "WString.h"
#include "WiFiClient.h"
#include "WiFiClientSecure.h"
#include <string>

class HTTPClient {
 public:
  bool begin(const char* url) { (void)url; return false; }
  bool begin(WiFiClient& client, const char* url) { (void)client; (void)url; return false; }
  bool begin(WiFiClientSecure& client, const char* url) { (void)client; (void)url; return false; }
  void addHeader(const char* name, const char* value) { (void)name; (void)value; }
  void setAuthorization(const char* user, const char* pass) { (void)user; (void)pass; }
  int GET() { return -1; }
  int PUT(const char* body) { (void)body; return -1; }
  int getSize() { return -1; }
  String getString() { return String(""); }
  void end() {}
};
