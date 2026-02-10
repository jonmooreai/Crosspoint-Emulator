// Emulator entry point: init SDL/sim, then run Crosspoint setup() and loop().
// setup() and loop() are defined in Crosspoint src/main.cpp.

#include <Epub.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <SdFat.h>
#include "sim_display.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <string>
#include <thread>
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

// Background thumbnail prewarm — runs off the main thread so the UI is
// interactive immediately after setup().  The main loop is not blocked.
std::atomic<bool> g_prewarmDone{false};

void prewarmLibraryEpubThumbs() {
  FsFile root = SdMan.open("/", O_RDONLY);
  if (!root || !root.isDirectory()) {
    Serial.printf("[%lu] [SIM] Could not open SD root for thumb prewarm\n", millis());
    g_prewarmDone.store(true);
    return;
  }

  char name[512];
  for (FsFile file = root.openNextFile(); file; file = root.openNextFile()) {
    if (file.isDirectory()) {
      file.close();
      continue;
    }
    if (!file.getName(name, sizeof(name))) {
      file.close();
      continue;
    }
    file.close();

    std::string filename(name);
    if (!endsWithEpub(filename)) {
      continue;
    }

    const std::string path = "/" + filename;
    Epub epub(path, "/.crosspoint");
    if (!epub.load(true, true)) {
      Serial.printf("[%lu] [SIM] Failed to load EPUB for thumb prewarm: %s\n", millis(), path.c_str());
      continue;
    }

    if (!epub.generateThumbBmp(kLibraryThumbHeight)) {
      Serial.printf("[%lu] [SIM] Failed to prewarm thumb: %s\n", millis(), path.c_str());
    } else {
      Serial.printf("[%lu] [SIM] Prewarmed thumb: %s\n", millis(), path.c_str());
    }
  }
  root.close();
  g_prewarmDone.store(true);
  Serial.printf("[%lu] [SIM] Thumb prewarm complete\n", millis());
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

  // Launch thumbnail prewarm in a background thread so the UI is usable
  // immediately.  The thread is detached — it writes only to the SD card
  // cache directory and sets g_prewarmDone when finished.
  std::thread prewarmThread(prewarmLibraryEpubThumbs);
  prewarmThread.detach();

  while (true) {
    if (!sim_display_pump_events()) {
      break;  // quit requested
    }
    loop();
  }

  sim_display_shutdown();
  return 0;
}
