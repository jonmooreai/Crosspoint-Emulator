#pragma once

#include <cstdint>

// Minimal PNGdec compatibility layer for emulator builds.
// This allows code to compile when the real PNGdec dependency is absent.

struct PNGFILE {
  void* fHandle = nullptr;
};

struct PNGDRAW {
  int y = 0;
  int iPixelType = 0;
  int iHasAlpha = 0;
  uint8_t* pPixels = nullptr;
  uint8_t* pPalette = nullptr;
  void* pUser = nullptr;
};

enum {
  PNG_SUCCESS = 0,
};

enum {
  PNG_PIXEL_GRAYSCALE = 0,
  PNG_PIXEL_TRUECOLOR = 2,
  PNG_PIXEL_INDEXED = 3,
  PNG_PIXEL_GRAY_ALPHA = 4,
  PNG_PIXEL_TRUECOLOR_ALPHA = 6,
};

// The real PNGdec uses internal line buffers; this constant is only used for
// sizing temporary grayscale buffers in Crosspoint code.
constexpr size_t PNG_MAX_BUFFERED_PIXELS = 8192;

class PNG {
 public:
  PNG() = default;

  int open(const char* filename, void* (*openFn)(const char*, int32_t*), void (*closeFn)(void*),
           int32_t (*readFn)(PNGFILE*, uint8_t*, int32_t), int32_t (*seekFn)(PNGFILE*, int32_t), void* user) {
    (void)filename;
    (void)openFn;
    (void)closeFn;
    (void)readFn;
    (void)seekFn;
    (void)user;
    // Stub reports success with zero dimensions.
    return PNG_SUCCESS;
  }

  int decode(void* user, int options) {
    (void)user;
    (void)options;
    return PNG_SUCCESS;
  }

  void close() {}

  int getWidth() const { return 0; }
  int getHeight() const { return 0; }
  int getBpp() const { return 8; }
};
