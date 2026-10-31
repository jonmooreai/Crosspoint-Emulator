// Parts of the firmware HAL that only the emulator can provide, plus the FreeInk SDK
// drivers underneath the HAL sources the emulator compiles unchanged (HalClock,
// HalFrontlight, HalTiltSensor). The emulated board is an Xteink X4: it has no
// frontlight and no IMU, and its "RTC" is the host clock.

#include <FrontlightManager.h>
#include <HalPowerManager.h>
#include <HalSystem.h>
#include <Imu.h>
#include <Rtc.h>
#include <SDL.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>

#include "sim_display.h"

// ---- HalPowerManager -------------------------------------------------------

HalPowerManager powerManager;

void HalPowerManager::begin() {}

void HalPowerManager::setPowerSaving(bool enabled) { isLowPower = enabled; }

// On hardware this never returns: the chip halts until a wake reset. As an empty stub it
// returned at once, so main.cpp's idle check fired again on the very next iteration (nothing
// had reset its inactivity timer), the firmware re-entered sleep, and the sleep screen was
// repainted every loop. That is the rapid flicker, and it is a photosensitivity hazard.
//
// Park here instead, the way the hardware would, until a wake key is held. Returning while the
// key is still down matters: the firmware's next gpio.update() then sees a press, resets its
// inactivity timer, and SleepActivity::loop() routes back into the book or Home. Returning
// after the release would leave the timer stale and drop straight back into sleep.
void HalPowerManager::startDeepSleep(HalGPIO&, bool) const {
  for (;;) {
    if (!sim_display_pump_events()) {
      // Window closed while asleep. Exit the way main_sim does, skipping static teardown.
      sim_display_shutdown();
      std::_Exit(0);
    }
    // Polled state, not events, so this agrees with how sim_gpio reads the buttons.
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER] || keys[SDL_SCANCODE_P]) return;
    SDL_Delay(16);  // ~60Hz, enough to stay responsive without spinning a core
  }
}

uint16_t HalPowerManager::getBatteryPercentage() const { return 100; }

HalPowerManager::Lock::Lock() : valid(true) {}

HalPowerManager::Lock::~Lock() = default;

// ---- HalSystem -------------------------------------------------------------

namespace HalSystem {
void begin() {}
void checkPanic() {}
void clearPanic() {}
std::string getPanicInfo(bool) { return {}; }
bool isRebootFromPanic() { return false; }
}  // namespace HalSystem

// ---- FreeInk SDK drivers ---------------------------------------------------

namespace freeink {

// The RTC keeps UTC, like the hardware one; HalClock applies the timezone.
bool Rtc::begin() {
  begun_ = true;
  return true;
}

bool Rtc::now(DateTime& out) {
  const time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  out.year = static_cast<uint16_t>(utc.tm_year + 1900);
  out.month = static_cast<uint8_t>(utc.tm_mon + 1);
  out.day = static_cast<uint8_t>(utc.tm_mday);
  out.hour = static_cast<uint8_t>(utc.tm_hour);
  out.minute = static_cast<uint8_t>(utc.tm_min);
  out.second = static_cast<uint8_t>(utc.tm_sec);
  out.weekday = static_cast<uint8_t>(utc.tm_wday);
  return true;
}

// The host clock is authoritative; setting it from the firmware is a no-op.
bool Rtc::set(const DateTime&) { return true; }
bool Rtc::armWakeTimer(uint32_t) { return false; }
bool Rtc::cancelWakeTimer() { return true; }
bool Rtc::readWakeTimer(bool& fired) {
  fired = false;
  return true;
}
bool Rtc::adjust(int32_t, DateTime* out) { return out ? now(*out) : true; }

bool Imu::begin() { return false; }
bool Imu::hasGyroscope() const { return false; }
bool Imu::read(Sample&) { return false; }
bool Imu::sleep() { return true; }
bool Imu::wake() { return false; }

}  // namespace freeink

void FrontlightManager::begin() {}
void FrontlightManager::setBrightness(uint8_t) {}
void FrontlightManager::setBrightnessLevel(uint8_t) {}
void FrontlightManager::off() {}
void FrontlightManager::on() {}
void FrontlightManager::setColorTemperature(uint8_t) {}

// ---- uzlib checksums -------------------------------------------------------
// The firmware never inflates zlib/gzip-wrapped streams, so on the device the linker
// drops the only callers in tinflate.c; a host link still needs the symbols.

extern "C" uint32_t uzlib_adler32(const void* data, unsigned int length, uint32_t prevSum) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  uint32_t a = prevSum & 0xFFFF;
  uint32_t b = prevSum >> 16;
  for (unsigned int i = 0; i < length; i++) {
    a = (a + bytes[i]) % 65521;
    b = (b + a) % 65521;
  }
  return (b << 16) | a;
}

extern "C" uint32_t uzlib_crc32(const void* data, unsigned int length, uint32_t crc) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (unsigned int i = 0; i < length; i++) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc;
}

// ---- Logging's Serial proxy -------------------------------------------------
// The firmware routes Serial through MySerialImpl but leaves its definition to the
// board support code.

#include <Logging.h>

#include <cstdarg>
#include <cstdio>

MySerialImpl MySerialImpl::instance;

size_t MySerialImpl::printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[512];
  const int n = std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (n <= 0) return 0;
  const size_t len = n < static_cast<int>(sizeof(buffer)) ? static_cast<size_t>(n) : sizeof(buffer) - 1;
  return logSerial.write(reinterpret_cast<const uint8_t*>(buffer), len);
}

size_t MySerialImpl::write(uint8_t b) { return logSerial.write(b); }

size_t MySerialImpl::write(const uint8_t* buffer, size_t size) { return logSerial.write(buffer, size); }

void MySerialImpl::flush() { logSerial.flush(); }
