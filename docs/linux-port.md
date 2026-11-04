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
must leave the PE build byte-identical (that is checked by the PE gates, run as a
manual audit, and by the owner's local Win11 llvm-mingw build — CI on this branch
is the linux-native job only, the mingw job having been retired).

## Why i386 is the ship target

The decompiled logic assumes the 32-bit MSVC x86 ABI (`d2_platform.h` models it;
byte-annotated struct offsets carry embedded 4-byte pointers). On x86-64 every
pointer-bearing struct changes layout and the game state corrupts. So:

- `D2LINUX_ARCH=i386` (default): `-m32`, what game code requires. Needs a
  multilib toolchain (Debian/Ubuntu: `gcc-multilib g++-multilib
  libc6-dev-i386` — the last one is the actual provider of the 32-bit crt
  objects and headers).
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
- **M1 — game logic compiles on Linux i386 (sandbox-green)**: the shim layer —
  Winsock→BSD sockets, SEH/VEH→signals (the crash twin over /proc + dladdr),
  registry→file-backed under `~/.config/d2linux/`, the MSVC CRT bits, and the
  inventory pass's full surface (platform/linux/INVENTORY.md). The slice: 44
  game TUs (Fog/Storm foundation + PKWARE). CI builds `-DD2LINUX_ARCH=i386`
  with multilib and configures SDL console-only (runners have no `:i386` X11
  dev libraries; the smoke uses the dummy drivers anyway). CI rounds 1–2
  (2026-11-04) died inside `check_cxx_compiler_flag(-m32)` while a plain
  `gcc -m32` linked fine in the same job: the module passes the flag through
  the try_compile child's `add_definitions` — compile-only — so the child
  links a 32-bit object against 64-bit crt and false-fails working multilib.
  The gate is a raw try_compile probe now (its `COMPILE_DEFINITIONS` route
  reaches compile AND link), FATALing with the compiler's own verdict when
  the toolchain genuinely cannot do i386.
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

The banner self-IDs the shape: `milestone 0 (skeleton + shims, native arch)`
vs `milestone 1 (skeleton + shims + game slice: 44 TUs, i386)`. M1 is a
link-time milestone — the game TUs join the binary but nothing calls them
until M2's boot chain — so the screen cannot show it; the banner does, with
the slice TU count computed from the build itself.

SDL3 needs at least one desktop backend's dev packages on Linux (it hard-errors
otherwise; `SDL_UNIX_CONSOLE_BUILD=ON` skips that for container-only builds):
`libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev
libxss-dev libxtst-dev libwayland-dev libxkbcommon-dev libdecor-0-dev
libegl1-mesa-dev libasound2-dev`. The CI `linux-native` job installs this set.
Two of these behave differently at configure time: `libxtst-dev` (XTEST) is a
HARD dep (SDL3 fatal-errors without it), while `libdecor-0-dev` (Wayland
client-side decorations) is SOFT — a warning only, and the binary then ships
without decoration support: bare windows on GNOME/Weston Wayland (KDE is
unaffected; Mutter provides no server-side fallback).

Runtime requirements (running the binary, not building): SDL3 is statically
linked, and X11/Wayland/ALSA are dlopen'd — no -dev packages needed. GNOME
Wayland sessions need `libdecor-0-0` + `libdecor-0-plugin-1-gtk` for a
titlebar/resize; on any session `SDL_VIDEODRIVER=x11 ./d2linux` forces the
WM-decorated X11 path.

Windows owner note: the skeleton runs under WSL2/WSLg (GUI apps just work);
`--smoke` runs anywhere, display or not.

## Troubleshooting: `No available video device`

`./d2linux` needs a display; `--smoke` does not (it forces the dummy drivers).
When `SDL_Init(SDL_INIT_VIDEO)` fails, the binary prints its own diagnosis -
the drivers compiled into THIS binary, the session environment, and the fix
that follows from the combination. `./d2linux --list-video` prints the same
report without running. The four verdicts:

- **`video drivers compiled in: dummy offscreen`** — this binary is
  console-only: either a CI artifact (the i386 job builds
  `SDL_X11=OFF SDL_WAYLAND=OFF` by design — no :i386 X11/Wayland dev
  libraries exist) or a local configure without the dev libraries. It cannot
  open a window on any machine. Fix: install the dev set from Build above and
  reconfigure on a **fresh** build dir: `cmake -S . -B build-native -G Ninja
  -DD2LINUX_ARCH=native`.
- **`kmsdrm` only** — the binary draws on the Linux console (a real VT), not
  inside a desktop session. Run it from a TTY (may need video-group/root for
  DRM master), or rebuild with the X11/Wayland dev set.
- **`x11`/`wayland` compiled in, `DISPLAY`/`WAYLAND_DISPLAY` unset** — no
  display session in this shell: run from a desktop session, or `ssh -X`, or
  WSLg (Windows 11) / an X server on the Windows side with DISPLAY set.
- **`x11`/`wayland` compiled in, session present** — force the backend the
  session actually runs: `SDL_VIDEODRIVER=x11 ./d2linux` (X11/XWayland) or
  `SDL_VIDEODRIVER=wayland ./d2linux`.

Configure-time companion: when the vendored SDL3 lands without a desktop
backend, cmake warns in the same words (search the configure log for
"WITHOUT a desktop video backend"). CI logs record the report of every build
via `check-build-linux.sh` → `--list-video`.

## Rules for this track

1. Decomp TU edits are `#ifdef`-guarded, PE-identical when the guard is off —
   the PE gates (manual audit) + the owner's local Win11 build are the referee;
   CI on this branch is the linux-native job only.
2. PE line files (`FEATURES.md`, `SMOKE-CHECKLIST.md`, `check-vanilla-plus.sh`,
   `Application.cpp` self-ID) are not touched for Linux-only work.
3. Everything new ships with its own verification (M0's is `--smoke`).
