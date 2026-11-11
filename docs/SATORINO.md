# Satorino fork notes

Hype train indicators were removed fully in **Stage G batch 15**. Train
references in the earlier stage sections below are historical, not current
features. Game live dots/stars, pins and redeem stars remain.

This file tracks every place where satorino **intentionally** diverges from
its upstream `leafyzito/leafyrino` (branch `leafyrino`). Read it before and
after every upstream sync. The upstream chain is:

```
Chatterino/chatterino2 ──> SevenTV/chatterino7 ──> leafyzito/leafyrino ──> satorino
```

## Identity decisions

- Names (2026-11-02): packaging display name **Satorino** (installer
  wizard, `.desktop` `Name=`, DMG volume name, deb `Maintainer`);
  everything the app shows at runtime is lowercase **satorino** (window
  titles, tray, About, `--version`, user agent); and every
  binary/artifact **file name** is lowercase: `satorino.exe`,
  `satorino.Installer.exe`, `satorino.Portable.zip`, `satorino.app`,
  `satorino-x86_64-Qt6.AppImage`, `satorino.dmg`. Internal identifiers
  lowercase `satorino`. C++ namespaces, class names, source file names and
  CMake target names (`chatterino-lib`, `chatterino`) are **unchanged** so
  merges stay cheap. `project()` is renamed in `CMakeLists.txt`; the target
  names are pinned in `src/CMakeLists.txt` (tests/mocks/benchmarks and
  `.CI/setup-clang-tidy.sh` reference `chatterino-lib` by name).
- **Icon colour: `#cba6f7`** (light purple, Catppuccin Mocha "mauve") -
  see "app icon recolour" below.
- **App id: `cc.satos.Satorino` (final, decided 2026-11-01).** It
  appears in: `.desktop` `Icon=` + installed file names (`src/CMakeLists.txt`
  install rules), `setDesktopFileName` (`src/main.cpp`), GLib toast app id
  (`src/singletons/Toasts.cpp`), macOS bundle id (`src/CMakeLists.txt`),
  flatpak app-id + install names (`.CI/satorino.yml`), appdata `<id>`/
  `<launchable>` (`resources/com.chatterino.chatterino.appdata.xml`), and the
  Windows AppUserModelID (`src/common/Version.cpp` +
  `.CI/chatterino-installer.iss` — those two must stay in sync).
- **Version stays `2.5.6` in lockstep with leafyrino** (decided 2026-11-01):
  `project(... VERSION 2.5.6)` in `CMakeLists.txt`, `CHATTERINO_VERSION` in
  `src/common/Version.hpp`, `MyAppVersion` in
  `.CI/chatterino-installer.iss`. Only move it when an upstream sync moves it.
- **Inno Setup `AppId` is a fresh GUID**
  (`{CC804039-1CF2-4868-BACF-CE61C9EAD4BB}`, generated 2026-11-01) so
  Satorino gets its own Add/Remove Programs entry — Chatterino and
  Leafyrino still share the inherited one. Data deletion is unchanged:
  uninstall removes only `{userappdata}\Chatterino2\Cache`; install
  removes that cache always and the ENTIRE `{userappdata}\Chatterino2`
  folder only when the opt-in "Fresh install (delete old settings/logs)"
  checkbox is ticked. That folder is shared with real Chatterino installs
  (see below), so leave that box unticked on shared-settings machines.
- **Settings directory intentionally SHARED** with chatterino/leafyrino:
  `setApplicationName("chatterino")` in `src/main.cpp` is unchanged
  (`~/.local/share/chatterino`, `%APPDATA%`, flatpak `xdg-data/chatterino`).
  Organisation domain `chatterino.com` also unchanged.
- Browser-extension IPC queue `chatterino_gui` (`src/util/IpcQueue.cpp`)
  unchanged — the extension can talk to either app. The single-instance key is
  renamed to `satorino-gui-activate-*` (`src/RunGui.cpp`) so satorino and
  leafyrino can run at the same time.
- **Updater disabled**: `CHATTERINO_UPDATER` defaults to `OFF`
  (`CMakeLists.txt`); the 7TV feed URL in `src/singletons/Updates.cpp` was
  replaced with an `https://updates.satorino.invalid/...` placeholder so even
  a build configured with `CHATTERINO_UPDATER=ON` can never pull SevenTV's or
  leafyrino's binaries. Nightly-update prompts point at the placeholder repo
  `github.com/satorino/satorino`.
- Source **file names** kept: `resources/com.chatterino.chatterino.desktop`,
  `resources/com.chatterino.chatterino.appdata.xml`,
  `.CI/chatterino-installer.iss` (only contents changed). The flatpak manifest
  was renamed to `.CI/satorino.yml` per the task list.
- All URLs of the form `https://github.com/satorino/satorino` are placeholders
  until the real repo/homepage exists.

## Upstream files touched

Run `git diff upstream/leafyrino...satorino --stat` for the authoritative list.
Summary of what each commit touches (all intentional hooks are tagged
`// satorino:` in code, `# satorino:` in CMake/CI/scripts — after a sync,
re-grep `satorino` and `leafyrino`):

### Task 1 — rename to satorino

