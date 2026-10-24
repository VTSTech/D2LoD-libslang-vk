// glide3xvk — the 45 Glide exports D2Client::Renderer::Glide resolves via
// GetProcAddress, with the exact decorated names from that loader
// (_guGammaCorrectionRGB@12 ... _grGlideShutdown@0). glide3x.def maps them.
//
// P1 scaffold (PLAN.md section 5): every export is a real symbol with state
// tracking; grGet/grGetString/grSstWinOpen/grBufferSwap carry the actual
// contract; draw calls are counted no-ops (nothing rasterized by design).
#include "g3x_types.h"
#include "g3x_state.h"
#include "g3x_log.h"
#include "g3x_frame.h"
#include "vk_backend.h"

#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <new>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#ifndef G3XVK_BUILD_ID
#define G3XVK_BUILD_ID "unknown"
#endif

static GrContext_t s_ctx = 0;
static unsigned    s_open_w = 640, s_open_h = 480;

// All 45 exports must keep their undecorated stdcall names (_grXxx@N) for the
// def file and GetProcAddress — so: extern "C".
extern "C" {

// Export prototypes: the __asm__ labels give every symbol a DOUBLE leading
// underscore. GNU ld strips exactly one underscore when writing the export
// table, so glide3x.def (single-underscore names) yields the literal
// decorated strings D2Client/Renderer/Glide.cpp resolves via GetProcAddress.
// -Wl,-u flags in CMakeLists force the archive member in for the def lookup.
void __stdcall grGlideInit() __asm__("__grGlideInit@0");
void __stdcall grGlideShutdown() __asm__("__grGlideShutdown@0");
GrContext_t __stdcall grSstWinOpen(FxU32 hWnd, GrScreenResolution_t screen_resolution, GrScreenRefresh_t refresh_rate, GrColorFormat_t color_format, GrOriginLocation_t origin_location, int nColBuffers, int nAuxBuffers) __asm__("__grSstWinOpen@28");
FxBool __stdcall grSstWinClose(GrContext_t context) __asm__("__grSstWinClose@4");
void __stdcall grSstSelect(int which_sst) __asm__("__grSstSelect@4");
void __stdcall grFinish() __asm__("__grFinish@0");
void __stdcall grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU32 depth) __asm__("__grBufferClear@12");
void __stdcall grBufferSwap(FxU32 swap_interval) __asm__("__grBufferSwap@4");
FxU32 __stdcall grGet(FxU32 pname, FxU32 plength, FxI32* params) __asm__("__grGet@12");
char* __stdcall grGetString(FxU32 pname) __asm__("__grGetString@4");
GrProc __stdcall grGetProcAddress(char* procName) __asm__("__grGetProcAddress@4");
void __stdcall grColorCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert) __asm__("__grColorCombine@20");
void __stdcall grAlphaCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert) __asm__("__grAlphaCombine@20");
void __stdcall grTexCombine(GrChipID_t tmu, GrCombineFunction_t rgb_function, GrCombineFactor_t rgb_factor, GrCombineFunction_t alpha_function, GrCombineFactor_t alpha_factor, FxBool rgb_invert, FxBool alpha_invert) __asm__("__grTexCombine@28");
void __stdcall grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df, GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df) __asm__("__grAlphaBlendFunction@16");
void __stdcall grChromakeyMode(GrChromakeyMode_t mode) __asm__("__grChromakeyMode@4");
void __stdcall grChromakeyValue(GrColor_t value) __asm__("__grChromakeyValue@4");
void __stdcall grClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy) __asm__("__grClipWindow@16");
void __stdcall grColorMask(FxBool rgb, FxBool a) __asm__("__grColorMask@8");
void __stdcall grConstantColorValue(GrColor_t value) __asm__("__grConstantColorValue@4");
void __stdcall grDepthBufferFunction(GrCmpFnc_t function) __asm__("__grDepthBufferFunction@4");
void __stdcall grDepthBufferMode(GrDepthBufferMode_t mode) __asm__("__grDepthBufferMode@4");
void __stdcall grDepthMask(FxBool mask) __asm__("__grDepthMask@4");
void __stdcall grDitherMode(GrDitherMode_t mode) __asm__("__grDitherMode@4");
void __stdcall grEnable(GrEnableMode_t mode) __asm__("__grEnable@4");
void __stdcall grDisable(GrEnableMode_t mode) __asm__("__grDisable@4");
void __stdcall grCoordinateSpace(GrCoordinateSpaceMode_t mode) __asm__("__grCoordinateSpace@4");
void __stdcall guGammaCorrectionRGB(FxFloat red, FxFloat green, FxFloat blue) __asm__("__guGammaCorrectionRGB@12");
void __stdcall grLoadGammaTable(FxU32 nentries, FxU32* red, FxU32* green, FxU32* blue) __asm__("__grLoadGammaTable@16");
FxU32 __stdcall grTexTextureMemRequired(FxU32 evenOdd, GrTexInfo* info) __asm__("__grTexTextureMemRequired@8");
FxU32 __stdcall grTexMinAddress(GrChipID_t tmu) __asm__("__grTexMinAddress@4");
FxU32 __stdcall grTexMaxAddress(GrChipID_t tmu) __asm__("__grTexMaxAddress@4");
void __stdcall grTexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo* info) __asm__("__grTexSource@16");
void __stdcall grTexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo* info) __asm__("__grTexDownloadMipMap@16");
void __stdcall grTexDownloadTable(GrTexTable_t type, void* data) __asm__("__grTexDownloadTable@8");
void __stdcall grTexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode) __asm__("__grTexFilterMode@12");
void __stdcall grTexMipMapMode(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend) __asm__("__grTexMipMapMode@12");
void __stdcall grDrawVertexArrayContiguous(FxU32 mode, FxU32 Count, void* pointers, FxU32 stride) __asm__("__grDrawVertexArrayContiguous@16");
FxU32 __stdcall grDrawVertexArray(FxU32 mode, FxU32 Count, void* pointers) __asm__("__grDrawVertexArray@12");
void __stdcall grDrawPoint(void* pt) __asm__("__grDrawPoint@4");
void __stdcall grDrawLine(void* v1, void* v2) __asm__("__grDrawLine@8");
void __stdcall grDrawTriangle(void* a, void* b, void* c) __asm__("__grDrawTriangle@12");
void __stdcall grVertexLayout(FxU32 param, FxI32 offset, FxU32 mode) __asm__("__grVertexLayout@12");
FxBool __stdcall grLfbLock(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode, GrOriginLocation_t origin, FxBool pixelPipeline, GrLfbInfo_t* info) __asm__("__grLfbLock@24");
FxBool __stdcall grLfbUnlock(GrLock_t type, GrBuffer_t buffer) __asm__("__grLfbUnlock@8");

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void __stdcall grGlideInit() {
    g3x_log_once("grGlideInit", "grGlideInit");
}

