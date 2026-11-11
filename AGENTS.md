# Adderall: instructions for agents

Adderall is a minimalist, grid-based habit tracker written in **C** (C11). **Stay in C.** See `README.md`: it is the spec.

## Goal for v0.1.0
A demo that shows off **everything in the README**, close to the final design. We optimize afterward. Features the README marks *(planned)* (such as progress summaries) may be stubs.

If you deviate from the README, update the README in the same change.

## Build order (finish and commit each slice before the next)
1. SDL3 window and a monochrome board with one habit line: empty squares, a big dot for filled, a diagonal line for skipped, the larger squircle on today, click to fill, right-click to skip
2. The data file: single plain `.txt` file, parser and writer, crash-safe saving with a backup, OS data directory
3. Day logic: start date per habit, sealing of past days, future days editable, time zone and new-day time
4. Multiple habits: up to 8 stacked, the bottom-left habit button and its pop-up (create, rename, archive, restore, delete, archived list), name rules, and the add-a-ninth prompt
5. Layouts: horizontal (endless to the right) and vertical (wrapping), board-wide, left-to-right fill, scrolling back to each habit's first day
6. The measure tool: click and drag, with cells grabbed, filled, skipped, empty, streaks and longest streak, shown in a large tooltip near the cursor
7. Settings (opened from a settings button): world-map time zone pop-out, new-day time, board layout, copy data path, open data folder, editable hotkeys, light and dark, scaling, high contrast, reduced motion, keyboard navigation

## Hard rules
- **Privacy:** no network requests, no telemetry, no accounts, no update checks, no crash reports. Data stays local.
- **One file:** habits, marked days, and settings live in a single plain text file in the OS standard data directory. No database.
- **Crash-safe saves:** write to a temporary file, flush it to disk, swap it over the real file, and keep a backup of the previous version. Never leave a half-written data file.
- **Never lose data silently:** if the file has a line the parser can't read, report which line and leave the file alone. If the main file is damaged, fall back to the backup and tell the user.
- **Only touch Adderall's own data folder.** Never read, write, or scan anything else on the user's device.
- **Past days are sealed.** Once a day ends, its state can't be edited. Today and future days stay editable. Enforce this in the data layer, not just the UI.
- **No guilt-inducing streak counter.** Streaks only appear through the measure tool, when the user drags across cells. Never show a streak number by default, and never add flames, chains, resets, or "don't break the streak" messaging.
- **Cells are filled one at a time by clicking each cell.** Nothing fills automatically, and dragging never fills cells; dragging is for the measure tool.
- **The measure tool requires a mouse.** Everything else must work from the keyboard.
- **Up to 8 habits** on the board. Adding a ninth prompts the user to archive or delete one.
- **Habit names** are a single line of at most 40 characters, with leading and trailing spaces trimmed, and **unique** (compared without regard to capitals). Reject names that would break the file format.
- **Layout is board-wide**, not per habit.
- **A habit started mid-day** has that same day as its first cell.
- **Hotkeys are editable.** Ship sensible defaults (the README lists them) and store changes in the data file as `key <action> <key>` lines.
- **One writer at a time:** hold an OS-level lock on the data file (for example `flock` on Unix, `LockFileEx` on Windows) so a second launch tells the user Adderall is already running and exits. Don't use a lock file whose mere existence counts as locked, because a crash would leave it stuck.
- **Font and text size are user settings.** Ship one bundled, openly licensed, minimalist sans-serif as the default, and let the user pick any `.ttf` or `.otf` file and a text size. Store both in the data file. Record the font's and the text library's licenses in the README (SDL_ttf is zlib licensed; confirm the font's license before bundling).
- Must run on Windows, Linux (Wayland and X11), and macOS. No display-server-specific code.
- Dependencies must be compatible with the Unlicense; prefer public domain or permissive libraries. Do not add GPL code.

## Workflow
- Work on `dev`. **Never push directly to `main`.**
- Versioning: `x.0.0` major, `0.x.0` minor (big features), `0.0.x` patch (small features, fixes, security).
- Build: `cmake -S . -B build && cmake --build build && ctest --test-dir build`
- Add tests for the data file parser and writer, including crash-safety (interrupted writes), the sealing rules, the new-day rollover, and the measure tool's calculations. CI builds on all three OSes; keep it green.
- Layout: `src/` sources, `include/` headers, `tests/` tests.

## CI
Add `.github/workflows/ci.yml` building on Linux, Windows, and macOS in the first slice. When you add SDL3 or any other dependency, update the workflow in the same slice to install it on all three OSes so CI stays green.

## Data file
The README documents the file format. If you change it, update the README's example and rules in the same change, and bump the version line at the top of the format. Keep it line-based and readable in a plain text editor. Dates are `YYYY-MM-DD`.

## UI design
No mockups exist. Design the UI yourself to match the README's values.
- **Minimalist, clean, black and white.** No accent colors, gradients, or decoration. Support black on white and white on black.
- **Cell marks are fixed:** empty is a plain square, filled is a big dot in the middle of the cell, skipped is one diagonal line (`/`) from corner to corner, and today is marked by a larger squircle. Sealed days look like any other cell of their state and just ignore clicks. Never rely on color alone.
- **Moving around:** scroll (wheel or trackpad) or use the arrow keys to move through days, back to the first day the habit was created. Dragging never scrolls, because dragging is the measure tool. The board opens with today in view. Arrow keys move a focused cell and the board scrolls to keep it in view.
- **Row labels:** each row starts with its own unique cell showing a symbol for that habit (star, triangle, hexagon, and so on). Hovering it shows a tooltip with the habit's name. Draw the symbols as simple monochrome shapes, and keep them clearly different from the cell marks (the filled dot and the `/` line), so no circle. Ship at least 8 distinct symbols. No two habits on the board share one. Assign the next free symbol to each new habit and let the user change it from the habit pop-up. Store it in the data file as a `symbol <name>` line.
- **Two corner buttons:** a habit button in the bottom-left opens a pop-up for managing habits (create, rename, archive, restore, delete, and an archived section). A settings button opens settings. Put the settings button in the bottom-right unless the design calls for better.
- **Measure tool results** appear in a large tooltip near the cursor while dragging.
- **Nothing blinks, bounces, or celebrates.** Animation is minimal and can be turned off.
- Make today's cell easy to find, and keep cells large enough to click comfortably.
- Design for accessibility from the start: keyboard navigation (except the measure tool), scalable text, and high contrast.
- Keep the tone of the app consistent with the README: calm and a little playful, never pushy.
- Document major UI decisions in the README.

## Time zones
Handle daylight saving correctly. Windows has no built-in IANA time zone database, so ship the public domain IANA database with Adderall (use it on every OS, or fall back to it), with no network access. The settings pop-out shows a world map with separated time zones, bundled with the app; the user picks a zone and a new-day time. Changing the time zone only affects days from now on. Sealed days stay sealed. Check the license of the map data before bundling it: it must be public domain or permissively licensed (Natural Earth is public domain, for example). Do not use data under a share-alike or attribution-restricted license without asking.

## Licensing
Adderall is released under the Unlicense (see `LICENSE`). Check the license of every dependency before adding it and record each choice and its license in the README.