| File | Why |
|---|---|
| `CMakeLists.txt` | `project(satorino ...)` (target names pinned separately) |
| `src/CMakeLists.txt` | pin `chatterino-lib`/`chatterino` targets, `OUTPUT_NAME "satorino"`, install renames (`cc.satos.Satorino.desktop/.png`), macOS bundle name/id |
| `src/common/Version.cpp` | `"satorino "` full-version prefix, AppUserModelID, commit URL |
| `src/RunGui.cpp` | single-instance key `satorino-gui-activate-*` |
| `src/main.cpp` | `setDesktopFileName("cc.satos.Satorino")` |
| `src/common/network/NetworkRequest.cpp` | default UA `satorino/<version> (<hash>)` |
| `src/providers/twitch/eventsub/Controller.cpp` | EventSub WS UA (same string) |
| `src/singletons/Updates.cpp` | feed URL → `.invalid` placeholder, nightly URLs/prompts → placeholder repo |
| `src/singletons/Toasts.cpp` | toast app id, `satorino.exe` check, `setAppName(L"satorino")`, `notify_init("satorino")` |
| `src/widgets/dialogs/SettingsDialog.cpp` | window title |
| `src/widgets/settingspages/AboutPage.cpp` | fork identity/credits label |
| `cmake/resources/windows.rc.in` | Windows version-info strings |
| ~26 more `src/**` files | pure `Leafyrino` → `Satorino` display strings (window titles, tooltips, tray, dialogs, log prefixes) — one-line each, see the task 1 commit |
| `.CI/*`, `.github/workflows/*`, `flake.nix`, `default.nix` | installer, deb/AppImage/DMG, artifacts, branches, changelog script |
| `resources/com.chatterino.chatterino.desktop`, `resources/com.chatterino.chatterino.appdata.xml` | rewritten display metadata (appdata had stale Moltorino7 identity) |
| `.github/FUNDING.yml` | neutralised (was upstream author's tip link) |
| `README.md`, `docs/SATORINO.md` | fork readme + this file |

### Task 2 — Themes settings page

| File | Why |
|---|---|
| `src/singletons/Theme.hpp` / `Theme.cpp` | added public `rescanThemes()` (marked lines) so the page can pick up new custom themes without a restart; added `ThemePreviewColors previewColors(key)` (marked) reading a theme's background + four accents from its own JSON with the same `metadata.fallbackTheme` resolution as `parseFrom` — no per-theme table, custom themes included |
| `src/widgets/settingspages/GeneralPage.cpp` | the "Theme" dropdown and the "Dark/Light system theme" dropdowns were removed (three-line `// satorino:` marker left behind) — the card grid replaces the picker, and the system theme dropdowns moved to the Themes page (same setting keys, same `conditionallyEnabledBy` semantics, no duplicates) |
| `src/widgets/dialogs/SettingsDialog.cpp` | one marked `addTab(ThemesPage)` line + include; the tab icon is `:/settings/themes.svg` (a white half-filled circle, new file `resources/settings/themes.svg`). It used to be `:/buttons/switchDark.svg`, which is filled `#222` = the dialog background, so it was invisible - and that file is shared with the poll/prediction/pinned banners, so do not edit it |
| `src/CMakeLists.txt` | sources registered in the marked `# satorino:` block |

New files: `src/widgets/settingspages/ThemesPage.{hpp,cpp}` (a wrapping
`FlowLayout` grid of cards under a standard `TitleLabel` section title, plus
the "Adaptive system theme" dropdowns under a `SubtitleLabel` — the same
section-header language as the General page, styled by settings.qss) and
`src/widgets/settingspages/ThemeCard.{hpp,cpp}` (fixed-size preview card:
background + accent swatches, centred two-line name, 1px `#444444` border,
2px `#4FC3F7` when selected, hover + Enter/Space activation). Card chrome
reuses the app's own dialog palette — `#333333` is `QPalette::Base`, the
surface of every combo box in the dialog, and `#4FC3F7` is the accent
settings.qss paints section titles with — and is pinned to the dialog's
fixed dark style, so only the swatches vary by theme. Activating a card
writes the same `/appearance/theme/name` setting as `/settheme`, and the
highlighted card follows that setting in both directions.

### Task 3 — `/settheme` command

| File | Why |
|---|---|
| `src/controllers/commands/CommandController.cpp` | one marked `registerCommand("/settheme", ...)` line + include |
| `src/controllers/completion/sources/CommandSource.cpp` | one marked usage-hint entry `{"/settheme", "<theme name>"}` |
| `src/CMakeLists.txt` | sources registered in the marked `# satorino:` block |

New files: `src/controllers/commands/builtin/SetTheme.{hpp,cpp}`.

### lowercase satorino - binary, packaging and in-app strings (2026-11-02)

One commit on top of the rebrand; see the commit for the full file list.

| File | Why |
|---|---|
| `src/CMakeLists.txt` | `OUTPUT_NAME "satorino"` (binary + packaging follow) |
| `cmake/resources/windows.rc.in` | `ProductName`/`FileDescription`/`InternalName`/`OriginalFilename` lowercase |
| `.CI/chatterino-installer.iss` | `MyAppExeName "satorino.exe"`, staging dir `satorino`, `OutputBaseFilename satorino.Installer` |
| `.CI/build-installer.ps1`, `bundle-portable.ps1`, `deploy-crt.ps1` | staging folder `satorino`, artifact base names `satorino.Installer` / `satorino.Portable` |
| `.CI/CreateAppImage.sh`, `CreateUbuntuDeb.sh`, `MacDeploy.sh`, `CreateDMG.sh` | `bin/satorino`, `satorino.app` |
| `.CI/format-recent-changes.py` | installer artifact names |
| `.CI/satorino.yml` | `command: satorino` |
| `.github/workflows/build.yml`, `create-installer.yml` | `C2_APP_EXECUTABLE`, `C2_APP_BUNDLE`, staging folder, release artifact names |
| `.github/workflows/satorino-windows-test.yml` | exe/folder rename + **case-exact** package verification + launch smoke test (must stay alive 10 s; runs after the upload) |
| `resources/com.chatterino.chatterino.desktop` | `Exec=satorino`, `StartupWMClass=satorino` |
| `resources/com.chatterino.chatterino.appdata.xml` | `<binary>satorino</binary>` |
| `default.nix` | `mainProgram = "satorino"` |
| ~30 `src/**` files | display strings: window titles, tray tooltip/menu, About, `--version`, UA (`TwitchNameHistory`), system messages, tooltips |
| `src/widgets/settingspages/ThemesPage.cpp` | seeded example theme file renamed `satorino-example.json` |

Deliberately **kept** (do not "fix" these on a sync):

- app id `cc.satos.Satorino` everywhere (AppUserModelID, GLib toast id,
  `setDesktopFileName`, install renames, flatpak app-id);
- `SATORINO_*` environment variable names (`scripts/sync-upstream.sh`);
- SPDX `The Satorino authors` headers, README and this file's prose;
- packaging **display** names: installer `MyAppName`/`MyAppPublisher`/
  `MyAppProductName`/`MyAppDescription` + `DefaultDirName`, DMG volume name
  (`CreateDMG.sh`), deb `Maintainer`/`Description`, `.desktop` `Name=`,
  appdata `<name>`, `MACOSX_BUNDLE_BUNDLE_NAME`, nix `description`
  (rc.in `CompanyName` is lowercase `satorino`; its URL stays the
  placeholder, see below);
- `ChatterinoUpdater.exe` inside the portable bundle (third-party updater
  binary, `bundle-portable.ps1`);
- `setApplicationName("chatterino")` + org domain (shared settings dir);
- legacy `Leafyrino`/`Chatterino2` staging-folder fallbacks in scripts.

Smoke-test note: the workflow does NOT use `--version`. On Windows that path
calls `attachToConsole()` (`src/util/AttachToConsole.cpp`), which passes a
null `FILE**` to `freopen_s`; the CRT invalid-parameter handler then aborts
with 0xC0000409 whenever a parent console exists (e.g. the CI runner, or a
terminal). Upstream code, left unfixed on purpose (merge conflicts); the
one-line fix would be `FILE *dummy; freopen_s(&dummy, ...)`. The smoke test
instead launches the real GUI and requires the process to survive 10 s
(a missing DLL exits 0xC0000135, a startup crash exits early). It runs
after the artifact upload and is `continue-on-error` until it has passed
once on a runner.

### app icon recolour - light purple #cba6f7 (2026-11-02)

The icon set is recoloured to the fork colour **#cba6f7**.
`scripts/recolour-icon.py` is the generator - re-run it after an upstream
sync replaces any icon file; it is also the record of the exact method:

- hue shift to the mauve hue with a power-law remap of saturation and
  luminance, anchored on the source's dominant saturated colour (that
  colour maps exactly to #cba6f7);
- neutrals (grey shadow layer, white, black) and alpha are untouched -
  only coloured pixels turn mauve;
- the rasters and the SVG had diverged (rasters = leafyrino's green,
  `icon.svg` still chatterino blue), so each is recoloured with its own
  anchor; both are mauve now.

Formats regenerated in place (file names unchanged):
`resources/icon.ico` (16-256 px; exe icon via `windows.rc.in`, installer
`SetupIconFile`, tray via `:/icon.ico` in `TrayController.cpp`),
`resources/icon.png` (256 px; Linux install/flatpak/AppImage),
`resources/chatterino.icns` (16-1024 px; macOS bundle) and
`resources/icon.svg` (fills).

icns detail: the legacy RLE `ic04`/`ic05` chunks (16/32 px ARGB) were
replaced by PNG `icp4`/`icp5` chunks - same sizes, the encoding macOS
itself emits via iconutil; every other chunk kept its type and order,
and the binary `info` chunk is copied verbatim.
The settings-sidebar tab logo `resources/settings/leafyrino.png` (the
"Leafyrino" tab in `SettingsDialog.cpp`) deliberately **stays leafyrino
green**: that tab is about Leafyrino's own features. It was recoloured
once and reverted on the owner's request - do not recolour it again.
The other green images in `resources/` (avatars, `technorino*.png`,
`buttons/unban.png`) are not the app logo and are left alone.
`resources/moltorino.icns` is a stale, unreferenced leftover from the
Moltorino7 chain and was left alone. Emotes, badges and avatars are NOT
recoloured.

Verified programmatically (see the task report): every size present in
every format, dominant colour exactly #cba6f7, shape and luminance
structure preserved, no old green left. macOS rendering itself is
untested - no macOS build exists.

### Stage C, part 1 — Satorino settings tab

| File | Why |
|---|---|
| `src/widgets/dialogs/SettingsDialog.cpp` | marked include and Satorino tab immediately after General |
| `src/widgets/helper/SettingsDialogTab.hpp` | append Satorino ID without changing existing IDs |
| `src/singletons/Settings.hpp` | marked tab scroll setting in the public settings block (default off since Stage G batch 18) |
| `src/widgets/Notebook.cpp` | live refresh of every SplitNotebook when tab scroll changes |
| `src/CMakeLists.txt` | register SatorinoPage sources in the marked block |

New files: `src/widgets/settingspages/SatorinoPage.{hpp,cpp}` and
`resources/settings/satorino.png`. The sidebar PNG is a byte-for-byte copy
of the 256x256 RGBA purple app icon; it uses the same canvas and framing as
the green Leafyrino sidebar logo, checked side by side at tab size. The
source rasters have slightly different antialiasing. No existing icon is changed.

### Stage C, part 2 — single-row tab scroll

| File | Why |
|---|---|
| `src/widgets/Notebook.{hpp,cpp}` | small hooks for the clipped layout, wheel routing, selection visibility, and strip-aware hit testing; remove SplitNotebook's redundant reparent |
| `src/widgets/helper/NotebookTab.cpp` | translate drag positions into notebook coordinates and stop old animations for immediate scroll moves |
| `src/CMakeLists.txt` | register the strip implementation and pure layout helper in the marked block |
| `tests/CMakeLists.txt` | register pure helper regression tests |

New files: `src/widgets/NotebookTabStrip.cpp`,
`src/util/NotebookTabLayout.{hpp,cpp}`, and
`tests/src/NotebookTabLayout.cpp`. Only the Main window's user-managed
SplitNotebook uses the new path, with Top/Bottom tabs and Tab scroll on.
A child QWidget clips the tab row; the custom buttons and right-aligned +
stay in notebook coordinates. Wheel deltas pan immediately, with fractional
angle-delta motion retained, pixel deltas preferred, and bounds clamped.
Selection, filtering, size changes, and reorder reveal the selected tab.
Dialogs, popup windows, vertical tabs, hidden tabs, and the setting-off
path retain the existing horizontal/vertical layout functions.
Drag auto-scroll at the viewport edges is intentionally omitted.

### Stage C, part 3 — pinned tabs

New files: `src/widgets/NotebookPinnedTabs.cpp` (pin order and user-close
guard), `src/widgets/helper/NotebookTabSatorino.cpp` (pin action, icon,
tooltip, and bulk-action counts), and
`tests/src/NotebookPinnedTabs.cpp`. The pure helper gains pin/insertion/
reorder boundaries. Pinning appends to the pinned group; unpinning starts
the unpinned group. New tabs are unpinned. A pinned tab's duplicate starts
the unpinned group; ordinary duplicates keep their existing adjacency.
Alphabetical sorting operates inside each group. Pinned tabs scroll with
the row and also work in the existing wrapped and vertical layouts.

User closes go through `requestRemovePage`; raw `removePage` remains
available to programmatic teardown. Bulk menus count actual visible,
unpinned targets and enable each action only when it has a target (so
Close All can close a single remaining unpinned tab). The pin is always
painted in the title/X color, including with close buttons disabled. Its
area selects the tab, shows a tooltip, and never closes or unpins it.
The layout encoder emits `"pinned": true` only for pins; loading defaults
to false and restores each pin at the boundary, repairing hand-edited
order even when the layout is locked. SplitContainer's popup copying is
unchanged, so popping out a pinned tab does not pin the new popup tab.

## What "leafyrino" is allowed to remain

`git grep -in leafyrino` after a sync should only return:

- `src/providers/moltorino/MoltorinoPresence.cpp` — dormant dead code,
  excluded from the build (`src/CMakeLists.txt`); do not build on it.
- `src/widgets/settingspages/LeafyrinoPage.*` and its
  `SettingsDialogTab.hpp` enum / `SettingsDialog.cpp` tab entry — the leafyrino
  feature page stays as-is.
- `/leafyrino/*` settings keys (`src/singletons/Settings.hpp`) — renaming
  would reset users' settings.
- `leafyrino.client_detection` filter identifier
  (`src/controllers/filters/lang/*`) — renaming would break existing filters.
- SPDX "Contributors to leafyrino" headers, code comments describing leafyrino
  behaviour, README/AboutPage fork credits.

## Hot files (conflict on nearly every sync)

`src/messages/MessageBuilder.cpp` (badge block — untouched so far),
`src/singletons/Settings.hpp` (no satorino keys added so far — keep it that
way), `src/Application.{hpp,cpp}` (untouched), `src/singletons/WindowManager.cpp`
(only one display string), `src/widgets/dialogs/SettingsDialog.cpp` (title +
one marked addTab line), `src/CMakeLists.txt` (marked satorino block +
OUTPUT_NAME + installs), `src/messages/MessageElement.hpp` (untouched — next
free badge bit was `1LL << 49` as of writing).

### Stage C hot-files table

| Upstream file | Why it changes |
|---|---|
| `src/CMakeLists.txt` | register every new application source in the marked block |
| `src/singletons/Settings.hpp` | one marked tab scroll key (default off since Stage G batch 18) |
| `src/widgets/dialogs/SettingsDialog.cpp` | include and add Satorino directly after General |
| `src/widgets/helper/SettingsDialogTab.hpp` | append Satorino without renumbering existing IDs |
| `src/widgets/Notebook.hpp` | declare strip/pin hooks and their local state |
| `src/widgets/Notebook.cpp` | route layout/wheel/selection, enforce insertion/reorder/sort groups, guard hotkey closes, connect live setting |
| `src/widgets/helper/NotebookTab.hpp` | declare pin state, action hooks, painter, and tooltip event |
| `src/widgets/helper/NotebookTab.cpp` | small pin/menu/paint/input hooks, skip pins in bulk closes, and map drag coordinates |
| `src/common/WindowDescriptors.hpp` | optional per-tab pinned flag |
| `src/common/WindowDescriptors.cpp` | load pinned with a false default |
| `src/singletons/WindowManager.cpp` | encode true pins and restore pinned-first order |
| `tests/CMakeLists.txt` | register pure math and notebook pin regression tests |

## Post-sync checklist

1. `git submodule update --init --recursive`
2. `git grep -n 'satorino'` — every hook should still be present
   (`// satorino:` / `# satorino:` tags).
3. `git grep -in 'leafyrino'` — only the allowed leftovers above.
4. Re-check the hot files listed above by hand.
5. Configure + build (see below), fix conflicts fallout.
6. Smoke test: Settings → Satorino immediately after General; search for
   "Tab scroll", verify its default-on checkbox and live toggle. Leafyrino
   must keep its green logo.
7. Smoke test: with 25 tabs, Tab scroll on (Top and Bottom), wheel to both
   ends; verify clipped edge tabs, fixed buttons and +, selection reveal
   (click/Ctrl+Tab/Ctrl+1..9), resize, close/add, live-only filtering, and
   drag reorder after scrolling. Toggle off for stock wrapping/growing.
   Check dialogs, popup windows, Left/Right, hidden tabs, UI scaling, and
   wheel over chat content for unchanged behavior.
8. Smoke test: pin/unpin, add/duplicate, drag and move-tab hotkeys across the
   pin boundary, alphabetical sort, and layout lock. Test wrapped, scrolling,
   and vertical tabs. With Dark/White themes at 1x/2x and compact/normal tabs,
   verify always-visible pins (close buttons on/off), long-title clipping,
   tooltip, pin clicks, and X/middle/menu/hotkey/bulk close protection. Save
   and reload, including hand-edited out-of-order pins with layout locked;
   check unpinned JSON lacks the key, window close, and popup copying.
9. Smoke test: Settings → Themes (card grid, apply, system theme dropdowns,
   open folder, seeding), General has no theme controls, `/settheme Dark`,
   `/settheme` (no args — lists themes), About page,
   launch twice (single instance still activates the running window).

## Windows test builds (GitHub Actions)

`.github/workflows/satorino-windows-test.yml` (added 2026-11-01) builds a
**portable** Windows x64 zip on every push to `satorino` and on demand via
workflow_dispatch. The zip contains `satorino.exe`, the windeployqt output,
the VCRT runtime DLLs (`.CI/deploy-crt.ps1`) and a `modes` file containing
`portable` next to the exe — per `src/common/Modes.cpp` the app then keeps
ALL settings, caches and logs next to the binary, so a test build never
reads or writes `%APPDATA%\Chatterino2`. Extract anywhere and run
`satorino\satorino.exe`; delete the folder to clean up completely.

Heads-up: the main `build.yml` also triggers on pushes to `satorino` (the
rebrand made it the default branch) and then runs the full platform matrix
including the nightly-release job on your fork. Disable "Build" in the
repo's Actions tab if that is unwanted.

## Build (Linux quick reference)

```shell
sudo apt install cmake ninja-build g++ git pkg-config \
  qt6-base-dev qt6-svg-dev qt6-image-formats-plugins qt6-charts-dev \
  libboost-dev libboost-json-dev libnotify-dev libssl-dev libsecret-1-dev
git submodule update --init --recursive
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCHATTERINO_NO_AVIF_PLUGIN=On
cmake --build build -j"$(nproc)"
./build/bin/satorino
```

(The AVIF flag works around the `MAJOR_QT_VERSON` typo in `cmake/AVIF.cmake`
that breaks configure when `libavif` is present. Fixing that typo is
deliberately left to a separate commit.)

## Sync procedure (approved 2026-11-01, configured)

**Merge, do not rebase** from upstream — run `scripts/sync-upstream.sh`:

```shell
scripts/sync-upstream.sh
```

The script enables `rerere` (`git config rerere.enabled true` — rerere is a
local git setting and cannot be committed), adds the `leafyrino` remote if
missing (`https://github.com/leafyzito/leafyrino.git`), refuses to run on a
dirty tree, then `git fetch` + `git merge` (never rebase) from
`leafyrino/leafyrino` and updates submodules. Remote/branch can be overridden
via `SATORINO_UPSTREAM_REMOTE` / `SATORINO_UPSTREAM_URL` /
`SATORINO_UPSTREAM_BRANCH` / `SATORINO_BRANCH`.

Rationale: satorino publishes nothing to anyone yet, so there is no force-push
cost; merging keeps upstream commit SHAs identical to leafyrino's, which makes
`git log leafyrino/leafyrino..satorino` (your delta) trivial to review and
keeps `format-patch`/bundles stable. Rebasing would give a tidier history but
rewrites every satorino commit on each sync and invalidates any patches or
branches you hand out. rerere records each conflict resolution once and
replays it on every later sync, which is what makes repeated merges cheap.

After a conflicted merge: resolve, `git add`, `git commit`, re-run the script
(it should then report "Already up to date."), and walk the post-sync
checklist above.

## Stage D — title and channel highlights

### Main window title

The Main window's OS title is exactly lowercase `satorino`, including in
safe mode and after account changes. The custom frame follows the same
windowTitleChanged signal. Version::fullVersion, About, --version, user
agents, and other windows keep their existing version information.

### Embedded animated title paint

The Main window's custom frame uses a small Label subclass. The user's
unaltered WebP is embedded as :/satorino-paint.webp. Its 150 frames are
50 ms each (7.5 seconds), with infinite looping. On first show a worker
uses Qt's webp QImageReader to require animation support and decode all
frames. Until ready, on failure, or for a nonanimated resource, the stock
plain label is painted. The font and padding come from Label; the image
covers the text rectangle and fills the glyphs with a texture brush.
Only this label is updated per frame. Its precise single-shot timer pauses
when hidden/minimized, retaining the remaining frame delay and loop state.
A gtest probes the embedded asset through the installed qwebp handler;
it has not been run here. No 7TV, account, network or Desktop path is used
at runtime. Native OS title bars remain plain text.

### Live title paint toggle

Settings -> Satorino -> Appearance has Animated title paint, default on,
stored at /satorino/title/animatedPaint. Turning it off paints the stock
plain title and stops the timer immediately. Turning it back on resumes
the frame/delay state, or lazily starts decoding if it has never loaded.
It does not depend on 7TV settings. All title behavior remains unverified
on Windows until the compile and runtime checks are run.

### Per-channel highlight matching and persistence

Channel phrase lists are stored as an object at /satorino/highlights/channels,
keyed by platform:lowercase-channel (twitch, kick, youtube). Empty lists are
omitted. A controller-owned ChatterinoSetting uses the existing settings
manager, including portable mode and settings snapshots. Each channel's
editors share one SignalVector; synchronous edits rebuild and atomically
publish compiled checks without the global editor's delayed update.

The old HighlightPhrase schema is reused, with checks built by the active
MessageHighlight matcher (same regex, case, boundaries and self suppression).
The current global HighlightModel/editor was replaced upstream by a different
three-column Model plus ConfigureDialog; the channel table retains the
requested phrase-only columns through a small dedicated model adapter.

Channel matching uses a borrowed platform/name key with heterogeneous hash
lookup under a shared lock, without allocating a normalized QString on the
message path. Only matching channels visit phrase checks. The existing
MessageBuilder/Kick/YouTube highlight blocks already consume check()'s result,
including mentions, sound, alerts and color, so no duplicate builder hook is
needed. Channel phrases override global color AND sound (including silence);
alert and mentions flags are ORed. Old messages are not rechecked. The active
pipeline's blacklisted-user and self-message suppression are preserved.

### Channel phrase editor

The split menu has Channel highlights… directly after Set filters…. It is
enabled only for a real Twitch, Kick, or YouTube channel, and rechecks the
current selected channel whenever the menu opens. Aggregate multi-channel
splits edit their active real child; special feeds/mentions/whispers are
disabled. The window title is Channel highlights - #<channel> and its BasePopup
uses the settings stylesheet. It edits the existing HighlightPhrase schema
through SignalVectorModel and EditableModelView: pattern, mentions, taskbar
flash, sound, custom file, colour, regex and case-sensitive columns, with
add/delete and row movement. Double-click colour/custom-file cells to pick
values; Use default sound clears the custom file while preserving the sound
enable checkbox. Invalid regexes show their error and do not match.

This current tree no longer has the old HighlightModel. The new channel-only
adapter restores its requested table shape without changing the active global
three-column Model or legacy migrations. Matching still uses the active global
MessageHighlight implementation. All GUI and matching behavior is read-only
review here until compilation and runtime tests are run.

### Sync notes

| Upstream file | Hook |
|---|---|
| `src/widgets/Window.cpp` | plain Main title and Main-only custom-label flag |
| `src/widgets/BaseWindow.hpp` | one Main-title flag |
| `src/singletons/Settings.hpp` | one default-on title paint key in the marked public block |
| `src/widgets/settingspages/SatorinoPage.cpp` | live Animated title paint checkbox |
| `src/widgets/BaseWindow.cpp` | construct the flagged title Label subclass |
| `src/CMakeLists.txt` | register new sources inside the marked block |
| `tests/CMakeLists.txt` | register qwebp and channel highlight regression tests |
| `src/controllers/highlights/HighlightController.hpp` | own and expose channel highlight storage |
| `src/controllers/highlights/HighlightController.cpp` | one final channel-result priority call |
| `src/widgets/splits/SplitHeader.cpp` | one include and menu call after Set filters… |

Stage D compile and runtime behavior are unverified; see REPORT-D.md.

## Stage F batch 1

Mentions is the global TwitchMentions channel (/mentions); both Twitch and
Kick repost the original message with its channel name, platform and ID.
Stock already showed reply controls, but inferred the pseudochannel instead
of the message's origin. A data-only MentionReply helper now validates the
message and thread root, and looks up the existing Twitch/Kick channel at
action time. A reply without a loaded root is unavailable. No channel is
joined or created. Missing, disconnected, anonymous or unwritable origins
produce a local error. The input keeps its weak channel reference, shows
“Replying to @name in #channel”, and resolves the origin again before sending
(including after outgoing translation). Sending uses the existing channel
sendReply path and its limiter, preparation and reply parent ID. Normal
splits keep their existing reply behavior. There is no separate per-channel
Mentions type; filtered Mentions splits use the same path.

Pin/Unpin Tab now says “(turn off Lock Tab Layout in this menu)” while locked.
That exact setting is in the tab's context menu, not General settings.
The disabled label is used so the explanation is visible without relying on
platform-specific disabled-action tooltips. Pin behavior is unchanged.

New sources: util/MentionReply.{hpp,cpp} and
widgets/splits/SplitInputSatorino.cpp, registered in the marked CMake block;
13 regression tests in tests/src/MentionReply.cpp. Upstream hooks are marked
// satorino: in ChannelView, SplitInput and TwitchChannel (one read-only
connection-state accessor). The existing fork-owned NotebookTabSatorino
holds the label change. No chat automation was added.

