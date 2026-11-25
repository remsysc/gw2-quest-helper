# GW2 Quest Helper v1.6.2 (Raidcore Nexus Addon)

An in-game **Guild Wars 2** questline, collection, achievement, and Wiki waypoint tracker built for **Raidcore Nexus** (`x64` DirectX11 overlay).

---

## Features

- **Questline Context, Stage Roadmap & Auto-Advance**
  - **Parent Breadcrumb & Stage Counter:** Shows your parent collection progress and questline stage directly beneath the title (e.g., `Newborn Skyscales (6/8) | 1/5`).
  - **Hover & Collapsible Stage Roadmap:** Resolves parent collections (via GW2 reward trophy `Is part of collection`), multi-stage Wiki navboxes (such as *Skyscale*, *Aurora*, *Vision*, *Seasons of the Dragons*), `Has prerequisite` chains, and Story Chapters with 1-click stage switching.
  - **Smart "Up Next" & Auto-Advance:** Surfaces NPC turn-in reminders (e.g., *"Return to Gorrik in Dragonfall..."*) and lets you advance with one click — or **auto-replace** completed stages when **Auto-Advance** is enabled in Settings.
  - **Category-Display Meta-Achievements:** Automatically expands achievements like *"Return to the Domain of Istan"* by fetching their constituent category achievements, progress, and category icons via `/v2/achievements/categories`.

- **Auto-Fitting In-Game HUD Overlay (`ALT+SHIFT+H`)**
  - **Scrollable & Readable UI:** Dark-slate and gold ImGui theme, bottom gradient fade + `+N more` overflow indicator, segmented progress bars with crisp outlined labels, native GW2 font (`NexusLink->Font`), and async GW2 icons.
  - **Area-Grouped & Multi-Column Checklists:** Groups consecutive collection steps by zone sub-headers and switches checklists with `> 8` visible steps into a compact 2-column layout.
  - **Instant Local Step Check-Off & Cache-Aware Sync:** Click any incomplete step circle (`○`) on the HUD to mark it done immediately without waiting for ArenaNet's API cache (with click-to-unmark and `Undo`); automatically reconciles when the API catches up, reads HTTP cache headers to poll right when caches roll over, and syncs on map changes.
  - **In-Game Wiki Walkthroughs & Waypoint Copy:** Hover collection steps for Wiki directions and click any cyan waypoint diamond to copy its `[&...]` chat link straight to the clipboard.
  - **Map-Aware Priority & Filtering:** Highlights active-map goals with a green map-pin icon, floats them to the top, optionally filters the HUD to the current map, and triggers Nexus toast alerts on progress.

---

## Installation for Players

### Option 1: Via Raidcore Nexus Addon Library (Once Listed)
1. In Guild Wars 2, press `Ctrl + O` to open **Raidcore Nexus**.
2. Open **Addons** -> **Library**, search for **GW2 Quest Helper**, and click **Install**.
3. Automatic updates are configured via GitHub Releases (`UP_GitHub`).

### Option 2: Manual Install from GitHub Releases
1. Download `GW2QuestHelper.dll` from the [Latest GitHub Release](https://github.com/remsysc/gw2-quest-helper/releases/latest).
2. Place `GW2QuestHelper.dll` into `<Guild Wars 2>/addons/GW2QuestHelper.dll`.
3. In Guild Wars 2, open Raidcore Nexus (`Ctrl + O`) -> **Addons** and load **GW2 Quest Helper**.

### Steam + Proton / Linux Quick Install
```bash
./scripts/install_to_steam.sh
```

---

## Quick Start & API Key Setup

1. Press **`ALT + SHIFT + Q`** to open **Quest Helper - Manager** (or **`ALT + SHIFT + H`** to toggle the HUD).
2. Generate an API key at [account.arena.net/applications](https://account.arena.net/applications) with these **4 scopes**:
   - `account`
   - `progression`
   - `unlocks`
   - `characters`
3. Paste your key in **API Key & Settings** -> **Save & Verify Key** (or store `GW2_API_KEY=...` in `<Guild Wars 2>/addons/GW2QuestHelper/.env`).
4. Pin collections, achievements, or Storylines from **Search & Pin** or **In-Progress Achievements**.

---

## Privacy & ArenaNet ToS Compliance

- **Official APIs Only:** Uses only official Raidcore Nexus APIs (`NexusLink`, `MumbleLink`, `Textures_GetOrCreateFromURL`, `GUI_SendAlert`), official ArenaNet GW2 REST APIs (`api.guildwars2.com`), and public Guild Wars 2 Wiki Semantic MediaWiki APIs (`wiki.guildwars2.com`).
- **Local Key Storage:** Your GW2 API key is stored locally on disk (`settings.json` / `.env`) and sent strictly over HTTPS (`Authorization: Bearer ...`) to `api.guildwars2.com`.
- **Zero Memory Reading or Automation:** Never reads private GW2 process memory, injects game inputs, or automates gameplay.

---

## Building from Source

### Linux Cross-Compile (`clang++` + `clang-cl` / `.xwin`) & Unit Tests
```bash
./scripts/build.sh
```
This runs native C++ unit tests (`ctest`) in `build-linux/` and builds `build-win64/GW2QuestHelper.dll`.

### Windows (Visual Studio / MSVC x64 + CMake)
```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## License

MIT License — see [LICENSE](LICENSE). Third-party notices for Dear ImGui, nlohmann/json, Nexus API, and Mumble headers are preserved in `third_party/`.