void __stdcall grGlideShutdown() {
    // Always-on totals: the round-6 report arrived without any summary (the
    // every-300-swaps gate never fired at 112 frames), so the exit evidence
    // was lost. Print the full picture unconditionally.
    unsigned ds[5];
    g3x_frame_draw_stats(ds);
    g3x_log("grGlideShutdown: swaps=%u clears=%u draws=%u (vac=%u parr=%u "
            "strip=%u fan=%u point=%u line=%u tri=%u) skip=%u texdl=%u paldl=%u "
            "texmiss=%u",
            g_st.n_swaps, g_st.n_clears, g_st.n_draw_calls,
            ds[0], ds[1], ds[2], ds[3], g_st.n_point, g_st.n_line, g_st.n_tri,
            ds[4], g_st.n_tex_downloads, g_st.n_palette_downloads,
            g3x_frame_tex_miss_count());
    g3x_vk_shutdown();
}

GrContext_t __stdcall grSstWinOpen(FxU32 hWnd, GrScreenResolution_t screen_resolution,
                                   GrScreenRefresh_t refresh_rate, GrColorFormat_t color_format,
                                   GrOriginLocation_t origin_location, int nColBuffers,
                                   int nAuxBuffers) {
    unsigned w = 0, h = 0;
    g3x_resolution_size(screen_resolution, &w, &h);
    if (screen_resolution > 15) {
        g3x_log("grSstWinOpen: unknown resolution %u -> falling back 640x480",
                (unsigned)screen_resolution);
    }
    s_open_w = w; s_open_h = h;
    g3x_log("grSstWinOpen: hwnd=0x%08X res=%u (%ux%u) refresh=%u colorfmt=%u origin=%u "
            "colbuf=%d aux=%d", hWnd, screen_resolution, w, h, refresh_rate, color_format,
            origin_location, nColBuffers, nAuxBuffers);
    if (!g3x_vk_init((HWND)(uintptr_t)hWnd, w, h)) {
        // Fail-soft (PLAN.md section 4): the game continues; the window just
        // stays black until Vulkan works. The log names the reason.
        g3x_log("grSstWinOpen: Vulkan unavailable -> black-screen mode (game continues)");
    }
    g3x_frame_reset(w, h);   // game framebuffer exists even if Vulkan does not
    if (s_ctx == 0) s_ctx = 1;
    return s_ctx;
}