clang-format 19.1.7 and patch replay are checked for the batch. Compilation,
gtests and Windows GUI/network behavior: **NOT RUN**; no native C++/Qt
toolchain or WSL distribution is installed. See the delivered REPORT-F1.md
for the executed checks and Windows checklist.

## Stage F batch 2

Branch A: the existing Channel Rewards popup already loads the viewer catalog
through TwitchGql::getChannelPointRewards (persisted ChannelPointsContext) and
redeems through explicit user actions. Unavailable rewards are now retained,
greyed, and labeled instead of filtered out. The repository exposes isEnabled,
isPaused, isInStock and isUserInputRequired; it does not establish a distinct
stream-only, cooldown deadline or once-per-stream field. Offline labels do not
promise that a reward will become available when live.

New util/RedeemData.{hpp,cpp} provides pure cache, ordering and JSON logic.
The registered /satorino/redeems/state setting stores version-1 JSON by numeric
Twitch room ID (returned community ID fallback, never display name). Up to
128 rewards per cached channel, 32 cached channels and 30 days of last-seen
history are retained; eviction preserves pin preferences. Cached rows are
explicitly marked and cannot redeem. A successful live snapshot with both
reward arrays reconciles omissions, removing missing cache entries and pins;
offline, partial and failed fetches do not establish deletion. There is no
verified reward-deletion event in this path. Never-seen offline rewards cannot
be reconstructed, and offline deletions may remain cached until refresh/expiry.

