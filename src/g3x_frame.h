// glide3xvk — game-frame module (P2.1).
//
// Owns the CPU "game framebuffer" that all Glide write paths converge on:
//   - grBufferClear        -> g3x_frame_clear
//   - grDrawVertexArrayContiguous -> g3x_frame_draw_fan (P_8 textured fan quads)
//   - grDrawVertexArray    -> g3x_frame_draw_pointers (ground/wall strips+fans:
//                            D2Glide DrawGroundTile submits 5x mode-4 strips of
//                            12 verts (or one mode-5 fan of 4 in low quality);
//                            DrawWallTile* submits 2x mode-4 strips of 10)
//   - grLfbLock (Bink)     -> game writes straight into g3x_frame_pixels
//   - grBufferSwap / LFB unlock -> g3x_frame_present (scale + gamma -> swapchain)
//
// Rendering model (decoded from the game tree + real 3dfx headers, P2.2):
//   textures are GR_TEXFMT_P_8 (8-bit palette indices), one active 256-entry
//   ARGB palette (grTexDownloadTable), TMU-addressed (grTexDownloadMipMap
//   stores, grTexSource binds); vertex = 20B used of 0x1C stride: x,y float,
//   PARGB dword, oow, s, t.
//   TEXTURE COORDS (P2.7): the game ALWAYS submits VOODOO SQUARE-MIP-SPACE
//   coordinates — texel = coord * (2^largeLod / 256) on BOTH axes, for every
//   aspect. Ground truth: DrawPalettedQuad/DrawPerspectiveImage scale raw
//   coords by gaPerspectiveImageSizeTable[aspect] spans {S,T} = (256,256)
//   square, (256,256>>a) wide, (256>>|a|,256) tall — which reduces to the
//   uniform square rule for every aspect (the short axis's span makes texel
//   = coord*2^lod/256, identical to the long axis). The P2.6 per-axis extent
//   auto-detect is retired: it mis-scaled the short axis of non-square
//   textures (dim/256 instead of 2^lod/256), vertically crushing 64x32
//   buttons and 128x64 panels, and horizontally crushing 16x64/16x128
//   sprites. Wrap stays at the declared (w,h) texel dims; the valid image
//   occupies the [0,w)x[0,h) sub-rectangle of the 2^lod square.
//   Textures: GR_TEXFMT_P_8 (fmt 5) via the palette + GR_TEXFMT_RGB_565
//   (fmt 0xa, the in-game Smacker video quads).
//   colorCombine: mode 1 = SCALE_OTHER x LOCAL (texel * vertex color),
//   mode 2 = LOCAL constant (grConstantColorValue solid fill: gray in bits
//   15..8, blend alpha in bits 7..0 — RENDERER_GLIDE_ScalePaletteValue << 8).
#pragma once

#include "g3x_types.h"

// (Re)size the frame for the current game resolution; contents cleared black.
void     g3x_frame_reset(unsigned w, unsigned h);
void     g3x_frame_clear(uint32_t color);          // ARGB dword, 0 = black
uint8_t* g3x_frame_pixels();                       // w*h*4, B8G8R8A8 byte order
unsigned g3x_frame_width();
unsigned g3x_frame_height();
unsigned g3x_frame_stride();

// State fed by glide_exports at call time.
void     g3x_frame_set_palette(const void* entries256);      // 256 x ARGB dword
void     g3x_frame_set_chromakey(unsigned mode, unsigned value);
void     g3x_frame_set_blend(unsigned rgb_sf, unsigned rgb_df);  // GR_BLEND pair
void     g3x_frame_set_constant(unsigned argb);              // grConstantColorValue
void     g3x_frame_set_color_fill(bool fill);                // colorCombine mode 2
void     g3x_frame_set_gamma(float r, float g, float b);

// Shared (lod, aspect) -> (w, h) per the game's convention: max dim = 2^lod,
// other = 2^(lod-|aspect|); aspect sign = wide (+) / tall (-). Also used by
// glide_exports' grTexTextureMemRequired so both agree.
void     g3x_tex_dims(unsigned lod, int aspect, unsigned* w, unsigned* h);

// TMU model: download stores a copy keyed by (tmu, addr); source binds.
bool     g3x_frame_tex_download(unsigned tmu, unsigned addr, const GrTexInfo* info);
void     g3x_frame_tex_source(unsigned tmu, unsigned addr);
unsigned g3x_frame_tex_miss_count();   // grTexSource bindings with no download
void     g3x_frame_tex_table_stats(unsigned* used, unsigned* dropped); // P2.8

// Chroma toggle count since context open (P2.8): the game flips
// grChromakeyMode OFF->ON around DrawSolidRect/DrawLightMap batches; the
// per-frame delta says whether those batches run in the current phase.
unsigned g3x_frame_chroma_toggles();

// Draw-path totals since context open: vac fans, pointer-array draws,
// pointer-array strips, pointer-array fans, draws skipped (no texture),
// sub-256-mip draws, RGB565-texture draws, constant-fill quads (P2.7),
// uniform-black-vertex textured quads (P2.8: the shadow/darken class —
// opaque-black rects unless chroma punches the transparent texels).
// Feeds the periodic frame lines + shutdown summary (P2.4/P2.6/P2.7/P2.8).
void     g3x_frame_draw_stats(unsigned out[9]);

// TRIANGLE_FAN (mode 5): count vertices at `base`, `stride` bytes apart.
void     g3x_frame_draw_fan(unsigned count, const void* base, unsigned stride);

