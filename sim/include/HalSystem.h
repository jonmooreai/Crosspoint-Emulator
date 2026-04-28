#pragma once

#include <cstdint>
#include <string>

namespace HalSystem {
struct StackFrame {
  uint32_t sp = 0;
  uint32_t spp[8] = {};
};

inline void begin() {}
inline void checkPanic() {}
inline void clearPanic() {}
inline std::string getPanicInfo(bool = false) { return {}; }
inline bool isRebootFromPanic() { return false; }
}  // namespace HalSystem