Each reward has a separate pin button. Pins sort first in pin order, with the
existing cost/title ordering retained for other rewards. JSON reads tolerate
missing/malformed data. The per-channel auto object is reserved for future
per-reward flags, always empty and ignored; there is no star UI or automation.
The existing manual redemption requests and auth path are unchanged.

New widgets/dialogs/ChannelPointsDialogSatorino.cpp holds persistence/UI hooks.
All new app files are in the marked CMake block, and 19 RedeemData gtests are
registered. Minimal marked hooks remain in ChannelPointsDialog, TwitchGql
(retained status and snapshot shape only), and the public Settings block.
No new API calls or dependencies were added. Current viewer GQL availability,
integrity-token requirements and Twitch access policy are **unverified** from
this repository; no new integrity-token handling was added.

Formatting and clean-clone patch replay are checked for the batch. Nothing
was compiled: no native C++/Qt toolchain or WSL distro is installed. C++ tests,
Windows layout/persistence and live/offline Twitch behavior are **NOT RUN**;
see the delivered REPORT-F2.md and its Windows checklist.

## Stage F batch 3

Follow-up on Batch 2 commit f764a67aa (preserved). This section supersedes
Batch 2's always-visible default, per-row pin buttons and status-driven height.
Settings -> satorino -> Channel point redeems now has ordinary searchable
checkboxes under /satorino/redeems/: showDisabled, showPaused,
showOfflineStream, hideCooldown (all false), and enablePinning (true).
Open reward popups update on setting changes without a restart or API request.

