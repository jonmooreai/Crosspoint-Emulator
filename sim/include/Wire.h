#pragma once

// Stub for Arduino Wire (I2C) library - not needed for emulator

class TwoWire {
 public:
  void begin() {}
  void begin(uint8_t address) {}
  void setClock(uint32_t freq) {}
  void beginTransmission(uint8_t address) {}
  uint8_t endTransmission() { return 0; }
  uint8_t requestFrom(uint8_t address, uint8_t quantity) { return 0; }
  size_t write(uint8_t data) { return 1; }
  size_t write(const uint8_t* data, size_t length) { return length; }
  int read() { return -1; }
  int available() { return 0; }
  void flush() {}
};

extern TwoWire Wire;
