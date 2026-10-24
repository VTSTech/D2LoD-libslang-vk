# glide3xvk — our own `glide3x.dll` for D2LoD (Glide 3 → Vulkan)

The game's Glide loader (`D2Client/Renderer/Glide.cpp`) does
`LoadLibraryA("glide3x.dll")` and resolves **45 stdcall exports**. This
directory builds a drop-in replacement for that DLL: it exports exactly those
45 entry points, implements the Glide 3 state machine and raster semantics D2
actually uses, and presents the frame through a Vulkan swapchain. It runs on a
**vanilla retail 1.14d install** (`Game.exe -3dfx`) — a rebuilt `d2.exe` is
not required.

Status: the game renders correctly through it — menu, world, units, UI,
shadows, text, cutscenes (field-verified over rounds 4–12; see
`FIELD-TEST.md`). The CPU raster feeds a BGRA compose buffer; the GPU side is
bootstrap + present. **P3 shipped (0.4.0): the libslang-vk preset chain sits
at the present pass** — set `preset=` in `d2slangvk.conf` and every presented
frame runs through the slang preset at up-scaled resolution (field round
pending).

```
d2.exe ── LoadLibraryA("glide3x.dll") ──► glide3x.dll (this directory)
              45 Glide exports                │
              grDraw* / grTex* / grLfb*       ├─ P2 (done): Glide raster core
              grBufferSwap:                   │   paletted textures, combine
               compose buffer                 │   states, fan/strip/point
               + gamma                        │   raster, chroma, LFB, fills
               + version-tag overlay          ├─ P3 (done, 0.4.0): preset chain
               → queue submit                 │   slangvk_image (game res) →
                                              │   slangvk_chain_render_swapchain
                                              │   (d2slangvk.conf, hot-reload)
                                              └─ present → Vulkan swapchain
```

`libslang-vk` is **embedded** at the present pass — it is not the DLL itself
(it doesn't speak Glide; it turns an input image into a preset-filtered
swapchain image). The DLL links the static `slangvk` target and adopts our
own Vulkan context (`slangvk_device_adopt`); without `d2slangvk.conf` the
chain never engages and the DLL behaves exactly like 0.3.x.

## Layout

| Path | What |
|---|---|
| `src/glide_exports.cpp` | All 45 exports with exact signatures; the game-contract entry points (`grGet(0xF)`, `grSstWinOpen`, ini reading, session banner) |
| `src/g3x_frame.{h,cpp}` | The P2 raster core: texture cache (P_8 / ARGB_8332 / RGB_565), palettes, scanline span rasterizer (fan-quad, pointer-array strips/fans, points/lines), combine/blend/chroma semantics, fill-rect fast path, LFB, gamma LUT, compose + present seam |
| `src/g3x_state.{h,cpp}` | Glide state tracking (grChromakey/grColorCombine/... current values) |
| `src/g3x_overlay.{h,cpp}` | The version-tag overlay: `glide3xvk <version> (<build id>)` drawn top-right every frame |
| `src/g3x_slang.{h,cpp}` | P3: the libslang-vk preset chain — adopts our Vulkan context, parses `d2slangvk.conf`, owns the input image + preset + chain, hot-reloads on file change, fails soft to the unfiltered path |
| `presets/` | Two starter presets (scanline, sharpen — MIT, ours) + `d2slangvk.conf.sample` |
| `src/g3x_font8x8.h` | Embedded 8x8 1-bit bitmap font for the overlay (generated from DejaVu Sans Mono Bold) |
| `src/g3x_log.{h,cpp}` | `glide3x.log` beside the game exe (`%TEMP%` fallback), log-once budgets, periodic frame summaries, exit summary |
| `src/g3x_types.h` | Minimal Glide type surface (mirror of the game's `d2_platform.h` block) + values observed in the in-tree `D2Glide` call sites |
| `src/vk_backend.{h,cpp}` | Vulkan bootstrap: volk → instance → win32 surface → device → swapchain → present; fail-soft everywhere |
| `glide3x.def` | The 45 decorated export names (`_grBufferSwap@4`...), repo `binkw32.def` syntax |
| `vendor/volk/` | volk (runtime Vulkan loader, MIT — loads `vulkan-1.dll`, no SDK needed) |
| `vendor/Vulkan-Headers/` | Khronos Vulkan C headers (Apache-2.0) |
| `BUILD-32BIT.md` | **P1 spike notes**: building libslang-vk itself as i686 Windows |
| `FIELD-TEST.md` | The field log: per-round builds, diagnostics, and results on a vanilla retail 1.14d install |

## The overlay

Every present draws one line — `glide3xvk 0.3.1 (<build id>)` — into the
**top-right** corner (4px from the edge): 8x8 bitmap font, 65%-black backdrop,
1px drop shadow. It is rendered post-gamma at swapchain resolution, so it is
identical in the 640x480 probe phases, the 800x600 menu, gameplay, and the
LFB/Bink video path, and it never enters the game's own framebuffer (fade and
screenshot logic cannot see it). The corner choice is deliberate: the game's
own `/fps` readout draws top-left, and both should be visible at once.

`glide3xvk.ini` beside `Game.exe` can turn it off (re-read on every context
open):

```ini
[glide3xvk]
overlay=0
```

## The preset chain (P3, 0.4.0)

Every present, the gamma-corrected frame is uploaded at its **native game
resolution** into a `slangvk_image`, and `slangvk_chain_render_swapchain`
draws the preset chain straight into the acquired swapchain image — so the
preset's own scaling algorithms see the true source size (640x480 in,
whatever the window is out). The version-tag overlay is drawn onto the chain
input, so it filters along with the frame (screenshots show what the player
sees). The scaling/vsync/overlay relationship:

