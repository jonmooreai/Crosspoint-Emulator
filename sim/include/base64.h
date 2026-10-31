#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "WString.h"

// Arduino-ESP32's base64 helper.
class base64 {
 public:
  static String encode(const uint8_t* data, size_t length);
  static String encode(const String& text) {
    return encode(reinterpret_cast<const uint8_t*>(text.c_str()), text.length());
  }
  static String encode(const char* text) { return encode(reinterpret_cast<const uint8_t*>(text), std::strlen(text)); }
};
