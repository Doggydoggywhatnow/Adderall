# Adderall

**A grid-based habit tracker for the things you keep meaning to do.**

Adderall is a graphical habit tracker written in C. It shows your habits as lines of cells, one cell per day, and makes keeping up with them feel as simple as filling in a square. No accounts, no uploads, no subscriptions: just your habits, on your device. It will also absolutely, definitely change your life.

> **Status:** version 0.1.0, a demo of everything described below. Sections marked *(planned)* describe where Adderall is headed rather than what ships today.

---

## Table of contents

- [Why Adderall?](#why-adderall)
- [About the name](#about-the-name)
- [Values](#values)
- [Features](#features)
- [First launch](#first-launch)
- [Habits and grids](#habits-and-grids)
- [Managing habits](#managing-habits)
- [Marking days](#marking-days)
- [Measuring progress](#measuring-progress)
- [Time zones and new days](#time-zones-and-new-days)
- [Look and feel](#look-and-feel)
- [Interface notes](#interface-notes)
- [Platforms](#platforms)
- [How it works](#how-it-works)
- [Saving your data](#saving-your-data)
- [Download (Linux AppImage)](#download-linux-appimage)
- [Building from source](#building-from-source)
- [Usage](#usage)
- [Configuration](#configuration)
- [Privacy](#privacy)
- [Accessibility](#accessibility)
- [Roadmap](#roadmap)
- [FAQ](#faq)
- [Contributing](#contributing)
- [Dependencies and licenses](#dependencies-and-licenses)
- [Acknowledgments](#acknowledgments)
- [Versioning](#versioning)
- [License](#license)

---

## Why Adderall?

Most habit trackers want an account, a subscription, a notification permission, and a detailed look at your life. You wanted to track whether you drank water.

Adderall brings it back to basics: a line of squares per habit, a click to fill one in, and a program that has never heard of the internet. Habits are easier to keep when tracking them is easy, and easier to be honest about when nobody is watching. A missed day is an empty square, not a failure.

## About the name

Adderall is a joke about how people think taking Adderall will change their life. One little pill and suddenly you're organized, focused, and finally doing everything you've been putting off. This program has the same energy: a grid of squares is not going to change your life either, but it's a start. It's just a joke.

## Values

1. **Private.** Your habits are your business. They never leave your device. Period.
2. **Simple.** A line of days per habit. That's the whole idea.
3. **Yours.** You choose how your habits are laid out and displayed, and your data is a Markdown file you can open anywhere.
4. **Lightweight.** Native C, no heavyweight runtime, no bloat.

## Features

- A graphical interface built around one idea: a line of days per habit
- **Habits stacked as rows,** up to 8, each starting on the day you start tracking it
- **Two layouts:** horizontal, an endless line growing to the right, or vertical, wrapping down line after line
- **Click to fill** each day, and **right-click to skip** one without it counting as a miss
- **Plan ahead:** fill in or skip future days; past days are sealed
- **No guilt-inducing streak counter:** miss a day and nothing screams at you
- **Freeform measure tool:** click and drag across any cells to measure them yourself, whenever you feel like it
- **Archive or delete** habits to keep your board to a handful, all from one habit button in the bottom-left corner
- **Time zones your way:** pick yours from a small world map and choose when a new day begins
- **Editable hotkeys,** and a font and text size of your choosing
- **Scroll or use the arrow keys** to move through your days
- **Everything in one Markdown file,** saved crash-safe
- Minimalist, black-and-white interface
- Progress summaries *(planned)*
- Runs on Windows, macOS, and Linux, on Wayland or X11
- Lightweight, written in C
- Fully local: no network features, accounts, or telemetry

## First launch

On first startup, Adderall lets you:

- **create your first habit**, or skip and start with an empty board
- **choose a layout** for your whole board: horizontal or vertical

The habit's line starts on today, the day you begin tracking it. You can add more habits any time, and each new one stacks under the previous and starts on the day you create it. Your time zone and new-day time default to your computer's settings until you change them.

## Habits and grids

Every habit is a line of cells, and every cell is one day. A habit begins on the day you start tracking it, not before. Habits stack on the board one on top of another, up to **8 habits**, and a newer habit starts on its own first day, whatever the older ones are up to:

```
Habit 1: 1  2  3  4  ...
Habit 2: 1  2  3  4  ...
```

Days always fill **left to right**. You choose how the board behaves when a line runs out of room:

| Layout | Description |
| --- | --- |
| Horizontal | The default. Each habit's line keeps expanding to the right forever, like a timeline that never ends. |
| Vertical | Cells run all the way to the right edge, then start again from the left on the line underneath, over and over, like text wrapping down a page. Each piece of the timeline shows all your habits, one line each, so today's squircles still line up. |

The layout is **board-wide**: it applies to all your habits at once, and you can switch it any time in settings.

A larger **squircle** marks the current day, so you can always find where you are.

**Moving around:** scroll (mouse wheel or trackpad) or use the arrow keys to move through your days. You can go back as far as the first day the habit was created (the board starts at your oldest habit; newer habits' rows are simply empty before their own first day). In the vertical layout you scroll up and down, and in the horizontal layout left and right. Dragging never scrolls, because dragging belongs to the measure tool. The board opens with today in view.

With the keyboard, the arrow keys move a focused cell (the first press shows where the focus is) and the board scrolls to keep it in view. A line at the bottom says which habit and day it is, and whether the day is sealed. In the vertical layout, up and down walk through the lines like text: habit after habit, then on to the next piece of the timeline. `Tab` moves between the corner buttons and `Esc` brings you back to the board.

**Telling rows apart:** each row starts with its own **unique cell** showing a symbol that stands for the habit: a star, a triangle, a hexagon, and so on. No two habits on the board share a symbol. Hover over the cell and a tooltip tells you which habit it is.

*(planned: more display options such as cell size and spacing, documented as they're finalized.)*

## Managing habits

In the bottom-left corner of the window there's a **habit button**. Click it and a small area pops up for managing your habits:

- **Create** a new habit. Its line starts on today, even if you create it halfway through the day.
- **Rename** a habit.
- **Change its symbol** to any symbol no other habit on the board is using. Each new habit gets the next free one automatically.
- **Archive**, **restore**, or **delete** a habit.
- **Archived habits** are listed in their own section of the same pop-up, so you can find them again.

Habit names are a single line of up to **40 characters**, and **no two habits can share a name** (capitals don't count as a difference, so "Read" and "read" are the same name). Leading and trailing spaces are ignored.

The board holds up to 8 habits, so you'll sometimes need to make room:

- **Archive** takes a habit off the board and frees its slot, but keeps all of its history in your data file. Archived habits can be viewed any time, and restored as long as there's a free slot. Their days stay sealed.
- **Delete** removes a habit and its history for good. Adderall asks you to confirm first, since there's no undo (the backup copy of your data file is the only way back).

If you try to add a ninth habit, Adderall asks you to archive or delete one first. Both only change your data file. They never touch anything else on your device.

## Marking days

- **Click** a cell to fill it in. You fill cells one at a time, by clicking each individual cell. Nothing fills in automatically, and dragging never fills cells.
- **Click a filled cell** again to clear it, as long as the day isn't sealed.
- **Right-click** a cell to **skip** that day. A skipped day isn't filled, but it isn't a miss either. Right-click it again to undo the skip.
- **Future days** can be filled in or skipped ahead of time.
- **Past days are sealed.** Once a day has happened, its state can't be edited. Today stays editable until it ends. (This is enforced in the data layer, not just by the interface: even a bug in the window code can't change a sealed day.)

Sealing keeps the record honest: you can't quietly rewrite last Tuesday. It also means a mis-click yesterday is permanent.

### Cell states

| State | How it looks |
| --- | --- |
| Empty | A plain square cell. |
| Filled | A big dot in the middle of the cell. |
| Skipped | One diagonal line (`/`) from one corner of the cell to the other. It's not a miss. |
| Today | The current day sits inside a larger squircle, so it's easy to find. |

Sealed days look like any other cell of their state. They just don't respond to clicks anymore.

## Measuring progress

*(planned)* Adderall will show how you're doing right under each habit, like how many days you've hit.

There is **no guilt-inducing streak counter**. Missing a day doesn't reset anything, break a chain, or make a little flame go out. The grid just has an empty square in it, and tomorrow is another square.

If you like streaks, there's a **freeform measure tool**. Click and drag across a run of cells in any habit and Adderall measures what you grabbed:

- how many cells you grabbed
- how many days are filled, skipped, and empty
- the streaks inside the grabbed area, including the longest
- the share of the grabbed days you filled

A few details, so the numbers never surprise you:

- You grab cells of **one habit**, from the cell you pressed on to the cell you let go on, in either direction. Cells before the habit's first day aren't counted.
- A **streak** is a run of filled days. A skipped day isn't a miss, so it neither counts toward a streak nor breaks one. An empty day ends it.
- The share is **filled days out of the days that have happened**. Days still ahead of you are listed as "ahead" and are never counted as misses.
- Clicking fills or skips a cell when you *let go* on the same cell you pressed. If you drag to another cell, it's a measurement, and nothing is changed.

For example, grab 14 days and it might tell you: 14 cells, 10 filled, 2 skipped, 2 empty, longest streak 6, 71% filled.

You pick the cells, so you decide what counts as a streak: this month, the last week, or just the stretch you felt good about. The results appear in a **large tooltip near your cursor** while you drag, and go away when you let go. The measure tool needs a mouse.

## Time zones and new days

In settings, a small pop-out shows a **map of the world with its time zones separated**. Click your zone to choose it. Then pick **when a new day begins** in that zone, so a day can roll over at midnight, or at 4am if you're a night owl.

1. **Rollover:** at your new-day time, today becomes yesterday, yesterday is sealed, and a new cell becomes today.
2. **Daylight saving** is handled for you, using your time zone's rules.
3. **Changing your time zone** only affects days from now on. Days that are already sealed stay sealed. Adderall remembers the latest day it has shown as today (`lastday` in the data file), so flying west, where your clock's date goes back, never reopens a sealed day: today simply stays put until the local date catches up. (If that remembered day is more than two days ahead of the clock, the clock must have been set wrong at some point, so the clock wins.)
4. **Defaults:** your computer's time zone, with the day starting at midnight.
5. **Built in:** the world map and a time zone database come with Adderall, so it works the same on every system, including Windows, and never needs the internet. The database is the public-domain [IANA Time Zone Database](https://www.iana.org/time-zones) (release 2026e, the zones of `zone.tab`), compiled to TZif files in the `data/zoneinfo` folder next to the program; Adderall reads them itself rather than asking the operating system.
6. **New-day time** can be anywhere from 00:00 to 11:59, and follows your zone's wall clock, so it stays at 04:00 through a daylight saving change.
7. **Planning ahead** works up to ten years from today.

## Look and feel

The whole interface is **minimalist, clean, and black and white**. No gradients, no accent colors, no decoration: just your habits.

- Use it as black on white or white on black.
- With no color to lean on, every cell state has its own distinct mark: a big dot for filled, a diagonal line for skipped, and a plain square for empty.
- A larger squircle marks the current day, since it's the one you'll click most.
- Each habit has its own symbol (star, triangle, hexagon, and more), drawn in black and white like everything else.
- A settings button and a habit button sit in the corners, and nothing else crowds the board.
- Nothing blinks, bounces, or celebrates, and animation can be turned off.

## Interface notes

The decisions behind the screens, for anyone poking at the code:

- **Two corner buttons.** *Habits* sits bottom-left and *Settings* bottom-right. Nothing else crowds the board.
- **The board.** A thin header names the months (horizontal) or the first day of each line (vertical). Hover a cell for its date, or a symbol cell for the habit's name. Cells are never colored: a plain square is empty, a big dot is filled, a `/` is skipped, and today sits in a larger squircle. A habit's symbol cell is a solid square with the symbol cut out of it, so it can never be mistaken for a day.
- **Sealed days** look like any other cell of their state; clicking them does nothing.
- **Habit pop-up.** Opens above the Habits button. Each habit is a row with its symbol, its name, and *Rename*, *Symbol*, *Archive* and *Delete*. *Rename* edits in place; *Symbol* opens a row of the 12 symbols with the ones other habits use dimmed; *Delete* asks "Delete for good? There is no undo." on the spot. Below the board's habits is the "New habit" field, then the *Archived* section with *Restore* and *Delete*. Adding a ninth habit swaps the pop-up for a short list of the eight, each with *Archive* and *Delete*; choose one and the new habit is added right after.
- **First launch.** A small welcome: name a first habit (or skip) and pick the layout.
- **Messages** (a damaged file that was recovered, a failed save, a copied path) appear in a bar at the top and stay until dismissed. A failed save keeps its bar, with a *Retry* button, until a save works.
- **Animation.** The only motion is a short slide when the board scrolls to today or to the keyboard's cell. Settings → Animation turns it off.
- **Keyboard.** Everything except the measure tool works without a mouse: `Tab` and `Shift+Tab` move between buttons, `Enter` or `Space` press them, `Esc` closes pop-ups.

## Platforms

Adderall is available on **Windows**, **Linux**, and **macOS**. On Linux it runs on **Wayland** or **X11**; it isn't tied to any particular display server, compositor, or windowing system.

Adderall ships as a **separate, dedicated build for each operating system**, rather than a single universal package.

## How it works

Adderall is a small graphical layer over one Markdown file:

- **Interface** is built with [SDL3](https://libsdl.org), the same as [THC](https://github.com/Doggydoggywhatnow/THC), and runs the same way on Windows, macOS, and Linux (Wayland and X11).
- **Text** is drawn with a bundled, openly licensed font by default, and you can switch to any font file you like.
- **Days** are calendar dates. Today comes from your computer's clock, your chosen time zone, and your new-day time.
- **Habit data** (habits, marked days, and settings) is stored in a single `.md` file in your OS's standard data directory.
- **No network:** Adderall never talks to the internet. Nothing is sent anywhere.

## Saving your data

Everything lives in **one Markdown file**, `adderall.md`, so you can read it, copy it, and back it up. Open it in any text editor or Markdown viewer.

### Where it lives

The standard data folder for your operating system *(planned: exact paths may change)*:

| OS | Folder |
| --- | --- |
| Linux | `~/.local/share/adderall/` (or `$XDG_DATA_HOME/adderall/`) |
| macOS | `~/Library/Application Support/Adderall/` |
| Windows | `%APPDATA%\Adderall\` |

In settings you can **copy the path** or **open the folder** in one click. For testing, or to keep a separate copy of your data, `adderall --data-dir <folder>` (or the `ADDERALL_DATA_DIR` environment variable) uses a different folder.

### The file format

Plain Markdown: headings, lists, and checkboxes, so it reads well in any editor and renders nicely in a Markdown viewer. It starts with a version line. An example, which may change before 1.0:

```markdown
# Adderall

adderall-format: 2

## Settings

- timezone: America/New_York
- daystart: 04:00
- layout: horizontal
- font: default
- textsize: 16
- theme: light
- scale: 100
- contrast: normal
- motion: full
- lastday: 2026-11-12
- hotkey today: t

## Habit: Drink water

- symbol: star
- start: 2026-11-11

- [x] 2026-11-11
- [x] 2026-11-12
- [/] 2026-11-13

## Habit: Read

- symbol: triangle
- start: 2026-11-12
- archived: yes

- [x] 2026-11-12
```

- `## Settings` holds your time zone, day start, layout, font and text size, look, and any hotkeys you've remapped.
  - `timezone` is an IANA name such as `Europe/Paris`. Leave it out and Adderall follows your computer.
  - `daystart` is when a new day begins, from `00:00` to `11:59`.
  - `layout` is `horizontal` or `vertical`. `theme` is `light` or `dark`. `contrast` is `normal` or `high`. `motion` is `full` or `reduced`.
  - `font` is `default` or the path of a `.ttf`/`.otf` file. `textsize` is 8 to 48. `scale` is the interface scale in percent, 50 to 300.
  - `lastday` is the latest day Adderall has ever shown as today. It is what keeps sealed days sealed if you travel west and your clock goes "back" a day.
  - `hotkey <action>: <keys>` lines only appear for hotkeys you changed. Keys look like `T`, `Ctrl+,` or `Enter | Space` (several keys for one action are separated by ` | `). The actions are `left`, `right`, `up`, `down`, `today`, `fill`, `skip`, `habits`, `settings` and `layout`.
- `## Habit: <name>` starts a new habit. Habits appear on the board in the order they're listed.
- `symbol` is the habit's unique symbol: `star`, `triangle`, `hexagon`, `diamond`, `pentagon`, `plus`, `cross`, `hourglass`, `arrow`, `bolt`, `heart` or `house`.
- `start` is the habit's first day, and dates are written `YYYY-MM-DD`. It has to come before the habit's days.
- `- [x]` is a filled day and `- [/]` is a skipped day. Days that aren't listed are empty (`- [ ]` is accepted too and means the same).
- `archived: yes` marks a habit that's off the board.

Version 1 files, like the example in earlier versions of this README, are still read. Adderall writes version 2, and refuses (without touching the file) to open a file written by a newer version.

If Adderall can't make sense of a line, it tells you which one and leaves the file alone. It never silently throws your data away. It rewrites the file in its own format when it saves, so notes, extra headings, and other text you add to it aren't kept: they would count as a line Adderall can't read. Windows line endings and a leading byte-order mark are fine.

If the file is damaged beyond that (empty, binary, or not an Adderall file at all), Adderall falls back to the backup and tells you. If a single line can't be read, it shows you that line and offers to use the backup instead; either way the unreadable file is kept as `adderall.md.damaged` rather than overwritten.

### One window at a time

Adderall locks its data folder while it's open. If you launch it a second time, it tells you it's already running instead of opening a second copy that could overwrite the first one's changes. The lock is an operating-system file lock on `adderall.lock`, held for as long as Adderall runs, so it's released automatically if Adderall quits or crashes. The lock file itself can safely be left lying around: it's the lock that counts, not the file.

### Crash-safe saving

Adderall saves as you go, so a crash or power cut can't leave you with half a file:

1. **Write:** changes go to a temporary file next to your data file.
2. **Flush:** the temporary file is flushed to disk.
3. **Swap:** it replaces the real file in one step.
4. **Back up:** the previous version is kept as a backup copy beside it, `adderall.md.bak`.

(In that order the backup is made just before the swap, so at every moment there is a complete file to open.)

If your main file is ever damaged, Adderall tells you and falls back to the backup instead of starting empty.

## Download (Linux AppImage)

The Linux AppImage for **v0.1.0** (x86-64) is rebuilt by CI on every push to `dev` and stored in the [`downloads/`](https://github.com/Doggydoggywhatnow/Adderall/tree/dev/downloads) folder:

| Distribution | Download | Run |
| --- | --- | --- |
| Any (AppImage) | [Adderall-0.1.0-x86_64.AppImage](https://github.com/Doggydoggywhatnow/Adderall/raw/dev/downloads/Adderall-0.1.0-x86_64.AppImage) | `chmod +x Adderall-*.AppImage && ./Adderall-*.AppImage` |

It is built by `.github/workflows/appimage.yml`; the recipe lives in `packaging/appimage/`. Windows and macOS packages are planned.

## Building from source

Requirements:

- A C11 compiler (GCC, Clang, or MSVC)
- CMake 3.16 or newer
- [SDL3](https://libsdl.org) and SDL3_ttf. If CMake can't find them installed, it downloads and builds them for you (release 3.4.8 and 3.2.2). That download happens only when you build; the finished program never touches the network. On Linux, building SDL3 from source needs the usual desktop development packages (X11, Wayland, and so on); see `.github/workflows/ci.yml` for the exact list.

```bash
git clone <repository-url> Adderall
cd Adderall
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/adderall
```

Use `-DADDERALL_FETCH_DEPS=OFF` to forbid downloads, and `-DADDERALL_BUILD_APP=OFF` to build and test only the window-free core.

## Usage

```bash
adderall
```

Launch Adderall, create a habit, pick a layout, and start clicking squares.

| Action | How |
| --- | --- |
| Fill in a day | Click its cell |
| Skip a day | Right-click its cell |
| Measure cells | Click and drag across them |
| Manage habits | The habit button, bottom-left |
| Open settings | The settings button, bottom-right |
| Look at older days | Scroll, or use the arrow keys |
| See which habit a row is | Hover over the cell at the start of the row |

A typical day: open Adderall, click today's cell on each habit you did, right-click the ones you're skipping, and close it. Your file is already saved. Curious about last week? Drag across those cells and the measure tool tells you what you grabbed.

Two options exist for testing and for keeping separate copies of your data: `--data-dir <folder>` (or the `ADDERALL_DATA_DIR` environment variable) picks the data folder, and `--screenshot <file.bmp>` draws one frame and saves it.

## Configuration

Everything below lives in **Settings** (the button in the bottom-right corner, or the settings hotkey) and is stored in your data file.

### Habits
- Horizontal or vertical layout, for the whole board
- Create, rename, archive, restore, and delete habits (from the habit button)
- *(planned: more display options such as cell size and spacing.)*

### Time
- Your time zone, chosen by clicking a world map, with *Previous zone* and *Next zone* buttons (or the left and right arrow keys) for choosing without a mouse. The map shows the regions of the world by UTC offset; a click picks the nearest zone of that region, and the selected zone is named with its offset and the time there right now. *Follow my computer* goes back to the default.
- When a new day begins, from 00:00 to 11:59 in 15-minute steps

### Data
- Copy the data file's path, or open its folder

### Hotkeys
- **Editable hotkeys:** every keyboard shortcut can be remapped. *Change* replaces an action's keys with the next key you press, *Add a key* gives it another (up to three), and *Reset* restores the default. A key that another action already uses is refused. `Tab` and `Esc` are reserved.

Defaults, all editable:

| Action | Default key |
| --- | --- |
| Move between cells | Arrow keys |
| Jump to today | `T` |
| Fill the selected cell | `Enter` or `Space` |
| Skip the selected cell | `S` |
| Open the habit pop-up | `H` |
| Open settings | `Ctrl+,` (`Cmd+,` on macOS) |
| Switch between horizontal and vertical | `L` |

### Appearance
- **Font:** a clean, minimalist sans-serif ([Inter](https://rsms.me/inter/)) by default. Choose any font file (`.ttf` or `.otf`) you like instead; if it can't be loaded, Adderall says so and keeps the built-in font
- **Text size**, 8 to 48
- Black on white, or white on black
- **Reduced motion:** turn off animation

### Accessibility

Adderall aims to be usable by everyone: keyboard navigation for the whole interface (except the measure tool, which needs a mouse), large text and scaling, a high-contrast mode on top of the black-and-white design, cell states that never rely on color alone, and reduced motion. If something gets in your way, please [open an issue](../../issues). Accessibility problems are treated as bugs.

Things to know: Adderall draws its own interface with SDL3, so it doesn't yet expose itself to screen readers; the line at the bottom of the board that names the focused cell is the closest thing. Native file and folder choosers (for the font) are the system's own.

## Roadmap

- [x] Habits stacked as rows of cells, up to 8
- [x] Horizontal (endless) and vertical (wrapping) layouts
- [x] Click to fill, right-click to skip, past days sealed
- [x] Habit button: create, rename, archive, restore, and delete habits
- [x] A unique symbol for each habit, with hover tooltips
- [x] Single Markdown data file, crash-safe, with a backup
- [x] Time zone map and custom new-day time, with a bundled time zone database
- [x] Freeform measure tool
- [x] Black-and-white interface, light and dark
- [x] Settings button, with editable hotkeys
- [x] Font and text size settings
- [x] Scrolling and arrow-key navigation
- [x] Lock so only one window edits the data file
- [x] Copy the data path and open its folder from settings
- [x] Keyboard navigation and accessibility options
- [ ] Progress summaries (no streak guilt)
- [x] Windows, Linux (Wayland and X11), and macOS builds
- [ ] Cell size and spacing options

## FAQ

### Why not just use a notes app or a spreadsheet?

You can! But a line of days per habit is a very specific thing, and Adderall is built to do exactly that and nothing else: a click to fill a day, past days that stay put, and your progress a glance away. No formulas, no setup, no account.

### Why can't I edit past days?

On purpose. Sealed days keep the record honest, and they keep you from spending your evenings tidying up last week. Fill today in today.

### Can I have two habits with the same name?

No. Every habit needs its own name, so you can always tell them apart. Names are unique regardless of capitals.

### Why only 8 habits?

A short list is easier to keep. If you want to track something new, archive a habit you've finished with and its history stays in your file.

### Why is there no streak counter?

Because a number that resets to zero after one bad day does more harm than good. If you want to know your streak, grab the cells with the measure tool. It's there when you want it and silent when you don't.

### Why no reminders, accounts, or syncing?

Adderall is deliberately small and private: no notifications, no accounts, no cloud, no social features. The data is one Markdown file, so you can copy it between computers yourself or keep it in a folder you already sync.

### Does Adderall need an internet connection?

No. It never makes network requests, so it works the same offline as anywhere else.

### What if I change time zones or travel?

Choose your new time zone in settings. Days that are already sealed stay as they are, and new days follow your new zone.

### Will this change my life?

No. Neither will the pill the name is making fun of. But a row of filled squares is a nice thing to look at, and it beats doing nothing.

## Contributing

Contributions are welcome once the project is public. Ideas, bug reports, and pull requests that fit Adderall's four values are especially appreciated. Contribution guidelines will be added here.

## Dependencies and licenses

Adderall's own code is under the Unlicense. Everything it uses is public domain or permissively licensed (nothing GPL, nothing share-alike), and every choice is recorded here.

| What | Used for | License |
| --- | --- | --- |
| [SDL3](https://libsdl.org) 3.4 | window, input, drawing | zlib |
| [SDL_ttf](https://github.com/libsdl-org/SDL_ttf) 3.2 | drawing text | zlib |
| [FreeType](https://freetype.org) (bundled inside SDL_ttf) | reading font files | FreeType License (BSD-style; we use it under the FTL, not the GPL option) |
| [Inter](https://rsms.me/inter/) Regular, in `data/fonts` | the default font | SIL Open Font License 1.1 (the license text ships beside it in `data/fonts/Inter-LICENSE.txt`; the font is used unmodified) |
| [IANA Time Zone Database](https://www.iana.org/time-zones) 2026e, in `data/zoneinfo` | time zones and daylight saving | public domain |
| [Natural Earth](https://www.naturalearthdata.com) land and time zone regions, simplified, in `data/map` | the time zone map | public domain |

No code from any of these is GPL, and none needs the network at run time.

## Acknowledgments

- [SDL](https://libsdl.org) for making one codebase work on every desktop
- The [Inter](https://rsms.me/inter/) project, the IANA time zone community, and [Natural Earth](https://www.naturalearthdata.com) for giving their work away
- The Wayland and X11 communities, for making native software possible on both
- Everyone who has ever tried to build a habit and just wanted a simple way to keep track

## Versioning

Adderall uses `x.y.z` version numbers:

- **`x.0.0` (major):** major releases
- **`0.x.0` (minor):** big new features
- **`0.0.x` (patch):** small new features, bug fixes, and security updates

The first release, `0.1.0`, is a demo that shows off everything in this README.

## License

Adderall's own source code is released under [the Unlicense](https://unlicense.org), which dedicates it to the public domain. See [LICENSE](LICENSE).