FxBool __stdcall grSstWinClose(GrContext_t context) {
    // Phase marker: the game opens/closes contexts for probes, videos and the
    // real window — the totals here separate those phases in the log.
    unsigned ds[5];
    g3x_frame_draw_stats(ds);
    g3x_log("grSstWinClose: ctx=%u (swaps=%u vac=%u parr=%u strip=%u fan=%u "
            "skip=%u texmiss=%u)",
            context, g_st.n_swaps, ds[0], ds[1], ds[2], ds[3], ds[4],
            g3x_frame_tex_miss_count());
    g3x_vk_shutdown();
    return FXTRUE;
}

void __stdcall grSstSelect(int which_sst) {
    g3x_log_once("grSstSelect", "grSstSelect -> %d", which_sst);
}

void __stdcall grFinish() {
    // no-op: everything P1 submits is fenced inside the backend
}

// ---------------------------------------------------------------------------
// Buffer / present
// ---------------------------------------------------------------------------
void __stdcall grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU32 depth) {
    ++g_st.n_clears;
    g3x_frame_clear(color);   // observed: color=0 (black) every time
    g3x_log_once("grBufferClear", "color=0x%08x alpha=0x%02x depth=0x%08x",
                 color, alpha, depth);
}

void __stdcall grBufferSwap(FxU32 swap_interval) {
    ++g_st.n_swaps;
    g3x_frame_present();
    // P2.5: rolling frame time so the field log shows the raster's real pace
    // (round-7 session ran at ~4.3 fps — the number this build must move).
    static LARGE_INTEGER s_prev = {};
    static double s_ms_acc = 0.0;
    static unsigned s_ms_n = 0;
    LARGE_INTEGER now, freq;
    if (QueryPerformanceCounter(&now) && QueryPerformanceFrequency(&freq)) {
        if (s_prev.QuadPart) {
            s_ms_acc += (double)(now.QuadPart - s_prev.QuadPart) * 1000.0 / (double)freq.QuadPart;
            ++s_ms_n;
        }
        s_prev = now;
    }
    // P2.4: periodic activity line — gameplay phases run with every per-call
    // tag budget-quiet, so these are the only evidence of what each phase
    // actually submitted (and how fast the CPU raster is keeping up).
    if (g_st.n_swaps % 120 == 0) {
        unsigned ds[5];
        g3x_frame_draw_stats(ds);
        g3x_log("frame %u: avg=%.1fms vac=%u parr=%u(strip=%u fan=%u) skip=%u "
                "texdl=%u paldl=%u lfb=%u",
                g_st.n_swaps, s_ms_n ? s_ms_acc / (double)s_ms_n : 0.0,
                ds[0], ds[1], ds[2], ds[3], ds[4],
                g_st.n_tex_downloads, g_st.n_palette_downloads, g_st.n_lfb_locks);
        s_ms_acc = 0.0; s_ms_n = 0;
    }
    if (g_st.n_swaps % 300 == 0) {
        g3x_log("frame %u: swaps=%u draws=%u texdl=%u paldl=%u lfblocks=%u",
                g_st.n_swaps, g_st.n_swaps, g_st.n_draw_calls, g_st.n_tex_downloads,
                g_st.n_palette_downloads, g_st.n_lfb_locks);
        g3x_log_summary();
    }
}

