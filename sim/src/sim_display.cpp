#include "EInkDisplay.h"
#include "HalDisplay.h"
#include "sim_spi_bus.h"

#include <SDL.h>
#include <cstdlib>
#include <cstring>

// Rotated 90° clockwise: logical 800×480 → window 480×800
static const int WINDOW_WIDTH = static_cast<int>(EInkDisplay::DISPLAY_HEIGHT);   // 480
static const int WINDOW_HEIGHT = static_cast<int>(EInkDisplay::DISPLAY_WIDTH);  // 800

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_texture = nullptr;

namespace {
bool g_hasBw = false;
bool g_hasGrayLsb = false;
bool g_hasGrayMsb = false;
uint8_t g_bwBuffer[EInkDisplay::BUFFER_SIZE];
uint8_t g_grayLsbBuffer[EInkDisplay::BUFFER_SIZE];
uint8_t g_grayMsbBuffer[EInkDisplay::BUFFER_SIZE];

inline bool bit_is_set(const uint8_t* buf, int x, int y) {
  const size_t byteIdx = static_cast<size_t>(y) * EInkDisplay::DISPLAY_WIDTH_BYTES + (x / 8);
  return (buf[byteIdx] & (0x80 >> (x & 7))) != 0;
}

void render_bw_to_texture(const uint8_t* buf) {
  if (!g_renderer || !g_texture || !buf) return;
  uint8_t* pixels = nullptr;
  int pitch = 0;
  if (SDL_LockTexture(g_texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch) != 0) return;

  // Process the framebuffer one byte (8 horizontal pixels) at a time.
  // This eliminates per-pixel bit extraction and reduces loop iterations 8×.
  // Rotation: logical (x, y) → window (H-1-y, x).
  const int H = static_cast<int>(EInkDisplay::DISPLAY_HEIGHT);
  const int W = static_cast<int>(EInkDisplay::DISPLAY_WIDTH);
  const int WB = static_cast<int>(EInkDisplay::DISPLAY_WIDTH_BYTES);

  for (int y = 0; y < H; y++) {
    const int rotX = H - 1 - y;                // column in the rotated window (fixed for this row)
    const size_t rowBase = static_cast<size_t>(y) * WB;

    for (int byteIdx = 0; byteIdx < WB; byteIdx++) {
      const uint8_t byte = buf[rowBase + byteIdx];
      const int xBase = byteIdx * 8;
      const int remaining = W - xBase;
      const int count = remaining < 8 ? remaining : 8;

      for (int b = 0; b < count; b++) {
        const uint8_t v = (byte & (0x80 >> b)) ? 255 : 0;
        const int rotY = xBase + b;             // row in the rotated window
        const size_t off = static_cast<size_t>(rotY) * static_cast<size_t>(pitch)
                         + static_cast<size_t>(rotX) * 3;
        pixels[off + 0] = v;
        pixels[off + 1] = v;
        pixels[off + 2] = v;
      }
    }
  }

  SDL_UnlockTexture(g_texture);
  SDL_RenderClear(g_renderer);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
  SDL_RenderPresent(g_renderer);
}

void render_gray_to_texture(const uint8_t* bw, const uint8_t* lsb, const uint8_t* msb) {
  if (!g_renderer || !g_texture || !bw || !lsb || !msb) return;
  uint8_t* pixels = nullptr;
  int pitch = 0;
  if (SDL_LockTexture(g_texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch) != 0) return;

  // Precomputed grayscale LUT: 3 bits (bw, msb, lsb) → shade.
  // Index: bit2 = bwWhite, bit1 = msbBit, bit0 = lsbBit.
  static constexpr uint8_t grayLut[8] = {
    0,    // 000: bw=0 msb=0 lsb=0 → black
    0,    // 001: bw=0 msb=0 lsb=1 → black (msb=0 → black regardless of lsb)
    170,  // 010: bw=0 msb=1 lsb=0 → light gray
    85,   // 011: bw=0 msb=1 lsb=1 → dark gray
    255,  // 100: bw=1 → white
    255,  // 101: bw=1 → white
    255,  // 110: bw=1 → white
    255,  // 111: bw=1 → white
  };

  const int H = static_cast<int>(EInkDisplay::DISPLAY_HEIGHT);
  const int W = static_cast<int>(EInkDisplay::DISPLAY_WIDTH);
  const int WB = static_cast<int>(EInkDisplay::DISPLAY_WIDTH_BYTES);

  for (int y = 0; y < H; y++) {
    const int rotX = H - 1 - y;
    const size_t rowBase = static_cast<size_t>(y) * WB;

    for (int byteIdx = 0; byteIdx < WB; byteIdx++) {
      const uint8_t bwByte  = bw[rowBase + byteIdx];
      const uint8_t lsbByte = lsb[rowBase + byteIdx];
      const uint8_t msbByte = msb[rowBase + byteIdx];
      const int xBase = byteIdx * 8;
      const int remaining = W - xBase;
      const int count = remaining < 8 ? remaining : 8;

      for (int b = 0; b < count; b++) {
        const uint8_t mask = 0x80 >> b;
        const int lutIdx = ((bwByte & mask) ? 4 : 0)
                         | ((msbByte & mask) ? 2 : 0)
                         | ((lsbByte & mask) ? 1 : 0);
        const uint8_t v = grayLut[lutIdx];
        const int rotY = xBase + b;
        const size_t off = static_cast<size_t>(rotY) * static_cast<size_t>(pitch)
                         + static_cast<size_t>(rotX) * 3;
        pixels[off + 0] = v;
        pixels[off + 1] = v;
        pixels[off + 2] = v;
      }
    }
  }

  SDL_UnlockTexture(g_texture);
  SDL_RenderClear(g_renderer);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
  SDL_RenderPresent(g_renderer);
}
}  // namespace