The pure redeemVisible rule approximates the website from the existing fields:
by default hide disabled, paused, cached and offline out-of-stock rows; live
out-of-stock rows remain visible and greyed. Show disabled/paused controls the
corresponding category. The existing parser folds pause into disabled, so the
paused category takes precedence and needs only Show paused. Offline cached
or unavailable rows additionally need the offline-stream option. Hide cooldown
removes live out-of-stock rows. Stream-only identity, exact cooldown reason/time
and exact website parity remain unverified; no new API fields/calls were added.
Cache bounds, expiry, live-snapshot deletion reconciliation, manual redemption
and the empty reserved auto object remain as documented in Batch 2.

Turning pinning off skips ordering and removes reward pin controls/marks while
retaining preferences. List tiles show only a tiny passive mark when pinned.
Selecting a tile still opens its existing detail view, where a small pin button
is overlaid inside the preview. Neither control adds a layout item or row.
Tile size hints and title geometry no longer depend on availability; a short
elided status is painted inside the existing image square. There is no guessed
countdown. Pinning, selection and option changes cannot initiate redemption.

12 new gtests in tests/src/RedeemVisibility.cpp cover the default field matrix,
toggle effects, cooldown visibility and pinning-off persistence; Batch 2's
19 tests still cover cache/order/JSON behavior. No new app sources were needed.
clang-format 19.1.7 and full-stack patch replay are checked for this batch.
Nothing was compiled; C++ tests and Windows/Twitch behavior are **NOT RUN**
because no native C++/Qt toolchain or WSL distro is available. See REPORT-F3.md.

## Stage F batch 4

BATCH3_HEAD: af7a7240aa3ab679d1767164b4978de57e2095eb (merged upstream).
Redeem pins now explicitly show/raise the selected-preview overlay, consume
its clicks, validate the room-ID/catalog-ID fallback, and explain missing IDs.
Pin edits are local and no longer depend on a pending redeem request. They
read the latest registered JSON state, verify the saved value, show Pinned /
Unpinned feedback, and refresh the detail and list. ID-less automatic rewards
have a local type/pricing pin key; this never replaces an API reward ID.
Final cost/title sorting followed by stable pin priority is centralized in
RedeemData::displayOrder. Cache merges retain effective pin keys. The actual
manual redeem/send methods are unchanged. Windows pinning remains unverified
until the next test build.

Settings -> satorino -> Live status reuses the stock Notifications -> Selected
Channels list (/notifications/twitch). Empty games, desktop alerts off and
Discord off are the defaults. /satorino/live/games holds one trimmed exact
game name or ID per line; matching normalizes Unicode and folds case.
A matching live Twitch channel in any split/aggregate paints an original
controller glyph in the existing live-indicator corner, using its theme color
and DPI scale. Other channels retain stock dots. No tab size hints change.
The historical pure priority enum reserved train > game > dot; no train/star feature was
implemented. Existing stream-status signals and setting changes repaint tabs.

Existing TwitchLiveController polling (30 seconds, batches of 100, initial
checks coalesced at one second) supplies open-channel metadata. The existing
NotificationController closed-channel watcher polls its selected list every
60 seconds with Helix Get Streams. Both now feed the same bounded transition
tracker, with first observations/initial reconnects and repeated stream IDs
suppressed, plus a ten-minute per-login cooldown. There is no second polling
system or new Twitch API endpoint. Stock desktop notifications remain in
charge when the new desktop option is off; enabling it uses transition-only
toasts and suppresses the duplicate stock toast. Existing sound/flash settings
are unchanged. Notifications work only while satorino is running.

Discord uses the existing async networking classes: HTTPS Discord hosts and
webhook paths only, masked URL with show toggle, local-settings secret warning,
and an explicit test button. There is no posted test during development.
The queue holds at most 16 jobs, one active, with at most two retries for
429/5xx and bounded Retry-After waits. Content is capped at 2000 UTF-16 units
and disables mentions. Opt-in NetworkRequest::sensitive redacts all HTTP URL/
body logging even under verbose body logging, disables caching, and redirects
are disabled. NetworkResult exposes optional Retry-After metadata. No secret
URL is written to reports, fixtures or error text.

New sources are registered in the marked CMake block: util/SatorinoLive,
controllers/notifications/{SatorinoLiveNotifications,NotificationControllerSatorino},
widgets/helper/NotebookTabLiveSatorino and widgets/settingspages/SatorinoLivePage.
20 new gtests are registered (16 live/webhook, 4 pin fix); existing redeem tests
remain. Upstream hooks are marked in network classes, NotificationController,
TwitchLiveController, NotebookTab, SplitContainer and Settings. No workflows,
sync script, badges, dependencies, automatic chat or channel-point actions
were added. Nothing was compiled: no native toolchain/WSL distro is available.
C++ tests and Windows/Twitch/Discord runtime checks are **NOT RUN**. See
REPORT-F4.md for every pin suspect, verification and the Windows checklist.
Batch 4 compile fix: use public QWidget repaint without changing the protected mock hook; audit/harden direct includes; compilation NOT RUN (REPORT-F4-FIX.md).

## Stage F batch 5

Game highlights now recolour the stock round live dot instead of drawing a
controller glyph. Dot size, position, stock/rerun/offline paths and tab layout
are unchanged. A matching dot gets the selected game's opaque colour and a
thin contrasting outline in the same ellipse. Across splits and aggregates,
the first matching entry in the user's game list wins. The pure priority stays
train > game > dot; a regular/golden-kappa/other colour table was reserved only.
There is no train detection, train colour UI or active train colour producer.

Live status now has a searchable game picker using the existing Helix
searchGames category endpoint. Search requires two characters, waits 300 ms,
and ignores stale query/account responses. Compact results show async box art
and Add/Added controls. Selected games have colour, remove and up/down controls;
the maximum is 50. Eight non-red default hues expand into distinct shades.
Thumbnail downloads/decode have four slots, weak row targets, placeholders,
negative caching and a 96-entry cache; decoding uses the existing Qt worker pool.

The registered /satorino/live/gamesV2 setting is an actual ordered JSON array
of id/name/boxArtUrl/colour objects. Only an absent key migrates the old text;
the old key is untouched. Explicit empty or malformed V2 values stay empty
and cannot resurrect removed lines. Numeric legacy entries start without a
name; existing stream observations or search results enrich known names/art.
Matching uses ID when both IDs exist, otherwise exact NFC/case-folded names.
Known duplicate aliases collapse while retaining the first entry's colour.
The watch-list heading is Live notifications, with a grey shared-list help
line and Streamer column. Desktop and Discord controls use their existing paths.