// ---------------------------------------------------------------------------
// Queries (the caps contract; see g3x_types.h for the field-test v1 story —
// GR_NUM_TMU == 0 is a hard D2Glide init failure, the v1 zero-frame cause)
// ---------------------------------------------------------------------------
FxU32 __stdcall grGet(FxU32 pname, FxU32 plength, FxI32* params) {
    FxI32 value = 0;
    switch (pname) {
        case G3X_GR_GET_GAMMA_ENTRIES:    value = 32; break;
        case G3X_GR_GET_MAX_TEXTURE_SIZE: value = 256; break;
        case G3X_GR_GET_MAX_TEXTURE_AR:   value = 3;  break;
        case G3X_GR_GET_MEMORY_UMA:       value = 0;  break;
        case G3X_GR_GET_NUM_BOARDS:       value = 1;  break;
        case G3X_GR_GET_NUM_FB:           value = 1;  break;
        case G3X_GR_GET_NUM_TMU:          value = 1;  break;
        case G3X_GR_GET_TEXTURE_ALIGN:    value = 8;  break;
        case G3X_GR_GET_BITS_GAMMA:       value = 8;  break;
        default:
            // Real glide3x returns 0 (FXFALSE) for unknown pnames; D2's windowed
            // gamma path checks the return before using the value, so failing a
            // query honestly is the safe contract.
            g3x_log_once("grGet:other", "grGet pname=0x%x plength=%u -> 0 (unhandled)", pname, plength);
            if (params && plength >= 4) params[0] = 0;
            return 0;
    }
    if (params && plength >= 4) params[0] = value;
    static bool s_caps_logged = false;
    if (!s_caps_logged) {
        s_caps_logged = true;
        g3x_log("caps: Voodoo-class profile — BOARDS=1 TMU=1 FB=1 MAX_TEX=256 TEXAR=3 UMA=0 TEXALIGN=8 GAMMA=8bit/32");
    }
    return plength;
}

static char s_str_ext[2]   = "";  // no extensions advertised in P1
static char s_str_hw[64]   = "3Dfx Voodoo Graphics";
static char s_str_rdr[64]  = "glide3xvk " G3XVK_BUILD_ID " (Vulkan scaffold)";

char* __stdcall grGetString(FxU32 pname) {
    switch (pname) {
        case 1: return s_str_ext;   // GR_EXTENSION
        case 2: return s_str_hw;    // GR_HARDWARE
        case 3: return s_str_rdr;   // GR_RENDERER
        default:
            g3x_log_once("grGetString", "pname=%u -> empty", pname);
            return s_str_ext;
    }
}

GrProc __stdcall grGetProcAddress(char* procName) {
    g3x_log_once("grGetProcAddress", "'%s' -> null (no extensions in P1)",
                 procName ? procName : "(null)");
    return nullptr;
}

// ---------------------------------------------------------------------------
// State machine (tracked; feeds the P2 pipeline-variant cache later)
// ---------------------------------------------------------------------------
void __stdcall grColorCombine(GrCombineFunction_t function, GrCombineFactor_t factor,
                              GrCombineLocal_t local, GrCombineOther_t other, FxBool invert) {
    g_st.color_combine[0] = function; g_st.color_combine[1] = factor;
    g_st.color_combine[2] = local;    g_st.color_combine[3] = other;
    g_st.color_combine[4] = (unsigned)invert;
    // Game decode (D2Glide/Renderer/Glide.cpp RENDERER_GLIDE_SetColorCombine):
    //   mode 1: grColorCombine(3=SCALE_OTHER, 1=FACTOR_LOCAL, 0, 1=OTHER_TEXTURE)
    //           -> output = vertex color * texel (the textured sprite path)
    //   mode 2: grColorCombine(1=LOCAL, 0, 1=LOCAL_CONSTANT, 2) -> output =
    //           grConstantColorValue (the solid-fill / fade-rect path)
    g3x_frame_set_color_fill(function == 1 && local == 1);
    g3x_state_note_u32("colorCombine", (function << 24) | (factor & 0xFFFFFF));
}

void __stdcall grAlphaCombine(GrCombineFunction_t function, GrCombineFactor_t factor,
                              GrCombineLocal_t local, GrCombineOther_t other, FxBool invert) {
    g_st.alpha_combine[0] = function; g_st.alpha_combine[1] = factor;
    g_st.alpha_combine[2] = local;    g_st.alpha_combine[3] = other;
    g_st.alpha_combine[4] = (unsigned)invert;
    g3x_state_note_u32("alphaCombine", (function << 24) | (factor & 0xFFFFFF));
}