// Pointer-array variant (grDrawVertexArray): `pointers` is an array of count
// vertex pointers, each to a 0x1C-stride vertex. mode 5 = fan, mode 4 = strip.
void     g3x_frame_draw_pointers(unsigned mode, unsigned count, void* const* pointers);

// P2.5: grDrawPoint / grDrawLine (D2Glide DrawLine/DrawPoint: automap-style
// strokes, radial-light dots — constant-fill color through the current blend).
// Point = single rounded pixel; line = Bresenham with per-pixel frame clip.
void     g3x_frame_draw_point(float x, float y);
void     g3x_frame_draw_line(float x0, float y0, float x1, float y1);

// Scale + gamma the frame into the swapchain and present. Also the Bink
// unlock path (the video loop never calls grBufferSwap).
bool     g3x_frame_present();

// ---- Route-B Stage 1 record sink (ROUTE-B.md §4/§6) ------------------------
// When attached (g3x_frame_routeb_sink), the frame module mirrors two exact
// streams out of the raster, with ZERO behavior change when the sink is
// null (every emission is behind a null check on a cold static branch):
//   api level    the public g3x_frame_* call stream, in call order — what a
//                replay feeds back into the production API verbatim
//                (fxparity capture: src/g3x_fxcap.cpp writes fxcapture.cap).
//   raster level the POST-DISPATCH primitives the dispatcher actually
//                rasterizes — strips pre-normalized, fill rects
//                shape-resolved, lines expanded to their written pixels —
//                each stamped with the state snapshot and table content
//                versions it ran under. The GPU packer (spike/routeb_gpu.cpp)
//                bins exactly this stream; it never re-derives dispatcher
//                decisions, so CPU and GPU can not disagree about WHAT to
//                draw, only (proven by hash) about the how.
// Table content is versioned: every palette download bumps the palette
// version, every chroma rebuild and every texture re-download bumps theirs,
// and the table_* callbacks deliver the new content under that version —
// mid-frame rebind storms therefore snapshot correctly per primitive.
struct G3xRouteBPrim {
    uint32_t kind;          // 0 = triangle (A,B,C), 1 = fill rect (A=(xa,ya),
                            //     B=(xb,yb)), 2 = point pixel (A=(x,y))
    float ax, ay, au, av; uint32_t aargb;
    float bx, by, bu, bv; uint32_t bargb;
    float cx, cy, cu, cv; uint32_t cargb;
    float tsx, tsy;         // texel scale (triangle prims only)
    uint32_t sf, df;        // GR_BLEND rgb pair as held at draw time
    uint32_t const_alpha, constant;
    uint32_t color_fill, chroma_on;
    int32_t  tex_slot;      // bound texture slot at draw time, -1 = none
    uint32_t tex_w, tex_h, tex_lod, tex_bpp;
    uint32_t tex_ver;       // content version of that slot
    uint32_t pal_ver, chroma_ver;
};

struct G3xRouteBSink {
    void* ud;
    // raster level — the GPU packer consumes these
    void (*prim)(void* ud, const G3xRouteBPrim* p);
    void (*table_pal)(void* ud, uint32_t ver, const uint32_t* pal256);
    void (*table_chroma)(void* ud, uint32_t ver, const uint8_t* hit256);
    void (*table_tex)(void* ud, int32_t slot, uint32_t ver, const void* data,
                      uint32_t bytes, uint32_t w, uint32_t h,
                      uint32_t lod, uint32_t bpp);
    // api level — the capture writer (and through the file, the replayer)
    void (*api_frame)(void* ud, uint32_t w, uint32_t h);
    void (*api_clear)(void* ud, uint32_t color);
    void (*api_palette)(void* ud, const uint32_t* pal256);
    void (*api_texdown)(void* ud, uint32_t tmu, uint32_t addr, uint32_t lod,
                        int32_t aspect, uint32_t fmt, const void* data,
                        uint32_t bytes, uint32_t w, uint32_t h, uint32_t bpp);
    void (*api_texsrc)(void* ud, uint32_t tmu, uint32_t addr);
    void (*api_blend)(void* ud, uint32_t sf, uint32_t df);
    void (*api_constant)(void* ud, uint32_t argb);
    void (*api_colorfill)(void* ud, uint32_t flag);
    void (*api_chromakey)(void* ud, uint32_t mode, uint32_t value);
    void (*api_drawfan)(void* ud, uint32_t count, const void* verts);  // 20B each
    void (*api_drawptrs)(void* ud, uint32_t mode, uint32_t count, const void* verts);
    void (*api_point)(void* ud, float x, float y);
    void (*api_line)(void* ud, float x0, float y0, float x1, float y1);
};

// Attach/detach the record sink. Null restores the plain production path.
void     g3x_frame_routeb_sink(const G3xRouteBSink* s);
// Conditional detach: clears the slot only when it still holds THIS sink.
// The capture and the GPU path share the single slot (last attach wins, by
// design — one consumer per stream); a bare-null detach from whichever
// module stops second silently disarmed the other mid-session.
void     g3x_frame_routeb_detach(const G3xRouteBSink* s);

// Route-B Stage 2: emit-only mode (raster=gpu). The dispatcher still expands
// strips, resolves fill-rect shapes and expands lines — the decisions the
// record stream is DEFINED by — but skips the per-pixel CPU raster (the
// kernel reproduces it bit-exactly; CI-proven). The primitive stream the
// sink sees is byte-identical to the CPU-raster mode's: same emissions, same
// order, same state stamps.
void     g3x_frame_gpu_emit_only(bool on);
