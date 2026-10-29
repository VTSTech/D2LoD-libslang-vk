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

### 3.2 Pass 1 — binning (the ordering guarantee) — AS BUILT (Stage 1)

The CPU raster's implicit contract is painter's order: primitives rasterize
in submission order; a pixel's final value is the **last** primitive's
write. Unordered compute writes would race on every translucent overlap
(additive glows over tiles). As built, binning is **host-side** in the
packer (`spike/routeb_gpu.cpp`), not a GPU atomicAdd pass — the packer
already holds the whole primitive stream from the record sink, so:

- 32×32-pixel tiles (20×15 = 300 bins at 640×480);
- per primitive: bbox → tile range (`bx0>>5 .. (bx1-1)>>5`, the same
  clamp forms `raster_tri` uses; rects bin by their `fill_rect_exec`
  bounds; point pixels bin once) → append `(buffer_index << 2 | kind)`
  to each touched tile's list, in submission order;
- one flat SSBO: `[NT counts][NT offsets][entries]` — counts + exclusive
  prefix on the host, no overflow counter, no fill dispatch, no fallback
  path (the host list is exact by construction).

The stream the packer bins is the **record sink's** post-dispatch
primitive stream (§4) — strips already normalized, fill rects already
shape-resolved, lines already expanded to their written pixels — so CPU
and GPU cannot disagree about WHAT to draw, only (proven by hash) about
the how. Bin entries carry buffer-local indices (tris vs rects/points
buffers); the record index rides inside each entry's payload.

### 3.3 Pass 2 — raster (the port) — AS BUILT (Stage 1)

`spike/routeb.comp`: dispatch `(tiles_x, tiles_y)` workgroups of
`local_size 32` — one **invocation per (tile, row)**. The invocation walks
its tile's bin in submission order and, per primitive, runs the row body:
the per-row interval math (P2.5 `lo`/`hi` from the three directed edges,
candidate-window fixups, `inside_exact` on boundary pixels only), then the
`fill_span` pixel loop (texel fetch, chroma, palette × VC, `blend_px` as
an SSBO read-modify-write — the storage IMAGE is Stage 3's chain-input
seam, deliberately not spent here).

**The bit-exactness crux of tiling** (the thing Stage 0's single-writer
corpus could not test): the CPU fills each row span in one pass with
attributes stepping `+slope` per pixel FROM the span's first inside pixel.
A tile only stores its x-slice, so the kernel computes the span and its
attribute origin EXACTLY as the CPU does (same interval math, same fixups,
same frame-wide clamps — never tile-clamped), then advances `u/v/vc` to
its tile window with the identical add sequence before running the
per-pixel body on the slice. The skipped pre-tile pixels skip only the
texel fetch and the store — both side-effect-free — so every float the
tile's pixels see is the bit the CPU saw. The row interval math is direct
per-row evaluation (no cross-row accumulation), so rows parallelize
freely.