void __stdcall grTexCombine(GrChipID_t tmu, GrCombineFunction_t rgb_function,
                            GrCombineFactor_t rgb_factor, GrCombineFunction_t alpha_function,
                            GrCombineFactor_t alpha_factor, FxBool rgb_invert,
                            FxBool alpha_invert) {
    if (tmu < 2) {
        g_st.tex_combine[tmu][0] = rgb_function; g_st.tex_combine[tmu][1] = rgb_factor;
        g_st.tex_combine[tmu][2] = alpha_function; g_st.tex_combine[tmu][3] = alpha_factor;
        g_st.tex_combine[tmu][4] = (unsigned)rgb_invert; g_st.tex_combine[tmu][5] = (unsigned)alpha_invert;
    }
    g3x_state_note_u32("texCombine", (tmu << 28) | (rgb_function << 24) | (rgb_factor & 0xFFFFFF));
}

void __stdcall grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df,
                                    GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df) {
    g_st.blend[0] = rgb_sf; g_st.blend[1] = rgb_df; g_st.blend[2] = alpha_sf; g_st.blend[3] = alpha_df;
    g3x_frame_set_blend(rgb_sf, rgb_df);   // rgb pair drives the P2.1 raster
    g3x_state_note_u32("alphaBlend", (rgb_sf << 24) | (rgb_df << 16) | (alpha_sf << 8) | alpha_df);
}

void __stdcall grChromakeyMode(GrChromakeyMode_t mode) {
    g_st.chroma_mode = mode;
    g3x_frame_set_chromakey(mode, g_st.chroma_value);
    g3x_state_note_u32("chromakeyMode", mode);
}

void __stdcall grChromakeyValue(GrColor_t value) {
    g_st.chroma_value = value;
    g3x_frame_set_chromakey(g_st.chroma_mode, value);
    g3x_state_note_u32("chromakeyValue", value);
}

void __stdcall grClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy) {
    g_st.clip[0] = minx; g_st.clip[1] = miny; g_st.clip[2] = maxx; g_st.clip[3] = maxy;
    g3x_log_once("grClipWindow", "%u,%u-%u,%u", minx, miny, maxx, maxy);
}

void __stdcall grColorMask(FxBool rgb, FxBool a) {
    g_st.mask_rgb = rgb; g_st.mask_a = a;
    g3x_state_note_u32("colorMask", ((unsigned)rgb << 1) | (unsigned)a);
}

void __stdcall grConstantColorValue(GrColor_t value) {
    g_st.constant_color = value;
    // P2.5 lane decode: the game builds this as ScalePaletteValue(v) | alpha =
    // palette_entry(0xFFRRGGBB) << 8 | alpha -> [R:24-31][G:16-23][B:8-15]
    // [alpha:0-7]; the raster's fill color is (value >> 8) & 0xFFFFFF in our
    // 0xAARRGGBB pixels, blend alpha stays in bits 7..0.
    g3x_frame_set_constant(value);
    g3x_state_note_u32("constantColor", value);
}

void __stdcall grDepthBufferFunction(GrCmpFnc_t function) {
    g_st.depth_func = function;
    g3x_state_note_u32("depthFunc", function);
}

void __stdcall grDepthBufferMode(GrDepthBufferMode_t mode) {
    g_st.depth_mode = mode;
    g3x_state_note_u32("depthMode", mode);
}

void __stdcall grDepthMask(FxBool mask) {
    g_st.depth_mask = mask;
    g3x_state_note_u32("depthMask", (unsigned)mask);
}

void __stdcall grDitherMode(GrDitherMode_t mode) {
    g_st.dither_mode = mode;
    g3x_state_note_u32("ditherMode", mode);
}

void __stdcall grEnable(GrEnableMode_t mode) {
    g_st.enable_value = 1; g_st.enabled[0] = mode;
    g3x_state_note_u32("enable", mode);
}

void __stdcall grDisable(GrEnableMode_t mode) {
    g_st.enable_value = 0; g_st.enabled[0] = mode;
    g3x_state_note_u32("disable", mode);
}

void __stdcall grCoordinateSpace(GrCoordinateSpaceMode_t mode) {
    g_st.coord_space = mode;
    g3x_state_note_u32("coordinateSpace", mode);
}

void __stdcall guGammaCorrectionRGB(FxFloat red, FxFloat green, FxFloat blue) {
    g_st.gamma[0] = red; g_st.gamma[1] = green; g_st.gamma[2] = blue;
    g3x_frame_set_gamma(red, green, blue);   // applied in the present scale pass
    g3x_log_once("guGammaCorrectionRGB", "%.3f %.3f %.3f (applied in present)",
                 red, green, blue);
}

void __stdcall grLoadGammaTable(FxU32 nentries, FxU32* red, FxU32* green, FxU32* blue) {
    g_st.gamma_entries = nentries;
    g3x_log_once("grLoadGammaTable", "entries=%u", nentries);
}

