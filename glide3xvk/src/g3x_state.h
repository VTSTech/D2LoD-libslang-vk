// glide3xvk — Glide state tracking (P1: record + log; P2: feed the pipeline
// variant cache — see PLAN.md section 5 P2 "state machine").
#pragma once

#include "g3x_types.h"

struct G3xState {
    // combine / blend / depth / misc — opaque values until P2 maps them
    FxU32                 color_combine[5]   = {}; // fn, factor, local, other, invert
    FxU32                 alpha_combine[5]   = {};
    FxU32                 tex_combine[2][7]  = {}; // [tmu] rgb fn/factor, a fn/factor, 2x invert
    FxU32                 blend[4]           = {}; // rgb sf/df, alpha sf/df
    GrCmpFnc_t           depth_func         = 0;
    GrDepthBufferMode_t  depth_mode         = 0;
    FxBool               depth_mask         = 0;
    GrDitherMode_t       dither_mode        = 0;
    GrChromakeyMode_t    chroma_mode        = 0;
    GrColor_t            chroma_value       = 0;
    GrColor_t            constant_color     = 0;
    FxBool               mask_rgb = 0, mask_a = 0;
    FxU32                clip[4]            = {}; // minx, miny, maxx, maxy
    GrEnableMode_t       enabled[2]         = {}; // last enable/disable
    int                  enable_value       = 0;
    GrCoordinateSpaceMode_t coord_space     = 0;
    // gamma
    float                gamma[3]           = {1.f, 1.f, 1.f};
    FxU32                gamma_entries      = 0;
    // texture state
    GrTextureFilterMode_t tex_filter[2][2]  = {}; // [tmu][min,mag]
    GrMipMapMode_t       tex_mipmap[2]      = {};
    FxBool               tex_lodblend[2]    = {};
    GrChipID_t           tex_source_tmu[2]  = {};
    FxU32                tex_addr[2]        = {};
    GrTextureFormat_t    tex_format[2]      = {};
    // live palette (type-2 download), copied — the game may free its buffer
    FxU32                palette[256]       = {};
    FxBool               palette_valid      = 0;
    // counters
    uint32_t             n_draw_calls       = 0;
    uint32_t             n_tex_downloads    = 0;
    uint32_t             n_palette_downloads = 0;
    uint32_t             n_lfb_locks        = 0;
    uint32_t             n_swaps            = 0;
    uint32_t             n_clears           = 0;
    // per-path draw counters (P2.4 diagnostics; strips/fans live in g3x_frame)
    uint32_t             n_vac              = 0;   // grDrawVertexArrayContiguous
    uint32_t             n_parr             = 0;   // grDrawVertexArray
    uint32_t             n_point            = 0;
    uint32_t             n_line             = 0;
    uint32_t             n_tri              = 0;
};

extern G3xState g_st;

// Track a state slot: logs the first time each (slot, value) pair appears
// (8-line budget per slot via g3x_log_once), silently counts the rest.
void g3x_state_note_u32(const char* slot, unsigned value);
void g3x_state_note_f32(const char* slot, float value);
