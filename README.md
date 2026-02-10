# Crosspoint Native Emulator

Run the [Crosspoint](https://github.com/crosspoint/crosspoint) e-reader firmware on your computer with an SDL2 window (800×480), directory-backed "SD card," and keyboard input. No device or flashing required; useful for UI development and quick iteration.

---

## Table of Contents

1. [Overview](#overview)
2. [Download & Setup](#download--setup)
3. [Building](#building)
4. [Running](#running)
5. [Architecture](#architecture)
6. [New Features in Crosspoint Core](#new-features-in-crosspoint-core)
7. [Usage Guide](#usage-guide)
8. [Troubleshooting](#troubleshooting)
9. [Development](#development)

---

## Overview

The Crosspoint Native Emulator provides a complete simulation environment for the Crosspoint e-reader firmware. It runs the same application code as the physical device, but replaces hardware-specific components (e-ink display, GPIO buttons, SD card) with desktop equivalents:

- **Display**: SDL2 window (480×800, rotated from logical 800×480)
- **Storage**: Local directory (`./sdcard/`) mapped to virtual SD card
- **Input**: Keyboard mapped to device buttons
- **Networking**: Stubbed (WiFi, OTA updates not available)

This allows rapid UI development, testing, and debugging without needing physical hardware or flashing firmware.

---

## Download & Setup

### Prerequisites

Before building, ensure you have:

1. **C++17 compiler**
   - **macOS**: Xcode Command Line Tools (`xcode-select --install`)
   - **Linux**: `gcc` or `clang` (usually pre-installed)
   - **Windows**: MSVC 2017+ or MinGW-w64

2. **CMake 3.16+**
   - **macOS**: 
     - Via Homebrew: `brew install cmake`
     - Or download `.dmg` from [cmake.org/download](https://cmake.org/download/)
   - **Linux**: `sudo apt-get install cmake` (Debian/Ubuntu) or equivalent
   - **Windows**: Download installer from cmake.org

3. **SDL2** (version 2.x, **not** SDL3)
   - **macOS**: `brew install sdl2`
   - **Linux**: `sudo apt-get install libsdl2-dev` (Debian/Ubuntu)
   - **Windows**: Download from [libsdl.org](https://www.libsdl.org/download-2.0.php)
   - **Manual build**: See [Building SDL2 from Source](#building-sdl2-from-source) below

4. **Python 3** (for pre-build HTML generation)
   - Usually pre-installed on macOS/Linux
   - Verify: `python3 --version`

5. **Crosspoint Repository**
   - Must be available as a sibling directory to this project
   - Or set `CROSSPOINT_ROOT` environment variable/CMake flag

### Downloading the Emulator

```bash
# Clone the emulator repository
git clone https://github.com/your-org/crosspoint-emulator.git
cd crosspoint-emulator

# Ensure Crosspoint is available (as sibling directory)
# If not, clone it:
git clone https://github.com/your-org/Crosspoint.git ../Crosspoint
```

### Building SDL2 from Source

If you need to build SDL2 manually (e.g., from Downloads folder):

1. **Download SDL2 source** (2.x release, e.g., SDL2-2.30.2):
   ```bash
   # Download from https://github.com/libsdl-org/SDL/releases
   # Extract to ~/Downloads/SDL2-2.30.2
   ```

2. **Build and install**:
   ```bash
   cd ~/Downloads/SDL2-2.30.2
   mkdir build && cd build
   cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local
   make -j4
   make install
   ```

3. **Use custom SDL2** when configuring emulator:
   ```bash
   cmake .. -DCROSSPOINT_ROOT=/path/to/Crosspoint -DSDL2_ROOT=$HOME/.local
   ```

---

## Building

### Step-by-Step Build Instructions

1. **Navigate to project directory**:
   ```bash
   cd /path/to/crosspoint-emulator
   ```

2. **Create build directory**:
   ```bash
   mkdir build && cd build
   ```

3. **Configure CMake**:
   
   **If Crosspoint is a sibling directory** (`../Crosspoint`):
   ```bash
   cmake ..
   ```
   
   **If Crosspoint is elsewhere**:
   ```bash
   cmake .. -DCROSSPOINT_ROOT=/absolute/path/to/Crosspoint
   ```
   
   **If using custom SDL2**:
   ```bash
   cmake .. -DCROSSPOINT_ROOT=/path/to/Crosspoint -DSDL2_ROOT=/path/to/SDL2-install
   ```

4. **Build the executable**:
   ```bash
   cmake --build .
   ```
   
   Or use `make` if available:
   ```bash
   make -j4
   ```

5. **Verify build**:
   ```bash
   ls -lh crosspoint_emulator
   # Should show the executable
   ```

### Optional: ArduinoJson Dependency

If you want web server screens to compile (they're stubbed but won't crash):

1. From the Crosspoint repo, run PlatformIO once:
   ```bash
   cd ../Crosspoint
   pio run
   ```
   
   This creates `.pio/libdeps/default/ArduinoJson`, which the emulator CMake will detect and use.

### Build Output

The build process:
- Compiles all Crosspoint application sources (`src/*.cpp`)
- Compiles Crosspoint libraries (GfxRenderer, Epub, Txt, Xtc, fonts, etc.)
- Compiles emulator HAL stubs (`sim/src/*.cpp`)
- Links everything into `crosspoint_emulator` executable

**Build time**: Typically 1-3 minutes depending on hardware.

---

## Running

### Basic Usage

1. **Ensure SD card directory exists**:
   ```bash
   # From project root (not build/)
   mkdir -p sdcard
   ```

2. **Add books to SD card**:
   ```bash
   cp /path/to/book.epub sdcard/
   cp /path/to/book.txt sdcard/
   ```

3. **Run the emulator**:
   ```bash
   # From build directory
   ./crosspoint_emulator
   
   # Or from project root
   ./build/crosspoint_emulator
   ```

4. **Exit**: Close the SDL window (click X or press Alt+F4).

### Running from Different Directories

The emulator automatically detects `./sdcard/` relative to the current working directory. If run from `build/`, it checks `../sdcard/` automatically.

**Recommended**: Always run from project root:
```bash
cd /path/to/crosspoint-emulator
./build/crosspoint_emulator
```

### Keyboard Controls

| Key | Action | Device Button |
|-----|--------|--------------|
| **Arrow Keys** | Navigate | Left / Right / Up / Down |
| **Enter** | Confirm / Select | Confirm |
| **Backspace** or **Escape** | Back / Cancel | Back |
| **P** | Power (hold for sleep in settings) | Power |

### Storage (Virtual SD Card)

The emulator uses `./sdcard/` (relative to current working directory) as the virtual SD card root.

**Supported file formats**:
- **EPUB** (`.epub`) - Full support with metadata, covers, progress tracking
- **TXT** (`.txt`) - Plain text files
- **XTC** (`.xtc`) - Custom format

**Example workflow**:
```bash
# Create SD card directory
mkdir -p sdcard

# Add books
cp ~/Documents/books/*.epub sdcard/
cp ~/Documents/books/*.txt sdcard/

# Organize into folders
mkdir -p sdcard/Novels
mkdir -p sdcard/Technical
mv sdcard/*.epub sdcard/Novels/

# Run emulator
./build/crosspoint_emulator
# Navigate: Home → My Library → Novels → select book
```

---

## Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Crosspoint Emulator                     │
├─────────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────────┐         ┌──────────────────┐     │
│  │  Crosspoint App  │────────▶│   Sim HAL Layer  │     │
│  │   (main.cpp)     │         │  (sim/include/)  │     │
│  │                  │         │                  │     │
│  │  • Activities   │         │  • HalDisplay    │     │
│  │  • Themes      │         │  • HalGPIO       │     │
│  │  • Readers     │         │  • SDCardManager │     │
│  │  • UI Logic    │         │  • Arduino Stubs │     │
│  └──────────────────┘         └──────────────────┘     │
│         │                              │                    │
│         │                              │                    │
│         ▼                              ▼                    │
│  ┌──────────────────┐         ┌──────────────────┐     │
│  │  Crosspoint Libs  │         │   Host Platform  │     │
│  │                  │         │                  │     │
│  │  • GfxRenderer  │         │  • SDL2 Window  │     │
│  │  • Epub/Txt/Xtc │         │  • File System   │     │
│  │  • Fonts        │         │  • Keyboard      │     │
│  │  • Utf8        │         │                  │     │
│  └──────────────────┘         └──────────────────┘     │
│                                                           │
└─────────────────────────────────────────────────────────────┘
```

### Component Flow

```
┌─────────────┐
│   main()    │  ← Entry point (main_sim.cpp)
└──────┬──────┘
       │
       ├─▶ sim_display_init()  → SDL2 window creation
       │
       ├─▶ setup()              → Crosspoint initialization
       │
       └─▶ loop()               → Main event loop
            │
            ├─▶ sim_display_pump_events()  → Keyboard input
            │
            ├─▶ HalGPIO::update()         → Button state
            │
            ├─▶ Activity::loop()          → UI logic
            │
            └─▶ HalDisplay::displayBuffer() → Render to SDL2
```

### HAL Abstraction Layer

The Hardware Abstraction Layer (HAL) provides a consistent interface between Crosspoint application code and platform-specific implementations:

```
┌──────────────────────────────────────────────────────────────┐
│              Crosspoint Application Code                   │
│  (Activities, Themes, Readers, UI Logic)               │
└────────────────────┬───────────────────────────────────┘
                     │
                     │ Uses HAL interfaces
                     │
┌────────────────────▼───────────────────────────────────┐
│                    HAL Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐ │
│  │ HalDisplay   │  │  HalGPIO     │  │SDCardMgr  │ │
│  │             │  │             │  │           │ │
│  │ • display() │  │ • isPressed │  │ • open()  │ │
│  │ • clear()   │  │ • wasPress()│  │ • read()   │ │
│  └──────────────┘  └──────────────┘  └──────────┘ │
└────────────────────┬───────────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
         ▼                       ▼
┌─────────────────┐    ┌─────────────────┐
│  Device HAL      │    │   Sim HAL        │
│  (ESP32)        │    │  (SDL2/Desktop) │
│                 │    │                 │
│ • E-ink driver │    │ • SDL2 renderer │
│ • GPIO pins     │    │ • Keyboard map   │
│ • SD card SPI   │    │ • File system   │
└─────────────────┘    └─────────────────┘
```

### Display Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│         Crosspoint Rendering Pipeline                     │
├─────────────────────────────────────────────────────────────┤
│                                                         │
│  1. Application draws to framebuffer                   │
│     ┌─────────────────────────────────────┐            │
│     │  uint8_t frameBuffer[BUFFER_SIZE]  │            │
│     │  (800×480 bits = 48,000 bytes)    │            │
│     └─────────────────────────────────────┘            │
│                    │                                   │
│                    ▼                                   │
│  2. HalDisplay::displayBuffer()                       │
│     • Copies to internal buffer                       │
│     • Triggers render                                │
│                    │                                   │
│                    ▼                                   │
│  3. sim_display.cpp: render_bw_to_texture()          │
│     • Processes 8 pixels per byte (optimized)         │
│     • Rotates: logical (x,y) → window (H-1-y, x)    │
│     • Converts bits → RGB24 pixels                    │
│                    │                                   │
│                    ▼                                   │
│  4. SDL2 Texture Update                             │
│     • SDL_LockTexture()                              │
│     • Write RGB24 data                               │
│     • SDL_UnlockTexture()                           │
│                    │                                   │
│                    ▼                                   │
│  5. SDL2 Render Present                            │
│     • SDL_RenderCopy()                               │
│     • SDL_RenderPresent()                           │
│                    │                                   │
│                    ▼                                   │
│  6. Window Display (480×800)                         │
│     ┌─────────────────────┐                          │
│     │   SDL2 Window      │                          │
│     │   (Rotated view)   │                          │
│     └─────────────────────┘                          │
│                                                         │
└─────────────────────────────────────────────────────────────┘
```

### Storage Architecture

```
┌─────────────────────────────────────────────────────────────┐
│              Virtual SD Card System                      │
├─────────────────────────────────────────────────────────────┤
│                                                         │
│  Crosspoint App                                          │
│    │                                                     │
│    │ SDCardManager::open("/book.epub")                 │
│    ▼                                                     │
│  FsFile API                                             │
│    │                                                     │
│    │ resolvePath("/book.epub")                          │
│    │   → "./sdcard/book.epub"                          │
│    ▼                                                     │
│  POSIX File System                                       │
│    │                                                     │
│    │ fopen(), fread(), fwrite(), etc.                    │
│    ▼                                                     │
│  Host File System                                         │
│    │                                                     │
│    └─▶ ./sdcard/                                       │
│         ├── book1.epub                                   │
│         ├── book2.txt                                   │
│         └── Novels/                                    │
│              └── book3.epub                            │
│                                                         │
└─────────────────────────────────────────────────────────────┘
```

### Threading Model

```
┌─────────────────────────────────────────────────────────────┐
│              Thread Architecture                         │
├─────────────────────────────────────────────────────────────┤
│  Main Thread (UI Loop)                                   │
│  ┌─────────────────────────────────────┐                 │
│  │  while (true) {                   │                 │
│  │    sim_display_pump_events()       │                 │
│  │    loop()                        │                 │
│  │      └─▶ Activity::loop()        │                 │
│  │      └─▶ Activity::render()     │                 │
│  │  }                               │                 │
│  └─────────────────────────────────────┘                 │
│                                                         │
│  Background Thread (Thumbnail Prewarm)                    │
│  ┌─────────────────────────────────────┐                 │
│  │  prewarmLibraryEpubThumbs()      │                 │
│  │    • Scans sdcard/ for .epub    │                 │
│  │    • Generates thumbnails         │                 │
│  │    • Writes to cache dir         │                 │
│  │    • Non-blocking                │                 │
│  └─────────────────────────────────────┘                 │
│                                                         │
│  Note: UI is interactive immediately; thumbnails          │
│        appear as they're generated in background.         │
│                                                         │
└─────────────────────────────────────────────────────────────┘
```

---

## New Features in Crosspoint Core

This section documents the major features and improvements added to the Crosspoint core firmware that are available in the emulator.

### UX Foundation (February 2026)

#### Centralized Constants System

**Problem**: Timing thresholds, button labels, and layout metrics were duplicated across 10+ activity files, leading to inconsistencies and maintenance burden.

**Solution**: Created `UxConstants.h` with centralized definitions:

- **Timing Constants** (`UxTiming::`):
  - `kGoHomeMs` - Long-press duration to return home
  - `kSkipPageMs` - Long-press duration to skip page in library
  - `kChangeTabMs` - Tab switching threshold
  - All timing values defined once, imported everywhere

- **Button Labels** (`UxLabel::`):
  - `kBack` - "« Back"
  - `kHome` - "« Home"
  - `kSelect` - "Select"
  - `kNext` - "Next"
  - `kPrevious` - "Previous"
  - Standardized across all screens

**Impact**: Eliminated 12+ duplicated constant definitions, ensuring consistent UX timing and labels.

#### Screen Consistency Improvements

**Renamed "Browse Files" → "My Library"**
- Clearer, more user-friendly name on Home screen
- Better reflects the visual grid experience

**Standardized Back Button Labels**
- All Back buttons now use `<<` prefix consistently
- Previously mixed: `< Home`, `<< Home`, `<< Back`
- Now unified: `<< Back`, `<< Home` via `UxLabel::` constants

**Improved Empty States**
- **Library**: "No books here. Add files to the SD card."
- **Recents**: "No recent books. Open one from My Library."
- Clear, actionable messaging

**Migration**: 15+ `mapLabels()` call sites migrated to use `UxLabel::` constants.

### Library Grid System

#### Visual Grid Layout

**Feature**: When viewing a folder containing book files, the library displays a **2-column grid** instead of a text-only list.

**Each grid tile shows**:
- **Cover thumbnail** (top) - Generated from EPUB/XTC metadata
- **Title** (below cover) - One or two lines, truncated if needed
- **Progress indicator** (below title):
  - "Not started" - Book never opened
  - "XX%" - Reading progress (0-99%)
  - "Completed" - Finished (100%)

**Folders** appear in the same grid with:
- Folder icon (no cover)
- Folder name
- No progress indicator

**Grid Dimensions**:
- **Columns**: 2 (optimized for 480px width)
- **Tile height**: ~140px (configurable via theme metrics)
- **Rows per page**: 3-4 visible rows
- **Pagination**: Navigate by page for large libraries

#### Progress Tracking Enhancement

**EPUB Progress Format Extension**:

**Previous format** (6 bytes):
```
[spineIndex: 2 bytes][currentPage: 2 bytes][pageCount: 2 bytes]
```

**New format** (8 bytes):
```
[spineIndex: 2 bytes][currentPage: 2 bytes][pageCount: 2 bytes][percent: 2 bytes]
```

**Benefits**:
- Library can show progress **without opening the book**
- Faster library rendering (no EPUB parsing needed)
- Accurate percentage display (0-100)

**Backward Compatibility**: Library detects old 6-byte format and handles gracefully.

#### Memory-Safe Thumbnail Loading

**Optimization**: Only **one thumbnail is loaded at a time** during grid rendering.

**Process**:
1. Theme's `drawLibraryGrid()` iterates through visible items
2. For each book, loads thumbnail BMP from cache
3. Draws thumbnail to framebuffer
4. Releases memory immediately
5. Moves to next item

**Cache Location**: `/.crosspoint/epub_<hash>/thumb_<height>.bmp`

**Background Prewarm**: Thumbnails are generated in a background thread at startup (non-blocking).

### Performance Optimizations

#### Thumbnail Prewarm (Non-Blocking)

**Previous**: Thumbnail generation blocked UI startup until all EPUBs were processed.

**New**: Thumbnails prewarm in a **detached background thread**:
- UI is interactive immediately after boot
- Thumbnails appear as they're generated
- No blocking of main event loop

**Implementation**: `main_sim.cpp` launches `prewarmLibraryEpubThumbs()` thread after `setup()`.

#### Framebuffer Rendering Optimization

**Black & White Rendering** (`render_bw_to_texture`):

**Previous**: Processed individual bits, extracting one pixel at a time.

**New**: Processes **8 pixels per byte** (one byte = one horizontal 8-pixel group):
- Reduces loop iterations by **8×**
- More cache-friendly memory access pattern
- Faster rendering

**Grayscale Rendering** (`render_gray_to_texture`):

**Previous**: Per-pixel conditional branching to determine shade.

**New**: Uses **precomputed 8-entry lookup table (LUT)**:
- Index: `(bwBit << 2) | (msbBit << 1) | lsbBit`
- Direct array lookup → shade value
- Eliminates branching, improves performance

#### Image Conversion Optimization

**Ditherer Allocation**:
- **Previous**: Heap-allocated ditherer objects
- **New**: Stack-allocated ditherers

**Row Clearing**:
- **Previous**: `std::fill()` or heap allocation
- **New**: `memset()` for zero-cost clearing

**Impact**: Reduced memory allocations, faster image processing.

### Micro-Interaction Polish

#### Button Press Feedback

**Feature**: `drawButtonHintsWithPress()` added to both Classic and Lyra themes.

**Behavior**:
- Pressed buttons show **inverted style** (filled black background, white text)
- Immediate visual feedback on key press (not just release)
- `pressedIndex` parameter: 0=Back, 1=Confirm, 2=Prev, 3=Next

**Implementation**: Activities call `updateRequired = true` on `wasAnyPressed()` to trigger immediate re-render.

#### Long-Press Progress Indicator

**Feature**: `drawHoldProgress()` displays a thin progress bar during long-press actions.

**Use Cases**:
- Hold-to-go-home in Library
- Hold-to-skip-page navigation
- Any long-press action

**Visual**: Thin horizontal bar fills from 0% to 100% as hold duration increases.

**Implementation**: Called before `displayBuffer()` to show progress overlay.

### Architecture Improvements

#### Shared Utility Functions

**`LibraryProgress::hasUsableThumbBmp()`**:
- Extracted from `HomeActivity` and `MyLibraryActivity`
- Single source of truth for thumbnail availability check
- Used by library grid and home screen

**`LibraryProgress::getEpubProgressPercent()`**:
- Reads 8-byte progress.bin format
- Returns 0-100 percentage
- Handles both old (6-byte) and new (8-byte) formats

**`LibraryProgress::formatProgressLabel()`**:
- Converts percentage to display string
- "Not started", "XX%", or "Completed"
- Consistent formatting across UI

#### Theme System Extensions

**New Theme Methods**:
- `drawLibraryGrid()` - Renders 2-column book grid
- `drawButtonHintsWithPress()` - Button hints with press feedback
- `drawHoldProgress()` - Long-press progress bar

**New Theme Metrics**:
- `libraryCoverHeight` - Thumbnail height (e.g., 120px)
- `libraryTileHeight` - Total tile height including title/progress
- `libraryGridColumns` - Number of columns (typically 2)
- `libraryGridRows` - Rows per page

**Theme Support**: Both `BaseTheme` and `LyraTheme` implement these methods.

### Code Quality Improvements

#### DRY (Don't Repeat Yourself)

- **Before**: Constants duplicated across 10+ files
- **After**: Single source of truth in `UxConstants.h`

#### Single Source of Truth

- **Timings**: All thresholds in `UxTiming::`
- **Labels**: All button labels in `UxLabel::`
- **Layout**: All metrics in `ThemeMetrics` struct

#### Open/Closed Principle

- Extend by adding constants or theme methods
- Don't modify existing signatures without updating all call sites
- New screens follow established patterns

#### Composition Over Inheritance

- Theme drawing uses virtual dispatch on `BaseTheme`
- Activities compose input + render behavior
- No deep inheritance trees

#### Minimize Side Effects

- Data preparation (metadata, progress labels) separate from rendering
- Pure helper functions preferred
- Background threads for expensive operations

---

## Usage Guide

### Navigation Flow

```
┌─────────┐
│  Boot   │
└────┬────┘
     │
     ▼
┌─────────┐
│  Home   │ ◄──┐
└────┬────┘    │
     │         │
     ├─▶ My Library ──▶ [Book Grid] ──▶ Reader
     │         │
     ├─▶ Recents ──▶ Reader
     │         │
     ├─▶ Settings
     │         │
     └─▶ [Other]         │
                          │
                    Back/Home
```

### Library Navigation

**Grid Navigation**:
- **Left/Right**: Move between tiles in same row (wraps at edges)
- **Up/Down**: Move by row (or by one item)
- **Long-press Up/Down**: Skip a page of items
- **Enter**: Open book or enter folder
- **Back**: Return to parent folder or Home

**Grid Display**:
- Shows 2 columns × 3-4 rows per page
- Scrolls by page for large libraries
- One thumbnail loaded at a time (memory-safe)

### Reading Experience

**Reader Features** (same as device):
- Page turning (Left/Right arrows)
- Bookmarking
- Progress tracking (saved to progress.bin)
- Font size adjustment
- Theme switching (Classic/Lyra)

**Progress Persistence**:
- Progress saved automatically when closing book
- Stored in `/.crosspoint/epub_<hash>/progress.bin`
- 8-byte format: spine, page, pageCount, percent

### Settings

**Available Settings** (same as device):
- Display settings
- Reading preferences
- Theme selection
- Power management (stubbed in emulator)

**Note**: WiFi and OTA update settings show but don't function (network stubbed).

---

## Troubleshooting

### Build Issues

**CMake not found**:
```bash
# macOS
brew install cmake
# Or download from cmake.org

# Linux
sudo apt-get install cmake
```

**SDL2 not found**:
```bash
# macOS
brew install sdl2

# Linux
sudo apt-get install libsdl2-dev

# Verify
pkg-config --modversion sdl2
```

**Crosspoint repo not found**:
```bash
# Ensure Crosspoint is cloned
git clone https://github.com/your-org/Crosspoint.git ../Crosspoint

# Or set CROSSPOINT_ROOT
cmake .. -DCROSSPOINT_ROOT=/absolute/path/to/Crosspoint
```

**Compilation errors**:
- Ensure C++17 compiler (check: `g++ --version` or `clang++ --version`)
- Clean build: `rm -rf build && mkdir build && cd build && cmake ..`

### Runtime Issues

**Window doesn't appear**:
- Check SDL2 installation: `pkg-config --libs sdl2`
- Verify executable: `./build/crosspoint_emulator`
- Check for error messages in terminal

**Keyboard input not working**:
- Ensure SDL2 window has focus (click on it)
- Check keyboard mapping in `sim/src/sim_gpio.cpp`
- Verify SDL2 event pumping is working

**SD card not found**:
- Ensure `./sdcard/` directory exists (relative to CWD)
- Check permissions: `ls -la sdcard/`
- Verify files are readable: `ls sdcard/*.epub`

**Books don't appear in library**:
- Verify files are in `sdcard/` (not `sdcard/`)
- Check file extensions: `.epub`, `.txt`, `.xtc`
- Ensure files are readable: `file sdcard/book.epub`

**Thumbnails not generating**:
- Check cache directory: `ls -la .crosspoint/`
- Verify EPUB files are valid: try opening in another reader
- Check background thread logs in terminal

### Performance Issues

**Slow rendering**:
- Ensure optimized build: `cmake .. -DCMAKE_BUILD_TYPE=Release`
- Check SDL2 hardware acceleration: `SDL_RENDERER_ACCELERATED` flag
- Monitor CPU usage: may be normal for complex screens

**High memory usage**:
- Normal: emulator uses host heap (not limited like device)
- Check for memory leaks: use `valgrind` or similar
- Thumbnails are loaded one at a time (should be low memory)

---

## Development

### Project Structure

```
crosspoint-emulator/
├── CMakeLists.txt          # Build configuration
├── README.md              # This file
├── build/                 # Build output (gitignored)
├── docs/                  # Documentation
│   └── UI-UX-LIBRARY-PLAN.md
├── sdcard/                # Virtual SD card (gitignored)
└── sim/                   # Simulator HAL implementation
    ├── include/           # HAL headers (match device HAL)
    │   ├── HalDisplay.h
    │   ├── HalGPIO.h
    │   ├── SDCardManager.h
    │   └── [Arduino/ESP stubs]
    └── src/              # HAL implementations
        ├── main_sim.cpp   # Entry point
        ├── sim_display.cpp
        ├── sim_gpio.cpp
        ├── sim_storage.cpp
        └── [stub implementations]
```

### Key Files

**`sim/src/main_sim.cpp`**:
- Entry point (`main()`)
- Initializes SDL2 display
- Calls Crosspoint `setup()` and `loop()`
- Launches thumbnail prewarm thread

**`sim/src/sim_display.cpp`**:
- SDL2 window management
- Framebuffer → texture conversion
- Rendering pipeline

**`sim/src/sim_gpio.cpp`**:
- Keyboard → button mapping
- Button state tracking
- Press/release detection

**`sim/src/sim_storage.cpp`**:
- Virtual SD card (directory mapping)
- FsFile implementation
- SDCardManager implementation

### Adding Features

**Adding a new screen**:
1. Create activity class inheriting from `Activity`
2. Include `util/UxConstants.h` for timing/labels
3. Implement `loop()` and `render()`
4. Add press feedback: `if (mappedInput.wasAnyPressed()) updateRequired = true;`
5. Use `GUI.drawButtonHintsWithPress()` for button hints

**Adding HAL functionality**:
1. Add header to `sim/include/` (match device HAL)
2. Implement in `sim/src/`
3. Ensure API matches device HAL exactly
4. Update CMakeLists.txt if adding new source files

**Debugging**:
- Use `Serial.printf()` for logging (goes to terminal)
- Check SDL2 error messages: `SDL_GetError()`
- Verify HAL calls match device behavior

### Architecture Rulebook

See [Architecture & UX Rulebook](#architecture--ux-rulebook) section in README for design principles.

**Key Principles**:
1. **DRY** - No duplicated constants or helpers
2. **Single source of truth** - Centralized definitions
3. **Open/closed** - Extend without modifying
4. **Composition over inheritance** - Prefer composition
5. **Minimize side effects** - Pure functions preferred
6. **Performance budget** - No blocking main loop

---

## Architecture & UX Rulebook

### Design Principles

These rules apply to every change in both the emulator and Crosspoint repos:

1. **DRY** — No duplicated constants, labels, or helper functions. Shared values live in `UxConstants.h` or `LibraryProgress.h`.
2. **Single source of truth** — Timings (`kGoHomeMs`, `kSkipPageMs`, `kChangeTabMs`), labels (`UxLabel::kBack`, etc.), and layout metrics (`ThemeMetrics`) are defined once and imported everywhere.
3. **Open/closed** — Extend by adding new constants or theme methods; don't modify existing signatures without updating every call site.
4. **Composition over inheritance** — Theme drawing is virtual-dispatch on `BaseTheme`; activities compose input + render behavior without deep inheritance trees.
5. **Minimize side effects** — Data preparation (metadata, progress labels) is separate from rendering. Pure helper functions preferred.
6. **Performance budget** — No blocking work on the main loop. Expensive operations (thumbnail generation, EPUB parsing) run in background threads or lazily.

### Shared Constants Location

| File | Contents |
|------|----------|
| `Crosspoint/src/util/UxConstants.h` | Timing thresholds (`UxTiming::*`), common button labels (`UxLabel::*`) |
| `Crosspoint/src/components/themes/BaseTheme.h` | `ThemeMetrics` struct with all layout constants |
| `Crosspoint/src/util/LibraryProgress.h` | Progress helpers, shared `hasUsableThumbBmp()` |

### Preferred Interaction Patterns

- **Button hints** must use `UxLabel::` constants for standard labels. Context-specific labels (e.g. "Clear", "Retry") stay inline.
- **Long-press actions** show a hold-progress bar via `GUI.drawHoldProgress(renderer, percent)`.
- **Press feedback** is delivered via `GUI.drawButtonHintsWithPress(...)` with a `pressedIndex` (0=Back, 1=Confirm, 2=Prev, 3=Next).
- Activities call `updateRequired = true` on `wasAnyPressed()` so press feedback renders immediately.

### Performance Rules

- **Thumbnail prewarm** runs in a detached background thread (`main_sim.cpp`), never blocking the UI loop.
- **Framebuffer-to-texture** processes bytes (8 pixels at a time), not individual bits (`sim_display.cpp`).
- **Grayscale rendering** uses a precomputed LUT instead of per-pixel branching.
- **Image conversion** stack-allocates ditherers and uses `memset` for row clearing instead of `std::fill` or heap allocation.

### Adding a New Screen

1. Create your activity class inheriting from `Activity` (or `ActivityWithSubactivity`).
2. Include `util/UxConstants.h` — use `UxTiming::` for hold/skip thresholds and `UxLabel::` for standard labels.
3. In `loop()`, add `if (mappedInput.wasAnyPressed()) updateRequired = true;` for press feedback.
4. In `render()`, compute `pressedHint` and call `GUI.drawButtonHintsWithPress(...)`.
5. For long-press actions, call `GUI.drawHoldProgress(renderer, percent)` before `displayBuffer()`.

---

## Maintenance

When the Crosspoint HAL or `main.cpp` changes, the sim HAL in `sim/include/` and `sim/src/` may need updates so APIs and behavior stay in sync (see plan in the Crosspoint repo).

---

## Changelog

### UX Overhaul (Feb 2026)

**UX Foundation**
- Created `UxConstants.h` with centralized timing constants (`kGoHomeMs`, `kSkipPageMs`, `kChangeTabMs`) and shared button labels (`UxLabel::kBack`, `kHome`, `kSelect`, etc.).
- Eliminated 12+ duplicated constant definitions across 10 activity files.

**Screen Consistency**
- Renamed "Browse Files" to "My Library" on the Home screen for clarity.
- Standardized all Back button labels to use the `<<` prefix consistently (was mixed: `< Home`, `<< Home`, `<< Back`).
- Improved empty state messages in Library ("No books here. Add files to the SD card.") and Recents ("No recent books. Open one from My Library.").
- Migrated 15+ `mapLabels()` call sites to use `UxLabel::` constants.

**Performance**
- Moved thumbnail prewarm from blocking startup to a detached background thread — UI is interactive immediately after boot.
- Optimized `render_bw_to_texture`: processes framebuffer bytes (8 pixels at a time) instead of individual bit extraction, reducing loop iterations ~8x.
- Optimized `render_gray_to_texture`: uses a precomputed 8-entry LUT for grayscale mapping instead of per-pixel conditional branching.
- Eliminated heap allocation in image converter: ditherers are now stack-allocated; row clearing uses `memset`.

**Micro-Interaction Polish**
- Added `drawButtonHintsWithPress()` to both Classic and Lyra themes — pressed buttons show inverted (filled black, white text) for immediate visual feedback.
- Added `drawHoldProgress()` — a thin progress bar appears during long-press actions (e.g., hold-to-go-home in Library).
- All key activities (Home, Library, Recents, Settings, Reader Menu) now trigger re-render on button press, not just release.

**Architecture Cleanup**
- Extracted duplicated `hasUsableThumbBmp()` from HomeActivity and MyLibraryActivity into shared `LibraryProgress::hasUsableThumbBmp()`.
- Documented architecture rulebook and extension guidelines in README.

---

## License

[Add your license information here]

---

## Contributing

[Add contribution guidelines here]

---

## Acknowledgments

Built on top of the [Crosspoint](https://github.com/crosspoint/crosspoint) e-reader firmware project.
