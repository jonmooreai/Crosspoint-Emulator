// Emulator entry point: init SDL/sim, then run Crosspoint setup() and loop().
// setup() and loop() are defined in Crosspoint src/main.cpp.
// Behavior matches the real device: single core (prewarm on main thread with yields),
// shared SPI (display and SD serialized).

#include <Epub.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <SdFat.h>
#include "sim_display.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <string>
#include <unistd.h>

// Crosspoint app entry points (from main.cpp)
extern void setup();
extern void loop();

namespace {
constexpr int kLibraryThumbHeight = 100;

bool endsWithEpub(const std::string& name) {
  if (name.size() < 5) return false;
  std::string ext = name.substr(name.size() - 5);
  for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".epub";
}

std::atomic<bool> g_prewarmDone{false};

// Prewarm one EPUB per main-loop iteration so UI gets control between thumbnails
// (matches device: single core, yields in image generation).
static bool g_prewarmRootOpen = false;
static FsFile g_prewarmRoot;

void prewarmStep() {
  if (g_prewarmDone.load()) return;

  if (!g_prewarmRootOpen) {
    g_prewarmRoot = SdMan.open("/", O_RDONLY);
    if (!g_prewarmRoot || !g_prewarmRoot.isDirectory()) {
      Serial.printf("[%lu] [SIM] Could not open SD root for thumb prewarm\n", millis());
      g_prewarmDone.store(true);
      return;
    }
    g_prewarmRootOpen = true;
    return;
  }

  FsFile file = g_prewarmRoot.openNextFile();
  if (!file) {
    g_prewarmRoot.close();
    g_prewarmRootOpen = false;
    g_prewarmDone.store(true);
    Serial.printf("[%lu] [SIM] Thumb prewarm complete\n", millis());
    return;
  }
  if (file.isDirectory()) {
    file.close();
    return;
  }
  char name[512];
  if (!file.getName(name, sizeof(name))) {
    file.close();
    return;
  }
  file.close();

  std::string filename(name);
  if (!endsWithEpub(filename)) {
    return;
  }

  const std::string path = "/" + filename;
  Epub epub(path, "/.crosspoint");
  if (!epub.load(true, true)) {
    Serial.printf("[%lu] [SIM] Failed to load EPUB for thumb prewarm: %s\n", millis(), path.c_str());
    return;
  }
  if (!epub.generateThumbBmp(kLibraryThumbHeight)) {
    Serial.printf("[%lu] [SIM] Failed to prewarm thumb: %s\n", millis(), path.c_str());
  } else {
    Serial.printf("[%lu] [SIM] Prewarmed thumb: %s\n", millis(), path.c_str());
  }
}
}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  // Ensure ./sdcard is findable: if run from build/, chdir to project root
  if (access("./sdcard", F_OK) != 0 && access("../sdcard", F_OK) == 0) {
    if (chdir("..") != 0) {
      fprintf(stderr, "Could not chdir to project root (../sdcard)\n");
    }
  }

  if (!sim_display_init()) {
    fprintf(stderr, "sim_display_init failed\n");
    return 1;
  }

  printf("Crosspoint emulator: running setup() then loop(). Close window to exit.\n");
  setup();

  // Single main thread: one prewarm step per frame, then events and loop (matches device).
  while (true) {
    prewarmStep();
    if (!sim_display_pump_events()) {
      break;
    }
    loop();
  }

  sim_display_shutdown();
  return 0;
}
