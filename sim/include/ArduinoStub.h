#pragma once

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

#if defined(_MSC_VER) && !defined(__attribute__)
#define __attribute__(x)
#endif

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <thread>
#include <intrin.h>

// Minimal Arduino-like API for host build

inline unsigned long millis() {
  static const auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

inline unsigned long micros() {
  static const auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::microseconds>(now - start).count());
}

inline unsigned long simDelayCapMs() {
  static const unsigned long value = []() -> unsigned long {
    const char* raw = std::getenv("SIM_DELAY_CAP_MS");
    if (!raw || !*raw) return 1;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(raw, &end, 10);
    if (end == raw || *end != '\0') return 1;
    return parsed;
  }();
  return value;
}

// By default the emulator caps delay() to 1ms to keep the UI responsive.
// Network and background-job tests can raise this cap through SIM_DELAY_CAP_MS.
inline void delay(unsigned long ms) {
  const unsigned long capMs = simDelayCapMs();
  unsigned long capped = ms > capMs ? capMs : ms;
  if (capped > 0) std::this_thread::sleep_for(std::chrono::milliseconds(capped));
}

inline void yield() {
  std::this_thread::sleep_for(std::chrono::microseconds(100));
}

// Arduino random(): random(max) returns 0..max-1; random(min,max) returns min..max-1
inline long random(long max) {
  return max > 0 ? static_cast<long>(rand() % static_cast<unsigned long>(max)) : 0;
}
inline long random(long min, long max) {
  return min + random(max - min);
}
inline void randomSeed(unsigned long) { /* no-op */ }

class String;

class Print {
 public:
  virtual ~Print() = default;
  virtual void flush() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buf++);
    return n;
  }
  virtual size_t print(const char* s) {
    return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
  }
  virtual size_t println(const char* s) {
    size_t n = print(s);
    n += write('\n');
    return n;
  }
};

class Stream : public Print {
 public:
  using Print::write;
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
  size_t write(uint8_t c) override { return 0; }
};

class SerialStub : public Print {
 public:
  void begin(unsigned long) {}
  operator bool() const { return true; }
  int available() const { return 0; }
  int read() { return -1; }
  String readStringUntil(char);
  size_t write(uint8_t c) override {
    putchar(static_cast<char>(c));
    return 1;
  }
  size_t write(const uint8_t* buf, size_t size) override {
    for (size_t i = 0; i < size; i++) putchar(static_cast<char>(buf[i]));
    return size;
  }
#if defined(_MSC_VER)
  size_t printf(const char* fmt, ...) {
#else
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
#endif
    va_list args;
    va_start(args, fmt);
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) fwrite(buf, 1, static_cast<size_t>(n), stdout);
    return static_cast<size_t>(n);
  }
};

extern SerialStub Serial;

#include "ESP.h"
#include "SPI.h"