EInkDisplay::EInkDisplay(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t)
    : frameBuffer(frameBuffer0), isScreenOn(false) {}

bool sim_display_init(void) {
  if (g_window) return true;
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
  g_window = SDL_CreateWindow("Crosspoint Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  if (!g_window) return false;
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
  if (!g_renderer) return false;
  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                WINDOW_WIDTH, WINDOW_HEIGHT);
  if (!g_texture) return false;
  return true;
}

void sim_display_shutdown(void) {
  if (g_texture) {
    SDL_DestroyTexture(g_texture);
    g_texture = nullptr;
  }
  if (g_renderer) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = nullptr;
  }
  if (g_window) {
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
  }
  SDL_Quit();
}

void EInkDisplay::begin() {
  if (g_window) {
    frameBuffer = frameBuffer0;
    memset(frameBuffer0, 0xFF, EInkDisplay::BUFFER_SIZE);
    isScreenOn = true;
    return;
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return;
  g_window = SDL_CreateWindow("Crosspoint Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  if (!g_window) return;
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
  if (!g_renderer) return;
  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                WINDOW_WIDTH, WINDOW_HEIGHT);
  if (!g_texture) return;
  frameBuffer = frameBuffer0;
  memset(frameBuffer0, 0xFF, EInkDisplay::BUFFER_SIZE);
  isScreenOn = true;
}

void EInkDisplay::clearScreen(uint8_t color) const {
  memset(frameBuffer, color, EInkDisplay::BUFFER_SIZE);
}

void EInkDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            bool) const {
  SpiBusGuard guard;
  if (!imageData || !frameBuffer) return;
  const uint16_t rowBytes = (w + 7) / 8;
  for (uint16_t row = 0; row < h; row++) {
    uint16_t dstY = y + row;
    if (dstY >= EInkDisplay::DISPLAY_HEIGHT) break;
    for (uint16_t col = 0; col < w; col += 8) {
      uint8_t byte = imageData[row * rowBytes + col / 8];
      for (int b = 0; b < 8 && col + b < w; b++) {
        uint16_t dstX = x + col + b;
        if (dstX >= EInkDisplay::DISPLAY_WIDTH) break;
        if (byte & (0x80 >> b)) {
          size_t idx = dstY * EInkDisplay::DISPLAY_WIDTH_BYTES + dstX / 8;
          frameBuffer[idx] &= ~(0x80 >> (dstX & 7));
        }
      }
    }
  }
}

