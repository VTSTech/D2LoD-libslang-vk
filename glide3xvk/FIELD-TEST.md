# FIELD-TEST.md — P1 field test on a vanilla retail D2LoD 1.14d install

You do **not** need a rebuilt `d2.exe` for this. Retail 1.14d already contains the
seam we plug into: its Glide renderer does `LoadLibraryA("glide3x.dll")` and
resolves 45 stdcall exports — the Redivivus recovered source is exactly that code,
and nGlide / d2dx have used the same drop-in deployment model on stock installs
for years. The rebuilt d2.exe behaves identically if you have it, but it is not
required until P4 (optional VEH crash-report hook).

## What this DLL is right now (P1 scaffold)

- All 45 Glide exports, correct stdcall decorations, state-tracking stubs.
- `grGet(0xF)` (`GR_NUM_BOARDS`) answers 1 so the game **accepts** the renderer.
- `grSstWinOpen` → Vulkan: volk → instance → Win32 surface → device → swapchain.
- `grBufferSwap` → clear **black** + present. Nothing is rasterized yet (that's P2),
  no shader presets yet (that's P3).

## Requirements

- Any retail Diablo II: LoD **1.14d** install (GOG / Blizzard downloader). The game
  is 32-bit; it runs fine on 64-bit Windows 10/11.
- A Vulkan-capable GPU with a current driver. Drivers install the 32-bit loader
  by default — you can verify `C:\Windows\SysWOW64\vulkan-1.dll` exists.
- `glide3x.dll` from the field kit.

## Install

1. If the kit zip shows "blocked": right-click the zip → Properties → **Unblock**
   → OK, *then* extract (avoids any SmartScreen/AV friction with an unsigned DLL).
2. Copy `glide3x.dll` into the game root, next to `Game.exe`
   (e.g. `C:\Games\Diablo II\`).

That's the whole install. Nothing else in the game folder is touched.

## Run

```
Game.exe -3dfx
```

- **Fullscreen only** — 1.14d excludes the Glide renderer from windowed mode
  (do not pass `-w`).
- Alternative selection path: run `D2VidTst.exe` from the game folder and pick the
  **3dfx Glide** renderer — it writes the same video-mode setting.
- To revert: delete `glide3x.dll` (or just launch without `-3dfx`). The install is
  otherwise untouched.

## What success looks like (P1 exit criteria)

1. The game launches fullscreen to a **black screen — that is correct**. Nothing is
   rasterized yet; P1 only proves the DLL loads, Vulkan initializes, and present runs.
2. The menu is alive behind the black: music/menu sounds play, ESC and clicks
   register. The cursor may be invisible or glitchy — expected at this stage.
3. A `glide3x.log` appears next to `Game.exe`. A healthy run looks like:

   ```
   [g3xvk] glide3xvk scaffold <build-id> — loaded into C:\Games\Diablo II\Game.exe
   [g3xvk] vk: loader ok, api 1.x
   [g3xvk] vk: picked AMD Radeon RX 560 ...
   [g3xvk] vk: swapchain 640x480 fmt=... images=...
   [g3xvk] grSstWinOpen: hwnd=... res=7 (640x480) ... colbuf=2 aux=0
   [g3xvk] grGet:0xF(#1): TMU count -> 1
   [g3xvk] frame 300: swaps=300 draws=... texdl=... paldl=...
   ```

   `grGet:0xF -> 1` **before** `grSstWinOpen` is the game accepting the renderer;
   nonzero `texdl`/`paldl` counts mean P2 will have real work queued.
4. Idle in the menu ~5 minutes: no crash, log stays clean.

## If something's off

| Symptom | Meaning / fix |
|---|---|
| No `glide3x.log` at all | The DLL never loaded: wrong folder, AV quarantine, or the zip was blocked — Unblock, re-extract, confirm the DLL sits beside `Game.exe`. |
| Log says Vulkan unavailable / fail-soft | 32-bit loader missing: reinstall the full GPU driver package (includes 32-bit), recheck `SysWOW64\vulkan-1.dll`. The game keeps running; the screen just stays black. |
| Game errors about Glide / falls back to DDraw | Mode didn't take: try the `D2VidTst.exe` route, and confirm you're fullscreen (no `-w`). |
| Game window opens but immediately exits | Send back `glide3x.log` — the last lines name the failing export or Vulkan step. |

Battle.net is out of scope for this project — test offline / single-player only.

## What to report back

- The whole `glide3x.log` file.
- One line: black screen? menu sounds? cursor visible? any crash?
- GPU + driver version (e.g. "RX 560, Adrenalin 26.x").

---

# Round 2 — P1.1 (caps fix)

## Round 1 result (2026-10-24, RX 560, vanilla retail 1.14d)

The v1 DLL **loaded, initialized Vulkan end-to-end and shut down cleanly** — full
P1 plumbing validated on real hardware. But zero frames were presented: after
`grSstWinOpen`, D2Glide runs a caps block (`grGet` pnames 0xA/0xB/0x11/0x13/0x24/0xE)
and **hard-fails its init when `GR_NUM_TMU` (0x13) comes back 0**
(`if (!gnGlideTextureMemoryUsed) return 0;` in `D2Glide/Renderer/Glide.cpp`).
The v1 stub answered 0 → the game dropped us by its own rules. Decoded against
the real 3dfx glide3x source (diget.c, Voodoo2 build): the decompiler names were
misleading — 0xF is `GR_NUM_BOARDS` (the accept gate), 0x13 is `GR_NUM_TMU` (the
init gate), 0x24 is `GR_TEXTURE_ALIGN`, 0xE is `GR_MEMORY_UMA`.

## What changed in the DLL (v2 / P1.1)

1. **Voodoo-class caps table** — answers mirror 3dfx's own Voodoo2 glide3x:
   BOARDS=1, TMU=1, FB=1, MAX_TEXTURE_SIZE=256, TEX_ASPECT=3, UMA=0,
   TEXTURE_ALIGN=8, GAMMA=8-bit/32 entries. Single-TMU profile = the smallest,
   best-tested D2 code paths (no secondary-TMU combine layouts).
2. **Fallback detector** — at detach, the DLL logs if `D2Direct3D.dll` /
   `D2DDraw.dll` / `D2GDI.dll` are loaded (i.e. the game moved on to another
   renderer). `D2Glide.dll` is our caller, always present, not diagnostic.
3. Unknown `grGet` pnames now return 0 like real glide3x (v1 returned a
   success-looking value).

## Round 2 success criteria

1. Log shows the one-line **`caps: Voodoo-class profile — BOARDS=1 TMU=1 ...`**
   after the six caps queries.
2. After the caps block, expect a **burst of state calls** (`grVertexLayout`,
   `grTexCombine`, `grCoordinateSpace`, `grTexFilterMode`, …) — that is D2Glide
   finishing init, which it never reached in round 1.
3. The menu phase then shows up as periodic **`frame N: swaps=...`** summary
   lines. Menu visible + swaps counting = P1 fully exited, P2 gets its oracle
   data. The screen may still be black-ish (clear-only present) — sounds/UI
   clicks prove the game is alive.
4. If the menu appears but the log stays silent until you quit, the detach line
   will now **name the fallback renderer** that took over — that alone is a
   useful report.

Same install/run steps as above; just replace `glide3x.dll` with the v2 build
(build id in the banner is `77cb059-dirty` or later — check the first log line).

---

# Round 3 — P1.2 (Bink video / LFB present)

## Round 2 result (2026-10-24, RX 560, `Game -w -3dfx`)

The caps fix worked — the game got further than ever: state-call burst,
7 × `grTexDownloadMipMap` (P_8 palettized, exactly D2's art format), palette
uploads, one frame presented, and then the **Bink video path**: at 640x480 the
game called `grLfbLock(WRITE_ONLY, FRONTBUFFER, 8888)` in a ~25 fps rhythm.
Source-confirmed (D2Glide/glBlocks.cpp `GLBLOCKS_RenderBinkVideoFrame`): D2
plays cutscenes by `BinkCopyToBuffer`-ing frames **directly into the locked
front buffer** and never calls `grBufferSwap` — on a real Voodoo the front
write *is* the present. v2 dropped those writes on unlock → videos invisible.

Bonus finds from the round-2 log: the 800x600-swapchain/640x480-backend line
pair is windowed-mode reality (the Vulkan swapchain must match the 800x600
window client area while the game renders 640x480 video into it) — v3 handles
it explicitly by scaling in present. And the doubled `palette(#1)` log lines
were a logging bug (tag truncation), fixed.

## What changed in the DLL (v3 / P1.2)

1. **LFB front-buffer present** — `grLfbLock(WRITE_ONLY, FRONT)` hands the game
   a real shadow buffer; `grLfbUnlock` scales/converts it (8888 straight copy,
   565/555/1555 expand, row flip for LOWER_LEFT origin) and **presents**.
   This is the Bink cutscene path, live.
2. **Windowed scale** — present scales the game's frame (e.g. 640x480) to the
   window client area (e.g. 800x600); new log line names both sizes.
3. Logging fixes: palette counter, `grVertexLayout` params now decoded
   (`0x01=XY 0x30=PARGB 0x50=Q0 0x40=ST0` — the 20-byte D2 vertex, P2 contract).

## Round 3 success criteria

1. **You should SEE the intro/logo videos playing** (black letterbox around
   them is normal). Sound was always there; now picture joins it.
2. Log shows the new pair during video:
   `grLfbLock ... -> 640x480 shadow stride=2560 (unlock=present)` /
   `grLfbUnlock ... -> front-buffer frame presented (P1.2)`.
3. If you ran `-w`: a `vk: windowed scale — window 800x600, game renders
   640x480` line right after `backend ready`.
4. After the videos, the menu stays black — expected (menu needs P2
   rasterization: `grDrawVertexArrayContiguous` is still a counted no-op).
   Menu music + clicks = game alive behind it.
5. Same quit/restart safety as before; activity summary at detach should now
   show `grLfbLock`/`grLfbUnlock` counts.

Windowed note: v2 round-2 proved `Game -w -3dfx` loads the Glide renderer fine
on this install (the doc's earlier fullscreen-only warning was overly
conservative) — and `-w` is what exercises the scale path, so keep using it.

Build id: `77cb059-dirty`, banner line says **P1.2 = Bink LFB present +
windowed scale** — check the first two log lines to confirm you're on v3.

---

# Round 4 — P2.1 (textured quad raster: title screen)

## Round 3 result (2026-10-24, RX 560, `-w -3dfx`)

**Intro videos play** — the Bink/LFB path is live (61 lock/unlock presents).
After click-through, the title screen phase went into full Glide draw mode:
**13,729 `grDrawVertexArrayContiguous` fan-quads** (mode=5, count=4,
stride=0x1C), 13,733 `grTexSource` binds, 746 texture downloads, 8 palettes —
the menu renderer works end-to-end, it just had nothing rasterizing the quads.

Decode completed from game source + 3dfx headers:
- Vertex (stride 0x1C): x,y float @0, PARGB dword @8, oow @12, s,t @16/20.
  UI quads use oow=1.0 and screen-pixel coordinates.
- Textures always `GR_TEXFMT_P_8` (palette indices); TMU-addressed; s,t span
  [0,256] per wrap; lod = log2(max dim), aspect = log2(w/h) (the game's own
  `RENDERER_GLIDE_CalculateTextureSize` convention).
- Observed blends: (ONE,ZERO)=opaque, (ONE,ONE)=additive, (ZERO,SRC_COLOR)=
  multiply. Chromakey on with value 0xFF (P_8 index compare on real HW).

## What changed in the DLL (v4 / P2.1)

New `g3x_frame` module — a software rasterizer into the game framebuffer:
1. **Texture cache** — `grTexDownloadMipMap` stores a copy keyed by TMU
   address (the game re-binds by address, exactly like the real TMU);
   `grTexSource` binds; dims from lod/aspect.
2. **Quad raster** — fan triangles with barycentric interp, affine P_8
   sampling (`s/256 * texw` wrapped), palette lookup, chromakey index skip,
   vertex-color multiply, and the three observed blend modes.
3. **Unified framebuffer** — `grBufferClear` fills it, quads draw into it,
   Bink LFB writes into it (same buffer), `grBufferSwap`/LFB-unlock present
   it (scale to window + gamma 1.55 LUT).
4. Host-side smoke test (11 checks: palette, chroma, color multiply, all
   blend modes, barycentric/top-left fill correctness, present scale+gamma).

## Round 4 success criteria

1. **Title screen visible**: logo, menu buttons, text (with menu music).
   Colors/brightness may be slightly off in spots — report what you see.
2. Possible known-rough edges for this first raster build:
   - Wrong-looking text/tiles if a texture dims or s/t convention case we
     guessed wrong — screenshot-worthy either way.
   - Fade transitions (the multiply blend) may look off.
   - Depth-sorted world content (gameplay) is NOT this build's scope.
3. Log: `grTexDownloadMipMap` / `grTexSource` lines unchanged; banner says
   **P2.1 = P_8 textured quad raster (menu/title)**.

Same install: replace `glide3x.dll`, run `Game -w -3dfx`. Send back the full
log + what the title screen looks like (a screenshot says more than words).

---

# Round 5 — P2.2 (texel-unit s/t + constant fills)

## Round 4 result (2026-10-24, RX 560, `-w -3dfx`)

**The title screen renders** — logo, background, most UI visible. Two precise
defects reported: (1) **menu buttons appear black**, (2) **character-select
sprites are cut in half vertically**. Both root-caused in the game source —
they are the same bug in different shapes.

Root cause (decoded from `D2Glide/Renderer/Glide.cpp` + `globals/D2Glide.cpp`):
the game's texture-coordinate span table `gaPerspectiveImageSizeTable` encodes
the S/T spans **per aspect ratio** as `{0x100,0x100 | 0x100,0x80 | 0x100,0x40 |
0x100,0x20}` — i.e. **s,t are texel counts** (s spans [0,texwidth],
t spans [0,texheight]), both axes living in a 2^lod coordinate space. v4
sampled `texel = s/256 * texdim` (the classic "[0,256] normalized" Glide
convention), which is only correct for 256×256 squares:
- tall 128×256 char sprites: game sends s∈[0,128] → v4 sampled texel x∈[0,64)
  → **left half stretched over the whole sprite = "cut in half vertically"**;
- wide 256×64 buttons: game sends t∈[0,64] → v4 sampled rows 0..15
  → **top quarter of the button art stretched over the button = "black buttons"**;
- 256×256 squares: identical under both rules → everything else looked right.

Second gap: `SetColorCombine(2)` (= `grColorCombine(LOCAL, *, LOCAL_CONSTANT)`)
draws the **solid constant-color fill** used by fade/dim rects; v4 had no
`grConstantColorValue` support so those quads sampled whatever texture was
bound. Also decoded: the game packs the constant as `(gray << 8) | blend_alpha`
(`RENDERER_GLIDE_ScalePaletteValue(v) = table[v] << 8`) — alpha in the **low**
byte, gray in bits 15..8 — and `grAlphaBlendFunction(SRC_ALPHA, ...)` reads
that low byte (all D2 blend modes pair with `grAlphaCombine(LOCAL_CONSTANT)`).

## What changed in the DLL (v5 / P2.2)

1. **Texel-unit sampling** — `texel = s * (2^lod / 256)`, wrapped mod (w,h).
   Identity for 256² squares (no regression), full-width for tall sprites,
   full-height for wide buttons — the one rule that fits the game's table for
   every aspect and lod (verified against 16×8, 64×256, 128×256, 256×64 cases).
2. **Constant-fill combine** — `grColorCombine` mode 2 now rasterizes the
   constant (gray replicated to RGB) instead of the bound texture; blend alpha
   for SRC_ALPHA/ONE_MINUS_SRC_ALPHA modes comes from the constant's low byte.
   This repairs the fade/dim rect path (`DrawSolidRectAlpha`).
3. **Chromakey** — keeps the P2.1 palette-index rule (it held up in the field)
   and adds the format-consistent color match (`texel palette RGB == chroma
   value RGB`), covering D2's transparent-blue palette entries either way.
4. `grTexTextureMemRequired` dims convention fixed (was transposed for
   non-square; now shares the raster's `g3x_tex_dims`).
5. Field diagnostics for this round: `frame: bind tmu=... 0x... -> WxH (lod L)`
   for the first distinct texture bindings, `drawshape #n: mode= count=` for
   distinct draw shapes, and the first 8 fans' raw vertices
   (`fan#1: v0(x,y|s,t) ...`) — ground truth if anything still maps wrong.

## Round 5 success criteria

1. **Menu buttons show their art** (stone frame + label), and the
   **character-select sprites are whole** (no vertical half-cut).
2. Title screen should look the same or slightly better than round 4
   (square-path sampling is bit-identical; only the blend-alpha source changed).
3. Screen fades/transitions (black flash between screens) should now actually
   fade — watch for smooth dim-to-black instead of an abrupt cut.
4. Log: banner says **P2.2 = texel-unit s/t (2^lod space) + constant fills**;
   new `frame: bind` / `drawshape` / `fan#` diagnostic lines appear early.
5. If anything still looks wrong: full log + screenshot — the vertex-dump
   lines will pinpoint the remaining convention mismatch.

Same install: replace `glide3x.dll`, run `Game -w -3dfx`. Build id
`77cb059-dirty` (banner line is the version marker).

---

# Round 6 — P2.3 (pointer-array strips/fans + color-space chromakey)

## Round 5 field result (v5)

Title renders; button art and character sprites fixed. Three defects left:

1. **Buttons show their picture but not the text** — and the copyright line is
   only partly visible.
2. **In game: character visible, ground mostly black.**

## Root causes found in the game tree (D2Glide/Renderer/Glide.cpp)

1. **The world floor/walls never reached the rasterizer.** `DrawGroundTile`
   draws through `grDrawVertexArray` — the POINTER-ARRAY variant (an array of
   vertex pointers, not one contiguous buffer): 5 triangle strips of 12
   vertices per tile (2 strips of 10 for walls; a single 4-vertex fan in low
   quality). v5 implemented that export as a counted no-op, so the entire
   floor/wall layer was skipped and the black clear color showed through.
   The per-vertex light tint (`gpGlideLightingTable`, alpha forced 0xFF) and
   the 6×6-vertex tile grid decode clean against our existing vertex layout.
2. **The chromakey index rule ate the text.** Real Voodoo chroma compares the
   POST-PALETTE COLOR (D2 parks pure blue on its transparent entry; chroma
   value 0x000000ff). P2.1's extra raw rule — skip palette INDEX 255 — also
   discarded font/text ink texels that legitimately sit at the top of the
   palette: button/menu text vanished, bright cores dropped out of the
   copyright line.

## What changed in the DLL (v6 / P2.3)

1. **`grDrawVertexArray` rasterizes now**: mode 5 = fan (as before), mode 4 =
   triangle strip with per-triangle winding normalization (strips alternate;
   the top-left ownership rule still assigns every shared edge exactly once —
   no seams under opaque, no double-blend under additive).
2. **Chromakey is color-space only** (palette RGB == chroma value RGB). Index
   255 texels draw again — that is the text/glyph ink.
3. Texture table raised 1024 → 4096 slots (in-game tile churn headroom).
4. Diagnostics: `parr#n: mode= count= c0..c3=` (first pointer-array draws +
   vertex colors), `fan#` dumps now include vertex colors, palette downloads
   log entries 0/128/255, texture misses log the first 8 distinct addresses +
   a running total, detach line gained `texmiss=N`.

## Round 6 success criteria

1. **In game: the ground is THERE** — textured floor tiles with lighting
   (dark corners near walls, bright around light sources), walls drawn.
2. **Button/menu text is visible** (labels on plates, copyright line complete
   with no hollow cores).
3. Title/charselect unchanged or better (chroma change only un-skips index
   255; if any sprite now shows speckles where it was clean, that texture
   really uses index 255 as transparent — screenshot will say so).
4. Log: banner **P2.3 = pointer-array strips/fans (ground+walls) + color-space
   chromakey**; `parr#1..4` lines right after entering game (expect mode=4
   count=12 with c-values = 0xFFrrggbb lighting tints); palette lines now show
   `pal[0]/pal[128]/pal[255]`.
5. Paste the full log + a screenshot (in-game view + title screen). The
   `parr`/`fan` color dumps and the palette entries will pin down anything
   still off (next suspects: lighting-table values, text tint path).

Same install: replace `glide3x.dll`, run `Game -w -3dfx`. Build id
`77cb059-dirty` (banner line is the version marker).

---

# Round 7 — P2.4 (negative-s/t wrap + fill-rect fast path + session log)

## What the round-6 log showed

- The pasted session presented 112 frames: mode probes → Bink videos (51 LFB
  presents) → menu → a short gold-palette phase with multiply + 50%-alpha
  overlays. **Zero `grDrawVertexArray` calls and zero tex misses** in it —
  that phase was the loading screen, not gameplay. The ground you saw came
  from an earlier launch, whose log was erased: the DLL used to TRUNCATE
  glide3x.log on every start.
- Meanwhile the game source gave up a real ground defect:
  `RENDERER_GLIDE_InitGroundTileVertices` builds the 6x6 floor lattice with
  edge adjustments that push texture coords slightly NEGATIVE (S ≥ -3, T ≥
  -1 for a 256x256 tile). Glide textures WRAP — negative coords sample the
  far edge (which sits in the parked/chroma region, invisible on a real
  Voodoo). P2.1–P2.3 clamped them to texel 0 instead, smearing texel column
  0 along the top-left edges of every floor tile.
- Fades/dim rects (DrawSolidRectEx/Alpha = constant fill quads) ran through
  the per-pixel barycentric raster at 800x600 — that is the ~7 fps dip.

## v7 change list (banner: P2.4)

1. **Negative s/t floor-wrap** in the sampler — the ground diamond edges now
   wrap like real hardware instead of smearing column 0.
2. **Fill-rect fast path**: axis-aligned constant-fill quads fill scanlines
   directly (no barycentrics) — fades and dim overlays should be much
   smoother. Non-rect quads and all textured draws are untouched.
3. **Session log**: glide3x.log now APPENDS with a
   `==== glide3xvk session ... ====` banner per launch (2 MB rotation to
   `.1`). A quick relaunch no longer erases the previous session's log —
   copy the log after your LAST session, everything is in there.
4. **Per-path draw totals**: `grSstWinClose` and `grGlideShutdown` now print
   full counters unconditionally (vac / parr / strip / fan / point / line /
   tri / skip / texmiss), and every 120 frames an activity line
   (`frame N: vac=... parr=... skip=... texdl=... paldl=...`) fires even
   when the per-call tags are budget-quiet.

## Round 7 success criteria

1. Ground/walls/text still render as in round 6 (the wrap only changes a
   ~3-texel strip at each floor tile's top-left edges — same or slightly
   cleaner).
2. Fades and the loading screen feel smoother (fast path).
3. Log: banner **P2.4 = negative-s/t wrap ...**; at exit a
   `grGlideShutdown: swaps=... (vac=... parr=... strip=... fan=...)` line.
   **Play a real session** (menu → game, walk around, open inventory) and
   paste the log AFTER quitting — with append mode the whole thing survives
   extra launches.
4. The counters settle two open questions from this round:
   `parr>0` = the game is drawing ground strips (perspective or normal
   quality path live); `parr=0` with `vac` huge = everything goes through
   the fan path. Either way the next fix targets exactly what your session
   used.

Same install: replace `glide3x.dll`, run `Game -w -3dfx`. Build id
`77cb059-dirty` (banner line is the version marker).

# Round 8 — P2.5 (scanline raster + line/point + constant-color lanes)

## What the round-7 log showed

First FULL gameplay session: intro video (61 LFB presents), title screen
(23 frames), real in-game ground (46 frames, `parr=30504` = the mode-4
12-vertex strips drawing the floor), back to menu, clean exit. `texmiss=0`
— every texture bind hit the cache. The world layer is alive.

Two problems are left, both visible in the log:

1. **Speed.** 46 game frames took 10.9 s ≈ **4.3 fps**. The CPU rasterizer
   evaluated three cross-products per pixel plus two `floorf` calls per
   pixel — that cost is the whole frame budget.
2. **`grDrawPoint` x79 / `grDrawLine` x170 were counted but never drawn.**
   Those are the game's stroke paths (D2Glide `DrawLine`/`DrawPoint` —
   automap-style overlays and the radial-light dots). They render now.

Bonus fix found in the game tree: `grConstantColorValue` is built as
`ScalePaletteValue(v) | alpha` = palette entry (0xFFRRGGBB) **<< 8** | alpha,
so the dword is `[R:24-31][G:16-23][B:8-15][alpha:0-7]`. The old decode
replicated byte 1 into all three lanes — correct for grays (every fade), but
colored constants came out wrong: the round-7 log's `0xf4c04cc0` is
**orange (244,192,76)** — a fire/light glow — not dim gray.

## v8 change list (banner: P2.5)

1. **Scanline rasterizer.** Each triangle's three edges are affine along a
   scanline, so each row's coverage is one px interval (solved per edge);
   the interior fills with interpolated s/t/color and NO per-pixel edge
   tests. Only the boundary pixels run the exact old per-pixel test, so
   tie pixels and shared-edge ownership behave exactly like P2.4 (no new
   seams; verified pixel-exact against the old algorithm on 120 random
   triangles + the real 12-vertex ground strip in the host test rig).
2. **Sampler fast math**: the two per-pixel `floorf` calls are replaced by
   an exact integer trick (float + 4096 truncation — 4096 is a multiple of
   every texture width). Negative-wrap semantics unchanged (test-pinned).
3. **Blend classes hoisted**: the game's five blend pairs (opaque /
   additive / alpha / multiply / other) compile to direct inner-loop math
   instead of a per-pixel switch loop; lane math is bit-identical to P2.1.
4. **Vertex-color palette cache**: draws with one constant vertex color
   (your ground strips submit `ff808080` on all corners) multiply the
   palette once per color instead of once per pixel.
5. **Chroma as a 256-entry table**: the post-palette color match is
   precomputed per palette download (same rule as P2.3, cheaper per pixel).
6. **grDrawPoint / grDrawLine rasterized**: point = rounded pixel, line =
   Bresenham with clipping, colored by the constant fill through the
   current blend (opaque for alpha=0xff automap strokes, alpha-scaled
   otherwise).
7. **Constant-color RGB lanes fixed** (fire glows, tinted overlays).
8. **Present/clear fast paths**: identity gamma at 1:1 window = straight
   copy; clear color 0 = memset.
9. **Frame pace in the log**: the every-120-frames activity line now
   includes `avg=NN.Nms` — the field-visible speed number.

Host rig: 61/61 checks pass, including the new parity/reference,
blend-class, line/point, lane-decode and present tests. The round-7 field
draw mix (665 ground strips + 114 sprite quads per frame at 800x600)
benchmarks at ~4.6 ms/frame on the build host — roughly 40x the old
raster's speed on that mix.

## Round 8 success criteria

1. **Speed.** The menu and game should now feel fluid (tens of fps). The
   `avg=NN.Nms` number in the `frame ...` log lines is the ground truth —
   paste it either way.
2. **Ground/walls/text look the same as round 7** (the raster change is
   coverage-exact; interior texel boundaries can shift by one texel where a
   sprite's coordinates land exactly on a boundary — invisible).
3. **If you toggle the automap (TAB)** it should now draw its lines/dots.
4. Log: banner **P2.5 = scanline raster ...**; `frame ...` lines with
   `avg=`; the usual shutdown totals.

Same install: replace `glide3x.dll`, run `Game -w -3dfx`. If anything looks
wrong, a screenshot plus the log tells the whole story.

---

# Round 9 — P2.6 (build 77cb059-dirty, compiled Oct 24 2026 ~08:01)

## What round 8 showed (your log, 03:42:44–03:43:00)

Full session ran healthy and fast (avg 36.8 ms ≈ 27 fps, 293 frames, 146k
draws, zero texture misses). Two defects remained:

1. **Title buttons still not right** — the button art textures are 64x32
   (lod 6). Every sprite/UI quad with a texture smaller than 256 on a side
   was being sampled through the old scalar rule `texel = s * 2^lod/256`,
   which squeezed a 64x32 button into its **top-left 16x8 corner**. The
   ground tiles looked correct only because they are 256x128 (lod 8), where
   that factor is exactly 1.0.
2. **"A lot missing" in game** — same root cause, different victim: the
   wall layer. Wall subtiles are 32x32 textures sampled by the game's
   perspective lattice through S,T in [0,256]; with the old rule they
   sampled a 4x4 corner (smeared mush), and every small sprite, item icon
   and inventory-art quad was crushed the same way.

## Root cause (from the game tree, ground truth)

D2 submits **two coordinate conventions**, and P2.2–P2.5 applied one rule
to everything:

- **Texel units** — DrawPalettedQuad (sprites/UI/buttons/text: S/T spans =
  the texture dims via gaPerspectiveImageSizeTable) and the ground lattice
  (InitGroundTileVertices: spans x256/x128 = the 256x128 floor texture).
- **Voodoo 256-space** — the glBlocks wall lattice (InitPerspectiveTables:
  S=col*128, T=row*64, up to 256 regardless of the 32x32 subtile dims).

## P2.6 changes

1. **Per-draw, per-axis s/t space auto-detect**: if a quad's |s| (or |t|)
   extent exceeds the texture dim, that axis is in the 256-space and is
   sampled as `texel = coord * dim/256`; otherwise it is a direct texel
   unit. This fixes buttons, wall subtiles, item icons, inventory art and
   all small sprites without touching the paths that already worked.
2. **RGB_565 textures** (format 0xa): the in-game Smacker video quads
   (GLSMACK_DrawSmackerQuad, 256x64 565 textures) rendered black in every
   previous build — now decoded.
3. **New diagnostics**: `quad#N: tex WxH texel|256sp v0(...) ...` lines
   (first quads per distinct dims/space class — direct field proof of the
   fix), `sp256=` and `rgb565=` counters in the frame lines.

Host rig: 76/76 checks pass, including three new P2.6 tests (wall lattice
256-space on a 32x32 subtile, 64x32 button full-width texel sampling,
565 decode) and the re-based strip/chroma tests.

## Round 9 success criteria

1. **Title buttons**: full button art (frame + label) visible.
2. **In game**: walls standing, item/inventory art correct — "a lot
   missing" should collapse to small stuff if anything remains.
3. In-game Smacker/cutscene video quads (if any play) no longer black.
4. Log: banner **P2.6 = per-draw s/t space auto-detect ...**; the new
   `quad#` lines; `sp256=`/`rgb565=` in `frame ...` lines.

Same install: replace `glide3x.dll`, run `Game -w -3dfx`. Screenshot plus
log tells the whole story.

---

# Round 10 — v10 (P2.7): uniform square-mip s/t space

## Round 9 field report

"still a lot of black bars, text on buttons still not correct."

The round-9 log's 14 `quad#` lines were decisive. Checking every observed
draw against three candidate coordinate rules showed the P2.6 extent
auto-detect wrong on 7 of 14 classes while the **uniform square-mip rule**
(`texel = coord * 2^largeLod/256` on BOTH axes) is exactly 1:1 on all of
them — including the wall lattice, ground lattice, border strips and every
sprite/UI class:

| draw | declared | screen | P2.6 sampled | P2.7 samples |
|------|----------|--------|--------------|--------------|
| quad#5 button | 64x32 | 35x30 | 35x15 (2x vertical crush) | 35x30 |
| quad#12 mini-panel | 128x64 | 128x35 | 128x18 | 128x35 |
| quad#14 sprite | 16x128 | 16x93 | 2x93 (8x crush) | 16x93 |
| quad#4 strip | 32x128 | 32x88 | 8x88 | 32x88 |
| quad#8 sprite | 16x32 | 16x25 | 8x25 | 16x25 |
| quad#11 sprite | 16x64 | 16x35 | 4x35 | 16x35 |
| quad#6/#9/#13, walls, ground, borders | — | — | 1:1 | 1:1 (unchanged) |

Ground truth, one level deeper in the game tree:
`RENDERER_GLIDE_DrawPalettedQuad`/`DrawPerspectiveImage` scale raw sprite
coords by `gaPerspectiveImageSizeTable[aspect]` = {S,T} spans (256,256)
square, (256,256>>a) wide, (256>>|a|,256) tall. Reducing
`texel = coord/Span*dim` for every aspect collapses to
`texel = coord * 2^largeLod/256` on both axes — i.e. coordinates ALWAYS live
in the square mip space (256 units = one full square), with the image in the
[0,w)x[0,h) sub-rectangle. The P2.6 heuristic scaled the short axis by
`dim/256` instead of `2^lod/256` — exactly the crush table above. Crushed
sub-rect sampling also explains the black bars: icon/HUD/text quads sampled
mostly empty atlas padding (palette index 0 = black, drawn opaque).

## P2.7 changes

1. **Uniform square-mip sampling** — `tex_space()` now returns
   `2^largeLod/256` on both axes unconditionally (the P2.6 extent heuristic
   is retired). Every currently-correct class is bit-identical (lod-8
   textures scale 1:1; wall subtiles were already sampled dim/256 = square
   for 32x32); the seven crushed classes are fixed.
2. **Fill-rect diagnostics** — the round-9 log shows ~14 black LERP fills
   per in-game frame (SetDrawMode 0/1/2 = 25/50/75% black dim rects) and
   "black bars" is still unexplained; `fill#N: rect(...) src=0x... alpha=...
   blend=(sf,df)` lines now pin the first fill of each distinct class WITH
   its screen rect.
3. **Stack-overrun fix** — `grGlideShutdown`/`grSstWinClose` passed a 5-slot
   array into the 7-slot (now 8-slot) draw-stats writer; 2-3 unsigneds were
   silently smashed on every shutdown log since P2.6.
4. Frame lines now print `sub256=` (draws on sub-256-mip textures — the
   fixed class) and `fill=` (constant-fill quads); `quad#` lines print the
   applied scale (`sc=`) and lod.
5. New host tests pin the EXACT field geometry of round-9 quads #5 and #11.

Host rig: 84/84 checks pass.

## Round 10 success criteria

1. **Title buttons**: label text correct (no more vertical crush/garble).
2. **Inventory / item icons / skill icons / HUD art**: render at 1:1 — the
   "black bars" should collapse or vanish; whatever black rectangles remain,
   the new `fill#` lines now name their exact screen rects.
3. Walls/ground/title (all previously correct) unchanged.
4. Log: banner **P2.7 = uniform square-mip s/t space ...**; `quad#` lines
   with `sc=`; `fill#` lines (please capture a session that enters the game
   and opens the inventory again).

Same install: replace `glide3x.dll`, run `Game -w -3dfx`.

## Round 11 (P2.8, build d2-glide3xvk-p28) — diagnostics for the two open symptoms

Field evidence (first screenshots, v10 session): ground/walls/HUD/big sprites
all correct; two symptoms remain — (a) in-game solid black rectangles near
units/objects (offset up-left of visible sprites, also over wall areas), and
(b) menu button labels render as garbled glyph fragments on an opaque black
box (the "v 1.14d" corner text and the button plate art render correctly).

Game-source ground truth this round:
- Unit shadows: `DrawShadow` -> blend (ONE,ZERO) OPAQUE (non-blended setting)
  or LERP + constant 0xc0 (blended), then `DrawShadowQuad` submits the shadow
  texture with ALL-BLACK vertex colors. The soft blob shape exists ONLY
  because chromakey punches the texture's transparent texels; if the chroma
  rule misses, the quad is a solid black rectangle — shape matches (a).
- Menu labels: `DrawImage` drawMode 7 -> upload mode 2 (ink remap), drawn
  via `SetDrawMode(7)` = OPAQUE; transparency again comes purely from
  chromakey. The label black box = background texels surviving the chroma
  rule; the garbled fragments = ink texels colored by the WRONG palette
  entry — both point at the palette/chroma state at text-draw time.
- `DrawSolidRect`/`DrawSolidRectAlpha` (the ~14/frame in-game fills): LERP
  + constant (palette color, alpha 0x40/0x80/0xc0), chroma toggled OFF
  around them; `DrawLightMap` draws 48x48x8 radial points with chroma OFF.

What P2.8 changes (diagnostics only — no raster semantics touched):
1. `vc0#N:` lines — the first 12 textured quads with all-black vertex colors
   (the shadow/darken class): texture dims, screen rect, blend pair, chroma
   state. Overlay these rects on the screenshot black bars.
2. `pal#N:` lines — the first 64 palette downloads with pal[0]/[1]/[128]/
   [254]/[255]. Settles where the transparency marker (parked blue) lives in
   every palette regime, and whether palettes switch around text/shadows.
3. `chroma#N:` lines — the first 32 chromakey mode/value transitions.
4. `quad#` lines now carry `blend=(sf,df) ca=alpha chm=on`.
5. Frame lines gained `vc0=` `point=` `chm=`; shutdown gained a P2.8 class
   summary + texture-table high-water/dropped counters.
6. Fixes: `grTexTextureMemRequired` charged P_8 (fmt 5) and ARGB_8332 (fmt 8)
   2 bytes/texel (real glide.h: 1) — the game sizes its texture-cache
   allocations from this; kMaxTex 4096 -> 8192 (table-full drops would bind
   stale textures).

Host rig: 84/84 checks pass (fill double-draw guard regression caught and
fixed during development).

## Round 11 success criteria

1. Reproduce BOTH symptoms in one session: title screen (garbled labels),
   then enter game with units + shadows visible on screen.
2. The log will then contain, in order: banner P2.8, `chroma#` lines,
   `pal#` lines, `quad#` lines with blend/chroma, `vc0#` lines (if the
   black rects are the shadow class), `fill#` lines.
3. Send the FULL glide3x.log (and the two screenshots again if convenient) —
   the vc0#/pal#/chroma# combination pins which of the three candidate
   mechanisms produces each symptom, and the next build fixes it directly.

Same install: replace `glide3x.dll`, run `Game -w -3dfx`.

## Round 12 (P2.10, build glide3xvk-0.3.0) — field-confirmed clean, version-tag overlay

Round 11 field report: **"Great job! Everything actually looks correct."** —
the P2.9 chroma index-0 fix resolved both open symptoms (in-game black rects
around sprites/wall tiles/shadows; black bars behind the menu button labels).
The P2 render path is visually correct on the field rig; no further raster
changes in this build.

P2.10 adds the identification layer requested for field testing:

1. **Version-tag overlay** — `glide3xvk 0.3.0 (77cb059-dirty)` drawn top-left
   every frame: 8x8 bitmap font (generated from DejaVu Sans Mono Bold), 65%
   black backdrop, 1px drop shadow. Rendered at present time, post-gamma, at
   swapchain resolution — identical in the 640x480 probe phases, the 800x600
   menu, gameplay and the LFB/Bink video path. It never enters the game's own
   framebuffer, so GetBackBuffer reads / fade logic cannot see or smear it.
2. **Off-switch** — `glide3xvk.ini` beside `Game.exe`:
   `[glide3xvk]` / `overlay=0` (default on; re-read on every context open).
3. **Identity plumbing** — session banner prints the version;
   `grGetString(GR_RENDERER)` now returns `glide3xvk 0.3.0 (<build>)`.

Host rig: 92/92 checks pass (test 16 re-based to raw-bytes-with-overlay-off;
new test 22 pins the overlay: backdrop dim factor, glyph whites, shadow
blacks, box clamping, off-switch restores exact frame bytes).

Round 12 expectations: nothing raster changes — the game should look exactly
like the round-11 clean state, plus the tag line. Any future screenshot now
self-identifies its build. Optional checks: `overlay=0` removes it after a
relaunch; the tag also appears over intro/logo videos (LFB path proof).
