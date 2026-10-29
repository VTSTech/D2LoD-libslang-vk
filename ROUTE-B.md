# ROUTE-B — GPU-side raster via compute (design doc)

Status: design, not yet built. The 0.4.x raster (`src/g3x_frame.cpp`) is a CPU
scanline rasterizer feeding a system-memory compose buffer; this doc specifies
the endgame move — the same raster, same math, same bit-exact contract,
executed as Vulkan compute kernels writing the preset chain's input image
directly. Route B (compute) over Route A (graphics pipelines) is chosen for
one reason: **bit-identical output is provable**, and this repo's whole
methodology (fxparity hashes, field-verified pixel contracts) depends on it.

The honest ROI first (README "The 25 fps ceiling"): in-game frames are
engine-capped at 40 ms and 0.4.5 already fits at ~22 ms. GPU raster does NOT
raise the 25 fps cap. What it buys:

- the ~470 draws/frame of CPU raster + blend + palette work collapse to
  vertex streaming + a few kernel dispatches (headroom, thermals, laptops);
- menus / Esc / Bink unthrottled paths stop paying the CPU raster at all;
- the slang chain gets its input image rendered in-place — the full-frame
  host upload (`gamma_scale_copy` + `slangvk_image_update`) leaves the hot
  path;
- it is the foundation the 60 fps endgame (engine-level interpolation on a
  rebuilt exe) would sit on.

## 1. Why compute (Route B) and not pipelines (Route A)

| | Route A: graphics pipelines | Route B: compute rasterizer |
|---|---|---|
| Raster coverage rules | the GPU's fixed rasterizer (top-left, 8-bit subpixel snapping) — replaces `inside_exact` / `owns_edge` | **the CPU edge math, ported verbatim** (FP32 IEEE in compute == SSE2 float on the CPU) |
| Blend | fixed-function blend rounds differently than the CPU's `(x*y+255)>>8` forms; bit-exact requires in-shader dst reads (input attachment) | the blend reads/writes the tile pixel directly — **the exact integer forms port verbatim** |
| Texture sampling | sampler precision + wrap semantics to re-prove | texel fetch from a byte heap — **exactly what `fill_span` does today** |
| Bit-exactness | boundary pixels drift ±1 (contract change) | provable, hashable, CI-checkable |
| Novel code | pipeline/permutation matrix (~30-60 pipelines) | tiling + dispatch (~400-600 LOC kernel) |

Route A is the industry-typical shape; Route B is the house-style shape. The
CPU inner loop already encodes every 3dfx semantic this project reverse-
engineered (P2.1 edge tests, P2.5 scanline intervals, P2.7 texel space,
P2.9 chroma index-0, P2.5 blend classes). Porting the loop beats re-
expressing its semantics through fixed-function state and re-proving them.

## 2. The exactness argument (and its gotchas)

The CPU raster is SSE2 float (`-msse2 -mfpmath=sse`, 0.4.5) = IEEE-754
single. Compute shading is IEEE-754 single. The port is bit-exact **iff**:

1. **No FMA contraction.** The edge math computes
   `(B.x-A.x)*(C.y-A.y) - (B.y-A.y)*(C.x-A.x)` as separate mul/sub. A
   fused `a*b-c` (single rounding) produces different bits than
   `mul` then `sub` (double rounding). The GLSL must carry `precise` on
   every edge/interval expression, and Stage 0's CI check must assert the
   SPIR-V carries `NoContraction` decoration on those ops (glslang emits it
   for `precise`).
2. **Exact division.** `1.0f / area` — IEEE divide is correctly rounded on
   both sides when `precise` (no `a/b` → `a * rcp(b)` rewrites).
3. **Trunc casts.** `(int)` float→int is trunc-toward-zero in C; GLSL
   int(float) truncates too. The `+4096.0f` floor trick
   (g3x_frame.cpp:326) ports as-is.
4. **Bounded ranges → no denormals.** Coordinates live in [0, 640]x[0, 480],
   areas in ±(640·480·2), `inv` in ±1.6e-6 — nothing approaches 1e-38, so
   driver FTZ/DAZ modes cannot bite. (Stated as an argument, not a
   guarantee; the hash campaign is the guarantee.)
