#pragma once

#ifndef PROGMEM
#define PROGMEM
#endif

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <ctime>
#include <string>

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


// Cap delay to 1ms in the emulator to keep the UI responsive.
// On the real device delay(10) saves power; in the sim it just adds latency.
inline void delay(unsigned long ms) {
  unsigned long capped = ms > 1 ? 1 : ms;
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

class String;

class Printable {
 public:
  virtual ~Printable() = default;
  virtual size_t printTo(Print& p) const = 0;
};

class Stream : public Print {
 public:
  using Print::write;
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  String readStringUntil(char terminator);
  virtual int peek() { return -1; }
  size_t write(uint8_t c) override { return 0; }
  size_t readBytes(char* buffer, size_t length) {
    size_t n = 0;
    while (n < length) {
      const int c = read();
      if (c < 0) break;
      buffer[n++] = static_cast<char>(c);
    }
    return n;
  }
  size_t readBytes(uint8_t* buffer, size_t length) { return readBytes(reinterpret_cast<char*>(buffer), length); }
  void setTimeout(unsigned long timeoutMs) { timeoutMs_ = timeoutMs; }
  unsigned long getTimeout() const { return timeoutMs_; }

 private:
  unsigned long timeoutMs_ = 1000;
};

class SerialStub : public Print {
 public:
  void begin(unsigned long) {}
  void setTxTimeoutMs(uint32_t) {}
  operator bool() const { return true; }
  int available() const { return 0; }
  int read() { return -1; }
  String readStringUntil(char terminator);
  size_t write(uint8_t c) override {
    putchar(static_cast<char>(c));
    return 1;
  }
  size_t write(const uint8_t* buf, size_t size) override {
    for (size_t i = 0; i < size; i++) putchar(static_cast<char>(buf[i]));
    return size;
  }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
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

// HWCDC is an alias for the Serial port (used in Logging.h)
using HWCDC = SerialStub;

#include "ESP.h"
#include "SPI.h"

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

// GPIO: the sim has no pins; board bring-up code may still drive them.
#define LOW 0x0
#define HIGH 0x1
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define INPUT_PULLDOWN 0x09

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return HIGH; }
inline uint16_t analogRead(uint8_t) { return 0; }

#include "pgmspace.h"

// esp32-hal-time: the sim keeps the host clock, so only the timezone applies.
inline void configTzTime(const char* tz, const char*, const char* = nullptr, const char* = nullptr) {
  setenv("TZ", tz, 1);
  tzset();
}
