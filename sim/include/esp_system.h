#pragma once

#include <cstdlib>

#include "esp_random.h"

// A reboot ends the emulator; relaunch it to "boot" again.
[[noreturn]] inline void esp_restart() { std::_Exit(0); }
