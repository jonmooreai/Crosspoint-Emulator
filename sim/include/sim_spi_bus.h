#pragma once

// Display updates and SD file I/O acquire a shared mutex so they cannot run
// concurrently, matching the device's shared SPI bus.

void spi_bus_lock();
void spi_bus_unlock();

struct SpiBusGuard {
  SpiBusGuard() { spi_bus_lock(); }
  ~SpiBusGuard() { spi_bus_unlock(); }
  SpiBusGuard(const SpiBusGuard&) = delete;
  SpiBusGuard& operator=(const SpiBusGuard&) = delete;
};
