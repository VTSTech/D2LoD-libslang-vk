# d2linux — the native Linux port (branch `linux`)

This is the **native Linux track**: a Linux ELF binary that *is* the game host —
no Windows, no Wine, no PE. It lives on its own branch so the main line can keep
its charter unchanged.

## Mission split (read this first)

| | Windows PE line (`main`) | Native Linux track (`linux`) |
|---|---|---|
| Artifact | `d2.exe`, 32-bit PE | `d2linux`, native ELF (i386 for game code) |
| Charter | byte-match the retail 1.14d Game.exe | **no byte-match possible** — a port, verified by behavior |
| Toolchain | mingw-w64 i686 (cross on ubuntu CI) | host gcc/clang + vendored SDL3 |
| Versioning | Vanilla+ R-rounds (`Vanilla+0.0.6-RXX` self-ID) | `d2linux` build id + milestone ladder below |
| Gates | `tools/check-vanilla-plus.sh` | added only when game TUs join the build (M1+) |

The two tracks share the decompiled game source; neither may break the other.
Any decomp translation-unit change made *for* Linux must be `#ifdef`-guarded and
must leave the PE build byte-identical (that is checked by the existing PE gates
and CI job, which keep running on this branch).

## Why i386 is the ship target

The decompiled logic assumes the 32-bit MSVC x86 ABI (`d2_platform.h` models it;
byte-annotated struct offsets carry embedded 4-byte pointers). On x86-64 every
pointer-bearing struct changes layout and the game state corrupts. So:

- `D2LINUX_ARCH=i386` (default): `-m32`, what game code requires. Needs a
  multilib toolchain (Debian/Ubuntu: `gcc-multilib g++-multilib`).
- `D2LINUX_ARCH=native`: host arch, for building/CI-verifying the host skeleton
  without multilib. Milestone M0 uses this; M1 switches to i386.

## Why SDL3

- Greenfield host layer written in 2026; SDL2 is maintenance-only and the SDL
  project ships `sdl2-compat` — SDL3 is the forward path.
- The D2 presentation model survives intact: INDEX8 palettized surfaces
  (`SDL_CreateSurfacePalette`, available since 3.2.0) + one streaming texture
  per present — exactly the DirectDraw surface/palette model.
- SDL3's device-stream audio maps DirectSound mixing directly.
- Wayland-first, zlib license (static linking unrestricted — same self-contained
  ethos as the PE's `-static` link).
- Vendored as submodule `external/SDL`, pinned to tag **release-3.4.18**
  (resolution order in `platform/linux/build.cmake`: `-DD2_SDL3_DIR=` →
  `external/SDL` submodule → system `find_package(SDL3)`; static preferred).

## Milestone ladder

- **M0 — scaffold (shipped)**: `platform/linux/` host skeleton — SDL3 window +
  renderer, INDEX8 staging surface + palette + streaming-texture present,
  letterboxed logical presentation, 22.05 kHz S16-stereo audio device stream
  (retail D2Sound shape) fed with silence, d2boot-style self-ID banner, and a
  headless `--smoke` self-test (forces the SDL `dummy` video/audio drivers,
  runs 3 frames, exit 0). CI runs `tools/check-build-linux.sh`: clean clone →
  submodule init → configure → build → smoke. PE line untouched.
- **M1 — game logic compiles on Linux i386**: the shim layer — Winsock→BSD
  sockets, SEH/VEH→signals (+ `libbacktrace` elf path), registry→file-backed,
  remaining CRT bits; the CMake source list grows game TUs; CI switches to
  `-DD2LINUX_ARCH=i386` with multilib.
- **M2 — title screen offline**: MPQ loading via the vendored Storm, one
  renderer path onto the M0 presentation surface (the M0 pattern producer is
  replaced by the game's composed 8-bit frame), input shim. Cinematics stubbed.
- **M3 — playable offline**: single player end to end; the DirectDraw/D2Sound
  shims complete; palette-ramp lighting verified visually.
- **M4 — codecs**: binkw32→ffmpeg-based shim (long pole), smackw32→libsmacker,
  ijl11→libjpeg shim.
- **M5 — networking**: full Winsock surface, Bnclient paths.

## Build

```sh
# one-time: vendor SDL3 (or clone anywhere and pass -DD2_SDL3_DIR=)
git submodule update --init --depth 1 external/SDL

# host skeleton without multilib (M0; what CI does)
cmake -S . -B build-linux -G Ninja -DD2LINUX_ARCH=native
cmake --build build-linux -j --target d2linux
./build-linux/d2linux --smoke          # headless self-test, exit 0
./build-linux/d2linux                  # 800x600 window, ESC quits

# game-code target (M1+): 32-bit, needs multilib
cmake -S . -B build-linux -G Ninja     # D2LINUX_ARCH=i386 is the default
```

SDL3 needs at least one desktop backend's dev packages on Linux (it hard-errors
otherwise; `SDL_UNIX_CONSOLE_BUILD=ON` skips that for container-only builds):
`libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev
libxss-dev libwayland-dev libxkbcommon-dev libegl1-mesa-dev libasound2-dev`.
The CI `linux-native` job installs this set.

Windows owner note: the skeleton runs under WSL2/WSLg (GUI apps just work);
`--smoke` runs anywhere, display or not.

## Rules for this track

1. Decomp TU edits are `#ifdef`-guarded, PE-identical when the guard is off —
   the PE gates + PE CI job are the referee and keep running here.
2. PE line files (`FEATURES.md`, `SMOKE-CHECKLIST.md`, `check-vanilla-plus.sh`,
   `Application.cpp` self-ID) are not touched for Linux-only work.
3. Everything new ships with its own verification (M0's is `--smoke`).
