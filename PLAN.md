# PLAN.md — libslang-vk Integration (D2LoD-libslang-vk)

> Status: v5, 2026-10-28. **The d2.exe source line is `vanilla-plus`** — upstream `main`
> (eef5f2c) is merged; the glide fix chain (1–13), the drops fix (14) and the sound series
> (15–16, defects 12+13) are closed and field-verified; the sound fixes are upstreamed as
> jaenster/redivivus **PR #10** (open, merge-clean); the fullscreen-D3D recovery (fixes 17–27)
> is closed and field-verified; releases **`D2LoD-Vanilla-Plus-0.0.1`** (`4857c30`) and
> **`D2LoD-Vanilla-Plus-0.0.2`** (`9cf9f69` — first opt-in feature: alt-key item-label toggle
> + `d2vp.ini` auto-creation) (see `VANILLA-PLUS.md` + `handoff.md`). **The wrapper line `libslang-vk` is at 0.4.5** with P1–P3 delivered and
> packaging partially done (per-phase status in §5). **CI is back (2026-10-28)** —
> mingw-i686 green on the 0.0.2 tag; local Win11 builds (`tools/build-local-win11.ps1`)
> remain the authoritative gate; the sandbox holds a llvm-mingw toolchain for per-TU
> verification (§2). Owner: VTSTech. Agent session log: `/home/z/my-project/worklog.md`.
> Live task list: `handoff.md` §9.
> **All integration work happens on the `libslang-vk` branch (see §0); all d2.exe fixes
> happen on `vanilla-plus`.**

---

## 0. Branching & git workflow

Lesson learned from the DevilutionX fork: feature commits landed straight on
`master` and tangled it. This fork keeps the lines separate from day one:

