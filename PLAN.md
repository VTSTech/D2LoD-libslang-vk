# PLAN.md — libslang-vk Integration (D2LoD-libslang-vk)

> Status: v5, 2026-10-25. Baseline build verified in-sandbox (see §2); rebuilt
> d2.exe verified PLAYABLE on real hardware (silent-startup saga closed — see §0 for
> where the bootlog work lives). glide3xvk is **past P2 and P3 code-complete**:
> P2.1–P2.11 shipped and field-verified (rounds 1–13, verdict round 12:
> "Everything actually looks correct"), 0.4.0 shipped the P3 preset chain and the
> full-screen 0.4.3 field log proves it live on real hardware (crtglow_lanczos
> 7-pass chain: 516 filtered frames, 0 fallbacks), and **0.4.4 ships perf round 1**
> (P2.12: SSE2 float math, O(1) texture table, swapchain headroom —
> parity-verified bit-identical; field numbers pending). Owner: VTSTech.
> Agent session log: `/home/z/my-project/worklog.md`.
> **All integration work happens on the `libslang-vk` branch (see §0).**

---

## 0. Branching & git workflow

Lesson learned from the DevilutionX fork: feature commits landed straight on
`master` and tangled it. This fork keeps the lines separate from day one:

| Branch | Role |
|---|---|
| [`master`](https://github.com/VTSTech/D2LoD-libslang-vk) | Redivivus base + the **bootlog/diagnostic work**: `[d2boot]` boot trace, `support/d2_boot.{h,cpp}`, VEH `d2crash-*.txt` reporter, signature-verification fixes (`Bnclient/Bnclient.cpp` CERT_INFO + direct handle cleanup, `Storm/_unnamespaced.cpp` cleanup helper retired). Deliberately kept close to upstream; **may be PR'd upstream** once tidied. No feature work lands here. |
| [`libslang-vk`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/libslang-vk) | **Active development line — all glide3x / Vulkan / libslang-vk integration work happens here.** Created from current master state 2026-10-24. |

Rules:

- Every phase below (P1–P5) commits to `libslang-vk` only.
- `master` receives changes only via PR (`libslang-vk` → `master`) at stable
  milestones, or via a standalone bootlog PR if upstream wants the diagnostics.
- The glide3x DLL lands as a new `glide3xvk/` subtree (§4), so the branch diff
  vs `master` stays reviewable.

### Upstream byproducts (what can flow back to Redivivus)

glide3xvk needs zero game-source changes — the fork serves as a read-only spec
and optional diagnostics host — but the work still produces material useful to
Redivivus's own goals (fidelity + buildability):

- **Already on `master` (PR-able now):** `[d2boot]` boot trace,
  `support/d2_boot.{h,cpp}`, VEH `d2crash-*.txt` reporter, and the mingw
  build fixes (`Bnclient.cpp` CERT_INFO + direct handle cleanup,
  `Storm/_unnamespaced.cpp` cleanup-helper retirement).
- **Glide usage documentation:** the enumerated 45-export contract plus the
  call patterns `glide3x.log` captures (combine-mode sets, vertex stride 0x1C,
  palette flow, LFB sites) — reference material for anyone touching the
  `D2Glide` module.
- **Vanilla-vs-rebuilt differential harness:** the same DLL runs on vanilla
  `Game.exe` and the rebuilt `d2.exe`, so the `glide3x.log` Glide-call stream
  is a behavioral diff oracle — same save/scenario on both, and any divergence
  flags either a stub gap (ours) or a recovery bug (theirs). Cheap to run,
  high signal for a fidelity-driven project.
- **Toolchain findings:** llvm-mingw header-drift specifics and the GNU ld
  export-decoration workaround (`glide3xvk/README.md`) — relevant if
  Redivivus formalizes mingw-w64 CI.

---

## 1. Goal

Run Diablo II: LoD 1.14d (Redivivus recovered source) with its final frame routed
through libretro `.slangp` shader presets via **libslang-vk** (Vulkan), the same
pattern proven in **chocolate-doom-vk** (Doom) and **DevilutionX-libslang-vk**
(Diablo/Hellfire). Concretely: pick a preset (CRT Royale, etc.), launch the game,
and every presented frame passes through the preset chain at up-scaled resolution
with integer/fit scaling modes and hot-swappable presets.

Non-goals for v1: Battle.net (upstream limitation), HD asset replacement,
per-element game shaders (d2dx territory), Vulkanizing D2's internal 2D pipeline
beyond what the Glide surface requires.

---

## 2. Where we are (baseline, verified 2026-10-24)

The untouched fork builds clean in the agent sandbox. This matters because every
phase below compiles against this exact toolchain.

| Item | Value |
|---|---|
| Build output | `build/d2.exe`, `pei-i386`, 347,992,478 bytes (-g, no -O, per project policy) |
| Toolchain | g++-mingw-w64-i686 **13.3.0-9ubuntu1+26.6** (the exact CI set), binutils-mingw-w64-i686 2.43.1, mingw-w64 runtime 13.0.0 |
| Verify | 0 compile errors over 510 TUs; 0× `-fpermissive`; imports `msvcrt.dll`, `ddraw.dll` as expected |
| Sandbox recipe (no sudo) | debs from `archive.ubuntu.com` pool → `dpkg -x` into `~/.local/opt/mingw-root` → wrapper scripts (`d2w-gcc` = `i686-w64-mingw32-gcc-posix --sysroot=...`) → configure with `-DD2_TOOL_PREFIX=d2w-` |
| Why not llvm-mingw | clang 23 rejects the tree: mingw-w64 header drift (`_EXCEPTION_DISPOSITION` enum vs project `typedef int`, missing `threadlocinfo`). GNU/CI headers avoid all of it; zero source drift. |
| glide3xvk (P2, committed) | `glide3x.dll`, `pei-i386`, 45/45 exports, self-contained (KERNEL32 + msvcrt imports only); rendering field-verified correct on retail 1.14d (rounds 4–12); CI builds + uploads it per push |

The toolchain lives at `/home/z/.local/opt/` (venv: cmake 4.4.4 + ninja; llvm-mingw
20261020 kept as fallback). VTSTech's Ubuntu 26.04 VM can use the same deb-extract
recipe or plain `apt install g++-mingw-w64-i686`.

---

## 3. The seam: the game still speaks Glide 3 through a replaceable DLL

Reverse-engineering the renderer dispatch turned up the ideal integration point —
**no generated game code needs to change**:

- `D2Client/Renderer/Glide.cpp` → `RENDERER_GLIDE_LoadLibrarySymbols()` does
  `LoadLibraryA("glide3x.dll")` and resolves **45 stdcall exports**
  (`_grBufferSwap@4`, `_grSstWinOpen@28`, `_grDrawVertexArrayContiguous@16`, ...).
  A missing export halts via `ErrorManager` with a per-symbol line code.
- `D2Client/Renderer/Glide.cpp::Initialize()` gates on `grGet` answers — the
  decompiler mislabels the variables: `0xF` is `GR_NUM_BOARDS` (the accept
  gate, must be ≥ 1) and `0x13` is `GR_NUM_TMU` (the init gate, must be ≥ 1;
  answering 0 there was the v1 zero-frame cause). `0x05/0x0A/0x0B/0x11/0x24/
  0x2A/0x0E` round out the caps contract (Voodoo-class single-TMU profile,
  answered per the real 3dfx glide3x `diget.c`).
- The whole renderer is classic fixed-function Glide: palette-indexed 8-bit
  textures (`grTexDownloadMipMap`, `grTexDownloadTable` palettes), quad/line/point
  submits (`grDrawVertexArrayContiguous` — vertex stride 0x1C, `D2GlideVertexStrc`:
  x, y, ARGB color, oow, s, t, w), combine/blend state machines
  (`grColorCombine`/`grAlphaCombine`/`grAlphaBlendFunction`), clip window, gamma
  (`guGammaCorrectionRGB`), and LFB access (`grLfbLock`/`grLfbUnlock`).
- Frame structure (see `D2Glide/Renderer/Glide.cpp`): `BeginScene` → draw calls →
  `EndScene1`/`EndScene2` → `grBufferSwap(vsync)`. Cutscenes (Smacker/Bink) draw
  through the same vertex path.
- In-tree `D2Glide/` (glBlocks, glTextureCache, glFloorCache, glSubtileCache,
  Renderer/Glide.cpp) is the **behavioral spec** for exactly how D2 uses the API:
  which combine modes occur, how LOD/aspect are picked, how palettes ride along.

This is the **d2dx architecture** (bayaraa/d2dx ships a Glide→D3D11 wrapper DLL
for D2, MIT-licensed — primary prior art for API semantics), upgraded: instead of
D3D11, our `glide3x.dll` implements Glide on Vulkan and threads the final frame
through libslang-vk before present.

**Vanilla-compatible by construction.** The seam is retail code, not a patch: any
stock D2LoD 1.14d install (GOG / Blizzard downloader) already does
`LoadLibraryA("glide3x.dll")` and resolves the same 45 stdcall exports — the
Redivivus tree is a recovery of exactly that code. A drop-in `glide3x.dll` works
on vanilla `Game.exe`, mode selected via `Game.exe -3dfx` or D2VidTst.exe — the
same deployment model nGlide and d2dx use on stock installs. **Consequence: no
rebuilt d2.exe is required for any P1–P3 field test**; the Redivivus build only
becomes relevant in P4 (optional VEH crash-report hook). Field-test procedure:
`glide3xvk/FIELD-TEST.md`.

**Why not alternatives:**

- *New in-tree Vulkan renderer module*: the renderer contract
  (`D2RendererFunctionsStrc`) is large (draw tiles/sprites/shadows, lighting,
  cutscenes, LFB, window mgmt) and the tree is machine-regenerated — every
  hand-edit risks upstream regen conflicts. Rejected for now.
- *Wrap an existing glide3x (OpenGlide/nGlide) and filter only at present*:
  adds a third renderer dependency and GL readback hops; nGlide is closed-source.
  Rejected — we control the whole path instead.
- *Post-present DDraw/Direct3D hook*: D2's DDraw path is 8-bit paletted and
  low-level; the Glide path is the cleanest documented contract (45 named exports,
  in-tree behavioral spec). Rejected.

---

## 4. Architecture

```
+------------------------------------------------------------+
| d2.exe (Redivivus, unchanged)                              |
|   D2Client::Renderer::Glide  ---- LoadLibraryA ------------|--.
+------------------------------------------------------------|  |
                                                             v  |
+------------------------------------------------------------+--+
| glide3x.dll  (ours, i686, stdcall exports; subdir glide3xvk/ in fork)  |
|                                                                |
|  Glide3 API surface (45 exports) -> GlideState machine         |
|      |                                                         |
|      v                                                         |
|  VK raster backend:                                            |
|    - palette-indexed textures (8bpp index + palette LUT)       |
|    - pipeline variants per combine/alpha/chromakey state       |
|    - quads/tris/lines/points -> offscreen RGBA8 frame image    |
|      |  (game res: 640x480 / 800x600, per nResolutionMode)     |
|      v                                                         |
|  Present pass (per grBufferSwap):                              |
|    slangvk_chain_render_swapchain(chain, in=frame, out=swap)   |
|    -> libretro .slangp chain -> swapchain on game HWND         |
|    -> vkQueuePresentKHR                                        |
+------------------------------------------------------------+---+
                                                             |
                                             libslang-vk (static, C API)
                                             volk loads vulkan-1.dll (32-bit)
```

Key properties:

- **Zero game-source drift.** The DLL is a separate CMake target in a new
  `glide3xvk/` directory (globbed into the build automatically by the root
  CMakeLists — actually better: own `glide3xvk/CMakeLists.txt` producing
  `glide3x.dll`, kept out of the `d2` glob via the existing build-dir/regex
  excludes or a `.gitignore`-style filter). Upstream regens stay pain-free.
- **GPU-resident frame, no readback.** In the full implementation, D2's quads
  render into an offscreen image whose view feeds `slangvk_input_frame` directly
  (SHADER_READ_ONLY layout per libslang-vk contract). `grBufferSwap` becomes:
  chain render → present. No CPU round-trip.
- **libslang-vk owns Vulkan init** via `slangvk_device_create`
  (headless-friendly; loads the runtime through volk — no Vulkan SDK at build
  time), `slangvk_device_get_handles()` for swapchain creation in `grSstWinOpen`.
  Presets compile at runtime through the embedded glslang — no slang SDK either.
- **Fail-soft present.** If Vulkan is unavailable (no 32-bit loader/ICD), the DLL
  still loads and presents the raw frame (unfiltered, scaled) so the game remains
  playable; presets simply don't engage. Config flag controls the fallback.

### Present-pass frame loop (libslang-vk C API, verbatim contract)

```c
/* once per mode change (grSstWinOpen / resolution switch) */
slangvk_chain_create(dev, preset, SWAP_W, SWAP_H, swap_format, &chain);

/* per grBufferSwap */
slangvk_input_frame in = {.view=frame_view, .format=VK_FORMAT_R8G8B8A8_UNORM,
                          .width=GAME_W, .height=GAME_H};
slangvk_swapchain_frame out = {.swapchain_image_view=swap_views[idx],
                               .format=swap_format, .width=SWAP_W, .height=SWAP_H,
                               .acquire_semaphore=acq_sem};
slangvk_frame_info fi = { frame_count++, delta_us, +1 };
slangvk_chain_render_swapchain(chain, &in, &out, &fi); /* sync + fenced */
vkQueuePresentKHR(...);
```

---

## 5. Work breakdown

### P1 — Skeleton DLL + Vulkan bootstrap  *(de-risk first)*
Deliver `glide3x.dll` that loads in the game, opens a swapchain, clears black,
presents — game reaches the menu with a black screen and no crash.

- Export all 45 symbols with correct stdcall decorations; implement state
  tracking stubs; correct `grGet` (TMU count ≥ 1), `grGetString`, `grGetProcAddress`.
- `grSstWinOpen`: take the game's HWND → volk init → `slangvk_device_create` →
  surface + swapchain (32-bit!). `grBufferSwap`: acquire/present (unfiltered).
- **Spike (gate for everything): build libslang-vk itself as i686 Windows**
  with the sandbox mingw toolchain (C++23 on gcc-13 i686; static `slangvk`
  target + vendored volk/VMA). Fallbacks if C++23 gaps appear: compile the
  library as C++20 with pinned features, or build a 32-bit C shim against a
  reduced core. Record findings in `glide3xvk/BUILD-32BIT.md`.
  **DONE — PASSED 2026-10-24, R1 retired, no fallbacks needed (BUILD-32BIT.md).**
- **Resolved:** 1.14d exposes the Glide mode via the ini flag store — command
  line `-3dfx` or `[VIDEO] 3DFX=1` (Fog globals flag table, offset 0xa).
  Field-proved correction (rounds 2–9): **windowed `-w -3dfx` also loads
  Glide** on retail — the D2GFX.cpp:497 "fullscreen-only" read was wrong.
  Renderer table index 4 = `D2Client::Renderer::GlideFunctionTable`. Test on a
  **vanilla retail install**: `Game.exe -w -3dfx` — no rebuilt d2.exe
  needed (§3 vanilla note); the Redivivus build behaves identically when available.
- Exit: game boots, menu UI invisible but present (nothing rasterized yet),
  clean log, crash-free 5-minute idle in menu.
- **Shipped 2026-10-24 (rounds 1–3):** v1 scaffold → v2 caps fix
  (`GR_NUM_BOARDS`/`GR_NUM_TMU` — the zero-frame root cause) → v3 Bink LFB
  present + windowed scale (intro videos visible on retail). P1 exit met.

### P2 — Glide raster core  *(the meat)* — **SHIPPED: P2.1–P2.11 (rounds 4–13); field verdict round 12: "Everything actually looks correct"**
Render what D2 actually draws, unfiltered, through the new backend.

- Texture path: `grTexDownloadMipMap`/`grTexSource` uploads as 8bpp index
  textures + `grTexDownloadTable` palette LUTs; shader does `palette[idx]`
  lookup; TMU memory emulation kept minimal (the game addresses TMU like Voodoo
  but we can hash-allocate; in-tree `glTextureCache` shows usage).
- State machine: map `grColorCombine`/`grAlphaCombine`/`grAlphaBlendFunction`/
  chromakey/depth/dither onto pipeline variants (enumerate the exact states the
  game sets — bounded set; spec = in-tree D2Glide + d2dx).
- Geometry: `grDrawVertexArrayContiguous` quads/tris/fans, `grDrawLine/Point`;
  clip window; `D2GlideVertexStrc` layout (stride 0x1C) direct as vertex input.
- Special cases: LFB lock/unlock (fades, screenshots) → linear views over the
  frame image; gamma via `guGammaCorrectionRGB` (fold into present pass);
  Smacker cutscene quads.

Sub-milestone ledger — each row is one field round (user pastes `glide3x.log`
→ agent analyzes against the game tree → build → zip → tmpfiles.org → retest;
details in `glide3xvk/FIELD-TEST.md` + `worklog.md`):

| Step | Build | Shipped | Field result |
|---|---|---|---|
| P2.1 | v4 | fan-quad barycentric raster; P_8 + palette; 3 blend classes; gamma in present | title renders; buttons black, char sprites halved (s/t convention) |
| P2.2 | v5 | texel-unit s/t rule v1 (`2^lod/256`) + constant-fill path | button art appears; glyph text missing; ground black (parr path ignored) |
| P2.3 | v6 | pointer-array strips/fans (ground/walls) + color-space chromakey | ground appears; button/text and edge artifacts remain |
| P2.4 | v7 | negative-coord wrap; fill-rect fast path; append-mode logs | same pixels; 4.3 fps becomes the complaint |
| P2.5 | v8 | scanline span raster (~40x); `grDrawPoint`/`grDrawLine`; constant-color RGB lanes; `avg=ms` | ~27 fps @ 800x600; "a lot missing" + buttons still wrong |
| P2.6 | v9 | **s/t space auto-detect** (texel units vs Voodoo 256-space, per axis) + RGB565 quads | quad# field data: auto-detect wrong on 7 of 14 classes |
| P2.7 | v10 | **uniform square-mip s/t space** — texel = coord·2^largeLod/256 on BOTH axes (reduces exactly to D2's own `gaPerspectiveImageSizeTable` spans for every aspect) | 7 crushed classes fixed (button text, mini-panel, sprites); 2 residual symptoms: black rects in-game, black bars behind menu labels |
| P2.8 | v11 | evidence build: `vc0#`/`pal#`/`chroma#`/`fill#` diagnostic lines; `grTexTextureMemRequired` 1 B/texel fix; kMaxTex 8192 | evidence pinned: transparency lives in palette INDEX 0, not the blue chroma key |
| P2.9 | chroma-fix | chroma discards texel **index 0** (the DC6 transparent texel RLE leaves unwritten) in addition to RGB==key | round-11 field: **"Great job! Everything actually looks correct."** — both symptoms closed, one root cause |
| P2.10 | 0.3.0 | version-tag overlay (8x8 font, backdrop, shadow), `grGetString(GR_RENDERER)` identity, `glide3xvk.ini` off-switch, release packaging | delivered; every screenshot now self-identifies its build |
| P2.11 | 0.3.1 | overlay moved **top-right** (the game's `/fps` readout owns the top-left corner); CI builds `glide3x.dll` and uploads it as an artifact; README/PLAN refresh; all P2 work committed to the branch so the build id names a real pushed commit | this round |
| P2.12 | 0.4.4 | **perf round 1**: `-msse2 -mfpmath=sse` (x87 off the raster hot path), O(1) texture table (open-addressing hash + monotonic alloc cursor + same-texture rebind fast-out), swapchain `minImageCount+2`; agent-side fxparity harness (220-frame mixed scene) proves the raster bit-identical | field round pending: build the DLL, capture `frame N:` lines chain-on vs chain-off |

The P2.6 discovery (game-tree ground truth): D2 submits **two s/t
conventions** — texel units (`DrawPalettedQuad` sprites/UI/buttons/text via
`gaPerspectiveImageSizeTable`, and the ground lattice `InitGroundTileVertices`)
and the **Voodoo 256-space** (glBlocks wall lattice `InitPerspectiveTables`:
S=col\*128, T=row\*64 up to 256, sampling 32x32 subtiles). The single scalar
rule of P2.2–P2.5 was exact only for lod-8 textures — which is why the ground
fixed in rounds 6–8 while 64x32 title buttons (crushed to a 16x8 corner) and
32x32 wall subtiles (4x4 corner) never did. P2.6 auto-detected per draw and
per axis; P2.7 replaced that heuristic with the uniform square rule
`texel = coord·2^largeLod/256` on both axes, which reduces to the game's own
aspect-table spans for every aspect and measured 1:1 on all 14 field draw
classes.

The P2.9 root cause (one defect, both symptoms): DC6 sprites/tiles/textures
carry their transparent texels as palette **index 0** (RLE decompression skips
transparent runs, leaving the memset zero). The chroma rule only compared
post-palette RGB against the blue chroma key — palette 0 is black, not blue —
so every transparent texel painted opaque black: sprite/tile-corner rects
in-game and glyph-background bars in the menu. Chroma now discards index 0 in
addition to the RGB match.

The P2.12 trigger (perf round 1, 0.4.4): the full-screen 0.4.3 field log — RX
560, 800x600, crtglow_lanczos 7-pass chain active — averaged **51.6 ms/frame ≈
19 fps** in-game, with a per-frame draw mix of 152 fans + 170 strips + 142
RGB565 quads (≈ 470 draw calls) and **fill=0** — the fullscreen dim-fill path
is not a real-world cost. The chain itself filtered 516 frames with 0
fallbacks, so the budget sits in the CPU raster + texture-table upkeep, not in
the preset chain. Round 1 attacks that CPU side (ledger row above; backlog in
the §5 perf track) and is verified bit-identical by the agent-side fxparity
harness — a 220-frame deterministic mixed scene hashing every present buffer
before and after the change — so the round cannot move pixels, only time.

P2 exit: **met** (round-12 field verdict on a full single-player session:
menu, world, units, inventory UI, shadows, text, cutscenes all correct).
Leftover polish, deliberately deferred:

- Bilinear filtering: the ground runs `texFilterMode(1,1)` when `pTileData`
  is set — currently point-sampled (look delta only, not missing content).
- Mip chains: download bytes stored, sampling is base-lod only; no
  `grTexMipMapMode` calls observed in any log — revisit only if distance
  artifacts show up.
- Frame pace: the full-screen 0.4.3 field log measured avg=51.6 ms ≈ 19 fps
  @ 800x600 **with the crtglow_lanczos chain active** (round 8's chain-free
  baseline was 27 fps — the chain's fullscreen pass costs real time, but the
  CPU raster + texture upkeep still own the budget). P2.12 (0.4.4) takes the
  first CPU-side cut; the 60 fps endgame remains the GPU compose move.

### P3 — libslang-vk preset chain — **SHIPPED 0.4.0; chain field-proven live (full-screen 0.4.3 log: crtglow_lanczos 7-pass, 516 filtered frames, 0 fallbacks)**
The payoff. Everything above already runs through our swapchain.

- Offscreen frame image feeds `slangvk_input_frame`; chain created per preset at
  startup; resolution changes (video mode switch, cutscenes) →
  `slangvk_chain_resize` / chain recreate.
- Scaling modes: integer, fit (letterbox, `output_rect` all-zero default), stretch.
- Preset management: `d2slangvk.conf` (preset path, scale mode, vsync override,
  fallback behavior); hot-reload preset on file change (chain recreate).
- Exit: CRT Royale + a scanline + a simple sharpening preset run live in-game at
  1440p/4K output from 640x480 input; preset hot-swap works; frame cost measured.

Sub-milestone ledger (0.4.0, agent-sandbox verified; field round pending):

| Step | Shipped | Verification so far |
|---|---|---|
| P3.1 | libslang-vk linked: static `slangvk` target built i686 from the sibling checkout (`G3XVK_SLANGVK_DIR`, fail-soft stub without it); `slangvk_device_adopt` of our context; CI checks out the repo pinned (`d208ac5`) | 76/76 slangvk targets build pe-i386; 27 C symbols; DLL 8.67 MB, still KERNEL32+msvcrt only |
| P3.2 | present-pass chain: gamma'd NATIVE game-res frame → `slangvk_image` → `slangvk_chain_render_swapchain` into our swapchain (per-image views, COLOR_ATTACHMENT usage, acquire semaphore handed to the chain, present without our render semaphore); legacy unfiltered path preserved byte-for-byte as fallback | 45/45 export surface unchanged; fxtest 115/115 (conf parser, rect math, present-branch fallback) |
| P3.3 | `d2slangvk.conf` (`[slangvk] preset=/scale=/vsync=/fallback=`), portable parser; hot-reload (conf + preset file stat every 32 frames, mid-session chain rebuild); fail-soft (log + downgrade; 3 consecutive render failures disable the chain for the session); two MIT starter presets (scanline, sharpen) + conf sample | chain verified live on real hardware (full-screen 0.4.3 log: crtglow_lanczos 7-pass chain, 516 filtered frames, 0 fallbacks); frame cost moved to the P2.12 perf track |

P3 exit status: **code-complete and field-proven live.** The full-screen 0.4.3
log shows a 7-pass community chain filtering every frame on the RX 560
(516 filtered, 0 fallbacks) — the chain is not the bottleneck. F11/F12
hot-swap field confirmations ride along with the perf rounds. The deferred
perf item is now its own running milestone (P2.12 / the §5 perf track): 0.4.4
took the first CPU-side cut, GPU-side compose remains the 60 fps endgame.

### Perf track — P2.12 and beyond (runs alongside P4; owns the 60 fps question)

Trigger: the full-screen 0.4.3 field log (RX 560, 800x600, crtglow_lanczos
7-pass chain active) averaged **51.6 ms/frame ≈ 19 fps** in-game. The frame
mix (152 fans + 170 strips + 142 RGB565 quads, fill=0) put the budget in the
CPU raster + texture-table upkeep — not the chain (0 fallbacks) and not the
game's draw volume.

Round 1 — **shipped 0.4.4** (the P2.12 ledger row): SSE2 float math
(`-msse2 -mfpmath=sse`; i686 MinGW defaults ALL float math to x87 — 1.5–3x
expected on the span/tri inner loops), O(1) texture table (binds were a linear
scan over up to 8192 slots + a rescan-from-0 per download), swapchain
`minImageCount+2` (the log showed images=2, the bare minimum). Verified
bit-identical on the agent-side fxparity harness — zero pixel risk by
construction.

Round 2 candidates (expected-gain order, not started):

- **Fence pipelining**: the submit path waits the fence every frame (a single
  staging/cmd/semaphore set) — a 2-frames-in-flight ring overlaps CPU raster N
  with GPU present N-1. Same shape in the chain path (`slangvk_image_update` +
  `slangvk_chain_render_swapchain` are fenced serially today).
- **`fill_span` templating**: the per-pixel loop re-branches on blend class /
  chroma / varying-vc every texel; a (blend × chroma × vc) template expansion
  gives straight-line inner loops.
- **Phase-2 micro**: 3 divides + up to 12 exact texel fixups per scanline row;
  `-O3` for the glide3x target (free on top of SSE2).

Method note: every raster-affecting change re-runs the fxparity hash; a hash
mismatch needs an explicit pixel-delta justification before it ships.

### The 25 fps ceiling — engine design, source-verified (2026-10-25)

The 0.4.4 field logs ended the "is it us or the game?" question. Two facts
from the second 0.4.4 log (build be8737e):

- `avg=` in the `frame N:` line is **swap-to-swap wall time** (QPC between
  `grBufferSwap` calls, glide_exports.cpp). In-game it reads a dead-on
  **40.0–40.3 ms = 25.0 fps** while the Esc-menu windows read ~24 ms — with
  the **same per-frame draw volume** (parr = exactly 510/frame in both).
  Identical work at two different paces = the game chooses the pace.
- The game's own decompiled source confirms it: `CLIENT_GameLoopFrame`
  (`D2Client/GAME/Game/Game.cpp`) gates sim + render on a **40 ms budget**
  (`uGlobalTickCountB = 0x28`) and hardcodes `fps_drawn = 25`; the in-tree
  comment is explicit — *"In single player the gameserver runs as part of the
  drawing process, this is why in single player the game is capped so hard at
  25 frames, as serverside needs to be ran at 25 ticks per second."* The Esc
  menu takes an **early-return path that draws unthrottled** — hence 45 fps
  there, which is the wrapper's true current speed for a full scene.

Consequences for this plan:

- **In-game fps is engine-locked at 25 on vanilla.** No wrapper change moves
  it. The perf rounds did still matter: 0.4.3's 51.6 ms frames exceeded the
  game's 40 ms tick budget — that is tick-skip territory (the game's own
  frame-skip logic, `framesSkipped`, the choppiness source). 0.4.4's ~22 ms
  true cost fits the budget: every logic tick renders, no skips.
- **60+ fps smoothness is an engine-level project, not a wrapper project:**
  d2dx-style motion interpolation (render the 25 Hz sim state but interpolate
  sprite positions to the display refresh — MIT prior art) or a game patch to
  the tick gate (which speeds the SIMULATION up, not just the picture — breaks
  the game). Both are rebuilt-d2.exe territory (P4+), out of glide3xvk scope.
- The remaining wrapper perf items still pay: Esc menu 45 → 60 (FIFO cap),
  cutscenes, keeping heavy scenes (Baal runs) inside the 40 ms budget so the
  engine never skips, and present latency. `work=` in the frame line (0.4.4)
  now reports the wrapper's present-path cost per frame so future rounds
  can separate engine pacing from wrapper cost at a glance.

### P4 — Hardening + packaging
- Failure paths: no Vulkan → unfiltered present; device lost → libslang-vk's
  recovery path (SLANGVK_ERR_DEVICE_LOST_RECOVERY_FAILED handling); alt-tab,
  windowed mode (`-w`), resolution changes.
- Crash reporting: hook the fork's VEH crash reporter (`support/d2_crash.cpp`,
  `d2crash-*.txt` — lives on `master`, see §0) to include the DLL's
  last-N-Glide-calls ring buffer.
- Packaging: drop-in zip (`glide3x.dll` + `presets/` + `d2slangvk.conf`) laid
  over a D2 install; README with video-mode instructions.
- CI: **done for the DLL** (P2.11) — `.github/workflows/build.yml` builds the
  tree from a clean clone (`tools/check-build.sh`), then builds `glide3x.dll`
  from the checkout, verifies the 45-export surface via objdump, and uploads
  the DLL as the `glide3x-dll` artifact. Remaining (P4): package the drop-in
  zip (`glide3x.dll` + presets) in CI instead of the hand-run tmpfiles loop.

### P5 — Stretch
- Per-preset retro CRT curvature/geometry defaults per D2's 4:3 vs wide hacks.
- `grLfbWriteMode` screen-capture compatibility (F9 screenshot stays paletted-correct).
- Revisit an in-tree renderer only if Glide-surface limitations bite.

---

## 6. Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | libslang-vk C++23 doesn't build i686-mingw gcc-13 | **Retired 2026-10-24** | — | **Spike PASSED** (`glide3xvk/BUILD-32BIT.md`): builds clean with zero source changes; fallbacks not needed; verified recipe recorded there |
| R2 | No 32-bit Vulkan loader/ICD on VTSTech's box | **Retired 2026-10-24** | — | **Field-proved**: the driver stack serves Vulkan 1.3 to the 32-bit DLL (RX 560, rounds 1–9, 293-frame sessions); fail-soft unfiltered present stays in place regardless |
| R3 | 1.14d doesn't expose Glide video mode in UI | **Retired 2026-10-24** | — | **Field-proved**: `-3dfx` selects Glide on retail 1.14d, and `-w -3dfx` runs it windowed (rounds 2–9) |
| R4 | Glide state semantics subtly wrong (combine functions) | Low (reduced) | Med | Materialized twice and fixed: the two s/t conventions (P2.7 square-mip rule) and palette-index-0 transparency (P2.9 chroma rule). The in-tree D2Glide spec + field-log loop keeps catching the rest; open item: bilinear look only |
| R5 | Optimized codegen breaks decompiled game code | — | — | Non-issue: project policy pins -O0/-g; our DLL may optimize freely (separate target) |
| R6 | TMU memory emulation complexity | **Retired 2026-10-24** | — | Allocate-on-upload + (tmu,addr) hash store shipped in P2.1; texmiss=0 across 60k+ binds in round 8 |
| R7 | CPU raster can't reach the fps target | **Closed — superseded** | — | Source-verified: in-game 25 fps is the engine's tick lock (see the perf track). Wrapper cost ~22 ms (Esc-menu fps) fits the 40 ms budget; remaining perf work = headroom/menus/latency, not in-game fps |

---

## 7. Test plan

- **Sandbox (agent):** full build per phase; export-table verification
  (`objdump -p glide3x.dll`); for raster-affecting changes, the agent-side
  **fxparity harness** (220-frame deterministic mixed scene hashing every
  present buffer + counters) must reproduce the pre-change hash bit-for-bit;
  headless slangvk smoke (lavapipe per `libslang-vk/benchmarks/README.md`
  recipe) running a preset over a captured D2 frame from the corpus; parse
  bench over the 2,658-preset pack already in place from the libslang-vk
  benchmark session.
- **VTSTech VM (Ubuntu 26.04, VirtualBox):** build parity; wine smoke test
  optional (Vulkan via lavapipe) — menus only.
- **Real hardware (Win11 + RX 560, vanilla 1.14d install):** the authoritative
  pass per phase exit criteria — `Game.exe -w -3dfx` (rebuilt d2.exe optional
  until P4); frame-time capture at 640x480→1440p; preset sweep with the corpus.
  Operating loop since round 4: user pastes `glide3x.log` → agent analyzes it
  against the game tree → builds → zips (DLL + FIELD-TEST.md) → tmpfiles.org
  (60-min links) → user retests. Each round's log contract lives in
  `glide3xvk/FIELD-TEST.md`.

## 8. References

- libslang-vk: `include/slangvk/c.h` (whole integration on one page), `USAGE.md`,
  `benchmarks/README.md` (lavapipe recipe), `corpus/` (preset pack).
- d2dx (bayaraa): Glide→D3D11 wrapper for D2 — API semantics prior art (MIT).
- In-tree spec: `D2Client/Renderer/Glide.cpp` (loader, 45 exports),
  `D2Glide/Renderer/Glide.{h,cpp}` + `gl*Cache` (usage patterns),
  `D2Client/Renderer/Renderer.h` (renderer contract typedefs).
- chocolate-doom-vk, DevilutionX-libslang-vk: house precedents for the present-pass
  pattern and preset UX.
