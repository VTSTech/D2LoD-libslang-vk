// glide3xvk — game-frame module (P2.1).
//
// Owns the CPU "game framebuffer" that all Glide write paths converge on:
//   - grBufferClear        -> g3x_frame_clear
//   - grDrawVertexArray*   -> g3x_frame_draw_fan (P_8 textured fan quads)
//   - grLfbLock (Bink)     -> game writes straight into g3x_frame_pixels
//   - grBufferSwap / LFB unlock -> g3x_frame_present (scale + gamma -> swapchain)
//
// Rendering model (decoded from the game tree + real 3dfx headers):
//   textures are GR_TEXFMT_P_8 (8-bit palette indices), one active 256-entry
//   ARGB palette (grTexDownloadTable), TMU-addressed (grTexDownloadMipMap
//   stores, grTexSource binds); texture coords s,t span [0,256] per wrap;
//   vertex = 20B used of 0x1C stride: x,y float, PARGB dword, oow, s, t;
//   colorCombine SCALE_OTHER(=texel) x LOCAL(=vertex color).
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
void     g3x_frame_set_gamma(float r, float g, float b);

// TMU model: download stores a copy keyed by (tmu, addr); source binds.
bool     g3x_frame_tex_download(unsigned tmu, unsigned addr, const GrTexInfo* info);
void     g3x_frame_tex_source(unsigned tmu, unsigned addr);

// TRIANGLE_FAN (mode 5): count vertices at `base`, `stride` bytes apart.
void     g3x_frame_draw_fan(unsigned count, const void* base, unsigned stride);

// Scale + gamma the frame into the swapchain and present. Also the Bink
// unlock path (the video loop never calls grBufferSwap).
bool     g3x_frame_present();