| Branch | Role |
|---|---|
| [`main`](https://github.com/VTSTech/D2LoD-libslang-vk) | Redivivus baseline (`fd236ce`), kept close to upstream; no feature work lands here. |
| [`vanilla-plus`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/vanilla-plus) | **The d2.exe source line.** All game-source fixes (glide chain 1–13, drops 14, sound 15–16) plus the bootlog/diagnostic work: `[d2boot]` boot trace, `support/d2_boot.{h,cpp}`, VEH `d2crash-*.txt` reporter, signature-verification fixes (`Bnclient/Bnclient.cpp` CERT_INFO + direct handle cleanup). Merged with upstream `main` (`ad964f3`). Tip `9cf9f69` = release `D2LoD-Vanilla-Plus-0.0.2` (0.0.1 = `4857c30`); fullscreen-D3D recovery (fixes 17–27) closed and field-verified; first opt-in feature (alt-key item-label toggle + `d2vp.ini` auto-creation) shipped. Builds: local Win11 `tools/build-local-win11.ps1` or CI (mingw-i686, green on the 0.0.2 tag) → fat `d2.exe` + stripped `d2s-<sha>.exe` + DWARF map. |
| [`pr2/glide-renderer-fixes`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/pr2/glide-renderer-fixes) · [`pr3/mobs-drop-items`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/pr3/mobs-drop-items) · [`pr4/sound-except-intro`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/pr4/sound-except-intro) | Upstream-PR lines — what was proposed to jaenster/redivivus: PR #6 (glide, **closed by the maintainer 2026-10-27** — confirmed defects fixed in the Ghidra DB, land via the next export), drops, and the sound series (**upstream PR #10**, open, `Fixes #N` in place). The old [`jaenster-main`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/jaenster-main) CI-split line is retired (superseded by the merge era — `HISTORY.md` H2). |
| [`libslang-vk`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/libslang-vk) | **Active development line — all glide3x / Vulkan / libslang-vk integration work happens here.** Wrapper tip `0.4.5` (`3a52553`). |

Rules:

- Every phase below (P1–P5) commits to `libslang-vk` only.
- `main` receives changes only via PR (`libslang-vk` → `main`) at stable
  milestones; upstream-able fixes travel as a format-patch series onto
  upstream `main` (the route proven on sound PR #10 — `handoff.md` §7).
- The glide3x DLL lands as a new `glide3xvk/` subtree (§4), so the branch diff
  stays reviewable.

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

## 2. Where we are (baseline)

**Update 2026-10-27:** **CI is retired** (chronic 15-minute apt step-cap timeouts and a
corrupted snapshot killed it after the merge era — `HISTORY.md` H2; `build.yml` is parked
at the last working version `5931e15`, historical, not load-bearing). The authoritative
build is **VTSTech's Win11 box** via `tools/build-local-win11.ps1` (consolidated by fix
15): strict `-std=c++17`, zero `-fpermissive`, fat `d2.exe` + stripped `d2s-<sha>.exe` +
DWARF linker map (still produced for crash symbolization). The agent sandbox holds **no
mingw toolchain and no apt rights** — sandbox QA is diff review, patch round-trips and
anchor-grep gates; the local Win11 build is the compile gate for everything. **Update 2026-10-28:** CI is back
in service — mingw-i686 ran green on the 0.0.2 tag commit (`9cf9f69`) and ships the exe
artifacts; and a llvm-mingw 20261020 toolchain now lives in the sandbox (`tools/llvm-mingw-*`),
building the tree with exactly 11 documented pre-existing clang-strictness TU failures —
sufficient for per-TU verification of every fix/feature round since fix 26 (the gcc full
build remains the release gate).

| Item | Value |
|---|---|
| Build output | `build/d2.exe`, `pei-i386` (-g, no -O, per project policy; ~348 MB unstripped) |
| Toolchain | `i686-w64-mingw32-g++` (GCC 13-win32) over ~514 TUs, GNU mingw-w64 headers — the same set CI used |
| Why not llvm-mingw | clang 23 rejects the game tree: mingw-w64 header drift (`_EXCEPTION_DISPOSITION` enum vs project `typedef int`, missing `threadlocinfo`). GNU headers avoid all of it; zero source drift. Our DLL (`glide3xvk`) is a separate target and may use llvm-mingw freely. *(Update 2026-10-28: llvm-mingw now also builds the game tree for per-TU QA — 11 documented strictness TUs fail, everything else compiles; see §2 update.)* |
| Sandbox recipe | historical (deb-extract into `~/.local/opt/mingw-root`) — `HISTORY.md` H7; recreate on demand only |

---

## 3. The seam: the game still speaks Glide 3 through a replaceable DLL

Reverse-engineering the renderer dispatch turned up the ideal integration point —
**no generated game code needs to change**:

- `D2Client/Renderer/Glide.cpp` → `RENDERER_GLIDE_LoadLibrarySymbols()` does
  `LoadLibraryA("glide3x.dll")` and resolves **45 stdcall exports**
  (`_grBufferSwap@4`, `_grSstWinOpen@28`, `_grDrawVertexArrayContiguous@16`, ...).
  A missing export halts via `ErrorManager` with a per-symbol line code.
- `D2Client/Renderer/Glide.cpp::Initialize()` gates on `grGet(0xf, ...)` — the
  game requires **secondary TMU count ≥ 1** before accepting the Glide renderer.
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

**Phase status (2026-10-27):**

| Phase | Status |
|---|---|
| P1 — Skeleton DLL + Vulkan bootstrap | **Done** — `glide3xvk` boots the game to the menu; proven on real hardware; the libslang-vk i686 spike resolved on the gcc path. |
| P2 — Glide raster core | **Done** — palette-indexed textures, combine/alpha pipeline variants, fans/quads, LFB, cutscenes render in-game (0.4.x line). |
| P3 — libslang-vk preset chain | **Done** — the 0.4.5 wrapper carries window presets, the slang chain, controller support and base_res (tip `3a52553`). |
| P4 — Hardening + packaging | **Partial** — releases `D2LoD-Vanilla-Plus-0.0.1`/`0.0.2` ship the drop-in (Glide3xVK 0.4.5; the 0.0.2 7z bundles client + wrapper + launcher/debug scripts); still open: the d2crash + wrapper ring-buffer hook, an export-check gate for the DLL (was a CI step — re-home as a local-build check), and the 0.0.2 script-name swap (`Launch.cmd` installs shaders / `install_shaders.cmd` launches — fix before the next packaging run). |
| P5 — Stretch | Open. |

The d2.exe side of the house moved in parallel and is fully green: the glide fix chain
(1–13) that de-risked P2's assumptions about what the game actually submits, the drops
fix (14), and the sound series (15–16 — dead-hwnd music binding, Ghidra FP bit-pattern
constants, float-storage punning of integer out params) are all closed and
field-verified on `vanilla-plus`. The sound series is upstreamed as jaenster/redivivus
**PR #10** (open; `Fixes #N` in place); PR #6 (the glide chain) was **closed by the
maintainer** — the confirmed defects were fixed in the Ghidra DB and arrive with the
next upstream export (vanilla-plus holds its defect-2/defect-4 fixes until post-export
re-verification). Releases: **`D2LoD-Vanilla-Plus-0.0.1`** from `4857c30` (known issue then: fullscreen Direct3D
broken — `-3dfx` fullscreen and `-w` work in all modes) and **`D2LoD-Vanilla-Plus-0.0.2`** from
`9cf9f69` — the D3D recovery (fixes 17–27, HISTORY H18) closed that known issue and shipped the
first opt-in feature (alt-key item-label toggle + auto-created `d2vp.ini`, HISTORY H19). Live
task list: `handoff.md` §9.

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
- **Open question to resolve:** how 1.14d exposes the Glide video mode to the
  user (video test app, registry `VideoConfig`, command-line switch). The loader
  path exists in-source; find what selects it (trace `Initialize` callers, check
  `d2vidtst` equivalents; d2dx docs describe 1.14d specifics).
- Exit: game boots, menu UI invisible but present (nothing rasterized yet),
  clean log, crash-free 5-minute idle in menu.

### P2 — Glide raster core  *(the meat)*
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
- Exit: single-player session — menu, character select, a few acts' outdoor +
  indoor areas, combat, cutscene — visually correct vs original (screenshot
  diffs), 60 fps at 640x480 on RX 560.

### P3 — libslang-vk preset chain
The payoff. Everything above already runs through our swapchain.

- Offscreen frame image feeds `slangvk_input_frame`; chain created per preset at
  startup; resolution changes (video mode switch, cutscenes) →
  `slangvk_chain_resize` / chain recreate.
- Scaling modes: integer, fit (letterbox, `output_rect` all-zero default), stretch.
- Preset management: `d2slangvk.conf` (preset path, scale mode, vsync override,
  fallback behavior); hot-reload preset on file change (chain recreate).
- Exit: CRT Royale + a scanline + a simple sharpening preset run live in-game at
  1440p/4K output from 640x480 input; preset hot-swap works; frame cost measured.

### P4 — Hardening + packaging
- Failure paths: no Vulkan → unfiltered present; device lost → libslang-vk's
  recovery path (SLANGVK_ERR_DEVICE_LOST_RECOVERY_FAILED handling); alt-tab,
  windowed mode (`-w`), resolution changes.
- Crash reporting: hook the fork's VEH crash reporter (`support/d2_crash.cpp`,
  `d2crash-*.txt` — lives on `master`, see §0) to include the DLL's
  last-N-Glide-calls ring buffer.
