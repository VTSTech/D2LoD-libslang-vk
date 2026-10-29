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
