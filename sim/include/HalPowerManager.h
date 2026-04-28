#pragma once

#include "HalGPIO.h"

class HalPowerManager {
 public:
  static constexpr int LOW_POWER_FREQ = 10;
  static constexpr unsigned long IDLE_POWER_SAVING_MS = 3000;
  static constexpr unsigned long BATTERY_POLL_MS = 1500;

  void begin() {}
  void setPowerSaving(bool) {}
  void startDeepSleep(HalGPIO&) const {}
  uint16_t getBatteryPercentage() const { return 100; }

  class Lock {
   public:
    Lock() = default;
    ~Lock() = default;
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(Lock&&) = delete;
  };
};

inline HalPowerManager powerManager;