// ---------------------------------------------------------------------------
// Textures (P1: track; P2: upload as 8bpp index + palette LUT)
// ---------------------------------------------------------------------------
FxU32 __stdcall grTexTextureMemRequired(FxU32 evenOdd, GrTexInfo* info) {
    if (!info) return 0;
    // Same dims convention as the raster's g3x_tex_dims: max dim = 2^lod, the
    // other = 2^(lod-|aspect|); 8bpp formats cost 1 B/texel, the rest 2 B.
    FxU32 bytes = 0;
    for (unsigned lod = info->smallLodLog2; lod <= info->largeLodLog2 && lod < 12; ++lod) {
        unsigned w = 0, h = 0;
        g3x_tex_dims(lod, (int)info->aspectRatioLog2, &w, &h);
        bytes += w * h * ((info->format <= 0x3u) ? 1u : 2u);
    }
    return bytes;
}

FxU32 __stdcall grTexMinAddress(GrChipID_t tmu) {
    return 0;
}

FxU32 __stdcall grTexMaxAddress(GrChipID_t tmu) {
    return 0x00800000u; // 8 MB per TMU — generous (PLAN.md R6)
}

void __stdcall grTexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo* info) {
    if (tmu < 2) {
        g_st.tex_source_tmu[tmu] = tmu;
        g_st.tex_addr[tmu] = startAddress;
        if (info) g_st.tex_format[tmu] = info->format;
    }
    g3x_frame_tex_source(tmu, startAddress);   // bind for the P2.1 raster
    g3x_log_once("grTexSource", "tmu=%u addr=0x%08x fmt=%u", tmu, startAddress,
                 info ? info->format : 0);
}

void __stdcall grTexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd,
                                   GrTexInfo* info) {
    ++g_st.n_tex_downloads;
    g3x_frame_tex_download(tmu, startAddress, info);   // P2.1: real upload (CPU copy)
    g3x_log_once("grTexDownloadMipMap", "tmu=%u addr=0x%08x fmt=%u lods %u..%u",
                 tmu, startAddress, info ? info->format : 0,
                 info ? info->smallLodLog2 : 0, info ? info->largeLodLog2 : 0);
}

void __stdcall grTexDownloadTable(GrTexTable_t type, void* data) {
    if (type == G3X_GR_TEXTABLE_PALETTE && data) {
        // Palette-indexed path (observed: D2Glide/Renderer/Glide.cpp:200). Copy
        // 256 entries — the game may free or overwrite its buffer afterwards.
        memcpy(g_st.palette, data, sizeof(g_st.palette));
        g_st.palette_valid = FXTRUE;
        ++g_st.n_palette_downloads;
        g3x_frame_set_palette(data);   // active palette for the P2.1 raster
        // Field diagnostic (round 6): entries 0/128/255 — settles where the
        // transparent marker and the text-ink ramp live in each palette.
        g3x_log_once("grTexDownloadTable:palette", "256-entry palette copied "
                     "(pal[0]=0x%08x pal[128]=0x%08x pal[255]=0x%08x)",
                     g_st.palette[0], g_st.palette[128], g_st.palette[255]);
    } else {
        g3x_log_once("grTexDownloadTable", "type=%u (unhandled)", type);
    }
}

void __stdcall grTexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minfilter_mode,
                               GrTextureFilterMode_t magfilter_mode) {
    if (tmu < 2) {
        g_st.tex_filter[tmu][0] = minfilter_mode;
        g_st.tex_filter[tmu][1] = magfilter_mode;
    }
    g3x_state_note_u32("texFilterMode", (tmu << 16) | (minfilter_mode << 8) | magfilter_mode);
}

void __stdcall grTexMipMapMode(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend) {
    if (tmu < 2) {
        g_st.tex_mipmap[tmu] = mode;
        g_st.tex_lodblend[tmu] = lodBlend;
    }
    g3x_state_note_u32("texMipMapMode", (tmu << 16) | (mode << 8) | (unsigned)lodBlend);
}

