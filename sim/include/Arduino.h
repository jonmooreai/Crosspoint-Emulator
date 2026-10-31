#pragma once

#ifndef PROGMEM
#define PROGMEM
#endif

#include <cstddef>
#include <cstdint>

#include "ArduinoStub.h"
#include "WString.h"

// Like the ESP32 core, Arduino.h brings in FreeRTOS.
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef uint8_t byte;
