#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <HalDisplay.h>

#include "sim_spi_bus.h"

#include <SDL.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

static const int WINDOW_WIDTH = static_cast<int>(EInkDisplay::DISPLAY_HEIGHT);
static const int WINDOW_HEIGHT = static_cast<int>(EInkDisplay::DISPLAY_WIDTH);

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_texture = nullptr;

namespace {
// Panel-side output inversion (HalDisplay::setInverted); the framebuffer itself stays normal.
bool g_inverted = false;
bool g_hasBw = false;
bool g_hasGrayLsb = false;
bool g_hasGrayMsb = false;
uint8_t g_bwBuffer[EInkDisplay::BUFFER_SIZE];
uint8_t g_grayLsbBuffer[EInkDisplay::BUFFER_SIZE];
uint8_t g_grayMsbBuffer[EInkDisplay::BUFFER_SIZE];

void render_bw_to_texture(const uint8_t* buf) {
  if (!g_renderer || !g_texture || !buf) return;
  uint8_t* pixels = nullptr;
  int pitch = 0;
  if (SDL_LockTexture(g_texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch) != 0) return;

  const int H = static_cast<int>(EInkDisplay::DISPLAY_HEIGHT);
  const int W = static_cast<int>(EInkDisplay::DISPLAY_WIDTH);
  const int WB = static_cast<int>(EInkDisplay::DISPLAY_WIDTH_BYTES);

  for (int y = 0; y < H; y++) {
    const int rotX = H - 1 - y;
    const size_t rowBase = static_cast<size_t>(y) * WB;

    for (int byteIdx = 0; byteIdx < WB; byteIdx++) {
      const uint8_t byte = buf[rowBase + byteIdx];
      const int xBase = byteIdx * 8;
      const int remaining = W - xBase;
      const int count = remaining < 8 ? remaining : 8;

      for (int b = 0; b < count; b++) {
        const uint8_t v = ((byte & (0x80 >> b)) != 0) != g_inverted ? 255 : 0;
        const int rotY = xBase + b;
        const size_t off = static_cast<size_t>(rotY) * static_cast<size_t>(pitch) + static_cast<size_t>(rotX) * 3;
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

  static constexpr uint8_t grayLut[8] = {0, 0, 170, 85, 255, 255, 255, 255};

  const int H = static_cast<int>(EInkDisplay::DISPLAY_HEIGHT);
  const int W = static_cast<int>(EInkDisplay::DISPLAY_WIDTH);
  const int WB = static_cast<int>(EInkDisplay::DISPLAY_WIDTH_BYTES);

  for (int y = 0; y < H; y++) {
    const int rotX = H - 1 - y;
    const size_t rowBase = static_cast<size_t>(y) * WB;

    for (int byteIdx = 0; byteIdx < WB; byteIdx++) {
      const uint8_t bwByte = bw[rowBase + byteIdx];
      const uint8_t lsbByte = lsb[rowBase + byteIdx];
      const uint8_t msbByte = msb[rowBase + byteIdx];
      const int xBase = byteIdx * 8;
      const int remaining = W - xBase;
      const int count = remaining < 8 ? remaining : 8;

      for (int b = 0; b < count; b++) {
        const uint8_t mask = 0x80 >> b;
        const int lutIdx = ((bwByte & mask) ? 4 : 0) | ((msbByte & mask) ? 2 : 0) | ((lsbByte & mask) ? 1 : 0);
        const uint8_t v = grayLut[lutIdx];
        const int rotY = xBase + b;
        const size_t off = static_cast<size_t>(rotY) * static_cast<size_t>(pitch) + static_cast<size_t>(rotX) * 3;
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

HalDisplay display;

EInkDisplay::EInkDisplay(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) : frameBuffer(frameBuffer0), isScreenOn(false) {}

bool sim_display_init(void) {
  if (g_window) return true;
  // main() is ours, not SDL_main (main_sim.cpp does not include SDL.h), so tell SDL.
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
  g_window = SDL_CreateWindow("Crosspoint Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                              WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  if (!g_window) return false;
  // PRESENTVSYNC: without it, SDL_RenderPresent() can swap mid-scanout,
  // showing a torn frame -- a slice of the previous content at the top of
  // the window while the rest already shows the new frame. Purely a
  // display-presentation artifact of the emulator (real e-ink hardware
  // transfers the buffer atomically over SPI and can't tear this way).
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!g_renderer) return false;
  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH,
                                WINDOW_HEIGHT);
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
  if (!g_window && !sim_display_init()) return;
  frameBuffer = frameBuffer0;
  memset(frameBuffer0, 0xFF, EInkDisplay::BUFFER_SIZE);
  isScreenOn = true;
}

void EInkDisplay::clearScreen(uint8_t color) const { memset(frameBuffer, color, EInkDisplay::BUFFER_SIZE); }

void EInkDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool) const {
  SpiBusGuard guard;
  if (!imageData || !frameBuffer) return;
  const uint16_t rowBytes = (w + 7) / 8;
  for (uint16_t row = 0; row < h; row++) {
    const uint16_t dstY = y + row;
    if (dstY >= EInkDisplay::DISPLAY_HEIGHT) break;
    for (uint16_t col = 0; col < w; col += 8) {
      const uint8_t byte = imageData[row * rowBytes + col / 8];
      for (int b = 0; b < 8 && col + b < w; b++) {
        const uint16_t dstX = x + col + b;
        if (dstX >= EInkDisplay::DISPLAY_WIDTH) break;
        // Icon format: 1 = white (transparent), 0 = black (foreground).
        // Only draw black pixels; leave white pixels as-is (transparent).
        if (!(byte & (0x80 >> b))) {
          const size_t idx = dstY * EInkDisplay::DISPLAY_WIDTH_BYTES + dstX / 8;
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

void EInkDisplay::displayWindow(uint16_t, uint16_t, uint16_t, uint16_t) { displayBuffer(FAST_REFRESH); }
void EInkDisplay::displayGrayBuffer(bool) {
  SpiBusGuard guard;
  if (!g_hasBw || !g_hasGrayLsb || !g_hasGrayMsb) return;
  render_gray_to_texture(g_bwBuffer, g_grayLsbBuffer, g_grayMsbBuffer);
}
void EInkDisplay::refreshDisplay(RefreshMode mode, bool) { displayBuffer(mode); }
void EInkDisplay::displayGrayscaleBase(RefreshMode fallback, bool turnOffScreen) { displayBuffer(fallback, turnOffScreen); }
void EInkDisplay::grayscaleRevert() {}
void EInkDisplay::setCustomLUT(bool, const unsigned char*) {}
void EInkDisplay::deepSleep() { isScreenOn = false; }
void EInkDisplay::saveFrameBufferAsPBM(const char*) {}
// Real-hardware waveform preconditioning before a grayscale pass; no
// comparable panel timing concern in the sim.
void EInkDisplay::preconditionGrayscale() {}
void EInkDisplay::preconditionGrayscale(uint16_t, uint16_t, uint16_t, uint16_t) {}

HalDisplay::HalDisplay() : einkDisplay(0, 0, 0, 0, 0, 0) {}
HalDisplay::~HalDisplay() = default;

void HalDisplay::begin(bool) { einkDisplay.begin(); }
void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }
void HalDisplay::drawImage(const uint8_t* d, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool pm) const {
  einkDisplay.drawImage(d, x, y, w, h, pm);
}
void HalDisplay::drawImageTransparent(const uint8_t* d, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool pm) const {
  einkDisplay.drawImage(d, x, y, w, h, pm);
}
void HalDisplay::displayBuffer(RefreshMode mode, bool turnOffScreen) {
  einkDisplay.displayBuffer(static_cast<EInkDisplay::RefreshMode>(mode), turnOffScreen);
}
void HalDisplay::displayBufferAsync(RefreshMode mode) { displayBuffer(mode); }
void HalDisplay::waitRefreshComplete() {}
bool HalDisplay::supportsAsyncRefresh() const { return false; }
void HalDisplay::displayGrayscaleBase(RefreshMode fallback, bool turnOffScreen) {
  einkDisplay.displayGrayscaleBase(static_cast<EInkDisplay::RefreshMode>(fallback), turnOffScreen);
}
void HalDisplay::refreshDisplay(RefreshMode mode, bool turnOff) {
  einkDisplay.refreshDisplay(static_cast<EInkDisplay::RefreshMode>(mode), turnOff);
}
void HalDisplay::deepSleep() { einkDisplay.deepSleep(); }
void HalDisplay::preconditionGrayscale() { einkDisplay.preconditionGrayscale(); }
void HalDisplay::preconditionGrayscale(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  einkDisplay.preconditionGrayscale(x, y, w, h);
}
uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }
uint8_t* HalDisplay::lendFrameBufferStorage(uint32_t* sizeOut) {
  if (sizeOut) *sizeOut = BUFFER_SIZE;
  return einkDisplay.getFrameBuffer();
}
void HalDisplay::returnFrameBufferStorage() { memset(einkDisplay.getFrameBuffer(), 0xFF, BUFFER_SIZE); }
void HalDisplay::copyGrayscaleBuffers(const uint8_t* l, const uint8_t* m) { einkDisplay.copyGrayscaleBuffers(l, m); }
void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* l) { einkDisplay.copyGrayscaleLsbBuffers(l); }
void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* m) { einkDisplay.copyGrayscaleMsbBuffers(m); }
void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* b) { einkDisplay.cleanupGrayscaleBuffers(b); }
void HalDisplay::displayGrayBuffer(bool turnOffScreen) { einkDisplay.displayGrayBuffer(turnOffScreen); }
void HalDisplay::writeGrayscalePlaneStrip(bool lsbPlane, const uint8_t* rows, uint16_t yStart, uint16_t numRows) {
  if (!rows || yStart >= DISPLAY_HEIGHT) return;
  if (yStart + numRows > DISPLAY_HEIGHT) numRows = DISPLAY_HEIGHT - yStart;
  uint8_t* plane = lsbPlane ? g_grayLsbBuffer : g_grayMsbBuffer;
  memcpy(plane + static_cast<size_t>(yStart) * DISPLAY_WIDTH_BYTES, rows, static_cast<size_t>(numRows) * DISPLAY_WIDTH_BYTES);
  (lsbPlane ? g_hasGrayLsb : g_hasGrayMsb) = true;
}
bool HalDisplay::supportsStripGrayscale() const { return grayscaleCapabilities().stripUploads; }

HalDisplay::Controller HalDisplay::getController() const { return BoardConfig::ACTIVE.displayController; }

// What the X4's SSD1677 driver reports: overlay grayscale only, and none while inverted.
HalDisplay::GrayscaleCapabilities HalDisplay::grayscaleCapabilities(GrayscaleMode mode) const {
  if (g_inverted || mode != GrayscaleMode::Overlay) return {};
  return {GrayscaleEncoding::OverlayMasks, GrayscaleBase::Separate, true, true, false};
}
bool HalDisplay::supportsAsyncGrayscaleBase() const { return grayscaleCapabilities().asyncBase; }
bool HalDisplay::combinesGrayscaleBase() const { return grayscaleCapabilities().base == GrayscaleBase::Combined; }
bool HalDisplay::displayGrayscaleBase(GrayscaleMode mode, RefreshMode fallback, bool turnOffScreen) {
  if (!grayscaleCapabilities(mode).supported()) return false;
  displayGrayscaleBase(fallback, turnOffScreen);
  return true;
}

void HalDisplay::setInverted(bool inverted) { g_inverted = inverted; }
bool HalDisplay::toggleInverted() {
  g_inverted = !g_inverted;
  return g_inverted;
}
bool HalDisplay::isInverted() const { return g_inverted; }
uint16_t HalDisplay::getDisplayWidth() const { return DISPLAY_WIDTH; }
uint16_t HalDisplay::getDisplayHeight() const { return DISPLAY_HEIGHT; }
uint16_t HalDisplay::getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
uint32_t HalDisplay::getBufferSize() const { return BUFFER_SIZE; }

namespace {
// Cmd+S (Ctrl+S on Linux) writes what is on screen to screenshots/, at the panel's exact
// 800x480 -- unlike an OS window grab, which picks up chrome and Retina scaling.
//
// BMP because that is all SDL2 writes without SDL_image. miniz is vendored but compiled with
// MINIZ_NO_DEFLATE_APIS, so its PNG writer is not built, and turning deflate on would grow the
// firmware for an emulator convenience. Convert afterwards, e.g.
//   sips -s format png screenshots/*.bmp --out docs/images/screenshots/
void save_screenshot() {
  if (!g_renderer) return;

  int w = 0, h = 0;
  SDL_GetRendererOutputSize(g_renderer, &w, &h);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
  if (!surface) {
    printf("[SHOT] surface alloc failed: %s\n", SDL_GetError());
    return;
  }
  // Redraw the frame before reading. SDL_RenderPresent swaps buffers and leaves the backbuffer
  // undefined, so reading it after the last present returns a stale or empty frame -- which is
  // what the first version of this did. g_texture still holds the current frame, so copy it in,
  // read, then present again to leave the window as it was.
  SDL_RenderClear(g_renderer);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
  const int rc = SDL_RenderReadPixels(g_renderer, nullptr, SDL_PIXELFORMAT_RGB24, surface->pixels, surface->pitch);
  SDL_RenderPresent(g_renderer);
  if (rc != 0) {
    printf("[SHOT] read failed: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

#ifdef _WIN32
  _mkdir("screenshots");  // fails harmlessly when it already exists
#else
  mkdir("screenshots", 0755);  // fails harmlessly when it already exists
#endif
  char path[128];
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_r(&now, &tm);
  std::strftime(path, sizeof(path), "screenshots/crosspoint-%Y%m%d-%H%M%S.bmp", &tm);

  if (SDL_SaveBMP(surface, path) == 0) {
    printf("[SHOT] %s (%dx%d)\n", path, w, h);
  } else {
    printf("[SHOT] save failed: %s\n", SDL_GetError());
  }
  fflush(stdout);
  SDL_FreeSurface(surface);
}
}  // namespace

bool sim_display_pump_events(void) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) return false;
    // Buttons are read through SDL_GetKeyboardState, so consuming key events here does not
    // interfere with them. S is not bound to any button.
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_s && (e.key.keysym.mod & (KMOD_GUI | KMOD_CTRL)) &&
        e.key.repeat == 0) {
      save_screenshot();
    }
  }
  return true;
}