// ---------------------------------------------------------------------------
// Geometry (P2.1: TRIANGLE_FAN quads rasterize; other primitives still counted)
// ---------------------------------------------------------------------------
void __stdcall grDrawVertexArrayContiguous(FxU32 mode, FxU32 Count, void* pointers,
                                           FxU32 stride) {
    ++g_st.n_draw_calls;
    ++g_st.n_vac;
    if (mode == G3X_GR_TRIANGLE_FAN && Count >= 3) {
        g3x_frame_draw_fan(Count, pointers, stride);   // the D2 UI/world quad path
    }
    if (stride != G3X_D2_VERTEX_STRIDE) {
        g3x_log_once("draw:stride", "UNEXPECTED stride=0x%x (spec says 0x1C)", stride);
    }
    // Field diagnostic: distinct (mode,count) shapes — anything other than the
    // 4-vertex fan quads changes what the rasterizer must handle.
    static unsigned s_shape_mode[8], s_shape_count[8], s_shape_n = 0;
    bool known = false;
    for (unsigned i = 0; i < s_shape_n; ++i) {
        if (s_shape_mode[i] == mode && s_shape_count[i] == Count) { known = true; break; }
    }
    if (!known && s_shape_n < 8) {
        s_shape_mode[s_shape_n] = mode; s_shape_count[s_shape_n] = Count; ++s_shape_n;
        g3x_log("drawshape #%u: mode=%u count=%u", s_shape_n, mode, Count);
    }
    if (g_st.n_draw_calls <= 4) {
        g3x_log("grDrawVertexArrayContiguous: mode=%u count=%u stride=0x%x ptr=%p (call %u)",
                mode, Count, stride, pointers, g_st.n_draw_calls);
    } else {
        g3x_log_once("draw:vac", "mode=%u count=%u stride=0x%x", mode, Count, stride);
    }
}

FxU32 __stdcall grDrawVertexArray(FxU32 mode, FxU32 Count, void* pointers) {
    ++g_st.n_draw_calls;
    ++g_st.n_parr;
    // D2's ground/wall path (D2Glide/Renderer/Glide.cpp): `pointers` is an
    // array of Count vertex pointers, each to a 0x1C-stride vertex.
    //   DrawGroundTile: 5x mode-4 strips of 12 verts (normal quality) or one
    //   mode-5 fan of 4 (low quality) — P2.2 and earlier ignored this export
    //   entirely, which is why the world floor rendered black.
    //   DrawWallTile*: 2x mode-4 strips of 10 verts.
    if (pointers && Count >= 3) {
        g3x_frame_draw_pointers(mode, Count, (void* const*)pointers);
    }
    g3x_log_once("grDrawVertexArray", "mode=%u count=%u", mode, Count);
    return Count;
}

void __stdcall grDrawPoint(void* pt) {
    ++g_st.n_draw_calls;
    ++g_st.n_point;
    // P2.5: real raster. Callers (D2Glide DrawPoint path + the radial-light
    // dot helpers) always run constant-fill + chromakey-off; the vertex is
    // x,y floats at offset 0/4.
    if (pt) {
        float x, y;
        memcpy(&x, pt, 4);
        memcpy(&y, (const char*)pt + 4, 4);
        g3x_frame_draw_point(x, y);
    }
    g3x_log_once("grDrawPoint", "pt=%p", pt);
}

void __stdcall grDrawLine(void* v1, void* v2) {
    ++g_st.n_draw_calls;
    ++g_st.n_line;
    // P2.5: real raster (Bresenham). Callers set constant-fill + chromakey-off;
    // each endpoint vertex is 24 bytes with x,y floats at offset 0/4.
    if (v1 && v2) {
        float x0, y0, x1, y1;
        memcpy(&x0, v1, 4);
        memcpy(&y0, (const char*)v1 + 4, 4);
        memcpy(&x1, v2, 4);
        memcpy(&y1, (const char*)v2 + 4, 4);
        g3x_frame_draw_line(x0, y0, x1, y1);
    }
    g3x_log_once("grDrawLine", "v1=%p v2=%p", v1, v2);
}

void __stdcall grDrawTriangle(void* a, void* b, void* c) {
    ++g_st.n_draw_calls;
    ++g_st.n_tri;
    g3x_log_once("grDrawTriangle", "a=%p b=%p c=%p", a, b, c);
}

void __stdcall grVertexLayout(FxU32 param, FxI32 offset, FxU32 mode) {
    // Decode the observed D2 vertex (glide.h GrParam): a 20-byte vertex of
    // XY@0, PARGB@8, Q0@12, ST0@16 — the P2 vertex-input contract.
    const char* name = (param == 0x01u) ? "XY"
                     : (param == 0x30u) ? "PARGB"
                     : (param == 0x50u) ? "Q0"
                     : (param == 0x40u) ? "ST0" : "?";
    g3x_log_once("grVertexLayout", "param=0x%02x(%s) offset=%d mode=%u",
                 param, name, offset, mode);
}