- `scale=fit` — the chain fit-and-centers (letterbox). `scale=integer` — we
  compute the largest whole-number multiple of the game resolution that fits
  and hand the chain a centered rect. `scale=stretch` — full-rect.
- `vsync=` — swapchain present mode (FIFO default; `0` prefers IMMEDIATE).
- The overlay box anchors top-right exactly as in P2 (see above); with a
  preset on it warps/filters with everything else.

`d2slangvk.conf` beside `Game.exe` (copy it from
`presets/d2slangvk.conf.sample`):

```ini
[slangvk]
preset=presets/glide3xvk-scanline.slangp   ; or an absolute path into your
                                            ; RetroArch shaders (CRT-Royale)
scale=fit                                  ; fit | integer | stretch
vsync=                                     ; empty = FIFO, 0 = immediate
fallback=unfiltered                        ; preset failure -> raw frame
```

Behavior contract:

- **No conf / no `preset=` line = the chain never engages.** Identical pixels
  and logs to 0.3.x (the only log line is a one-shot "no preset" note).
- **Hot reload**: the conf and the preset file are stat'ed every 32 frames;
  any change reloads the preset and rebuilds the chain mid-session (a few
  frames of unfiltered present during the swap).
- **Fail-soft**: preset parse/compile/render failure logs the slang error and
  downgrades to the unfiltered path (the whole session after 3 consecutive
  render failures); the game never crashes on a bad preset.
- Starter presets (ours, MIT): `presets/glide3xvk-scanline.slangp` (the
  zero-risk smoke test — image identical apart from darkened alternate source
  lines) and `presets/glide3xvk-sharpen.slangp` (mild unsharp mask). Real CRT
  chains (CRT-Royale and friends) load from a RetroArch `shaders/` checkout
  by absolute or game-relative path.

## Build (same toolchain as the game)

From the repository root, with a **libslang-vk checkout beside the repo**
(the P3 preset chain links its static `slangvk` target; see `BUILD-32BIT.md`
for the i686 verification of that build):

```sh
git clone https://github.com/VTSTech/libslang-vk ../libslang-vk   # once
```

Without the checkout the DLL still builds — with the chain stubbed out
(same API, always inactive) and a loud CMake STATUS line saying so.

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build --target glide3x
# → build/glide3x.dll (the root project builds it from glide3xvk/ when present;
#   a full `cmake --build build` also lands it beside d2.exe automatically)
```

CI (`.github/workflows/build.yml`) does exactly this on every push and uploads
the DLL as the `glide3x-dll` artifact; it checks out `VTSTech/libslang-vk`
pinned (`ref: d208ac5`) so the chain builds reproducibly.

Verify the export surface against the game's own loader (must be an empty diff):

```sh
i686-w64-mingw32-objdump -p build/glide3x.dll \
  | sed -n '/Ordinal\/Name Pointer/,$p' \
  | grep -oE '_[A-Za-z]+@[0-9]+' | sort -u > /tmp/ours.txt
grep -oE '"_[A-Za-z]+@[0-9]+"' D2Client/Renderer/Glide.cpp | tr -d '"' | sort -u > /tmp/game.txt
diff /tmp/game.txt /tmp/ours.txt && echo "MATCH: all 45 GetProcAddress names covered"
```

> Toolchain note: GNU ld strips one leading `_` from every export name and
> searches def-file internals with an extra one. The scaffold defeats both:
> functions carry `__asm__("__name@N")` labels (double underscore → table gets
> single), `glide3x.def` lists the literal names, and `-Wl,-u` flags force the
> object archive member in for the def lookups. See the CMakeLists comment.

The root `CMakeLists.txt` picks this directory up automatically and keeps it
out of the `d2` glob — delete the directory and the game builds exactly as
before (zero game-source drift).

The DLL is self-contained (`-static-libgcc -static-libstdc++`, volk loaded at
runtime): KERNEL32 + msvcrt imports only. The build id embedded in the log
banner, `grGetString(GR_RENDERER)` and the overlay comes from
`git describe --always --dirty --tags` at compile time — build from a clean
checkout of the pushed branch and the tag names a commit you can find on
GitHub.

## Test procedure (field)

1. Copy `build/glide3x.dll` into the game root, next to the game exe.
2. Launch the game with `-3dfx` — that selects the Glide renderer
   (command line arg or `[VIDEO] 3DFX=1` in the ini). Works on a
   **vanilla retail 1.14d install** (`Game.exe -w -3dfx` windowed on the field
   rig). Full per-round walkthrough and history: `FIELD-TEST.md`.
3. A healthy run looks like: the menu renders, play works, and
   `glide3x.log` beside the exe carries the session banner
   (`glide3xvk 0.3.1 <build-id>`), periodic `frame N:` summaries and a clean
   exit summary on shutdown.
4. The version tag in the top-right corner identifies the DLL in any
   screenshot; `/fps` in the game stays readable top-left.

## Fail-soft behavior

If 32-bit Vulkan is unavailable (`vulkan-1.dll` x86 missing — see R2), the
log says so, the DLL still loads, and the game keeps running (black window).
The DirectDraw path (`-w`) is untouched by all of this.

## What is deliberately NOT here yet

- GPU-side raster (P3 exit for the 60 fps target): the CPU scanline raster
  measures ~27 fps @ 800x600, and the preset chain adds a fullscreen pass on
  top of that — the compose-to-GPU move is now the main remaining perf item.
- Bilinear texture filtering (look delta only): the ground requests
  `texFilterMode(1,1)` and is point-sampled today.
- Preset parameter UI / per-preset geometry defaults (P4): parameters are
  reachable through `slangvk_chain_set_parameter` but nothing drives them yet.
