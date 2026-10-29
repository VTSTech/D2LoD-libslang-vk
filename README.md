# Redivivus

[![Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/MHK2Dg9)
[![redivivus.app](https://img.shields.io/badge/redivivus.app-site-8f2b2b)](https://redivivus.app)

Diablo II: Lord of Destruction 1.14d, recovered as C++ source that builds.

The Ghidra databases behind the tree, 1.14d Windows and Mac, are open to read for anyone
with an AI agent; see [Contributing](#contributing).

*Redivivus* is Latin for a thing restored to life. This is not a reimplementation that
behaves like the game. It is the shipped `Game.exe`, taken apart function by function,
turned back into source code, and compiled into a working executable again.

It is the **whole** game. Up to 1.13 Diablo II shipped as a small `Game.exe` plus a dozen
DLLs (`D2Client`, `D2Common`, `D2Game`, `D2Win`, `D2gfx`, `Storm`, `Fog`, ...). Patch 1.14
linked all of them into one `Game.exe`, and that single executable is what is recovered
here: the client, the game server that single player runs in the same process, map
generation, the renderers, sound, networking, Battle.net, and Blizzard's Storm and Fog
libraries. The source tree keeps those module names as its top-level directories.

The code is machine-generated from a reverse-engineered database and then corrected by
hand where needed. It is not the original source: the names, types and structure are
reconstructions, and some functions read like a decompiler wrote them, because one did.

It is [Devilution](https://github.com/diasurgical/devilution)'s idea applied to Diablo II,
without the debug symbols Devilution had: every name and type here was reverse-engineered.
How that differs, and how the project was made since 2021:
[redivivus.app/about](https://redivivus.app/about#devilution).

## Status

- Every translation unit compiles with a strict `-std=c++17`, without `-fpermissive`,
  and the executable links with no undefined symbol.
- It starts, reaches the menus, and plays single player: create a character, enter the
  game, walk, fight, change areas.
- It is not finished. Some areas and features still crash. See
  [Known issues](#known-issues).

### How close to the original

[`asm_matching/`](asm_matching/README.md) recompiles the tree with Visual C++ 2005 SP1, the
compiler that built `Game.exe`, and compares every function with the shipped 1.14d binary.
At this commit:

|Measure|Byte-identical to the original|
|-|-|
|Functions|3,985 of 15,521 (25.7%)|
|Code|236,646 of 2,678,537 bytes (8.83%)|

The rest compiles and runs but differs somewhere in its instructions. Each difference
that the same generator rule or database gap causes everywhere is fixed there, so the
count only grows.

## Build and run

You need your own Diablo II: Lord of Destruction 1.14 installation; this repository holds no
game data. With a mingw-w64 cross compiler for 32-bit Windows, CMake and Ninja:

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build
```

Copy `build/d2.exe` next to `Game.exe` in your Diablo II folder and run `d2.exe -w`.

- [docs/building.md](docs/building.md): getting the game, the toolchain per platform.
- [docs/original-compilers.md](docs/original-compilers.md): building with the compilers that
  built 1.14d, Visual C++ 2005 SP1 and Apple LLVM 4.2, and their flags.

## Known issues

- **It crashes.** Not constantly, but a session of any length will probably hit one. Every
  crash so far has been a single wrong type or a lost value somewhere in the
  reconstruction, and each one fixed takes a class of others with it. A crash writes a
  report with a symbolised stack trace to `d2crash-<date>-<time>.txt` next to the
  executable; please attach it to an [issue](https://github.com/jaenster/redivivus/issues).
- **Battle.net does not work.** Logging in crashes in the version check. Single player
  is what works today.

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
agents: see https://redivivus.app/agent-setup/prompt.md. Corrections go in as proposals that are
checked against the databases, reviewed, and applied with you as the git author; see
[how proposals work](https://redivivus.app/about#contributing).

## Credits

- Much of the 1.14d reverse-engineering comes from [blizzhackers](https://blizzhackers.dev),
  dzik in particular.
- The byte-match oracle in `asm_matching/` is Niehztog's, and Nugwa628 found many of the
  difference classes it measures.
- [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) supplied many of the structure layouts
  this work started from in 2021, though it went another direction since.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
