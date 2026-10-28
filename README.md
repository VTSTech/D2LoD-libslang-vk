# D2LoD Vanilla+

[![Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/MHK2Dg9)
[![Vanilla+ Discord](https://img.shields.io/badge/Vanilla%2B%20Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/XcJ8mSCNZ)

**D2LoD Vanilla+** is a ready-to-play Windows build of Diablo II: Lord of Destruction
1.14d, compiled from the recovered source in this repository and shipped as release 7z's
bundled with a Vulkan render wrapper (`glide3x.dll`). The base is
[jaenster's Redivivus](https://github.com/jaenster/redivivus) — the shipped `Game.exe`
taken apart function by function and turned back into C++ that builds — plus this fork's
growing set of crash fixes and opt-in quality-of-life features. Vanilla comes first: with
no config file the game behaves like retail, and every extra (Alt-key item labels,
portable saves, more as they land) ships OFF and is enabled in `d2vp.ini`.

It is the **whole** game. Up to 1.13 Diablo II shipped as a small `Game.exe` plus a dozen
DLLs (`D2Client`, `D2Common`, `D2Game`, `D2Win`, `D2gfx`, `Storm`, `Fog`, ...). Patch 1.14
linked all of them into one `Game.exe`, and that single executable is what is recovered
here: the client, the game server that single player runs in the same process, map
generation, the renderers, sound, networking, Battle.net, and Blizzard's Storm and Fog
libraries. The source tree keeps those module names as its top-level directories.

The code is machine-generated from a reverse-engineered database and then corrected by
hand where needed. It is not the original source: the names, types and structure are
reconstructions, and some functions read like a decompiler wrote them, because one did.

## This fork

This repository is VTSTech's working fork of
[jaenster/redivivus](https://github.com/jaenster/redivivus). Upstream `main` is merged
in as it moves; work is organized into three lines:

| Branch | What it is | Docs |
|---|---|---|
| `main` | **The game-source line**: crash fixes and opt-in improvements made directly in the recovered source, kept merged with upstream `main`. (This was the `vanilla-plus` branch before the 2026-10-28 restructure; the old fork baseline `main` is retired.) | [`VANILLA-PLUS.md`](VANILLA-PLUS.md) (charter) · [`handoff.md`](handoff.md) (engineering handoff) · [`PLAN.md`](PLAN.md) |
| `Glide3xVK` | The `glide3xvk` wrapper line: a `glide3x.dll` that renders Glide on Vulkan and threads the final frame through libretro `.slangp` shader presets. (Branch was `libslang-vk` before the restructure.) | [`PLAN.md`](PLAN.md) |
| `jaenster-main` | A clean line tracking upstream with the CI artifact split: fat `d2.exe` for crash triage, `d2-stripped.exe` for play. | — |

CI (`.github/workflows/build.yml`) runs `tools/check-build.sh` on every push and uploads
workflow artifacts: `d2s-<sha>.exe` + `d2-map-<sha>.map` on `main` (the linker map
is load-bearing for crash symbolization), `d2-<sha>.exe` + `d2-stripped-<sha>.exe` on
`jaenster-main`.

Prebuilt releases: [`D2LoD-Vanilla-Plus-0.0.3`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.3)
ships a 7z bundling the rebuilt client (`d2s-569fc5c.exe`), the `glide3x.dll` Vulkan
wrapper (Glide3xVK 0.4.5) and launcher/debug scripts — extract into a 1.14d install
(see the release notes or [`VANILLA-PLUS.md`](VANILLA-PLUS.md)). The wrapper line tags
its own releases separately.

## The same idea as Devilution

[Devilution](https://github.com/diasurgical/devilution) did this for the first Diablo: it
reconstructed the shipped `Diablo.exe` as source code that builds, and
[DevilutionX](https://github.com/diasurgical/DevilutionX) grew from it into the port many
people play Diablo with today. Redivivus — the recovered-source base of this fork — is
the same concept for Diablo II.

Two things differ:

- **The evidence.** Devilution could lean on debug symbols left on a PlayStation release
  for many of its names and types. No such symbols exist for 1.14d, so every name, type
  and structure here was reverse-engineered, cross-checked where possible against the
  game's own assert messages and against the Mac build.
- **The target.** Devilution worked toward a binary that matches the original byte for
  byte. Redivivus aims first for one that behaves like the original; byte-matching
  individual functions comes after.

## How it was made

This project started in 2021, well before large language models were any use for this kind of
work. The reverse-engineering of 1.14d (naming, typing and understanding the code) was
done by hand over years. Once LLMs became usable for it they sped the work up
considerably: proposing types, chasing crashes, and building the tooling below. They
joined an existing project; they did not start it.

1. The 1.14d `Game.exe` was reverse-engineered in [Ghidra](https://ghidra-sre.org/):
   every function named, typed and given its calling convention; every global given a
   type and an extent.
2. [ghidra-reconstruct](https://github.com/jaenster/ghidra-reconstruct) pulls the
   decompilation out through [ghidra-mcp](https://github.com/jaenster/ghidra-mcp),
   parses it into a C++ syntax tree, undoes the decompiler's artifacts, and emits this
   tree.
3. The tree is built, run, and every crash is traced back to its cause. That cause is
   fixed where it belongs, usually in the Ghidra database, sometimes in the generator,
   and only rarely by hand in the source.

## Status

- Every translation unit compiles with a strict `-std=c++17`, without `-fpermissive`,
  and the executable links with no undefined symbol.
- It starts, reaches the menus, and plays single player: create a character, enter the
  game, walk, fight, change areas.
- It is not finished. Some areas and features still crash. See
  [Known issues](#known-issues).

## You need

- Your own installation of Diablo II: Lord of Destruction, version 1.14. This repository
  contains no game data and no Blizzard binaries; the executable reads the MPQ archives
  from your installation. If you have no installation at hand,
  [blizzard-legacy-dl](https://github.com/jaenster/blizzard-legacy-dl) downloads the
  installers from Blizzard's own servers:

  ```sh
  blizzard-legacy-dl fetch D2DV -o ./d2     # Diablo II
  blizzard-legacy-dl fetch D2XP -o ./d2     # Lord of Destruction
  ```

  You still need your own CD keys to install.
- To build: a mingw-w64 cross compiler for 32-bit Windows (`i686-w64-mingw32-g++`), CMake
  and Ninja.
  - macOS: `brew install mingw-w64 cmake ninja`
  - Debian/Ubuntu: `apt install g++-mingw-w64-i686 cmake ninja-build`
  - Windows: an MSYS2 MinGW32 shell

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build
```

That produces `build/d2.exe`, a 32-bit Windows executable. `tools/check-build.sh` does
the same from a clean clone and checks the result; CI runs it on every push and uploads
the exe (plus, on this fork's branches, a stripped copy and the linker map) as workflow
artifacts — grab one from the Actions tab instead of building locally.

## Run

Two ways:

- **From a release** (recommended for play): extract the
  [`D2LoD-Vanilla-Plus-0.0.3.7z`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.3)
  into your Diablo II folder, run `Launch.cmd` to start the game with the Vulkan
  wrapper, and run `install_shaders.cmd` once for the libretro slang shader pack.
- **From a build**: copy `d2.exe` into your Diablo II folder, next to the original
  `Game.exe`, and start it from there:

```
d2.exe -w
```

`-w` runs it in a window; `-3dfx` selects the Glide path (needs the wrapper's
`glide3x.dll` in the same folder).

## Known issues

- **It crashes.** Not constantly, but a session of any length will probably hit one. Every
  crash so far has been a single wrong type or a lost value somewhere in the
  reconstruction, and each one fixed takes a class of others with it. A crash writes a
  report with a symbolised stack trace to `d2crash-<date>-<time>.txt` next to the
  executable; please attach it to an [issue](https://github.com/jaenster/redivivus/issues).
- **Battle.net does not work.** Logging in crashes in the version check. Single player
  is what works today.

## Future ideas

Other Diablo II code is known to exist and could be recovered the same way:

- **D2Server**, the game-server module that hosted closed Battle.net games. Recovered as
  source, it would give a realm game server built from the same code as the game itself.
- **The Mac build.** It carries code the Windows executable never had, most visibly an
  OpenGL renderer. [D2OpenGL](https://github.com/jaenster/D2OpenGL) already brings that
  renderer to 1.14d on Windows.

## Contributing

Changes are welcome as pull requests or patches against a release tag. Most of the tree
is regenerated, so a fix that belongs in the reverse-engineering (a wrong type, a missing
parameter) is best reported as an issue with the evidence; it will land upstream and
survive the next regeneration.

## Credits

- Much of the 1.14d reverse-engineering comes from [blizzhackers](https://blizzhackers.dev),
  dzik in particular.
- [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) supplied many of the structure layouts
  this work started from in 2021, though it went another direction since.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
