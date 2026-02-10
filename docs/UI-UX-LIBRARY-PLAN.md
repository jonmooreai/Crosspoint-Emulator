# Library & UX Update Plan

## Goals

- **Visual library grid**: Cover art + title below + progress (or "Not started") for a better viewing, reading, and navigating experience.
- **Speed and ease of use**: Optimize for fast navigation and minimal taps.
- **Low memory**: Respect limited e-reader RAM; lazy-load covers and avoid holding multiple bitmaps.

---

## 1. Library Grid (My Library)

### 1.1 Layout

- **When viewing a folder that contains book files**: Show a **grid** of tiles instead of a text-only list.
- **Each tile**:
  - **Cover art** (thumbnail) at top.
  - **Title** below the cover (one or two lines, truncated).
  - **Progress** below the title: either `"Not started"` or `"XX%"` (and optionally "Completed" at 100%).
- **When the list includes directories**: Keep a compact list for folders (or a single row of folder cards) so "Up" navigation stays clear; books in the same folder use the grid.

### 1.2 Grid dimensions (memory-safe)

- **Columns**: 2 on 480px width (portrait) for readable tiles and touch targets.
- **Tile size**: Reuse theme metrics; introduce `libraryCoverHeight` (e.g. 120–140px) and `libraryTileHeight` so we get 3–4 rows on screen without scrolling the whole list at once (pagination or scroll by row).
- **Cover thumbnails**: Use existing EPUB/XTC thumb API at a **fixed small height** (e.g. 120px) so one thumbnail at a time is loaded, drawn, then we move on (no caching of all covers in RAM).

### 1.3 Data per book

- **Title**: From metadata (Epub/Xtc/Txt) when available; otherwise filename (stem). Use `RecentBooksStore::getDataFromBook(path)` for recents; for other books we open the book once to get title/cover path (or show filename until opened).
- **Cover**: Thumb path from `Epub::getThumbBmpPath(height)` (or Xtc/Txt equivalent). Generate on first open if missing; in library, if thumb doesn’t exist show a placeholder (e.g. book icon or "No cover").
- **Progress**:
  - **EPUB**: Read `progress.bin` from cache dir `/.crosspoint/epub_<hash(path)>/`. If file missing or spine 0 + page 0 → **"Not started"**. Otherwise show **"XX%"**. To avoid loading the full book in library, **store percent in progress.bin** when saving (see §2).
  - **TXT/XTC**: Same idea: read progress from cache; show "Not started" or "XX%" using existing progress format or a small extension.

### 1.4 Navigation

- **Left/Right**: Move between tiles in the same row (wrap at edges).
- **Up/Down**: Move by row (or by one item); **long-press** continues to skip a page of items for speed.
- **Confirm**: Open book or enter directory.
- **Back**: Parent folder or Home (unchanged).

### 1.5 Memory rules

- **Single cover buffer**: Load only the thumbnail for the **current** (or current + next) item when drawing; parse BMP, draw, release. Do not keep an array of decoded bitmaps.
- **Metadata**: For "all books in current folder" we only need: path, display title, thumb path, progress string. Lazy-resolve title/thumb from cache or quick metadata load when entering the folder or when scrolling into view (prefer on enter so first paint has titles).
- **Pagination**: Prefer "page of tiles" (e.g. 8 items per page) so we only need to resolve metadata/thumb for visible (or current) page.

---

## 2. Progress percent in progress.bin (EPUB)

- **Current format**: 4 or 6 bytes — spineIndex (2), currentPage (2), [pageCount (2)].
- **Change**: When saving progress in `EpubReaderActivity::saveProgress()`, append **2 bytes** (uint16_t) = **book percent 0–100** (rounded). Reader already computes `bookProgress` via `epub->calculateProgress()`; write that value so the library can show percent without opening the book.
- **Backward compatibility**: If library reads progress.bin and gets 6 bytes, treat as old format (show "In progress" or approximate); if 8 bytes, use last 2 bytes as percent. Reader always writes 8 bytes from now on.

---

## 3. Theme and drawing

