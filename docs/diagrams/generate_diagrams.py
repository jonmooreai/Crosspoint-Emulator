#!/usr/bin/env python3
"""
Generate SVG architecture and usage diagrams for the Crosspoint Emulator README.
Run from repo root: python3 docs/diagrams/generate_diagrams.py
"""
import os

# Shared SVG styling
FONT = "14px system-ui, -apple-system, sans-serif"
BOX_STYLE = "fill:#fff;stroke:#333;stroke-width:1.5"
ARROW_STYLE = "stroke:#333;stroke-width:1.5;fill:none"
TEXT_STYLE = f"font:{FONT};fill:#111;text-anchor:middle"
TEXT_LEFT = f"font:{FONT};fill:#111;text-anchor:start"


def box(svg, x, y, w, h, lines, style=BOX_STYLE):
    """Draw a box with one or more text lines centered."""
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" style="{style}"/>')
    if isinstance(lines, str):
        lines = [lines]
    line_height = 16
    start_y = y + (h - (len(lines) - 1) * line_height) / 2
    for i, line in enumerate(lines):
        ly = start_y + i * line_height
        esc = line.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        svg.append(f'<text x="{x + w/2}" y="{ly + 4}" style="{TEXT_STYLE}">{esc}</text>')


def arrow(svg, x1, y1, x2, y2, tip=True):
    """Draw arrow from (x1,y1) to (x2,y2)."""
    svg.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" style="{ARROW_STYLE}" marker-end="url(#arrowhead)" />')


def arrow_down(svg, x, y1, y2):
    arrow(svg, x, y1, x, y2)


def gen_high_level():
    """Crosspoint Emulator high-level architecture."""
    w, h = 520, 280
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    # Outer box
    svg.append(f'<rect x="10" y="10" width="{w-20}" height="{h-20}" style="fill:#fafafa;stroke:#333;stroke-width:2"/>')
    svg.append(f'<text x="{w/2}" y="32" style="font:bold 16px system-ui;fill:#111;text-anchor:middle">Crosspoint Emulator</text>')
    # Top row: App -> Sim HAL
    box(svg, 40, 50, 160, 88, ["Crosspoint App", "(main.cpp)", "• Activities • Themes", "• Readers • UI Logic"])
    box(svg, 320, 50, 160, 88, ["Sim HAL Layer", "(sim/include/)", "• HalDisplay • HalGPIO", "• SDCardManager • Stubs"])
    arrow(svg, 200, 85, 320, 85)
    # Down arrows
    arrow_down(svg, 120, 138, 175)
    arrow_down(svg, 400, 138, 175)
    # Bottom row
    box(svg, 40, 175, 160, 80, ["Crosspoint Libs", "• GfxRenderer", "• Epub/Txt/Xtc", "• Fonts • Utf8"])
    box(svg, 320, 175, 160, 80, ["Host Platform", "• SDL2 Window", "• File System", "• Keyboard"])
    return "\n".join(svg) + "\n</svg>"


def gen_component_flow():
    """Component flow: main() -> setup -> loop."""
    w, h = 380, 320
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    cx = w / 2
    box(svg, cx - 60, 20, 120, 44, "main()  ← Entry point (main_sim.cpp)")
    arrow_down(svg, cx, 64, 85)
    for i, (label, y) in enumerate([
        ("sim_display_init()  → SDL2 window", 95),
        ("setup()  → Crosspoint initialization", 135),
        ("loop()  → Main event loop", 175),
    ]):
        box(svg, 40, y, w - 80, 32, label)
        if i < 2:
            arrow_down(svg, cx, y + 32, y + 48)
    # Loop children
    arrow_down(svg, cx, 207, 230)
    box(svg, 40, 230, w - 80, 28, "sim_display_pump_events()  •  HalGPIO::update()  •  Activity::loop()  •  HalDisplay::displayBuffer()")
    return "\n".join(svg) + "\n</svg>"


def gen_hal():
    """HAL abstraction layer."""
    w, h = 480, 320
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    box(svg, 40, 20, w - 80, 50, ["Crosspoint Application Code", "(Activities, Themes, Readers, UI Logic)"])
    arrow_down(svg, w/2, 70, 95)
    svg.append(f'<text x="{w/2}" y="88" style="{TEXT_STYLE}">Uses HAL interfaces</text>')
    arrow_down(svg, w/2, 95, 115)
    box(svg, 40, 115, w - 80, 80, ["HAL Layer", "HalDisplay (display, clear)  •  HalGPIO (isPressed, wasPress)  •  SDCardMgr (open, read)"])
    arrow_down(svg, w/2, 195, 220)
    arrow(svg, w/2, 220, 140, 260)
    arrow(svg, w/2, 220, w - 140, 260)
    box(svg, 40, 260, 180, 48, ["Device HAL (ESP32)", "E-ink driver, GPIO, SD SPI"])
    box(svg, w - 220, 260, 180, 48, ["Sim HAL (SDL2/Desktop)", "SDL2 renderer, Keyboard, File system"])
    return "\n".join(svg) + "\n</svg>"


def gen_display_pipeline():
    """Display rendering pipeline (6 steps)."""
    w, h = 420, 520
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    svg.append(f'<text x="{w/2}" y="28" style="font:bold 16px system-ui;fill:#111;text-anchor:middle">Crosspoint Rendering Pipeline</text>')
    steps = [
        ("1. Application draws to framebuffer", "uint8_t frameBuffer[BUFFER_SIZE]  (800×480 bits)"),
        ("2. HalDisplay::displayBuffer()", "Copies to internal buffer, triggers render"),
        ("3. sim_display.cpp: render_bw_to_texture()", "8 pixels/byte, rotate (x,y)→(H-1-y,x), bits→RGB24"),
        ("4. SDL2 Texture Update", "SDL_LockTexture / Write RGB24 / SDL_UnlockTexture"),
        ("5. SDL2 Render Present", "SDL_RenderCopy / SDL_RenderPresent"),
        ("6. Window Display (480×800)", "SDL2 Window (rotated view)"),
    ]
    y = 50
    for title, sub in steps:
        box(svg, 30, y, w - 60, 62, [title, sub])
        y += 72
        if y < 480:
            arrow_down(svg, w/2, y - 10, y + 5)
    return "\n".join(svg) + "\n</svg>"


def gen_storage():
    """Virtual SD card system."""
    w, h = 420, 340
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    svg.append(f'<text x="{w/2}" y="28" style="font:bold 16px system-ui;fill:#111;text-anchor:middle">Virtual SD Card System</text>')
    arrow_down(svg, w/2, 40, 58)
    box(svg, 80, 58, w - 160, 36, "Crosspoint App  →  SDCardManager::open(\"/book.epub\")")
    arrow_down(svg, w/2, 94, 112)
    box(svg, 80, 112, w - 160, 36, "FsFile API  →  resolvePath(\"/book.epub\")  →  \"./sdcard/book.epub\"")
    arrow_down(svg, w/2, 148, 166)
    box(svg, 80, 166, w - 160, 36, "POSIX File System  (fopen, fread, fwrite)")
    arrow_down(svg, w/2, 202, 220)
    box(svg, 80, 220, w - 160, 100, ["./sdcard/", "  ├── book1.epub", "  ├── book2.txt", "  └── Novels/", "        └── book3.epub"])
    return "\n".join(svg) + "\n</svg>"


def gen_threading():
    """Thread architecture: main + background."""
    w, h = 520, 300
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    svg.append(f'<text x="{w/2}" y="28" style="font:bold 16px system-ui;fill:#111;text-anchor:middle">Thread Architecture</text>')
    box(svg, 40, 45, w/2 - 60, 110, [
        "Main Thread (UI Loop)",
        "while (true) {",
        "  sim_display_pump_events()",
        "  loop()",
        "    → Activity::loop()",
        "    → Activity::render()",
        "}"
    ])
    box(svg, w/2 + 20, 45, w/2 - 60, 110, [
        "Background Thread",
        "prewarmLibraryEpubThumbs()",
        "  • Scans sdcard/ for .epub",
        "  • Generates thumbnails",
        "  • Writes to cache dir",
        "  • Non-blocking"
    ])
    svg.append(f'<text x="{w/2}" y="200" style="font:12px system-ui;fill:#333;text-anchor:middle">UI is interactive immediately; thumbnails appear as they are generated in background.</text>')
    return "\n".join(svg) + "\n</svg>"


def gen_navigation_flow():
    """Navigation flow: Boot → Home → Library / Recents / Settings."""
    w, h = 380, 280
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    box(svg, w/2 - 50, 20, 100, 40, "Boot")
    arrow_down(svg, w/2, 60, 85)
    box(svg, w/2 - 50, 85, 100, 40, "Home  ◄──")
    arrow_down(svg, w/2, 125, 145)
    box(svg, 30, 145, w - 60, 110, [
        "My Library  →  [Book Grid]  →  Reader",
        "Recents  →  Reader",
        "Settings",
        "[Other]                    Back/Home"
    ])
    return "\n".join(svg) + "\n</svg>"


def gen_project_structure():
    """Project directory structure."""
    w, h = 480, 320
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="100%">',
           '<defs><marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto"><polygon points="0 0, 10 3.5, 0 7" fill="#333"/></marker></defs>']
    svg.append(f'<text x="{w/2}" y="28" style="font:bold 16px system-ui;fill:#111;text-anchor:middle">Project Structure</text>')
    lines = [
        "crosspoint-emulator/",
        "├── CMakeLists.txt    # Build configuration",
        "├── README.md",
        "├── build/           # Build output (gitignored)",
        "├── docs/",
        "│   └── UI-UX-LIBRARY-PLAN.md",
        "├── sdcard/          # Virtual SD card (gitignored)",
        "└── sim/             # Simulator HAL",
        "    ├── include/     # HAL headers (HalDisplay, HalGPIO, …)",
        "    └── src/         # main_sim.cpp, sim_display.cpp, sim_gpio.cpp, sim_storage.cpp"
    ]
    y = 50
    for line in lines:
        svg.append(f'<text x="40" y="{y}" style="{TEXT_LEFT}">{line.replace("&", "&amp;").replace("<", "&lt;")}</text>')
        y += 22
    return "\n".join(svg) + "\n</svg>"


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(script_dir, exist_ok=True)
    diagrams = [
        ("arch-high-level.svg", gen_high_level),
        ("arch-component-flow.svg", gen_component_flow),
        ("arch-hal.svg", gen_hal),
        ("arch-display-pipeline.svg", gen_display_pipeline),
        ("arch-storage.svg", gen_storage),
        ("arch-threading.svg", gen_threading),
        ("usage-navigation-flow.svg", gen_navigation_flow),
        ("dev-project-structure.svg", gen_project_structure),
    ]
    for name, gen in diagrams:
        path = os.path.join(script_dir, name)
        with open(path, "w") as f:
            f.write(gen())
        print(f"Wrote {path}")
    print("Done. Update README to reference docs/diagrams/*.svg")


if __name__ == "__main__":
    main()
