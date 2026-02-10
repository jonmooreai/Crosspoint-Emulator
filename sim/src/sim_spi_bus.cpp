// Shared SPI bus mutex: display and SD serialized to match the device.

#include "sim_spi_bus.h"
#include <mutex>

// Recursive so FsFile::openFullPath can call close() while holding the bus.
static std::recursive_mutex g_spiBusMutex;

void spi_bus_lock() {
  g_spiBusMutex.lock();
}

void spi_bus_unlock() {
  g_spiBusMutex.unlock();
}
