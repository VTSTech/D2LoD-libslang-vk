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
bootstrap + present. libslang-vk preset filtering is the P3 payoff (not wired
yet).

```
d2.exe ── LoadLibraryA("glide3x.dll") ──► glide3x.dll (this directory)
              45 Glide exports                │
              grDraw* / grTex* / grLfb*       ├─ P2 (done): Glide raster core
              grBufferSwap:                   │   paletted textures, combine
               compose buffer                 │   states, fan/strip/point
               + gamma                        │   raster, chroma, LFB, fills
               + version-tag overlay          ├─ present → Vulkan swapchain
               → queue submit                 └─ P3 (later): libslang-vk preset
                                                   chain at the present pass
```

`libslang-vk` is **embedded** at the present pass (P3) — it is not the DLL
itself (it doesn't speak Glide; it turns an input image into a preset-filtered
swapchain image).

## Layout

| Path | What |
|---|---|
| `src/glide_exports.cpp` | All 45 exports with exact signatures; the game-contract entry points (`grGet(0xF)`, `grSstWinOpen`, ini reading, session banner) |
| `src/g3x_frame.{h,cpp}` | The P2 raster core: texture cache (P_8 / ARGB_8332 / RGB_565), palettes, scanline span rasterizer (fan-quad, pointer-array strips/fans, points/lines), combine/blend/chroma semantics, fill-rect fast path, LFB, gamma LUT, compose + present seam |
| `src/g3x_state.{h,cpp}` | Glide state tracking (grChromakey/grColorCombine/... current values) |
| `src/g3x_overlay.{h,cpp}` | The version-tag overlay: `glide3xvk <version> (<build id>)` drawn top-right every frame |
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

## Build (same toolchain as the game)

From the repository root:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build --target glide3x
# → build/glide3x.dll (the root project builds it from glide3xvk/ when present;
#   a full `cmake --build build` also lands it beside d2.exe automatically)
```

CI (`.github/workflows/build.yml`) does exactly this on every push and uploads
the DLL as the `glide3x-dll` artifact.

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

- libslang-vk linkage (P3): the present pass is marked `TODO(P3)` in
  `src/vk_backend.cpp`; the 32-bit libslang-vk build spike is the gate
  (`BUILD-32BIT.md`). Preset config/hot-reload is P4.
- GPU-side raster (P3): the CPU scanline raster measures ~27 fps @ 800x600;
  moving the compose to the GPU is the P3 exit for the 60 fps target.
- Bilinear texture filtering (look delta only): the ground requests
  `texFilterMode(1,1)` and is point-sampled today.
