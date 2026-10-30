<img width="1282" height="747" alt="image" src="https://github.com/user-attachments/assets/fec6bdbb-aaad-462f-91e2-11b3ac3d0cc7" />

# D2LoD Vanilla+

[![Redivivus Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/MHK2Dg9)
[![VTSTech Discord](https://img.shields.io/badge/Vanilla%2B%20Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/XcJ8mSCNZ)

**D2LoD Vanilla+** is a ready-to-play Windows build of Diablo II: Lord of Destruction
1.14d, compiled from the recovered source in this repository and shipped as release 7z's
bundled with a Vulkan render wrapper (`glide3x.dll`). The base is
[jaenster's Redivivus](https://github.com/jaenster/redivivus) — the shipped `Game.exe`
taken apart function by function and turned back into C++ that builds — plus this fork's
growing set of crash fixes and opt-in quality-of-life features. Vanilla comes first: with
no config file the game behaves like retail, and every extra (Alt-key item labels,
portable saves, auto gold pickup, persistent globe text, more as they land) ships OFF and
is enabled in `d2vp.ini` — or in-game on the Vanilla+ Options page.

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

Prebuilt releases: [`D2LoD-Vanilla-Plus-0.0.5`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.5)
(2026-10-29) ships a 7z bundling the rebuilt client (`d2s-<sha>.exe`), the
`glide3x.dll` Vulkan wrapper (Glide3xVK 0.4.6) and launcher/debug scripts — extract
into a 1.14d install (see the release notes or [`VANILLA-PLUS.md`](VANILLA-PLUS.md)).
The wrapper line tags its own releases separately.

## Status

- Every translation unit compiles with a strict `-std=c++17`, without `-fpermissive`,
  and the executable links with no undefined symbol.
- Single player plays end to end on the Direct3D renderer — intro video and music,
  title, character select, loading, in-game, fullscreen or windowed. The
  fullscreen-D3D recovery (17 fix rounds) and the sound fixes (title music, SFX
  volume) are closed and field-verified; the known rendering defects (ground, sprites,
  palettes, shadows, lighting, UI text colors) are fixed.
- Opt-in features so far: Alt-key item-label toggle, portable saves (single-player
  saves pinned to the game folder), auto gold pickup (walk over gold piles to collect
  them, D2R "Gold Pickup" style — never moves your character, never fires in combat),
  persistent globe text (keeps the Life/Mana readout above the orbs visible without
  hovering — retail already prints it on hover), and the **Vanilla+ Options** page
  (Esc → Options → "Vanilla+ Options") that flips any of them in-game — click a row
  and it writes `d2vp.ini` and applies same-frame. All ship OFF — with no config file
  the game behaves like retail; the documented `d2vp.ini` template self-creates on
  first start.
- It is not finished. Some areas and features still crash. A crash writes a symbolised
  report to `d2crash-<date>-<time>.txt` next to the executable; see
  [Known issues](#known-issues).

### How close to the original

[`asm_matching/`](asm_matching/README.md) recompiles the tree with Visual C++ 2005 SP1, the
compiler that built `Game.exe`, and compares every function with the shipped 1.14d binary.
At this commit:

|Measure|Byte-identical to the original|
|-|-|
|Functions|3,889 of 15,521 (25.1%)|
|Code|227,755 of 2,678,537 bytes (8.50%)|

The rest compiles and runs but differs somewhere in its instructions. Each difference
that the same generator rule or database gap causes everywhere is fixed there, so the
count only grows.

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
the same from a clean clone and checks the result; CI runs it on every push to `main`
and uploads the artifacts you actually want: `d2s-<sha>.exe` (stripped, play/distribute)
plus `d2-map-<sha>.map` (the linker map used to symbolise crash addresses). On Windows,
`tools/build-local-win11.ps1` scripts the same build locally. Grab an artifact from the
Actions tab instead of building by hand.

## Run

Two ways:

- **From a release** (recommended for play): extract the
  [`D2LoD-Vanilla-Plus-0.0.5.7z`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.5)
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

With an AI agent you can read the Ghidra databases behind the tree (1.14d Windows and Mac)
through a read-only MCP server, and report such fixes with the evidence. In Claude Code:

```
claude plugin marketplace add jaenster/redivivus
claude plugin install redivivus@redivivus
```

The plugin brings the MCP server and the house rules; sign in with GitHub from `/mcp`. Other
agents: see https://redivivus.app/agent-setup/prompt.md

## Credits

- Much of the 1.14d reverse-engineering comes from [blizzhackers](https://blizzhackers.dev),
  dzik in particular.
- The byte-match oracle in `asm_matching/` is Niehztog's, and Nugwa628 found many of the
  difference classes it measures.
- [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) supplied many of the structure layouts
  this work started from in 2021, though it went another direction since.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
