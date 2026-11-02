# satorino

Satorino is a personal fork of [Leafyrino](https://github.com/leafyzito/leafyrino) — a chat client for [Twitch](https://twitch.tv) that collects features from several Chatterino forks, with added [Kick](https://kick.com) and [YouTube](https://youtube.com) support.

## Fork lineage / credits

Features taken from (go star them):

- [Chatterino](https://github.com/Chatterino/chatterino2) — the upstream client
- [itzAlex/chatterino7](https://github.com/itzAlex/chatterino7) / [SevenTV/chatterino7](https://github.com/SevenTV/chatterino7) — 7TV features
- [2547techno/technorino](https://github.com/2547techno/technorino)
- [EEVA-inc/Moltorino7](https://github.com/EEVA-inc/Moltorino7)
- [leafyzito/leafyrino](https://github.com/leafyzito/leafyrino) — the fork satorino tracks for upstream changes

## Building

To get the source code with the required submodules, run:

```shell
git clone --recurse-submodules https://github.com/satorino/satorino.git
```

or

```shell
git clone https://github.com/satorino/satorino.git
cd satorino
git submodule update --init --recursive
```

- [Building on Linux](BUILDING_ON_LINUX.md)
- [Building on Windows](BUILDING_ON_WINDOWS.md)
- [Building on Windows with vcpkg](BUILDING_ON_WINDOWS_WITH_VCPKG.md)
- [Building on macOS](BUILDING_ON_MAC.md)

Notes:

- Ubuntu 22.04 needs Boost ≥ 1.83 built with `--with-json` (see `.github/workflows/build.yml` for the recipe used in CI).
- If you do not have a static `libavif` / `kimageformats` build, configure with `-DCHATTERINO_NO_AVIF_PLUGIN=On` (the bundled `cmake/AVIF.cmake` currently contains a typo that breaks configure when `libavif` is found on the system).
- The updater is disabled: `CHATTERINO_UPDATER` defaults to `OFF`, and the in-code update feed points at an `.invalid` placeholder, so satorino never checks 7TV's or leafyrino's update servers.

## Fork-specific docs

- [docs/SATORINO.md](docs/SATORINO.md) — every intentional divergence from leafyrino, hot-file list, and the post-sync checklist.

## Git blame

This project has big commits in the history which touch most files while only doing stylistic changes. To improve the output of git-blame, consider setting:

```shell
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

This will ignore all revisions mentioned in the [`.git-blame-ignore-revs` file](./.git-blame-ignore-revs). GitHub does this by default.

## Code style

The code is formatted using [clang-format](https://clang.llvm.org/docs/ClangFormat.html). Our configuration is found in the [.clang-format](.clang-format) file in the repository root directory.

For more contribution guidelines, take a look at [Chatterino's wiki](https://wiki.chatterino.com/Contributing%20for%20Developers/).

## Original Chatterino 2 Readme

Chatterino 2 is a chat client for [Twitch.tv](https://twitch.tv).
The Chatterino 2 wiki can be found [here](https://wiki.chatterino.com).
