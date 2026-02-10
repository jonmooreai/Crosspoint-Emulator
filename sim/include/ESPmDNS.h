#pragma once

class MDNSClass {
 public:
  bool begin(const char* hostname) { (void)hostname; return false; }
  void end() {}
};

extern MDNSClass MDNS;
