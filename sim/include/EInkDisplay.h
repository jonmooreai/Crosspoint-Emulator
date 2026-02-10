#pragma once

#include <cstdint>

class EInkDisplay {
 public:
  EInkDisplay(int8_t sclk, int8_t mosi, int8_t cs, int8_t dc, int8_t rst, int8_t busy);
  ~EInkDisplay() = default;

  enum RefreshMode {
    FULL_REFRESH,
    HALF_REFRESH,
    FAST_REFRESH
  };

  void begin();

  static constexpr uint16_t DISPLAY_WIDTH = 800;
  static constexpr uint16_t DISPLAY_HEIGHT = 480;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                bool fromProgmem = false) const;

  void setFramebuffer(const uint8_t* bwBuffer) const;
  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer);
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer);

  void displayBuffer(RefreshMode mode = FAST_REFRESH, bool fadingFix = false);
  void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void displayGrayBuffer(bool fadingFix = false);
  void refreshDisplay(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false);
  void grayscaleRevert();
  void setCustomLUT(bool enabled, const unsigned char* lutData = nullptr);
  void deepSleep();

  uint8_t* getFrameBuffer() const { return frameBuffer; }
  void saveFrameBufferAsPBM(const char* filename);

 private:
  uint8_t frameBuffer0[BUFFER_SIZE];
  uint8_t* frameBuffer;
  bool isScreenOn = false;
};