5. **Integer blend forms.** All blend/chroma/palette math is `uint` math —
   identical by construction.

## 3. Architecture

```
game thread (grDraw*/grTex*/grState)          GPU (one submit per frame)
--------------------------------------        -------------------------------
read_vert → XYUV+ARGB ring  ──────────────►   [bin]  per-triangle → tile lists
tex download → texel heap (raw bytes)   ───►   [raster] per-tile workgroup,
state snapshot → draw records           ───►            scanline loop, RMW pixels
grBufferSwap:                                  [gamma]  fullscreen LUT pass
  record+submit all of the above        ───►   [overlay] composite (dirty-only)
  (chain on: slangvk_chain_render_             [chain]  slangvk render → swapchain
   swapchain consumes the image;               or [present] image → swapchain blit
   chain off: g3x_vk_present_image)
```

One command buffer per frame, one submit, one fence. The chain input image
gains `VK_IMAGE_USAGE_STORAGE_BIT` via the **existing**
`slangvk_image_options.usage` field (verified against libslang-vk's c.h —
`0 = sampled|color_attachment|transfer_src|transfer_dst`; we pass our own
mask). Zero libslang-vk API changes: `slangvk_image_get_handles` already
lends the VkImage, and `slangvk_image_update` remains the CPU-raster
fallback feed. Layout round-trips SHADER_READ_ONLY → GENERAL → SHADER_READ_ONLY
around the raster/gamma passes inside the frame's command buffer — the same
pattern the F12 upscale blit uses (SHADER_READ_ONLY → TRANSFER → back).

### 3.1 The kernel's device data model = the CPU cache model, verbatim

No samplers, no image samples, no format conversion at download:

- **Texel heap** — one device-local SSBO; each `Tex` occupies its slot's
  byte range as **raw downloaded bytes** (P_8 1 B/texel, RGB_565/ARGB_8332
  2 B/texel — the same bytes `g3x_frame_tex_download` memcpys today). The
  kernel texel-fetches `heap[base + ty*tw + tx]` exactly like `fill_span`.
- **Palette SSBO** — 256 ARGB dwords as downloaded (`s_palette`); the kernel
  does the palette lookup + the `(x*y+255)>>8` vertex-color multiply that
  `fill_span` does (the `pal_mult` uniform-VC fast path becomes a shader-
  local 256-entry recomputation when the kernel's per-triangle VC matches —
  or simply the per-pixel form; the per-pixel form IS the varying path and
  is bit-identical, so start there and only add the uniform-VC shortcut if
  the kernel shows in a profile).
- **Chroma SSBO** — the 256-byte `s_chroma_hit` table (index 0 = hit,
  P2.9 rule baked at `chroma_sync` time on the CPU).
- **Draw records** — per draw-call state snapshot: blend class, sf/df pair,
  `s_const_alpha`, `s_color_fill`, chroma on/off, tex offset/dims/lod,
  constant-fill RGB. Triangles carry their draw-record index.

### 3.2 Pass 1 — binning (the ordering guarantee)

The CPU raster's implicit contract is painter's order: triangles rasterize
in submission order; a pixel's final value is the **last** triangle's
write. Unordered compute writes would race on every translucent overlap
(additive glows over tiles). The fix is classic tile binning:

- one workgroup per triangle (dispatch `ceil(ntri/64)` × ...), each
  computes the tri's bbox, iterates the 16×16-tile grid cells it overlaps,
  and `atomicAdd`-appends its tri index to the tile's list;
- tile lists live in a device-local buffer: `ntiles × (u32 count + u32
  slots[128])`, counters reset per frame by a tiny fill dispatch (no host
  round-trip);
- a per-frame overflow counter (tile with >128 tris) → the frame falls back
  to CPU raster (fail-soft, log-once; the real mix — 152 fans + 170 strips
  + 142 quads over 40×30 tiles — sits orders of magnitude under the cap).

### 3.3 Pass 2 — raster (the port)