Per-pixel ordering within a tile = bin order = submission order = CPU
order. Adjacent tiles share no pixels. Rects port as their own record
kind (one row of `fill_rect_exec`, sliced the same way — its ceilf bounds
are pure per-row math, no accumulation to preserve). `grDrawPoint` /
`grDrawLine` port as per-pixel point records emitted by the sink where
the CPU writes them (the kernel's point path is one blend_px).

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
  the core through a 20-line `FrameHost` accessor struct. **Unchanged in
  Stage 1** — binning, the sink and the capture all live AROUND the core;
  the math header needed zero edits.
- **The record sink (Stage 1)**: `G3xRouteBSink` (g3x_frame.h) mirrors two
  exact streams out of the raster with zero behavior change when detached
  (CPU pin stayed byte-identical through the seam's landing):
  the *api level* — the public `g3x_frame_*` call stream, in call order,
  what a replay feeds back verbatim; and the *raster level* — the
  post-dispatch primitives the dispatcher actually rasterizes, each
  stamped with its state snapshot and table content versions. Table
  content is versioned (palette epoch, chroma rebuild counter, per-slot
  texture re-download counter), so mid-frame rebind storms snapshot
  correctly per primitive.
- **The fxparity capture**: `src/g3x_fxcap.cpp` (DLL, `[glide3xvk]
  fxcapture=1` + `fxcap_frames=N` in glide3xvk.ini) serializes the api
  stream to `fxcapture.cap` (spike/fxcap.h format: u32 word stream, one
  self-describing event per call). A replay feeds the same calls back
  into the production API in order — the CPU side of the fxparity gate is
  THE PRODUCTION RASTER, not a reimplementation. The primitive stream is
  deliberately NOT in the file: a replay re-derives it through the
  production dispatcher via the sink, so the file cannot drift from what
  the CPU actually drew.
- GPU side: `spike/routeb.comp` mirrors the core expression-for-expression
  (the header comment maps each function to its kernel counterpart),
  compiled by glslangValidator **in CI at build time** — the DLL ships no
  shader compiler (i686 budget stays KERNEL32+msvcrt). The kernel carries
  `precise` on every edge/interval/attribute expression; CI asserts the
  SPIR-V kept its `NoContraction` decorations (137 at Stage 1), so a
  driver cannot FMA-contract the math silently.
- Lockstep is **proven empirically, every push**: the `routeb` CI job
  rasterizes the corpus twice (production CPU raster via
  `spike/raster_regress cpu`; kernel via `raster_regress gpu` on Lavapipe)
  and diffs the per-frame hashes against each other AND against the
  committed pin (`spike/raster_hashes_stage1.txt`). A mismatch is red —
  fix the kernel, never casually re-pin.
- Divergence triage: `raster_regress cmp <spv>` runs BOTH backends in one
  process and prints per-frame differing-pixel counts plus the first
  `(x, y, cpu, gpu)` tuples; `ROUTEB_REGION=x0,y0,x1,y1` scopes the dump.
  `ROUTEB_DEBUG=1` (+ `ROUTEB_TILE=<n>`) prints the packer's per-frame
  bin/record/vertex dump and the tile's bin entries — the tools that
  found the Stage 1 packer bugs (bin counts unwritten, prim-index vs
  buffer-index conflation, region/binding order mismatch) in one session.
- The Stage 1 corpus (`spike/routeb_corpus.h`) DROPS Stage 0's
  single-writer constraint — that rule existed only because the binless
  kernel could not order writes. The corpus now draws deliberate overlaps
  (an OPAQUE quad UNDER an ADD quad on the same pixels), mid-frame
  rebind storms (palette re-downloads, chromakey toggles, texture
  re-downloads with NEW content at the same TMU address), the automap
  point/line primitives, tex_source misses, and every Stage 0 class
  (all five blends, chroma, uniform/varying VC, negative winding,
  degenerates, frame clips, texel wrap, fill rects and their declined
  fall-through). 16 frames, one fixed-seed LCG.
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

1. **CI (every push)**: Lavapipe executes the kernel over the 16-frame
   overlapping corpus — per-frame hashes diffed against each other AND the
   committed pin (`spike/raster_hashes_stage1.txt`) — and replays the
   committed field capture `spike/fxparity.cap` through both backends in
   one harness run (`raster_regress replay`): every frame boundary must
   come back CPU == GPU, bit-exact.
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
| 1 — binned raster | SHIPPED: host-side pass-1 binning (32x32 tiles) + per-(tile,row) pass 2 with the write-filter attribute design; record sink + fxparity capture/replay (`fxcapture=1`); corpus v2 with deliberate overlaps + rebind storms. The "OPAQUE/ADD first" scoping dissolved: the Stage 0 kernel already mirrored all five blend classes, chroma, P_8/565 texel paths and the fill-rect/point pixel ops verbatim — subsetting them would have added filter machinery without removing any real risk. Legacy present via `g3x_vk_present_image` is Stage 3's chain seam and stayed untouched. | CI: 16-frame overlapping corpus hashes clean on Lavapipe (both gates + the cap round-trip). CLOSED — first field capture landed (`spike/fxparity.cap`, 16381 api events, 1 frame) and replays CPU == GPU bit-exact on Lavapipe | ~1300 LOC |
| 2 — integration | REMAINING (shrunk by Stage 1's as-built): `scripts/gen_raster_glsl.py` structural single-source; the DLL-side GPU record path (pack + dispatch inside the DLL behind `raster=gpu`, off by default); `g3x_vk_present_image` storage-image seam; 8332/other texel formats if the field captures contain any | full field capture replays hash-clean; `raster=gpu` usable chain-off on real hardware | ~600 LOC |
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
