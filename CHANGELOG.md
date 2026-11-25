# Changelog

All notable changes to **GW2 Quest Helper** are documented in this file.
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.6.2] - 2026-11-25

### Changed
- **Public Release & Raidcore Nexus Auto-Updates**: Enabled `UP_GitHub` auto-update provider pointing to `https://github.com/remsysc/gw2-quest-helper` in `GetAddonDef()`, guarded Clang-only compiler flag `-Wno-nontrivial-memcall` in `CMakeLists.txt` for clean MSVC compatibility, updated `.env` scope docs in `install_to_steam.sh`, cleaned vendored `third_party/` headers for public source distribution, and expanded `README.md` with player install & ArenaNet ToS documentation.

## [1.6.1] - 2026-11-25

### Fixed
- **Waypoint Copy Spacing**: Widened the HUD body (`356px` single-col / `452px` two-col), computed live scrollbar-aware `availW` inside `#HudGoalsBody`, reserved trailing wrap-margin space inside `RenderHudStepItem` so `##StepWp` waypoint diamond buttons (`18px`) never crowd text or clip against the scrollbar, and added vertical spacing around the copied waypoint header badge.

## [1.6.0] - 2026-11-25

### Added
- **Instant Local Step Check-Off (Optimistic Progress)**:
  - Click any incomplete step circle (`○`) directly on the HUD to mark it done immediately without waiting for ArenaNet's API cache. Progress bars update right away (e.g., `7/21` -> `10/21`) and finishing the final step locally reveals "Up Next" turn-in / stage controls right away.
  - Added distinct teal/gold local-check glyphs (`StatusGlyph::LocalCompleted`), click-to-unmark, an inline **Undo** button for recent local checks, `local_bits` persistence in `settings.json`, and automatic reconciliation once the GW2 API catches up and confirms completion (without double-counting or suppressing completion alerts).
- **Cache-Aware & Map-Change Auto-Sync**:
  - Reads HTTP `Cache-Control` (`max-age` / `Age`) and `Expires`/`Date` headers from `/v2/account/achievements` in WinHTTP (`ParseHttpCacheRemainingSec`) to schedule the next poll right after ArenaNet's server cache window rolls over.
  - Triggers a sync on real map transitions (rate-limited to `>= 15s` between syncs), lowers manual sync cooldown from `8s` to `5s`, and lowers default auto-sync interval from `180s` to `60s` (`30s` minimum in Settings).

## [1.5.1] - 2026-11-25

### Fixed
- **Typing & Search Reliability**: Eliminated per-frame mutex contention in `AddonRender()` (calling `UpdateLiveContext` only on character/map changes), guarded `OnKeybindProcess` with `ImGui::GetIO().WantTextInput`, added `[&ChatLink]` handling and local `achievementMeta_` fallback to `PerformSearch`, upgraded multi-word Wiki SMW queries to generate wildcard patterns across articles/stopwords, and added inline Clear (`X`) buttons for Search & In-Progress filters.
- **Map Filter in Fractals & Map-Load Flash**: Fixed `filterCurrentMapOnly` so unresolved maps no longer temporarily expose non-matching goals during map transitions or undocumented instances, persisted `map_names_cache.json` + 404 instance handling (`Instance #<id>`), and tightened map-name matching with word/phrase boundaries (`ContainsWholePhraseCaseInsensitive`).
- **Category-Display Meta-Achievements (`Return to the Domain of Istan`)**: Added `/v2/achievements/categories` loading & disk caching (`ach_categories_cache.json`) so achievements with `"CategoryDisplay"` (and empty API `bits[]`) automatically display all constituent category achievements with live completion/progress, category icon, and one-click sub-achievement pinning.

## [1.5.0] - 2026-11-25

### Changed
- **Visual Polish Pass (HUD & Manager)**:
  - Added a dark-slate & warm-gold scoped ImGui theme (`ThemeScope`) across both the Manager and HUD windows.
  - Added an inline green vector map-pin icon on HUD goal title rows, top-sheen + segment tick dividers (`<= 25` steps) on progress bars, dimmed styling for visible completed steps, consecutive area sub-header grouping, a 2-column step grid when `> 8` steps are shown, and a scrollable HUD goals body with a bottom gradient fade and `+N more` overflow badge.
  - Upgraded the Manager window with segmented `Collections & Achievements` / `Storylines` toggle pills, GW2 icons + mini progress bars in Search / In-Progress / Pinned rows, search tips empty state, icon header controls (`Sync`, `HUD Eye`), and compact icon row actions (`Up`, `Down`, `Edit Note`, `Unpin`, `Wiki`).