One workgroup per **tile**, serially walking its triangle list **in
submission order**. For each triangle: clip the tile's 16 rows against the
scanline interval, run the row-interval edge math (the P2.5 code —
per-row `lo`/`hi` from the three directed edges, candidate-window fixups,
`inside_exact` on boundary pixels only), and fill pixels with the
`fill_span` body: texel fetch, chroma test, palette × VC multiply, then
`blend_px(dst, src)` as a storage-image read-modify-write. The image is
viewed as `rgba8uint` (same 8-bit RGBA format class as the chain input's
R8G8B8A8_UNORM — legal view; query STORAGE support on
`R8G8B8A8_UINT` at init, fail-soft to CPU raster if a driver refuses).

Per-pixel ordering within a tile = triangle order = CPU order. Adjacent
tiles share no pixels. Strips keep their per-triangle winding
normalization (draw_pointers, g3x_frame.cpp:1010) — ported as-is.

`fill_rect_fast` (the axis-aligned constant-fill fast path) ports as its
own tiny dispatch or simply as two triangles through the normal path —
decide in Stage 0 by hash, not by taste. `grDrawPoint`/`grDrawLine` are
CPU-rasterized into the legacy path for now (rare, automap-only callers).

### 3.4 Pass 3 — gamma + overlay

Gamma is a per-pixel point op applied **after** all blending — it must stay
its own fullscreen pass (`blend(x, gamma(y)) ≠ gamma(blend(x, y))`):
one workgroup per row-batch, `s_gamma_lut[3][256]` uploaded as a UBO,
`out = lut[c]` integer lookups — bit-identical to `gamma_scale_copy`.

The overlay composites in the same pass: the overlay module renders its
boxes into a small CPU-side RGBA texture (new dirty-flag seam; FPS text
changes 1×/s, toasts expire, the panel edits — most frames upload
nothing), sampled and alpha-merged post-gamma. On dirty frames it uploads
like any texture. The overlay's bit-exactness is inherited from the same
8x8 font + integer placement code — the composite is `src==0 ? dst : src`
over opaque box pixels, trivially exact.

## 4. The one-source-of-truth rule

The inner loop exists twice only if we let it. Instead:

- `src/g3x_raster_core.h` — the pure core: edge tests, row intervals,
  texel fetch, palette multiply, blend classes — templated on a **host
  state** contract. Zero Vulkan, zero windows.h — the house host-testable
  pattern. **Shipped (Stage 0)**: g3x_frame.cpp keeps the state and feeds
  the core through a 20-line `FrameHost` accessor struct; behavior pinned
  byte-identical to the pre-extraction raster by `spike/raster_hashes_stage0.txt`.
- GPU side: `spike/routeb.comp` mirrors the core expression-for-expression
  (the header comment maps each function to its kernel counterpart),
  compiled by glslangValidator **in CI at build time** — the DLL ships no
  shader compiler (i686 budget stays KERNEL32+msvcrt). The kernel carries
  `precise` on every edge/interval/attribute expression; CI asserts the
  SPIR-V kept its `NoContraction` decorations (132 of them at Stage 0), so
  a driver cannot FMA-contract the math silently.
- Lockstep is **proven empirically, every push**: the `routeb` CI job
  rasterizes the deterministic corpus twice (production CPU raster via
  `spike/raster_regress cpu`; kernel via `raster_regress gpu` on Lavapipe)
  and diffs the per-frame hashes against each other AND against the
  committed pin. A mismatch is red — fix the kernel, never casually re-pin.
- Divergence triage: `raster_regress cmp <spv>` runs BOTH backends in one
  process and prints per-frame differing-pixel counts plus the first
  `(x, y, cpu, gpu)` tuples; `ROUTEB_REGION=x0,y0,x1,y1` scopes the dump to
  one 80x80 cell. The corpus triangulation rules that keep the scene
  single-writer (perimeter fans for fill quads, zig-zag strips for textured
  ones, winding flips on textured cells only) are documented at the
  expansion site in `spike/routeb_corpus.h`.
- `scripts/gen_raster_glsl.py` (structural single-source) is the Stage 1
  refinement: once the binned kernel's shape stabilizes, the generator
  emits the .comp from the core's expressions and the CI hash gate keeps
  guarding it.

