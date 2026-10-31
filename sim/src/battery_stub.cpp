#include <BatteryMonitor.h>

// The emulated device is always on USB power with a full battery.

BatteryMonitor::BatteryMonitor() : BatteryMonitor(PIN_NONE) {}

BatteryMonitor::BatteryMonitor(int8_t adcPin, float dividerMultiplier, int8_t chargeStatusPin)
    : _adcPin(adcPin), _dividerMultiplier(dividerMultiplier), _chargeStatusPin(chargeStatusPin) {}

uint16_t BatteryMonitor::readPercentage() const { return 100; }

bool BatteryMonitor::readPercentageChecked(uint16_t& out) const {
  out = 100;
  return true;
}

BatteryMonitor::Status BatteryMonitor::readStatus() const {
  Status status;
  status.supported = true;
  status.percentageKnown = status.millivoltsKnown = status.chargingKnown = status.externalPowerKnown = true;
  status.percentage = 100;
  status.millivolts = 4200;
  status.externalPower = true;
  return status;
}

uint16_t BatteryMonitor::readMillivolts() const { return 4200; }
double BatteryMonitor::readVolts() const { return 4.2; }
bool BatteryMonitor::isCharging() const { return false; }

bool BatteryMonitor::isExternalPowerPresent(bool* known) const {
  if (known) *known = true;
  return true;
}

uint16_t BatteryMonitor::percentageFromMillivolts(uint16_t) { return 100; }
uint16_t BatteryMonitor::percentageFromMillivolts(uint16_t, uint16_t) { return 100; }
bool BatteryMonitor::loadDesignCapacity() { return false; }
bool BatteryMonitor::enterShipMode() { return false; }
bool BatteryMonitor::configureCharger() { return false; }