### Removed
- **Item & Wallet Currency Tracking**: Completely removed Item/Material and Wallet Currency tracking, inventory/wallet API sync calls (`/v2/account/bank`, `/v2/account/materials`, `/v2/account/inventory`, `/v2/account/wallet`, `/v2/currencies`, and full character bag scans), target-quantity inputs, and owned-item step markers to focus solely on Collections, Achievements, and Storylines (requiring only `account`, `progression`, `unlocks`, and `characters` API scopes). Legacy saved Item/Currency entries in `settings.json` are silently dropped on load.

## [1.4.1] - 2026-11-25

### Fixed
- **Progress bar readability**: Replaced the default ImGui bar with a dark-track bar and a centered, black-outlined label so counts like `7 / 21` are legible over both filled and empty areas.

## [1.4.0] - 2026-11-25

### Changed
- **Icon-First HUD Design**: Replaced ASCII text tags (`[ACH]`, `[STORY]`, `[x]`, `[ ]`, `[>]`) and text buttons (`+ Manage`, `Sync`, `Wiki`, `Chain >`, `WP`, `Go`, `Pin`) with anti-aliased vector icons (`ImDrawList`), GW2 render textures (including automatic reward-item icon fallback for collections), and clean ASCII separators (fixing `?` missing-glyph rendering in `NexusLink->Font`).
- **Stage-Only Questline Progression**: Questline roadmaps and `Next` actions now strictly list and jump between main Stage achievements (e.g., `Newborn Skyscales` → `Saving Skyscales` → `Raising Skyscales`) instead of listing individual child steps 1-by-1.
- **Automatic Personal Storyline Tracking**: Pinning a Storyline now tracks the chapter as a whole (`1 / 5`) and automatically locks onto your active character's current uncompleted story step without manual `Go` / `Switch Now` step-jumping buttons.

## [1.3.0] - 2026-11-25

### Removed
- **Crafting Recipe Tree Expansion**: Removed `/v2/recipes` fetching, sub-ingredient tree evaluation, and recipe HUD UI to keep the addon focused purely on questlines, collections/achievements, direct item counts, and wallet currencies.

## [1.2.0] - 2026-11-25

### Added
- **Questline Parent Breadcrumb & Multi-Stage Roadmap (`Chain >`)**: Displays parent meta-collection progress and questline stage (e.g., `Newborn Skyscales (6/8) • Stage 1/5`) on a single compact line without crowding the active checklist.
- **Universal Chain Discovery**: Automatically resolves parent collections via reward trophy `Is part of collection`, parses multi-stage GW2 Wiki navboxes (Skyscale, Aurora, Vision, Regalia), links `Has prerequisite` successors, and groups Story Quests by Story Chapter.
- **Smart "Up Next" & Auto-Advance Mode**: Extracts NPC turn-in reminders (e.g., *"Return to Gorrik in Dragonfall..."*), offers one-click `Next ->` replacement upon completion, and adds an optional **Auto-Advance to Next Quest in Chain** setting.
- **Single-Source-of-Truth Versioning**: Added `VERSION` -> CMake-generated `version.h` propagating SemVer across Raidcore Nexus `AddonDefinition_t`, UI headers, logs, and HTTP `User-Agent`.

## [1.1.0] - 2026-11-25

### Added
- **Auto-Fitting HUD Layout**: Eliminated fixed empty window space below collapsed goals; added configurable `HUD Max Height` scroll cap, native GW2 font (`NexusLink->Font`), and async 24×24 GW2 icons via `Textures_GetOrCreateFromURL`.
- **In-Game GW2 Wiki Walkthroughs & `WP` Copy**: Parses `{{collection table row}}` guides for pinned collections, displaying inline sub-areas, full hover directions, one-click `[&...]` Waypoint chat-link copy, and external `Wiki` button.
- **Net Crafting Recipe Trees & Wallet Currencies**: Expands sub-recipes (`/v2/recipes/search`) while deducting owned intermediate/base items, and adds Wallet currency tracking (`/v2/account/wallet`).
- **Map-Aware `[HERE]` Priority & Toast Alerts**: Sorts goals matching the active MumbleLink map to the top and triggers Raidcore Nexus toast alerts on progress deltas.

### Fixed
- Made GW2 Wiki Semantic MediaWiki name search case-insensitive with smart Title-Case OR-variants, `limit=150`, and relevance ranking.

## [1.0.0] - 2026-11-25

### Added
- Initial Raidcore Nexus addon (`GW2QuestHelper.dll`) with MumbleLink live character/map sync, background GW2 REST API account scanner (achievements, bank, material storage, shared inventory, all characters, wardrobe unlocks), GW2 `[&ChatLink]` decoder, In-Progress Achievements browser, and HUD Lock / Click-Through mode.
