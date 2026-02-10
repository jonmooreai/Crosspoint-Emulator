#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor(uint8_t adcPin, float dividerMultiplier)
    : _adcPin(adcPin), _dividerMultiplier(dividerMultiplier) {}

uint16_t BatteryMonitor::readPercentage() const { return 100; }
uint16_t BatteryMonitor::readMillivolts() const { return 4200; }
uint16_t BatteryMonitor::readRawMillivolts() const { return 2100; }
double BatteryMonitor::readVolts() const { return 4.2; }
uint16_t BatteryMonitor::percentageFromMillivolts(uint16_t) { return 100; }
uint16_t BatteryMonitor::millivoltsFromRawAdc(uint16_t x) { return x; }
