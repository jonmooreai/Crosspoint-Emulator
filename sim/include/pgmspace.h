#pragma once

#include <cstdint>
#include <cstring>

// Arduino pgmspace: flash and RAM share one address space on the host, as on the ESP32.
#ifndef PROGMEM
#define PROGMEM
#endif
#define PGM_P const char*
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t*>(addr))
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t*>(addr))
#define pgm_read_float(addr) (*reinterpret_cast<const float*>(addr))
#define pgm_read_ptr(addr) (*reinterpret_cast<const void* const*>(addr))
#define strlen_P strlen
#define strcmp_P strcmp
#define strncmp_P strncmp
#define strcpy_P strcpy
#define strncpy_P strncpy
#define memcmp_P memcmp
#ifndef memcpy_P
#define memcpy_P memcpy
#endif

class __FlashStringHelper;
#define FPSTR(p) (reinterpret_cast<const __FlashStringHelper*>(p))
#define F(s) FPSTR(PSTR(s))
