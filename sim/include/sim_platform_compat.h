#pragma once

// Force-included into every translation unit. Fills the POSIX gaps of MinGW-w64
// that the firmware and the sim rely on, plus what the Arduino core makes visible everywhere.

#ifdef __cplusplus
#include <cassert>
#include <cstdint>
#else
#include <assert.h>
#include <stdint.h>
#endif

#ifdef _WIN32
// Expat seeds its hash salt with rand_s, which the CRT only declares on request.
#define _CRT_RAND_S
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifndef localtime_r
static inline struct tm* sim_localtime_r(const time_t* t, struct tm* out) {
  return localtime_s(out, t) == 0 ? out : NULL;
}
#define localtime_r sim_localtime_r
#endif

#ifndef gmtime_r
static inline struct tm* sim_gmtime_r(const time_t* t, struct tm* out) { return gmtime_s(out, t) == 0 ? out : NULL; }
#define gmtime_r sim_gmtime_r
#endif

static inline size_t strlcpy(char* dst, const char* src, size_t size) {
  const size_t len = strlen(src);
  if (size > 0) {
    const size_t n = len < size - 1 ? len : size - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
  }
  return len;
}

static inline int setenv(const char* name, const char* value, int overwrite) {
  if (!overwrite && getenv(name) != NULL) return 0;
  return _putenv_s(name, value);
}

static inline int unsetenv(const char* name) { return _putenv_s(name, ""); }

#ifndef tzset
#define tzset _tzset
#endif

#endif  // _WIN32

// BSD alias that glibc, newlib and macOS provide (not always via the headers a file
// includes) and MinGW lacks; repeating an identical typedef is valid C11 and C++.
typedef unsigned int uint;

// Arduino pgmspace: flash and RAM share one address space on the host.
#ifndef memcpy_P
#define memcpy_P memcpy
#endif

// The firmware sets the system clock from NTP/RTC; the emulator keeps the host clock.
#include <sys/time.h>
static inline int sim_settimeofday(const struct timeval* tv, const void* tz) {
  (void)tv;
  (void)tz;
  return 0;
}
#define settimeofday sim_settimeofday
