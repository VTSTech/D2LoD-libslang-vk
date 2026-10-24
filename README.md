<img width="997" height="776" alt="glide3xvk 0.3.1 rendering Diablo II: LoD 1.14d through Vulkan, version tag top-right" src="https://github.com/user-attachments/assets/7c3ee833-6425-4006-869b-ea3d564f35c9" />

# D2LoD-libslang-vk

[![Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/XcJ8mSCNZ)

A drop-in **`glide3x.dll`** for **Diablo II: Lord of Destruction 1.14d** that renders the
game through **Vulkan** — with libretro slang shader presets (`.slangp`) routed through
**libslang-vk** as the goal of this branch.

When the game is started with `-3dfx`, its Glide loader
(`D2Client/Renderer/Glide.cpp`) does `LoadLibraryA("glide3x.dll")` and resolves
**45 stdcall exports**. This branch builds a replacement for exactly that DLL: it
exports those 45 entry points, implements the Glide 3 state machine and raster
semantics D2 actually uses, and presents every frame through a Vulkan swapchain.
It runs on a **vanilla retail 1.14d install** — copy the DLL next to `Game.exe`,
no rebuilt executable, no game-source changes. The payoff pattern — filtering the
presented frame through a user-chosen preset (CRT-Royale and friends) at up-scaled
resolution — is the same one proven in
[DevilutionX-libslang-vk](https://github.com/VTSTech/DevilutionX-libslang-vk)
(Diablo/Hellfire) and
[chocolate-doom-vk](https://github.com/VTSTech/chocolate-doom-vk) (Doom).

## Status — 0.3.1

- The full game renders correctly through the Vulkan swapchain: menu, world, units,
  UI, shadows, text, cutscenes (field-verified rounds 4–12 on real hardware; see
  [glide3xvk/FIELD-TEST.md](glide3xvk/FIELD-TEST.md)).
- ~27 fps @ 800x600 from the CPU raster today; moving the compose buffer to the GPU
  is the P3 exit for the 60 fps target.
- Not wired yet, on purpose: the libslang-vk preset chain (P3 — marked `TODO(P3)` in
  `glide3xvk/src/vk_backend.cpp`), GPU-side raster (P3), bilinear texture filtering
  (a look delta only; the ground's `texFilterMode(1,1)` request is point-sampled).

## Install

No build required:

1. Get `glide3x.dll` — either the
   [0.3.1 release](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/0.3.1)
   (pre-release) or the `glide3x-dll` artifact of the latest run in
   [Actions](https://github.com/VTSTech/D2LoD-libslang-vk/actions).
2. Copy it into your Diablo II folder, next to the original `Game.exe`
   (vanilla retail 1.14d). This repository contains no game data and no Blizzard
   binaries; the DLL only draws what the game hands it.
3. Launch: `Game.exe -w -3dfx` (`-w` windowed, `-3dfx` selects the Glide renderer).

A healthy run looks like: the menu renders, play works, and `glide3x.log` beside the
exe carries the session banner (`glide3xvk 0.3.1 <build-id>`), periodic `frame N:`
summaries, and a clean exit summary on shutdown. If 32-bit Vulkan is unavailable
(`vulkan-1.dll` x86 missing), the log says so, the DLL still loads, and the game keeps
running — the DirectDraw path (`-w` without `-3dfx`) is untouched by all of this.

## The overlay

Every present draws one line — `glide3xvk 0.3.1 (<build id>)` — into the **top-right**
corner (4px from the edge): 8x8 bitmap font, 65%-black backdrop, 1px drop shadow. The
corner is deliberate: the game's own `/fps` readout draws top-left, and both should be
visible at once. The tag renders post-gamma at swapchain resolution and never enters
the game's own framebuffer, so fades and screenshots cannot see it. Turn it off with
`overlay=0` under `[glide3xvk]` in `glide3xvk.ini` (re-read on every context open):

```ini
[glide3xvk]
overlay=0
```

## Build from source

```sh
apt install g++-mingw-w64-i686 binutils-mingw-w64-i686 cmake ninja-build   # Debian/Ubuntu

cmake -S . -B build-g3x -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build-g3x --target glide3x
# → build-g3x/glide3x.dll (pei-i386; KERNEL32 + msvcrt imports only)
```

CI (`.github/workflows/build.yml`) builds exactly this on every push to `main` or
`libslang-vk` and uploads the DLL as the `glide3x-dll` workflow artifact. The build id
embedded in the log banner, `grGetString(GR_RENDERER)` and the overlay comes from
`git describe --always --dirty --tags` at compile time — build from a clean checkout
of the pushed branch and the tag names a commit you can find on GitHub.

Verify the export surface against the game's own loader (must be an empty diff):

```sh
i686-w64-mingw32-objdump -p build-g3x/glide3x.dll \
  | sed -n '/Ordinal\/Name Pointer/,$p' \
  | grep -oE '_[A-Za-z]+@[0-9]+' | sort -u > /tmp/ours.txt
grep -oE '"_[A-Za-z]+@[0-9]+"' D2Client/Renderer/Glide.cpp | tr -d '"' | sort -u > /tmp/game.txt
diff /tmp/game.txt /tmp/ours.txt && echo "MATCH: all 45 GetProcAddress names covered"
```

## Documentation

- [glide3xvk/README.md](glide3xvk/README.md) — the renderer's architecture, module
  layout, build, toolchain notes (GNU ld export decoration), overlay internals,
  field-test procedure and fail-soft behavior
- [glide3xvk/FIELD-TEST.md](glide3xvk/FIELD-TEST.md) — the per-round field log:
  builds, diagnostics and results on a vanilla retail 1.14d install
- [glide3xvk/BUILD-32BIT.md](glide3xvk/BUILD-32BIT.md) — building libslang-vk itself
  as i686 Windows (the P3 gate)
- [PLAN.md](PLAN.md) — the P1–P5 integration plan with the phase ledger

## Roadmap

- **P3** — wire the libslang-vk preset chain into the present pass, and move the
  compose buffer to the GPU (60 fps target)
- **P4** — preset configuration / hot-reload, CI-packaged drop-in zip
- Deferred look item: bilinear texture filtering

## Upstream

This branch is part of a fork whose `master` line tracks the upstream 1.14d source
recovery; the recovered game source stays untouched here — glide3xvk needs zero
game-source edits, and the DLL is designed to run against the vanilla retail exe.
While testing, upstream PRs we may discover (recovery fixes, build or diagnostics
material) will be noted and, where they hold up, sent upstream — but the work on this
branch is glide3xvk, and glide3x.dll loading Vulkan / libretro slang with libslang-vk
is the goal.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
