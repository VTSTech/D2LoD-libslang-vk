# glide3xvk — our own `glide3x.dll` for D2LoD (Glide 3 → Vulkan → libslang-vk)

This is the P1 scaffold from `PLAN.md` (section 5). The game's Glide loader
(`D2Client/Renderer/Glide.cpp`) does `LoadLibraryA("glide3x.dll")` and resolves
**45 stdcall exports** — this directory builds a DLL exporting exactly those,
implements the acceptance contract, and boots a Vulkan swapchain. The frame is
cleared **black** and presented: nothing is rasterized yet (by design).

```
d2.exe ── LoadLibraryA("glide3x.dll") ──► glide3x.dll (this directory)
              45 Glide exports                │
              grDraw* / grTex* / grLfb*       ├─ P2 (later): Glide→Vulkan raster
                                              │    (paletted textures, combine
                                              │     states, vertex arrays)
                                              └─ grBufferSwap:
                                                   P1: clear black + present
                                                   P3 (later): slangvk_chain_
                                                   render_swapchain → presets
```

`libslang-vk` is **embedded** at the present pass (P3) — it is not the DLL
itself (it doesn't speak Glide; it turns an input image into a preset-filtered
swapchain image).

## Layout

| Path | What |
|---|---|
| `src/glide_exports.cpp` | All 45 exports with exact signatures; `grGet(0xF)`/`grGetString`/`grSstWinOpen`/`grBufferSwap` carry the real contract |
| `src/vk_backend.{h,cpp}` | Vulkan bootstrap: volk → instance → win32 surface → device → swapchain → clear-black present; fail-soft everywhere |
| `src/g3x_state.{h,cpp}` | Glide state tracking (feeds the P2 pipeline-variant cache later) |
| `src/g3x_log.{h,cpp}` | `glide3x.log` beside the game exe (`%TEMP%` fallback), log-once budgets, periodic summaries |
| `src/g3x_types.h` | Minimal Glide type surface (mirror of the game's `d2_platform.h` block) + values observed in the in-tree `D2Glide` call sites |
| `glide3x.def` | The 45 decorated export names (`_grBufferSwap@4`...), repo `binkw32.def` syntax |
| `vendor/volk/` | volk (runtime Vulkan loader, MIT — loads `vulkan-1.dll`, no SDK needed) |
| `vendor/Vulkan-Headers/` | Khronos Vulkan C headers (Apache-2.0) |
| `BUILD-32BIT.md` | **P1 spike notes**: building libslang-vk itself as i686 Windows |
| `FIELD-TEST.md` | Step-by-step P1 field test on a **vanilla retail 1.14d install** (`Game.exe -3dfx`) — no rebuilt d2.exe required |

## Build (same toolchain as the game)

```sh
# Ubuntu cross: the exact CI deb set, extracted as in scripts/setup-toolchain.sh
export PATH=/home/z/.local/opt/venv/bin:/home/z/.local/opt/mingw-wrappers:$PATH
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DD2_TOOL_PREFIX=d2w-
cmake --build build --target glide3x
# → build/glide3x.dll (lands beside d2.exe automatically)
```

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

## Test procedure (P1 exit criteria)

1. Copy `build/glide3x.dll` into the game root, next to the game exe.
2. Launch the game with `-3dfx` — that selects the Glide renderer
   (command line arg or `[VIDEO] 3DFX=1` in the ini; **fullscreen only** —
   the game refuses Glide in windowed mode, D2GFX.cpp:497).
   This works on a **vanilla retail 1.14d install** (`Game.exe -3dfx`) —
   retail ships the same `LoadLibraryA("glide3x.dll")` loader the recovered
   source has; a rebuilt d2.exe is NOT required for P1–P3. Full walkthrough:
   `FIELD-TEST.md`.
3. Expected: the game boots to the menu **behind a black screen** (nothing is
   rasterized yet), stays alive and stable — no crash, clean 5-minute idle.
4. Check `glide3x.log` beside the exe. A healthy P1 run looks like:

   ```
   [g3xvk] glide3xvk scaffold <build-id> — loaded into F:\games\Diablo II\d2.exe
   [g3xvk] vk: loader ok, api 1.x
   [g3xvk] vk: picked <GPU name>
   [g3xvk] vk: swapchain 640x480 fmt=... images=...
   [g3xvk] grSstWinOpen: hwnd=... res=7 (640x480) ... colbuf=2 aux=0
   [g3xvk] grGet:0xF(#1): TMU count -> 1
   [g3xvk] frame 300: swaps=300 draws=... texdl=... paldl=...
   ```

   `grGet:0xF -> 1` before `grSstWinOpen` is the game **accepting** the
   renderer. Tex/palette downloads appearing means P2 has real work queued.

## Fail-soft behavior

If 32-bit Vulkan is unavailable (`vulkan-1.dll` x86 missing — see R2), the
log says so, the DLL still loads, and the game keeps running (black window).
The DirectDraw path (`-w`) is untouched by all of this.

## What is deliberately NOT here yet

- Any rasterization (P2): texture uploads, combine-mode → pipeline variants,
  vertex submission, LFB over the frame image, gamma.
- libslang-vk linkage (P3): the present pass is marked `TODO(P3)` in
  `src/vk_backend.cpp`; the 32-bit libslang-vk build spike is the gate
  (`BUILD-32BIT.md`).
- Preset config/hot-reload, packaging (P4).
