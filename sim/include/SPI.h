#pragma once

#include <cstddef>
#include <cstdint>

class SPIClass {
 public:
  void begin() {}
  void beginTransaction(void*) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t x) { return x; }
  void transfer(void* buf, size_t count) { (void)buf; (void)count; }
};

extern SPIClass SPI;
