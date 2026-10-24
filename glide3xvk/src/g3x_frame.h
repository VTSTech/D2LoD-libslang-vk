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
//   TEXTURE COORDS: the game's gaPerspectiveImageSizeTable (globals/D2Glide.cpp)
//   gives the S/T span per aspect = the image dims in texels, with both axes
//   expressed in a 2^lod coordinate space: texel = s * (2^lod / 256), wrapped
//   mod (w,h). For 256x256 squares this equals the old [0,256] convention; for
//   tall 128x256 sprites S spans [0,128] and for wide 256x64 T spans [0,64] —
//   scaling both axes by 2^lod/256 is the only rule that fits every case.
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

// Draw-path totals since context open: vac fans, pointer-array draws,
// pointer-array strips, pointer-array fans, draws skipped (no texture).
// Feeds the periodic frame lines + shutdown summary (P2.4 diagnostics).
void     g3x_frame_draw_stats(unsigned out[5]);

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
