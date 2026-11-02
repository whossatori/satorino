# Satorino fork notes

This file tracks every place where satorino **intentionally** diverges from
its upstream `leafyzito/leafyrino` (branch `leafyrino`). Read it before and
after every upstream sync. The upstream chain is:

```
Chatterino/chatterino2 ──> SevenTV/chatterino7 ──> leafyzito/leafyrino ──> satorino
```

## Identity decisions

- Display name **Satorino**, binary/installer/DMG/AppImage `Satorino`, internal
  identifiers lowercase `satorino`. C++ namespaces, class names, file names and
  CMake target names (`chatterino-lib`, `chatterino`) are **unchanged** so
  merges stay cheap. `project()` is renamed in `CMakeLists.txt`; the target
  names are pinned in `src/CMakeLists.txt` (tests/mocks/benchmarks and
  `.CI/setup-clang-tidy.sh` reference `chatterino-lib` by name).
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
| `src/CMakeLists.txt` | pin `chatterino-lib`/`chatterino` targets, `OUTPUT_NAME "Satorino"`, install renames (`cc.satos.Satorino.desktop/.png`), macOS bundle name/id |
| `src/common/Version.cpp` | `"Satorino "` full-version prefix, AppUserModelID, commit URL |
| `src/RunGui.cpp` | single-instance key `satorino-gui-activate-*` |
| `src/main.cpp` | `setDesktopFileName("cc.satos.Satorino")` |
| `src/common/network/NetworkRequest.cpp` | default UA `satorino/<version> (<hash>)` |
| `src/providers/twitch/eventsub/Controller.cpp` | EventSub WS UA (same string) |
| `src/singletons/Updates.cpp` | feed URL → `.invalid` placeholder, nightly URLs/prompts → placeholder repo |
| `src/singletons/Toasts.cpp` | toast app id, `Satorino.exe` check, `setAppName(L"Satorino")`, `notify_init("Satorino")` |
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
| `src/singletons/Theme.hpp` / `Theme.cpp` | added public `rescanThemes()` (3 marked lines) so the page can pick up new custom themes without a restart |
| `src/widgets/dialogs/SettingsDialog.cpp` | one marked `addTab(ThemesPage)` line + include |
| `src/CMakeLists.txt` | sources registered in the marked `# satorino:` block |

New files: `src/widgets/settingspages/ThemesPage.{hpp,cpp}`.

### Task 3 — `/settheme` command

| File | Why |
|---|---|
| `src/controllers/commands/CommandController.cpp` | one marked `registerCommand("/settheme", ...)` line + include |
| `src/controllers/completion/sources/CommandSource.cpp` | one marked usage-hint entry `{"/settheme", "<theme name>"}` |
| `src/CMakeLists.txt` | sources registered in the marked `# satorino:` block |

New files: `src/controllers/commands/builtin/SetTheme.{hpp,cpp}`.

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

## Post-sync checklist

1. `git submodule update --init --recursive`
2. `git grep -n 'satorino'` — every hook should still be present
   (`// satorino:` / `# satorino:` tags).
3. `git grep -in 'leafyrino'` — only the allowed leftovers above.
4. Re-check the hot files listed above by hand.
5. Configure + build (see below), fix conflicts fallout.
6. Smoke test: Settings → Themes (list, apply, open folder, seeding),
   `/settheme Dark`, `/settheme` (no args — lists themes), About page,
   launch twice (single instance still activates the running window).

## Windows test builds (GitHub Actions)

`.github/workflows/satorino-windows-test.yml` (added 2026-11-01) builds a
**portable** Windows x64 zip on every push to `satorino` and on demand via
workflow_dispatch. The zip contains `Satorino.exe`, the windeployqt output,
the VCRT runtime DLLs (`.CI/deploy-crt.ps1`) and a `modes` file containing
`portable` next to the exe — per `src/common/Modes.cpp` the app then keeps
ALL settings, caches and logs next to the binary, so a test build never
reads or writes `%APPDATA%\Chatterino2`. Extract anywhere and run
`Satorino\Satorino.exe`; delete the folder to clean up completely.

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
./build/bin/Satorino
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
