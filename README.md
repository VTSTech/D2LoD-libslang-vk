<img width="997" height="776" alt="glide3xvk 0.3.1 rendering Diablo II: LoD 1.14d through Vulkan, version tag top-right" src="https://github.com/user-attachments/assets/7c3ee833-6425-4006-869b-ea3d564f35c9" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/ba3a6c67-0195-4581-82b5-d9208ef64661" />

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

## Status — 0.4.3

- The full game renders correctly through the Vulkan swapchain: menu, world, units,
  UI, shadows, text, cutscenes (field-verified rounds 4–12 on real hardware; see
  [glide3xvk/FIELD-TEST.md](glide3xvk/FIELD-TEST.md)).
- **The libslang-vk preset chain is live (0.4.0)**: point `preset=` in
  `d2slangvk.conf` at any libretro `.slangp` and every presented frame runs
  through it at native game resolution — hot-reload on file change, fit /
  integer / stretch scaling, unfiltered fallback on any failure.
- **The version stamp now lives on the game's own `Video:` debug line (0.4.1)**:
  at renderer init glide3xvk repoints the game's renderer-name string (`Glide`)
  at a static `glide3xvk 0.4.3 (<hash>)` tag — one 4-byte patch, fail-soft. The
  old top-right overlay box only draws if that patch fails (or `overlay=1`).
- **The overlay shows the active shader (0.4.2)**: while a preset filters
  frames, the fallback box's second line reads the preset's file name — pushed
  on chain ready (first open, conf change, hot reload), cleared whenever the
  chain dies; no preset = no line. Also fixed in 0.4.2: the 8x8 font's crushed
  descenders (`g p q j` — "can't see the bottom of the g in glide").