## 5. Seams that change, seams that don't

| Seam | Status |
|---|---|
| 45 exports, gr* semantics | unchanged |
| slangvk / libslang-vk | unchanged (usage field only) |
| `d2slangvk.conf` | new `raster=cpu|gpu` (default `cpu` until the hash campaign passes in the field) |
| LFB/Bink frames | unchanged — `grLfbLock(WRITE_ONLY, FRONT)` frames keep the CPU compose buffer + `g3x_vk_present_pixels`; Bink frames never call grDraw* so the mode switch is per-frame and free |
| PrintScreen | slang path unchanged (swapchain readback); legacy path on GPU frames switches to the same readback (compose buffer no longer holds the frame) |
| Hotkeys / FPS meter / conf panel / controller | unchanged — the present seam still funnels through `g3x_frame_present` |
| vk_backend | additions only: `g3x_vk_present_image` (image → swapchain blit for chain-off frames), the gamma pass, kernel plumbing; nothing existing moves |
| 32-bit VA budget | host-visible allocations stay modest (vertex ring 4 MB, staging 8 MB); texel heap + tile bins are device-local |

## 6. Verification protocol

1. **CI (every push)**: Lavapipe executes the kernel over the fxparity
   220-frame deterministic replay; per-frame hash == the CPU path's pinned
   hash. The existing harness already replays all blend classes, chroma,
   palettes, rebind storms and table-full stress — the same corpus, two
   backends.
2. **Field**: `raster=gpu` frames log `frame N: ghash=<hex>` alongside the
   CPU pins; the first mismatched frame auto-dumps both PNGs
   (CPU vs GPU) into `screenshots\rasterdiff\` and logs the first divergent
   (x, y) — the house diagnostic style (quad#, vc0#, fill# lines).
3. **Exit criteria per stage** are hash-based, not visual. "Looks right"
   is not a gate this repo recognizes.

## 7. Stages

| Stage | Content | Exit criterion | Size |
|---|---|---|---|
| 0 — spike | core template extraction (CPU behavior unchanged, byte-identical hashes) + `gen_raster_glsl.py` + binless kernel (non-overlapping synthetic scene) rendered to an offscreen image; Lavapipe CI proof | CI hash == CPU hash on the synthetic scene | ~800 LOC |
| 1 — binned raster | pass-1 binning + per-tile pass 2 + blend classes OPAQUE/ADD first; P_8 palette path; legacy present via `g3x_vk_present_image` | fxparity replay (which includes overlaps + rebind storms) hashes clean on Lavapipe | ~800 LOC |
| 2 — full semantics | LERP/MUL/GEN blends, 565/8332 texels, chroma, `fill_rect_fast` decision, strip winding | full replay hash-clean; `raster=gpu` usable chain-off | ~400 LOC |
| 3 — chain + overlay | storage-usage chain input, layout round-trips, gamma+overlay pass, slang-path shots | chain-on replay clean; WYSIWYG shot byte-identical to CPU path | ~500 LOC |
| 4 — field round | `raster=gpu` default-off conf flag, ghash log lines, dump-on-mismatch, RX 560 field log (frame `work=` before/after) | a full field session with zero ghash mismatches and a measured present-path win | ~200 LOC |

## 8. Risks

- **FMA contraction in the wild** — mitigated by `precise` + CI SPIR-V
  decoration check + the hash gate (a contraction fails loudly, not
  silently).
- **Storage image format support** — `R8G8B8A8_UINT` storage views: queried
  at init; plan B is a dedicated RGBA8UI image + one fullscreen copy into
  the chain input (cost: one 640x480 pass, contract unchanged).
- **Lavapipe vs. real-GPU divergence** — Lavapipe proves the math; the
  field ghash campaign proves the driver. Both gates, both cheap.
- **Tile-bin overflow** — counted, logged, CPU fallback per frame.
- **Scope creep** — the 60 fps dream, bilinear filtering, and widescreen
  interpolation are explicitly NOT in this route. GPU raster renders
  640x480 game-res output; everything above it is unchanged.