New sources are util/SatorinoGames.{hpp,cpp}, util/SatorinoGamesSettings.hpp and
widgets/settingspages/SatorinoGamePicker.{hpp,cpp}, in the marked CMake block.
20 gtests in tests/src/SatorinoGames.cpp cover JSON/setting storage, migration,
matching, dedupe/bounds, colours, metadata, URL sizing and search generations.
Nothing was compiled; C++ tests and Windows/runtime checks are **NOT RUN**.
See REPORT-F5.md for verification, decisions and the Windows checklist.

## Stage F batch 6

The small game-dot appearance came from Batch 5's contrasting pen: the rect
was stock-sized, but part of the small coloured fill was covered by the
outline. That outline is removed. Stock and highlighted indicators now share
one pure indicatorRect calculation, preserving stock float-to-int rounding,
DPI scale, selection/location offsets and the unchanged stock drawEllipse
path. Highlighted circles copy the stock pen geometry and colour the entire
stroke/fill. Low-contrast colours use a lighter/darker shade of the same hue
against the actual tab background, including custom-colour overlays; the
stored game colour and priority/colour resolver are unchanged.

Live status adds Show highlighted games as a star instead of a dot,
default on at /satorino/live/gameStar. The existing per-game swatch controls
both shapes (Dot / star colour). Shape and colour changes repaint tabs live.
An original ten-vertex five-point QPainterPath star stays within the same
stock painted box without a pen blunting its tips; it is not enlarged into
compact title/close/pin space. The historical pure Circle/Star selector reserved Train without activating
train detection or train UI. Unmatched live, rerun and offline paths stay stock;
tab size hints, layout, geometry and title/close/pin drawing are untouched.

New util/SatorinoIndicator.{hpp,cpp} sources are in the marked CMake block.
11 tests in tests/src/SatorinoIndicator.cpp cover stock geometry at scales
1/1.25/1.5/2, star vertices/symmetry/bounds, shape selection, unchanged colour
resolution, contrast and offscreen pixel coverage/state. An existing Settings
test fixture checks the registered default. Nothing was compiled: these
12 tests and Windows/runtime checks are **NOT RUN**. See REPORT-F6.md.

## Stage F batch 7