// ---------------------------------------------------------------------------
// LFB (framebuffer linear access) — the Bink video path. The observed caller
// is D2Glide/glBlocks.cpp GLBLOCKS_RenderBinkVideoFrame:
//   grLfbLock(1,0,GR_LFBWRITEMODE_8888,UPPER_LEFT)
//   -> BinkCopyToBuffer(hBink, lfbPtr, OUR stride, h, xOffset, yOffset, 2)
//   -> grLfbUnlock
// On a real Voodoo the front-buffer write is immediately visible and the video
// loop never calls grBufferSwap — so here, unlock = present. Since P2.1 the
// lock hands out the SAME game framebuffer the raster draws into (frame is
// memset black per video frame; Bink overwrites only its centered rect).
// The READ_ONLY back-buffer call (GetBackBuffer, 565 read-back for fades)
// returns the frame too; interpreting 32bpp rows as 565 is a known P2 gap.
// ---------------------------------------------------------------------------
static bool s_lfb_write_pending = false;

FxBool __stdcall grLfbLock(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode,
                           GrOriginLocation_t origin, FxBool pixelPipeline,
                           GrLfbInfo_t* info) {
    ++g_st.n_lfb_locks;
    if (!info || info->size < (int)sizeof(GrLfbInfo_t)) return FXFALSE;
    uint8_t* px = g3x_frame_pixels();
    if (!px) return FXFALSE;
    s_lfb_write_pending = (type == G3X_GR_LFB_WRITE_ONLY &&
                           buffer == G3X_GR_BUFFER_FRONTBUFFER);
    if (s_lfb_write_pending) {
        // game clears color=0 (black) and Bink overwrites only its centered
        // rect — start every video frame from black so the letterbox is clean.
        memset(px, 0, (size_t)g3x_frame_stride() * g3x_frame_height());
    }
    info->lfbPtr = px;
    info->strideInBytes = g3x_frame_stride();
    info->writeMode = writeMode;
    info->origin = origin;
    g3x_log_once("grLfbLock", "type=%u buffer=%u writeMode=0x%02x origin=%u -> %ux%u frame "
                 "stride=%u (unlock=present)", type, buffer, writeMode, origin,
                 g3x_frame_width(), g3x_frame_height(), g3x_frame_stride());
    return FXTRUE;
}

FxBool __stdcall grLfbUnlock(GrLock_t type, GrBuffer_t buffer) {
    if (s_lfb_write_pending) {
        g3x_frame_present();
        s_lfb_write_pending = false;
        g3x_log_once("grLfbUnlock", "type=%u buffer=%u -> front-buffer frame presented",
                     type, buffer);
    } else {
        g3x_log_once("grLfbUnlock", "type=%u buffer=%u", type, buffer);
    }
    return FXTRUE;
}

} // extern "C"

static void g3x_banner() {
    char exe[MAX_PATH] = "(?)";
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    g3x_log("glide3xvk scaffold %s — loaded into %s", G3XVK_BUILD_ID, exe);
    g3x_log("compiled " __DATE__ " " __TIME__
            "; P2.5 = scanline raster (span fill + exact boundary fixups)"
            " + grDrawPoint/grDrawLine + constant-color RGB lanes + perf paths");
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL; (void)lpReserved;
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g3x_log_open();
            g3x_banner();
            break;
        case DLL_PROCESS_DETACH:
            g3x_log("process detach (swaps=%u draws=%u texmiss=%u)", g_st.n_swaps,
                    g_st.n_draw_calls, g3x_frame_tex_miss_count());
            // Field diagnostics (v2): if the game dropped us after init, the
            // competing renderer backends appear as loaded modules. D2Glide.dll
            // is our own caller (always loaded — not diagnostic); the D3D/DDraw/
            // GDI backends only load if D2GFX fell back to them.
            for (const char* mod : {"D2Direct3D.dll", "D2DDraw.dll", "D2GDI.dll"}) {
                if (GetModuleHandleA(mod)) {
                    g3x_log("detach: %s loaded (game fell back to another renderer?)", mod);
                }
            }
            g3x_vk_shutdown();
            g3x_log_close();
            break;
        default:
            break;
    }
    return TRUE;
}