void EInkDisplay::setFramebuffer(const uint8_t*) const {}

void EInkDisplay::copyGrayscaleBuffers(const uint8_t* lsb, const uint8_t* msb) {
  if (!lsb || !msb) return;
  memcpy(g_grayLsbBuffer, lsb, EInkDisplay::BUFFER_SIZE);
  memcpy(g_grayMsbBuffer, msb, EInkDisplay::BUFFER_SIZE);
  g_hasGrayLsb = true;
  g_hasGrayMsb = true;
}
void EInkDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsb) {
  if (!lsb) return;
  memcpy(g_grayLsbBuffer, lsb, EInkDisplay::BUFFER_SIZE);
  g_hasGrayLsb = true;
}
void EInkDisplay::copyGrayscaleMsbBuffers(const uint8_t* msb) {
  if (!msb) return;
  memcpy(g_grayMsbBuffer, msb, EInkDisplay::BUFFER_SIZE);
  g_hasGrayMsb = true;
}
void EInkDisplay::cleanupGrayscaleBuffers(const uint8_t*) {
  g_hasGrayLsb = false;
  g_hasGrayMsb = false;
}

void EInkDisplay::displayBuffer(RefreshMode, bool) {
  SpiBusGuard guard;
  if (!frameBuffer) return;
  memcpy(g_bwBuffer, frameBuffer, EInkDisplay::BUFFER_SIZE);
  g_hasBw = true;
  g_hasGrayLsb = false;
  g_hasGrayMsb = false;
  render_bw_to_texture(frameBuffer);
}

void EInkDisplay::displayWindow(uint16_t, uint16_t, uint16_t, uint16_t) {
  displayBuffer(FAST_REFRESH);
}

void EInkDisplay::displayGrayBuffer(bool) {
  SpiBusGuard guard;
  if (!g_hasBw || !g_hasGrayLsb || !g_hasGrayMsb) return;
  render_gray_to_texture(g_bwBuffer, g_grayLsbBuffer, g_grayMsbBuffer);
}
void EInkDisplay::refreshDisplay(RefreshMode mode, bool) { displayBuffer(mode); }
void EInkDisplay::grayscaleRevert() {}
void EInkDisplay::setCustomLUT(bool, const unsigned char*) {}
void EInkDisplay::deepSleep() { isScreenOn = false; }
void EInkDisplay::saveFrameBufferAsPBM(const char*) {}

HalDisplay::HalDisplay() : einkDisplay(0, 0, 0, 0, 0, 0) {}
HalDisplay::~HalDisplay() = default;

void HalDisplay::begin() { einkDisplay.begin(); }
void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }
void HalDisplay::drawImage(const uint8_t* d, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool pm) const {
  einkDisplay.drawImage(d, x, y, w, h, pm);
}
void HalDisplay::displayBuffer(RefreshMode mode, bool fadingFix) {
  einkDisplay.displayBuffer(static_cast<EInkDisplay::RefreshMode>(mode), fadingFix);
}
void HalDisplay::refreshDisplay(RefreshMode mode, bool turnOff) {
  einkDisplay.refreshDisplay(static_cast<EInkDisplay::RefreshMode>(mode), turnOff);
}
void HalDisplay::deepSleep() { einkDisplay.deepSleep(); }
uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }
void HalDisplay::copyGrayscaleBuffers(const uint8_t* l, const uint8_t* m) {
  einkDisplay.copyGrayscaleBuffers(l, m);
}
void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* l) {
  einkDisplay.copyGrayscaleLsbBuffers(l);
}
void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* m) {
  einkDisplay.copyGrayscaleMsbBuffers(m);
}
void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* b) {
  einkDisplay.cleanupGrayscaleBuffers(b);
}
void HalDisplay::displayGrayBuffer(bool fadingFix) { einkDisplay.displayGrayBuffer(fadingFix); }

bool sim_display_pump_events(void) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) return false;
  }
  return true;
}