- **Chocolate-Doom-VK-style hotkeys (0.4.3)**: in-game **F11** cycles the
  content aspect (`4:3 -> 16:10 -> 16:9 -> auto`) and **F12** cycles the shader
  base resolution (`1x -> 2x -> 3x -> 4x`) — the frame is linearly upscaled N-x
  before the chain sees it, so source-relative pass FBOs (scanlines, glow)
  render finer. Toast feedback in the overlay, logged, session-only; both also
  settable via the new `aspect=` / `base_res=` conf keys (see the
  [d2slangvk.conf reference](#d2slangvkconf-reference) below).
- ~27 fps @ 800x600 from the CPU raster today (the preset chain adds a
  fullscreen pass on top); moving the compose buffer to the GPU is the
  remaining P3 exit item for the 60 fps target.
- Not wired yet, on purpose: GPU-side raster (P3 exit), bilinear texture
  filtering (a look delta only; the ground's `texFilterMode(1,1)` request is
  point-sampled).

## Install

No build required:

1. Get `glide3x.dll` — the
   [releases page](https://github.com/VTSTech/D2LoD-libslang-vk/releases)
   (0.3.1 there today) or the `glide3x-dll` artifact of the latest run in
   [Actions](https://github.com/VTSTech/D2LoD-libslang-vk/actions) (current
   code, preset chain included).
2. Copy it into your Diablo II folder, next to the original `Game.exe`
   (vanilla retail 1.14d). This repository contains no game data and no Blizzard
   binaries; the DLL only draws what the game hands it.
3. (Optional, the payoff) Copy `presets/d2slangvk.conf.sample` from the repo as
   `d2slangvk.conf` beside `Game.exe`, uncomment a `preset=` line, and every
   frame runs through that slang shader — hot-reload mid-game, `fit`/`integer`/
   `stretch` scaling, `aspect=`/`base_res=` geometry, F11/F12 live hotkeys,
   unfiltered fallback on any failure. No conf = the exact 0.3.x behavior.
   Every accepted value is documented in the
   [d2slangvk.conf reference](#d2slangvkconf-reference) below.
4. Launch: `Game.exe -w -3dfx` (`-w` windowed, `-3dfx` selects the Glide renderer).

A healthy run looks like: the menu renders, play works, and `glide3x.log` beside the
exe carries the session banner (`glide3xvk 0.4.3 <build-id>`), `drivertag: "Video:"
line patched -> "glide3xvk 0.4.3 (<hash>)"`, periodic `frame N:`
summaries, and a clean exit summary on shutdown. With a preset configured, the log
adds `slang: chain ready '<preset>' ...` and per-event slang lines; without one, it
says so once. If 32-bit Vulkan is unavailable
(`vulkan-1.dll` x86 missing), the log says so, the DLL still loads, and the game keeps
running — the DirectDraw path (`-w` without `-3dfx`) is untouched by all of this.

## The version stamp

The primary stamp is the game's own debug line. The `/fps` readout ends with
`Video: Glide` (drawn at fixed game coords from a hardcoded string); at renderer init
glide3xvk patches that string pointer to a static `glide3xvk 0.4.3 (<hash>)` tag, so
the line reads `Video: glide3xvk 0.4.3 (6f01f9a)` in the game's own font — no extra
draw, no collision. The patch is a single 4-byte store and strictly fail-soft: any
surprise (pool missing, ambiguous, page protect fails) is logged as `drivertag=miss`
and the fallback box below takes over untouched.

That fallback box is the original P2.10 overlay — `glide3xvk <ver> (<hash>)` drawn
every present into the **top-right** corner (8x8 bitmap font, 65%-black backdrop, 1px
drop shadow, post-gamma, never in the game's own framebuffer). With no ini key it
draws only when the patch missed; `overlay=1` forces it on (both stamps show the
same text), `overlay=0` hides it (re-read on every context open):

```ini
[glide3xvk]
overlay=1
```

## d2slangvk.conf reference

`d2slangvk.conf` lives beside `Game.exe` (same folder as `glide3x.dll` and
`glide3xvk.ini` — the two files are independent: the ini only carries the
overlay switch above, everything shader-related lives in the conf). It is read
once at renderer init and re-checked every 32 frames while a chain filters:
edit the conf or the `.slangp` it points at in place and the chain reloads
within ~half a second, no game restart.

Syntax: a `[slangvk]` section header, `key=value` lines, `;` or `#` comment to
end of line, blank lines ignored, later assignments win. Unknown keys are
ignored (forward compatibility); a malformed value keeps the previous/default
one and never fails the session. No file at all — or no `preset=` line — means
the chain stays disabled and frames present unfiltered, exactly the 0.3.x look.

Every key (0.4.3):

| Key | Values (default first) | What it does |
|-----|------------------------|--------------|
| `preset=` | *(empty)* = off, or a path to a `.slangp` | The libretro slang preset every presented frame runs through. Relative paths resolve against the game exe folder; drive-letter (`F:\...`) and UNC paths work as-is. Empty or missing line = chain disabled. Changing it (in the file) triggers a full preset reload. |
| `scale=` | `fit` \| `integer` \| `stretch` | How the chain's output sits in the window: `fit` fits + centers with letterbox; `integer` picks the largest whole-number multiple of the game resolution, centered; `stretch` fills the window edge to edge. Ignored while an explicit `aspect=` (or an F11 pick) is active. |
| `vsync=` | *(empty)* = FIFO \| `0` \| `1` | Present mode of the swapchain the chain renders into: empty = default FIFO (vsync on); `0` prefers IMMEDIATE (uncapped, silently falls back to FIFO where unsupported); `1` = FIFO. Takes effect when the swapchain is (re)created — game start or resolution change. |
| `aspect=` | `auto` \| `4:3` \| `16:10` \| `16:9` | Content-aspect override (0.4.3). `auto` keeps the `scale=` geometry. A fixed ratio letterboxes the chain output into the largest centered rectangle of that ratio inside the window: `4:3` on a 16:9 window = pillarbox with correct proportions; `16:9` on a 16:9 window = full-bleed. F11 cycles this live. |
| `base_res=` | `1x` \| `2x` \| `3x` \| `4x` (bare `1`..`4` accepted) | Shader base resolution (0.4.3). The game frame is linearly upscaled N-x on the GPU before the chain sees it, so every source-relative pass FBO renders at N-x — scanline pitch, glow and blur radii get finer as N rises; the final pass is fit back into the output rect. Costs one extra fullscreen blit. F12 cycles this live. |

A worked example (the field setup from the round-16 screenshots):

```ini
[slangvk]
preset=shaders/crt/crtglow_lanczos.slangp
scale=stretch
vsync=
aspect=auto
base_res=1x
```

### Hotkeys (in-game, 0.4.3)

While a preset chain filters frames and the game window has focus — the same
scheme as Chocolate-Doom-VK:

- **F11** — cycle content aspect: `4:3 -> 16:10 -> 16:9 -> auto -> ...`
- **F12** — cycle shader base resolution: `1x -> 2x -> 3x -> 4x -> 1x`

Each press shows a toast line in the overlay box (`aspect 16:9`,
`shader base 2x`) for ~2 seconds and logs a `slang: F11/F12 ...` line. Both
picks are **session-only**: they are never written back to `d2slangvk.conf`,
and a conf hot-reload snaps them back to whatever the file says. With no
preset configured the keys do nothing and fall through to the game untouched.

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

- **P3 (shipped, 0.4.0)** — the libslang-vk preset chain at the present pass
  (`d2slangvk.conf`, hot-reload, scaling modes, fail-soft); remaining exit
  item: GPU-side raster for the 60 fps target
- **P4** — preset parameter UI, CI-packaged drop-in zip (DLL + presets +
  conf), crash-reporter hook
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