- Packaging: drop-in zip (`glide3x.dll` + `presets/` + `d2slangvk.conf`) laid
  over a D2 install; README with video-mode instructions.
- CI: extend the fork's `tools/check-build.sh` pattern with a `check-glide3x.sh`
  (build DLL, verify exports present via objdump, link-check); agent sandbox
  build-only smoke (no ICD in CI).

### P5 — Stretch
- Per-preset retro CRT curvature/geometry defaults per D2's 4:3 vs wide hacks.
- `grLfbWriteMode` screen-capture compatibility (F9 screenshot stays paletted-correct).
- Revisit an in-tree renderer only if Glide-surface limitations bite.

---

## 6. Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | libslang-vk C++23 doesn't build i686-mingw gcc-13 | Med | High (blocks all) | P1 spike first; fallbacks: C++20 core / reduced core / clang the DLL only (llvm-mingw is fine for OUR DLL — header drift only affects the game tree, not a fresh library) |
| R2 | No 32-bit Vulkan loader/ICD on VTSTech's box | Low | Med | 32-bit vulkan-1.dll ships with modern GPU drivers (verify on RX 560 early); fail-soft unfiltered present regardless |
| R3 | 1.14d doesn't expose Glide video mode in UI | Med | Med | Registry/video-test investigation in P1; d2dx documents 1.14d Glide selection; worst case a tiny d2vidtst-style selector or conf-forced mode |
| R4 | Glide state semantics subtly wrong (combine functions) | Med | Med | In-tree D2Glide is a complete usage spec; d2dx as prior art; screenshot-diff loop vs original renderer in P2 |
| R5 | Optimized codegen breaks decompiled game code | — | — | Non-issue: project policy pins -O0/-g; our DLL may optimize freely (separate target) |
| R6 | TMU memory emulation complexity | Low | Low | Game's usage bounded by in-tree caches; allocate-on-upload + LRU eviction is enough (grTexMinAddress/MaxAddress answered generously) |

---

## 7. Test plan

- **Sandbox (agent):** static QA only — patch round-trips, anchor-grep gates,
  diff review, export-table reads of DLLs the user supplies. No local build
  (no mingw, no apt; recreate per `HISTORY.md` H7 only if a sandbox build
  becomes unavoidable). The headless slangvk smoke (lavapipe per
  `libslang-vk/benchmarks/README.md`) and the 2,658-preset corpus parse bench
  are recreate-on-demand for wrapper work.
- **VTSTech VM (Ubuntu 26.04, VirtualBox):** optional cross-build parity
  (`apt install g++-mingw-w64-i686`) and a wine smoke test (Vulkan via
  lavapipe) — menus only.
- **Real hardware (Win11 + RX 560):** the authoritative pass per phase exit
  criteria; frame-time capture at 640x480→1440p; preset sweep with the corpus.

## 8. References

- libslang-vk: `include/slangvk/c.h` (whole integration on one page), `USAGE.md`,
  `benchmarks/README.md` (lavapipe recipe), `corpus/` (preset pack).
- d2dx (bayaraa): Glide→D3D11 wrapper for D2 — API semantics prior art (MIT).
- In-tree spec: `D2Client/Renderer/Glide.cpp` (loader, 45 exports),
  `D2Glide/Renderer/Glide.{h,cpp}` + `gl*Cache` (usage patterns),
  `D2Client/Renderer/Renderer.h` (renderer contract typedefs).
- chocolate-doom-vk, DevilutionX-libslang-vk: house precedents for the present-pass
  pattern and preset UX.
- House state docs: `VANILLA-PLUS.md` (branch charter), `handoff.md` (current state +
  live tasks) + `HISTORY.md` (closed record), `/home/z/my-project/worklog.md` (journal).
- Upstream: jaenster/redivivus PR #10 (sound series — open, `Fixes #N` in place);
  PR #6 (glide) closed by the maintainer 2026-10-27 — defect fixes return via the next
  Ghidra export; fork releases `D2LoD-Vanilla-Plus-0.0.1`/`0.0.2`.
