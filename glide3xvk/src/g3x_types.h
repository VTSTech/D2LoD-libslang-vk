// glide3xvk — Minimal Glide 3 API surface used by Diablo II 1.14d.
//
// Mirror of the typedef block in the game tree's d2_platform.h, kept
// independent on purpose: glide3x.dll is a separate target and must never
// include game headers (zero game-source drift, PLAN.md section 4).
//
// Values marked "observed" were extracted from the fork's own call sites
// (D2Glide/Renderer/Glide.cpp, D2Glide/glBlocks.cpp) — the in-tree module is
// the behavioral spec for how D2 drives this API.
#pragma once

#include <cstdint>

// -- 3dfx base types (identical to d2_platform.h) ---------------------------
typedef float FxFloat;
typedef int FxI32;
typedef unsigned int FxU32;
typedef short FxI16;
typedef unsigned short FxU16;
typedef int FxBool;
typedef unsigned int GrAlphaBlendFnc_t;
typedef unsigned int GrAlpha_t;
typedef unsigned int GrAspectRatio_t;
typedef unsigned int GrBuffer_t;
typedef unsigned int GrChipID_t;
typedef unsigned int GrChromakeyMode_t;
typedef unsigned int GrCmpFnc_t;
typedef unsigned int GrColorFormat_t;
typedef unsigned int GrColor_t;
typedef unsigned int GrCombineFactor_t;
typedef unsigned int GrCombineFunction_t;
typedef unsigned int GrCombineLocal_t;
typedef unsigned int GrCombineOther_t;
typedef unsigned int GrContext_t;
typedef unsigned int GrCoordinateSpaceMode_t;
typedef unsigned int GrDepthBufferMode_t;
typedef unsigned int GrDitherMode_t;
typedef unsigned int GrEnableMode_t;
typedef unsigned int GrLOD_t;
typedef unsigned int GrLfbWriteMode_t;
typedef unsigned int GrLock_t;
typedef unsigned int GrMipMapMode_t;
typedef unsigned int GrOriginLocation_t;
typedef unsigned int GrScreenRefresh_t;
typedef unsigned int GrScreenResolution_t;
typedef unsigned int GrTexTable_t;
typedef unsigned int GrTextureFilterMode_t;
typedef unsigned int GrTextureFormat_t;
typedef void (*GrProc)();

#define FXTRUE 1
#define FXFALSE 0

struct GrLfbInfo_t {
    int size;
    void* lfbPtr;
    uint32_t strideInBytes;
    GrLfbWriteMode_t writeMode;
    GrOriginLocation_t origin;
};

struct GrTexInfo {
    GrLOD_t smallLodLog2;
    GrLOD_t largeLodLog2;
    GrAspectRatio_t aspectRatioLog2;
    GrTextureFormat_t format;
    void* data;
};

// -- Constants the scaffold acts on ------------------------------------------

// grScreenResolution_t -> pixel size (standard Glide 3 resolution list).
// D2 requests 640x480 (7) / 800x600 (8) in practice.
inline void g3x_resolution_size(GrScreenResolution_t res, unsigned* w, unsigned* h) {
    static const unsigned kRes[][2] = {
        {320, 200}, {320, 240}, {400, 256}, {512, 384}, {640, 200},
        {640, 350}, {640, 400}, {640, 480}, {800, 600}, {960, 720},
        {856, 480}, {512, 256}, {1024, 768}, {1280, 1024}, {1600, 1200},
        {400, 300}
    };
    if (res < sizeof(kRes) / sizeof(kRes[0])) {
        *w = kRes[res][0];
        *h = kRes[res][1];
    } else { // unknown -> fall back to 640x480 and let the caller log it
        *w = 640;
        *h = 480;
    }
}

// grDrawVertexArrayContiguous mode values (glide.h GrVertexList).
#define G3X_GR_POINTS          0u
#define G3X_GR_LINES           1u
#define G3X_GR_LINE_STRIP      2u
#define G3X_GR_TRIANGLES       3u
#define G3X_GR_TRIANGLE_STRIP  4u
#define G3X_GR_TRIANGLE_FAN    5u   // observed: D2 submits quads as 4-vertex fans

// D2's vertex layout for stride 0x1C (observed everywhere in D2Glide):
// float x, y; GrColor_t argb; float oow, s, t, w  — 7 x 4 bytes.
#define G3X_D2_VERTEX_STRIDE 0x1Cu

// grTexDownloadTable type 2 (observed) = palette table for 8bpp-indexed
// textures; this is the palette-indexed rendering path the whole P2 backend
// exists to reproduce.
#define G3X_GR_TEXTABLE_PALETTE 2u

// grLfbLock buffer/lock enums (observed calls: (0,1,0xff,...) read back
// buffer, (1,0,5,...) write front buffer from Bink cutscene blits).
#define G3X_GR_LFB_READ_ONLY  0u
#define G3X_GR_LFB_WRITE_ONLY 1u

// -- grGet caps table (field-test v1 finding, 2026-10-24) --------------------
//
// The fork's decompiled call sites query six pnames right after a successful
// grSstWinOpen (D2Glide/Renderer/Glide.cpp). The v1 stub answered 0 for all of
// them, and D2Glide's init treats GR_NUM_TMU == 0 as a hard failure
// (`if (!gnGlideTextureMemoryUsed) return 0;`) — the game tore down cleanly
// and never presented a frame. Names below are CANONICAL (3dfx glide.h /
// diget.c, cvg = Voodoo2 build); the decompiler locals in the fork
// ("nTexMemFbzColorPath", "gnGlideTextureMemoryUsed", ...) mislabel several
// of them. Answers mirror the real 3dfx Voodoo2 implementation so D2 configures
// itself exactly like on a period card: single board, single TMU, 256x256
// textures, no UMA, no SLI.
#define G3X_GR_GET_GAMMA_ENTRIES     0x05u  // GR_GAMMA_TABLE_ENTRIES  -> 32
#define G3X_GR_GET_MAX_TEXTURE_SIZE  0x0Au  // GR_MAX_TEXTURE_SIZE     -> 256
#define G3X_GR_GET_MAX_TEXTURE_AR    0x0Bu  // GR_MAX_TEXTURE_ASPECT_RATIO -> 3
#define G3X_GR_GET_MEMORY_UMA        0x0Eu  // GR_MEMORY_UMA           -> 0 (split-memory Voodoo)
#define G3X_GR_GET_NUM_BOARDS        0x0Fu  // GR_NUM_BOARDS           -> 1 (acceptance gate, D2Client/Renderer/Glide.cpp:452)
#define G3X_GR_GET_NUM_FB            0x11u  // GR_NUM_FB               -> 1 (non-SLI)
#define G3X_GR_GET_NUM_TMU           0x13u  // GR_NUM_TMU              -> 1 (HARD GATE: 0 = D2Glide init fails)
#define G3X_GR_GET_TEXTURE_ALIGN     0x24u  // GR_TEXTURE_ALIGN        -> 8 (SST_TEXTURE_ALIGN)
#define G3X_GR_GET_BITS_GAMMA        0x2Au  // GR_BITS_GAMMA           -> 8
