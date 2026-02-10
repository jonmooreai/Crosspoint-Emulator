// Stub implementation of OtaUpdater for emulator (no OTA in sim).
#include "network/OtaUpdater.h"

bool OtaUpdater::isUpdateNewer() const { return false; }
const std::string& OtaUpdater::getLatestVersion() const { return latestVersion; }
OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() { return OtaUpdater::NO_UPDATE; }
OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate() { return OtaUpdater::NO_UPDATE; }