- **New theme method**: `drawLibraryGrid(renderer, rect, items, selectedIndex, ...)` where `items` provide per-index: path, title, thumbPath, progressLabel ("Not started" / "45%" / "Completed"). Theme draws a 2-column grid, loads one thumb at a time inside the loop, draws placeholder when thumb missing.
- **Metrics**: Add to `ThemeMetrics`: `libraryCoverHeight`, `libraryTileWidth`, `libraryTileHeight`, `libraryGridColumns`, `libraryGridRows` (or derive from screen size).
- **BaseTheme**: Implement a simple grid (rects, text, optional placeholder). **LyraTheme**: Override for rounded corners / selection highlight consistent with home cover tiles.

---

## 4. MyLibraryActivity refactor

- **Separate folders vs files**: When listing current directory, split into `folders[]` and `books[]`. If current view is "books in this folder", use grid; if mixed, either show folders first as a list then books as grid, or show folders as a row then grid of books.
- **Book metadata cache**: For the current page of books, build a small vector of `{ path, title, thumbPath, progressLabel }`. Use `RecentBooksStore::getDataFromBook(path)` where possible; otherwise open Epub/Xtc/Txt briefly (metadata only, no full content) to get title and thumb path, and read progress from progress.bin (with new percent field). Consider caching this in activity for the session so we don’t re-open every frame.
- **Rendering**: Call `GUI.drawLibraryGrid(...)` with the current page of book items and `selectorIndex`; theme loads and draws thumbs one-by-one, no persistent bitmap array.

---

## 5. Other UI/UX improvements

- **Recents screen**: Optionally use the same grid style (cover + title + progress) for consistency; currently Recents uses a list. Can share the same `drawLibraryGrid` with a single row or small grid.
- **Reader**: No change unless we add a small "back to library" shortcut or improve status bar density (out of scope for this plan).
- **Constants**: Centralize magic numbers (e.g. progress.bin layout, thumb height for library) in one place (e.g. `LibraryConstants.h` or in theme metrics).
- **Empty state**: When no books in folder, show a clear "No books here" message and suggest adding files to the SD card.

---

## 6. Implementation order

1. **Progress.bin**: Add percent to save/read in EpubReaderActivity; add a small helper (e.g. `readProgressPercent(path)`) used by library.
2. **Theme**: Add `libraryCoverHeight` (and related) to metrics; add `drawLibraryGrid` to BaseTheme and LyraTheme.
3. **MyLibraryActivity**: Split folders/books; build book list with metadata + progress; use grid for books; keep list or simple row for folders.
4. **Recents**: Optionally switch to grid (same drawing path) for consistency.
5. **Polish**: Empty state, long-press page skip, any constants file.

---

## 7. Files touched (Crosspoint repo) — implemented

- `src/activities/home/MyLibraryActivity.cpp` (.h) — grid view, `LibraryItem`, `buildItems()`, metadata + progress.
- `src/activities/reader/EpubReaderActivity.cpp` — write 8-byte progress.bin (spine, page, pageCount, percent).
- `src/components/themes/BaseTheme.cpp` (.h) — `drawLibraryGrid`, `libraryCoverHeight`, `libraryGridColumns`.
- `src/components/themes/lyra/LyraTheme.cpp` (.h) — override `drawLibraryGrid`, Lyra metrics.
- `src/util/LibraryProgress.h` (.cpp) — `getEpubCachePath`, `getEpubProgressPercent`, `formatProgressLabel`.

## 8. Implemented summary

- **Library**: When the current folder has any book files, the library shows a 2-column grid: cover thumbnail, title below, progress below title ("Not started", "XX%", or "Completed"). Folders appear in the same grid with no cover and no progress. One thumbnail is loaded at a time when drawing (memory-safe).
- **Progress**: EPUB progress.bin now stores 8 bytes; the last 2 are book percent (0–100). Library reads this without opening the book.
- **Navigation**: Same as before (Up/Down/Left/Right, long-press skip page, Confirm to open/enter, Back to parent or Home). Grid page size is derived from theme metrics and content height.
- **Recents**: Still uses list view; can be switched to grid later if desired.