Highlighted stars target 1.45 times the previous painted-dot radius, with a
0.5 inner ratio (approximately the dot's visual area before clamping). The
shared stock circle rectangle and circle paint path remain unchanged. A pure
radius helper clamps the enclosing star disc to the tab edges and the actual
current title bounds, visible close glyph or stroked pin bounds, with one
physical pixel of clearance. Normal/compact layouts, all locations and both
selection states use those current geometries. A fully obstructed location
suppresses the star instead of overlapping. Title geometry is calculated
earlier for this check, but title drawing/clipping and tab size hints stay
unchanged. Colour, contrast shading, matching and the default-on star option
remain as in Batch 6.

Adding a game clears the query, hides/clears results, clears search status,
stops debounce, invalidates the active generation and focuses the query.
An unabortable Helix request can finish but its stale callback cannot restore
the old results. Remove/colour/reorder actions do not reset search.

Reward pin overlays are removed. Detail has one small icon-only, checkable
content-layout button centred beneath the unchanged reward image; it uses
Pin redeem / Unpin redeem or a short disabled/failure tooltip, with no pin
status label. Explicit mouse tracking keeps it in the Windows frameless
popup's client input path. A click edits only /satorino/redeems/state and
updates the same preview/button immediately. List and preview paint a passive
#ffd23f star directly in their own paintEvent, using the shared star points,
19% of image width and a 4% corner margin. No child marks or layout items are
used for the stars. Pinning off hides the button/stars and skips ordering,
while stored pins and all filters/manual redeem methods remain unchanged.

Room ID remains the preferred local key. A separate pin-only catalog field
prefers a valid channel.id before the legacy community.id fallback; the
existing manual-redemption channel ID is unchanged. An arriving room ID
refreshes open content. New util/RedeemPin.{hpp,cpp} owns the shared validation,
tooltip and passive-star helpers, registered in the marked CMake block.
Tests extend search reset, radius/clamp/layout fixtures and pin round-trip/
merge/order coverage, with a new registered six-case RedeemPin suite (12 net
additional tests). Nothing was compiled; all C++ tests and Windows/runtime
checks are **NOT RUN**. See REPORT-F7.md for the complete input/persistence
trace and Windows checklist.

## Stage F batch 8

Commit A preserves the exact chosen game-indicator RGB/alpha, including saved
alpha, for stars and circles in every tab state. Pins use one 0.85 scale factor
and a shared left-side paint/hit rectangle, true vertical centring and a left
title reservation replacing the old right reservation. Size hints are unchanged;
star clamping uses the transformed pin stroke bounds. Redeem pinning off clears
open-dialog caches/rebuilds immediately, guards both passive paint paths and
makes stored pins dormant without deleting them. Pure geometry, pixel, alpha
round-trip, ordering and flag tests were updated. Compilation/tests **NOT RUN**.


Commit B adds the opt-in Raids section (/satorino/raids/openTab false,
/satorino/raids/switchTab true, dependent checkbox). Existing EventSub user
sessions subscribe to channel.raid v1 with from_broadcaster_user_id, without
new scopes/dependencies. Only open real Twitch channels (including aggregate
children) are watched, regardless of live metadata; ten unique IDs maximum,
subject to the shared Twitch cost budget. A one-second scan runs only while
enabled and logged in; closing sources releases references, and disabling
stops scanning, disconnects account observers and releases all references.
Account changes release with the previous token; generation guards delete
late POST successes using a private original-token snapshot. Raid request
identity includes its owner; connection owner/subscription access is locked.

The pure policy/parser validates source/target, outgoing conditions, account,
blocks, platform and notification age/enable epoch. Dedupe lasts three minutes
(up to 128 pairs); new tabs are capped at one per 15 seconds and five per ten
minutes, retained across toggles/accounts. Existing targets in any window,
including aggregate children, are reused. New tabs append to the source
window and join through the normal Twitch registry. Switching needs the
option, an active main window and an empty current input. Queued delivery
uses a QObject context; QPointers and live checks protect GUI actions. It
never sends a message or redeems points. Live delivery/cost acceptance,
reconnect timing, Windows UI and all C++ tests are **NOT RUN**. Nothing was
compiled. See REPORT-F8.md and the batch8 output report/verification log.

## Stage F batch 9

Commit A raises the star target to 1.7 painted-dot radii and keeps the Batch 7
1.45 floor against obstacles; only tab edges can reduce it further. Tight
aligned title ink replaces the line box. The anchor, circle and exact colour
paths stay unchanged. Pin scale is 0.7; its hit/paint/tooltip box fits its ink
plus two scale units total padding, at a four-unit left margin. Pinned width
reserves that box once and preserves the full title; non-pinned sizing stays
stock. Pure floor/edge, pin-box/width and unchanged colour tests are updated.
Compilation and C++ tests **NOT RUN**. See REPORT-F9.md.


Commit B adds one grey raid status line under the two existing checkboxes;
its one-second timer runs only while the settings page is visible. The line
reports the actual startup Controller/DummyController choice (EventSub's
registered default is true), missing login/token/room ID, accepted/pending/
failed watches with HTTP reason and retry time, and the last event/action or
ignore reason. Hover shows every watch. Existing EventSub logging gets the
same transitions/decisions without tokens or raw raid payloads.

Failed/lost watches retry with 5/10/20/40/60-second backoff and a 90-second
pending timeout; sync also picks up delayed room IDs and credential refresh.
Own-channel ID can use the logged-in account before ROOMSTATE; offline is
not a gate. Credentials are captured on the GUI thread before Asio posting.
Raid references migrate to a Twitch-directed reconnect session. The small
marked handwritten EventSub session hook consumes raw channel.raid v1 without
NoMessageHandler and routes raid revocations into failure/retry state; no
generated table/dependency or other event type is changed. The old fail()
logged that error but did not close an already-open socket; it was not a
proven reconnect cause. Local arrival time guards the enable epoch, while
metadata UTC still enforces age/future limits (with explicit clock reasons).

Pure status/reason/backoff/parser/epoch tests and a registered offline test of
the actual library dispatch were added. No app source files were added.
Nothing compiled: C++ tests, Windows UI and real Twitch behavior **NOT RUN**.
The user's failure is narrowed to observable pipeline stages, not proven
without runtime evidence. See REPORT-F9.md and batch9 verification output.

Final review also keeps compact close ink clear of the star floor by moving
only that glyph inside its unchanged hit/hover box for highlighted stars;
circle/non-highlighted rendering and non-pinned size hints stay stock. A
session/source-checked pending-owner lookup handles an event arriving before
the POST response; it does not substitute the currently selected account.


## Stage F batch 10

Supersedes the raid-source/diagnostic implementation in batches 8 and 9;
all their tab visual fixes remain. The earlier research wrongly dismissed
the shared raid bar feed as countdown-only. PubSub raid.<roomId> already
provides target metadata; its client now forwards update/go/cancel v2.
One central raid.updated connection serves every open real Twitch split,
including aggregate children. There is no EventSub raid subscription, ten-
channel cap, sorted watch list, retry loop or polling timer. EventSub
Controller/Connection/SubscriptionRequest, Helix.cpp and the handwritten
library session.cpp are restored byte-for-byte to pre-batch-8 (0b329736e).

Go is the primary trigger. A cancellable one-shot uses the bar's creation/
receipt time plus forceRaidNowSeconds, with up to one second boundary grace,
as a fallback when go is missing/unusable. Repeated updates do not slide it.
Cancel, early active-state clearing, account/setting changes and closed
sources suppress fallback; go and fallback complete once. Existing confirmed
send-success clearing is marked as go, without initiating a raid. Terminal
cancel IDs are remembered for three minutes to reject reordered stale go.
Unexpected target mismatches disable a stale-target fallback.

TwitchChannel listens when showRaidStatusAboveInput OR satorinoRaidsOpenTab
is on; both setting bindings refresh already-open channels, and later room
IDs use the existing refresh path. Both off keeps the pre-existing PubSub
manager/topic lifetime behavior; the new consumer/deadline timer stop. The
bar update path remains, go does not restart its countdown, and cancel clears
the matching bar state. Normal PubSub pooling/reconnect behavior is reused.

The same policy/actions retain three-minute pair dedupe, one new tab per
15 seconds/five per ten minutes, blocks, self/invalid/account/source checks,
existing-tab reuse across windows/aggregates, normal registry chat joining
and draft/active-main/switch guards. The single grey line reports the shared
feed, open-channel request count, missing room IDs and last raid outcome;
its tooltip states go/countdown triggers and shared bar use. Counts describe
LISTEN requests, not a delivery guarantee. No restart is needed.

The registered SatorinoRaid suite now has 26 pure cases covering feed mapping,
go/cancel/reorder/countdown, listening, status, lookup and existing policy.
The obsolete EventSub dispatch suite is removed. No app source files or
dependencies were added. Nothing compiled; C++ tests, Windows UI, live feed
and live lookup are **NOT RUN**. See REPORT-F10.md / batch10 output evidence.

## Stage F batch 11

Commit A queues completed raid actions through a zero-delay GUI path
after submitted input clearing. Exact scoped
submission text is not an unsent draft; different text and unsent commands
still block. The guard checks application foreground (including in-app
popups), current drafts and settings again at switching time. Local early
send success, remote go and countdown expiry share one completion/dedupe
path. No send operation is added. Nothing compiled; tests and Windows
checks **NOT RUN**. See REPORT-F11.md for the source path/limits.

Commit B adds the pure bounded train state/expiry/type/palette model, a
GUI producer interface, Train > Game > Dot styling in the existing shared
paint path and a compact train-colour table. Only Golden Kappa uses the
star; game-star stays independent, stock geometry and exact RGBA remain.
/satorino/live/train/enabled defaults true; colours persist as a typed
JSON object under /satorino/live/train/colours. The producer at that stage was
explicitly NoData: settings say train data is not available. Research did
not establish a current viewer feed exposing reliable special-train types;
no EventSub scope, speculative PubSub/GQL parser, timer or listener was
added. Disabled/unavailable skips train collection; colour choices remain
saved. Fourteen registered pure cases cover state, missed-end expiry,
priority, shapes, alpha and setting persistence. Nothing compiled; all
C++ and Windows/live checks **NOT RUN**. REPORT-F11.md records evidence
and exactly what a future verified viewer producer requires.

## Stage F batch 12

Commit A separates raid page selection from activation. A browser/other-app
early send now selects the target silently, using Notebook::select(false)
and a no-focus split insertion path; it does not focus/raise/activate a
window. Foreground selection still focuses input and raises. Different
unsent drafts and the master switch block both selection modes; guards
are read again at selection time and status states the outcome/reason.
Go/local-success/expiry still complete once. Reports are output-only.
Compilation and Windows/live checks **NOT RUN**.

Commit B moves only trains to a horizontally mirrored left anchor. Right
stock/game indicators remain independent and can coexist with a left train.
The same stock size/vertical anchor, exact RGBA and star/floor helpers are
used. Collisions nudge the left marker above pin/title ink; only tab edges
can reduce a star below its floor. An impossible/degenerate fit is omitted
instead of overlapping. NoDataTrainProducer/settings remain unchanged.
Geometry/model tests updated; compilation/Windows checks **NOT RUN**.

Commit C adds local per-room/user flags, separate from Twitch monitoring.
User-card and author-username actions toggle only local version-1 JSON
under /satorino/flags/state (500 total / 100 per room), with timestamped
ID keys and cached labels. Master/mark default on; colour defaults to
#50ed8b63 and preserves alpha. Compact settings list removes entries or
clears all after confirmation. Missing IDs wait for existing popup data
and public roomIdAssigned; no lookup is added by flags.

MessageLayout uses its normal text element before the author name and
existing alpha background blend, without modifying shared Messages or
notifications. Flag backgrounds lose to all normal highlights, rewards,
warning/debug and client colours; the optional mark remains visible.
Changing flag state/options forces existing buffer relayout and refresh;
new messages get the same styling on first layout. Ignore/block filtering
is unchanged. No network, sharing, badge or native moderation operation
is added; native monitor/restrict research is in the output report only.
Nine registered pure tests cover store keys, bounds, merge/cleanup, JSON,
priority/defaults and settings persistence. Nothing compiled; C++ tests
and Windows/runtime checks **NOT RUN**.

Commit D corrects a DPR-2 diagonal pin-corner case: if a radial fit still
touches the conservative ink bounds, try another whole pixel upward before
omitting the train. The original mirror x and star floor/edge rules remain.
A specific regression fixture was added; compilation/tests **NOT RUN**.

Batch 12 build fix explicitly sets .focus = true last in insertSplit's
InsertOptions default argument, avoiding nested default-member initializer
use before SplitContainer is complete. Other caller designators remain in
declaration order. Nothing compiled; C++/Windows checks **NOT RUN**.

## Stage G batch 13

Flag controls now use the popup's theme/scaling-aware LabelButton, including
mouse tracking required by Windows frameless HTCLIENT hit-testing. A slim
row below the stock moderation controls is used when that block is present;
otherwise the same control stays in the top actions row. Labels flip between
Flag user and Unflag user synchronously. Capacity errors also display a
visible tooltip on click; missing IDs/master-off explain disabled state.
Keys still use the popup's underlying Twitch room and authoritative author ID.

Layouts now receive the full aggregate or popup source channel for flag room
lookup, so a synthetic usercard buffer need not rely on the registry and an
inactive aggregate child can resolve directly. Existing setting listeners,
generation changes and buffer invalidation cover main chat and usercards;
a late popup room ID also forces relayout. Store bounds, persistence, priority,
colour/master/mark options and all moderation actions remain unchanged.
Four pure regression cases extend the registered SatorinoFlags suite.
Nothing was compiled; C++ tests and Windows/live checks are **NOT RUN**.
Report and patch are output-only in satorino-stage-g-out/batch13.

## Stage G batch 14

Flag user now opens Twitch's native Monitor / Restrict / removal menu using
exactly the existing Helix addSuspiciousUser/removeSuspiciousUser command path.
The existing manage scope is unchanged; older logins must log in again if
Twitch reports the missing permission. Only explicit native menu selections
make those calls. The themed LabelButton stays in the moderation block;
the actions-row fallback is removed. Real Twitch, logged-in moderator/broadcaster
rights, visible moderation actions and a known regular target are required.
Self, broadcaster, moderators and unknown target roles stay hidden. Ordinary
buffered/authored channel messages provide target-role evidence; profile ID
alone does not. Errors are shown as tooltips, never inferred as success.

A GUI-only native-status cache observes the already-existing suspicious-user
message/update EventSub handlers and successful actions, with explicit positive
buffered flags as a fallback. No poll, new subscription or scope is added.
Unknown stays unknown; menus/labels reflect confirmed status. Cache limits are
128 users per room / 1024 total, evicting oldest accepted observations; up to
128 requests are gated globally by room/user to suppress duplicate clicks.
Feed observations during a request take precedence over its late result.
The isolated extra Highlight in my chat / Remove highlight entry still uses
the existing local store, settings and layout unchanged. Remove its marked
menu block and toggleLocalHighlight helper to retire that entry later.
The existing handler still skips monitored message bodies in favor of IRC;
this patch observes their status without adding duplicate chat rendering.
Eight new pure native-policy/cache cases are registered; existing local tests
remain, with obsolete placement coverage replaced by moderator-only visibility.
Nothing was compiled; C++ tests and Windows/live checks are **NOT RUN**.
Output-only report/patch: satorino-stage-g-out/batch14.

## Stage G batch 15

Part 1 gates raid tab actions at go/deadline arrival and again after queued
dispatch, using every window's selected page and all its splits/aggregate
children. Hidden-tab sources do not queue an action or touch policy dedupe/
rate caps. Countdown/feed/listening remain unchanged; eligible raids retain
existing focus/background/draft/settings behavior. Four pure
regression cases added; nothing was compiled, C++/Windows checks **NOT RUN**.

Part 2 removes the hype train state/palette/producer, settings and left tab
paint/geometry, including their dedicated files and tests. Historical sections
above describe the removed stages only. Shared indicatorRect/star/paint and
pin helpers and the right-hand game/dot paint geometry are unchanged. Old
saved train keys are ignored; no migration or reader remains. Nothing was
compiled; remaining game/pin/redeem C++ and Windows checks **NOT RUN**.

Part 3 removes the flagged-users table, row removal and Clear all controls,
plus the now-unused table-only removeLocalFlag adapter. Local master, mark
and alpha colour controls remain under Local user highlights. Store/bounds/
persistence, message layouts and both popup/native menus are unchanged.
The obsolete author-context local flag toggle is also removed, so individual
highlights can now be removed only through the popup's Remove highlight entry; there is no settings list or bulk-clear action. Nothing was
compiled; C++ and Windows/runtime checks **NOT RUN**.

## Stage G batch 16

Part 1 adds native monitored/restricted presentation to ordinary authored
messages using the existing room/user status cache and shared local flag room
resolver. One U+2691 prefix is reused; monitored orange (#66ff9500) and
restricted red (#66ff3b30) backgrounds preserve alpha. Native colour wins over
ordinary/local highlights, while mentions, redeems and protected system styles
keep priority; ignored highlights suppress colour but retain the mark. Existing
low-trust summaries/system messages and their rendering are unchanged.

Local user highlights now starts with Monitored user tools (default on,
/satorino/flags/native/enabled), then Monitored colour and Restricted colour
pickers; the existing local controls follow unchanged. Native master off hides
the popup control/menu and native message presentation live, without clearing
the cache or saved local highlights. Stored local highlights keep their own
master/mark/colour behavior; they cannot be toggled from that hidden popup menu.
Settings and native cache signals force existing main/usercard/history layouts
to rebuild. Pre-existing monitored users are learned from their next existing
suspicious-user message/update event; earlier buffered ordinary messages stay
unknown until such evidence arrives, then repaint. No extra role check, feed,
poll or request is added. Six pure regression cases added to the registered
native suite. Nothing was compiled; C++ and Windows/live checks **NOT RUN**.

Part 2 removes the obsolete local raid diagnostic command, registration,
WindowManager adapter, simulation dispatch, optional users lookup and its
pure command/lookup helpers and tests. Historical help/instructions for that
command are removed. The real feed, selected-tab scope, queue/action policy,
dedupe/caps and silent background behavior remain. Nothing was compiled;
remaining C++ and Windows/live checks **NOT RUN**.

Part 3 shortens the visible Raids status to Raid watch on N channels (1 channel
for one), or Raid watch off when disabled/logged out or not initialized. It
keeps the existing unique-open-room listening snapshot and visible-page refresh
timer. Last-raid/no-event text is removed from the line; feed details and missing
IDs remain in its tooltip. Listening gates, subscriptions and the real action
path are unchanged. Nothing was compiled; C++ and Windows checks **NOT RUN**.

## Stage G batch 18

Part 1 changes only the absent-key default of /satorino/tabs/scrollEnabled
from true to false in Settings.hpp. The key, Tab scroll checkbox and live
layout behaviour are unchanged. Saved true/false values remain effective;
there is no migration. Existing fixtures do not assume the old default.
Nothing was compiled; C++ tests and Windows/runtime checks **NOT RUN**.

Part 2 groups every existing Twitch/Kick channel, global and personal 7TV
section into Zero-width emotes then Emotes (since Stage G batch 19) from
Emote::zeroWidth, including
search. Headers reuse the exact emoji makeTitleMessage path; emoji code,
emote objects, sorting inside groups, links and rendering are unchanged.
A pure stable partition omits empty groups after the existing name/alias/tag
matcher. The same shared EmotePopup handles all entry points.

Existing Helix user-emote responses already contain emote_type; account
snapshots now retain only popup classification without changing JSON parsing
or fetching. Twitch globals show Default emotes (globals/smilies), Hype Train
emotes (hypetrain), then Other emotes (remaining/missing/unknown types). The
API cannot distinguish treasure-chest-only rewards, so the Hype Train label
is deliberately broader; no name/ID guessing, request, subscription or scope
change is added. Active search rebuilds on emote reload. Eight registered
pure grouping/type tests added; nothing was compiled, and C++/Windows/live
checks remain **NOT RUN**.

## Stage G batch 19

Part 1 puts Zero-width emotes before Emotes in every existing 7TV popup
source and search path, for Twitch and Kick. Production labels/classification
are shared with a new order regression case; existing partition tests now pin
that order. Empty groups, per-group sorting, emoji headers and Twitch groups
are unchanged. Nothing was compiled; C++/Windows checks **NOT RUN**.

## Stage G batch 21

Removed the badge order feature completely: settings UI, order/scope settings,
layout/rank hooks, cached preview/icon helpers and feature tests. Chat badges
use their original Twitch/Kick order. Old saved /satorino/badges/order and
/satorino/badges/orderScope values are simply ignored; no migration or user
settings-file edit is performed.

The 7TV zero-width-first popup order and tests, Twitch global emote grouping,
tab scroll default off and all earlier features are unchanged. Nothing was
compiled; C++ tests and Windows checks **NOT RUN**.

## Stage G batch 22

Fixed a crash when Twitch whispers were reposted into open chat splits.
Inline message layout can look up a channel, so the server now snapshots
the open channels under the registry mutex and releases it before invoking
callbacks. The Windows app build and live whisper checks are **NOT RUN**.

## Stage G batch 23

Added the Cyex Deck badge provider for Twitch chat, pinned messages and the
Twitch badge picker's chat preview. The Cyex checkbox in Visible badges is on
by default.
Badge kinds load from Cyex at startup. User assignments refresh every 30
minutes with If-None-Match, keep the previous list on 304 or failure, and
honor Retry-After on 429. Founders show their number in the tooltip and
custom badges use the user's own versioned 1x/2x/4x images. New messages
use the latest successful snapshot. The Windows app build and in-app badge
checks are **NOT RUN**.

## Stage G batch 24

Satorino's public badge feed supplies one active badge per Twitch ID.
The satorino toggle under Visible badges starts enabled. The app loads
the feed at startup and refreshes every minute with If-None-Match. A 304
keeps the current snapshot. Failed requests retry after five minutes.
New Twitch messages use the latest successful assignments. Badge names
appear in tooltips and chat filters can match satorino:<id>. Publish
new artwork with versioned image URLs to refresh the client image cache.
The Windows app build and in-app badge checks are **NOT RUN**.

## Stage G batch 25

The satorino badge provider reads ordered badge lists from
/api/badges/v2. It falls back to the single badge feed if the server
returns 404 for v2. The website limits regular supporters to one
badge, tier20 supporters to two, and the owner to all five. New Twitch
messages show the selected badges in order. The Windows app build
and in-app checks are **NOT RUN**.
