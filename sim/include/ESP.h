#pragma once

#include <cstdint>

// On the emulator, real allocations use the host process heap (no 256KB limit).
// Report desktop-scale values so the app doesn't think it's low on memory.
class ESPClass {
 public:
  uint32_t getFreeHeap() const { return 8 * 1024 * 1024; }   // 8 MB
  uint32_t getHeapSize() const { return 16 * 1024 * 1024; }   // 16 MB
  uint32_t getMinFreeHeap() const { return 6 * 1024 * 1024; }  // 6 MB
  void restart() { /* no-op in sim */ }
};

extern ESPClass ESP;
