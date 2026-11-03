# Satorino fork notes

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
| `src/singletons/Settings.hpp` | marked default-on tab scroll setting in the public settings block |
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
| `src/singletons/Settings.hpp` | one marked default-on tab scroll key |
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

### Sync notes

| Upstream file | Hook |
|---|---|
| `src/widgets/Window.cpp` | plain Main title and Main-only custom-label flag |
| `src/widgets/BaseWindow.hpp` | one Main-title flag |
| `src/singletons/Settings.hpp` | one default-on title paint key in the marked public block |
| `src/widgets/BaseWindow.cpp` | construct the flagged title Label subclass |
| `src/CMakeLists.txt` | register new sources inside the marked block |
| `tests/CMakeLists.txt` | register the embedded qwebp animation probe |

Stage D compile and runtime behavior are unverified; see REPORT-D.md.
