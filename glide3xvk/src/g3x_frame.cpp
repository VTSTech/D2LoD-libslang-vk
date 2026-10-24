#include "g3x_frame.h"
#include "g3x_log.h"
#include "vk_backend.h"
#include "g3x_overlay.h"
#include "g3x_slang.h"

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <new>

// Game-side dims convention (D2Glide/Renderer/Glide.cpp
// RENDERER_GLIDE_CalculateTextureSize + the real glide.h aspect encoding:
// GR_ASPECT_LOG2_8x1=3 .. 2x1=1 wide, 1x2=-1 .. 1x8=-3 tall): the max
// dimension is 2^lod, the other is 2^(lod-|aspect|).
void g3x_tex_dims(unsigned lod, int aspect, unsigned* w, unsigned* h) {
    if (lod > 11) lod = 11;
    if (aspect > 3) aspect = 3;
    if (aspect < -3) aspect = -3;
    if (aspect >= 0) {
        *w = 1u << lod;
        *h = 1u << (lod - (aspect > (int)lod ? (int)lod : aspect));
    } else {
        *w = 1u << (lod + (-aspect > (int)lod ? -(int)lod : aspect));
        *h = 1u << lod;
    }
}

namespace {

// ---- frame ----------------------------------------------------------------
uint32_t* s_px = nullptr;            // ARGB dwords (LE bytes = B,G,R,A)
unsigned  s_w = 0, s_h = 0;

// ---- state ----------------------------------------------------------------
uint32_t s_palette[256] = {};        // ARGB dwords as downloaded
unsigned s_palette_epoch = 0;        // bumped on every palette download
unsigned s_chroma_mode = 0;
unsigned s_chroma_value = 0;
bool     s_chroma_on = false;
unsigned s_chroma_toggles = 0;       // P2.8: grChromakeyMode/value changes
unsigned s_blend_sf = 4, s_blend_df = 0;   // GR_BLEND rgb pair; default ONE,ZERO
uint32_t s_constant = 0xFFu;         // grConstantColorValue, raw as passed
unsigned s_const_alpha = 0xFFu;      // game packs the blend alpha in bits 7..0
bool     s_color_fill = false;       // colorCombine mode 2 = constant fill
uint8_t  s_gamma_lut[3][256];        // per channel; identity until set_gamma
bool     s_gamma_identity = true;

// Constant fill source. The game builds grConstantColorValue as
// ScalePaletteValue(v) | alpha = palette_entry << 8 | alpha, and a palette
// entry is 0xFFRRGGBB — so the constant dword is [R:24-31][G:16-23][B:8-15]
// [alpha:0-7]. Shifting right by 8 lands R,G,B exactly in this framebuffer's
// 0xAARRGGBB lanes (gray constants are unchanged from the P2.2 decode; colored
// ones — e.g. the menu fire glow 0xf4c04cc0 = orange — now decode correctly).
inline uint32_t const_fill_rgb() {
    return (s_constant >> 8) & 0x00FFFFFFu;
}

// ---- blend ----------------------------------------------------------------
// The five GR_BLEND pairs D2 actually submits, hoisted out of the inner loops
// (P2.5). Fast-path lane math is PROVEN equal to the P2.1 shade_blend:
//   (x*255+255)>>8 == x for all x in 0..255, so ONE/ZERO terms collapse;
//   the LERP/MUL forms keep the exact (x*y+255)>>8 rounding the fxtest pins.
enum BlendClass { BC_OPAQUE, BC_ADD, BC_LERP, BC_MUL, BC_GEN };
BlendClass s_blend_class = BC_OPAQUE;

static void blend_reclass() {
    if      (s_blend_sf == 4 && s_blend_df == 0) s_blend_class = BC_OPAQUE;
    else if (s_blend_sf == 4 && s_blend_df == 4) s_blend_class = BC_ADD;
    else if (s_blend_sf == 1 && s_blend_df == 5) s_blend_class = BC_LERP;
    else if (s_blend_sf == 0 && s_blend_df == 2) s_blend_class = BC_MUL;
    else                                         s_blend_class = BC_GEN;
}

// Generic fallback — the P2.1 shade_blend body, verbatim.
inline uint32_t blend_gen(uint32_t dst, uint32_t src) {
    const unsigned sf = s_blend_sf, df = s_blend_df;
    const unsigned s[3] = { src & 0xFF, (src >> 8) & 0xFF, (src >> 16) & 0xFF };
    const unsigned d[3] = { dst & 0xFF, (dst >> 8) & 0xFF, (dst >> 16) & 0xFF };
    unsigned out[3];
    for (int i = 0; i < 3; ++i) {
        unsigned sc, dc;
        switch (sf) {
            case 0:  sc = 0; break;                          // ZERO
            case 1:  sc = s_const_alpha; break;              // SRC_ALPHA
            case 2:  sc = s[i]; break;                       // SRC_COLOR
            case 4:  sc = 255; break;                        // ONE
            case 5:  sc = 255 - s_const_alpha; break;        // ONE_MINUS_SRC_ALPHA
            default: sc = 255; break;
        }
        switch (df) {
            case 0:  dc = 0; break;                          // ZERO
            case 1:  dc = s_const_alpha; break;              // SRC_ALPHA
            case 2:  dc = s[i]; break;                       // SRC_COLOR (dst * src)
            case 3:  dc = (dst >> 24) & 0xFF; break;         // DST_ALPHA
            case 4:  dc = 255; break;                        // ONE
            case 5:  dc = 255 - s_const_alpha; break;
            default: dc = 0; break;
        }
        unsigned v = (s[i] * sc + 255) >> 8;
        v += (d[i] * dc + 255) >> 8;
        out[i] = v > 255 ? 255 : v;
    }
    return out[0] | (out[1] << 8) | (out[2] << 16) | 0xFF000000u;
}

inline uint32_t blend_px(uint32_t dst, uint32_t src) {
    switch (s_blend_class) {
        case BC_OPAQUE:
            return src | 0xFF000000u;
        case BC_ADD: {
            unsigned b = (dst & 0xFFu) + (src & 0xFFu);
            unsigned g = ((dst >> 8) & 0xFFu) + ((src >> 8) & 0xFFu);
            unsigned r = ((dst >> 16) & 0xFFu) + ((src >> 16) & 0xFFu);
            if (b > 255u) b = 255u;
            if (g > 255u) g = 255u;
            if (r > 255u) r = 255u;
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        case BC_LERP: {
            const unsigned a = s_const_alpha, ia = 255u - a;
            unsigned b = (((src & 0xFFu) * a + 255u) >> 8) + (((dst & 0xFFu) * ia + 255u) >> 8);
            unsigned g = ((((src >> 8) & 0xFFu) * a + 255u) >> 8) + ((((dst >> 8) & 0xFFu) * ia + 255u) >> 8);
            unsigned r = ((((src >> 16) & 0xFFu) * a + 255u) >> 8) + ((((dst >> 16) & 0xFFu) * ia + 255u) >> 8);
            if (b > 255u) b = 255u;
            if (g > 255u) g = 255u;
            if (r > 255u) r = 255u;
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        case BC_MUL: {
            unsigned b = ((dst & 0xFFu) * (src & 0xFFu) + 255u) >> 8;
            unsigned g = (((dst >> 8) & 0xFFu) * ((src >> 8) & 0xFFu) + 255u) >> 8;
            unsigned r = (((dst >> 16) & 0xFFu) * ((src >> 16) & 0xFFu) + 255u) >> 8;
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        default:
            return blend_gen(dst, src);
    }
}

// ---- chroma ---------------------------------------------------------------
// P2.9 ROOT-CAUSE FIX (round-11 field evidence: in-game black rects around
// every sprite/wall tile + solid black bars behind the menu button labels):
// the old rule (post-palette RGB == chroma key) never fired — the game sets
// grChromakeyValue(0xff) = pure blue while EVERY downloaded palette carries
// pal[0] = 0xff000000 black, so hit[] stayed all-zero and every DC6 sprite's
// transparent texels painted black.
// The D2 ground truth (D2CMP/SRC/FastCmp.cpp FASTCMP_DecompressRLE +
// RENDERER_GLIDE_UploadTextureToCache): DC6 RLE transparent runs are SKIPPED
// over a memset-0 upload buffer, so the transparent texel is palette INDEX 0
// in every sprite and tile texture (index 1 is the darkest art color; 0 is
// reserved). Real hardware punched those texels — so the working chroma rule
// is: discard texel when its index is 0, or when its post-palette RGB equals
// the key (kept for completeness).
uint8_t s_chroma_hit[256];
bool    s_chroma_dirty = true;

inline void chroma_sync() {
    if (!s_chroma_dirty) return;
    const uint32_t key = s_chroma_value & 0x00FFFFFFu;
    for (int i = 0; i < 256; ++i)
        s_chroma_hit[i] = (uint8_t)((s_palette[i] & 0x00FFFFFFu) == key);
    s_chroma_hit[0] = 1;   // P2.9: the DC6 transparent index
    s_chroma_dirty = false;
}

// ---- vertex-color-multiplied palette cache --------------------------------
// Ground/UI draws repeat one vertex color across a whole tile/sprite; the
// texel*color multiply then collapses to one LUT load per pixel.
uint32_t s_pal_mult[256];
uint32_t s_pal_mult_vc = 0xFFFFFFFFu;      // vc RGB the table was built for
unsigned s_pal_mult_epoch = 0xFFFFFFFFu;

void pal_mult_build(uint32_t vc) {
    const unsigned vr = vc & 0xFFu, vg = (vc >> 8) & 0xFFu, vb = (vc >> 16) & 0xFFu;
    for (int i = 0; i < 256; ++i) {
        const uint32_t p = s_palette[i];
        s_pal_mult[i] = 0xFF000000u
            | ((((p & 0xFFu) * vr + 255u) >> 8))
            | ((((p >> 8) & 0xFFu) * vg + 255u) >> 8) << 8
            | ((((p >> 16) & 0xFFu) * vb + 255u) >> 8) << 16;
    }
    s_pal_mult_vc = vc;
    s_pal_mult_epoch = s_palette_epoch;
}

// ---- textures -------------------------------------------------------------
struct Tex {
    uint8_t* data;                   // our own copy (base lod first)
    unsigned w, h;                   // texel dims of the base (large) lod
    unsigned lod;                    // largeLodLog2 — the 2^lod coord space
    unsigned bpp;                    // 1 for P_8
    bool     used;
};
constexpr unsigned kMaxTex = 8192;   // in-game tile churn needs headroom (943+ dl/round-5;
                                     // 4096 risked mid-session exhaustion -> stale binds)
Tex         s_tex[kMaxTex];
unsigned    s_keys[kMaxTex];         // (tmu<<28) | (addr & 0x0FFFFFFF)
unsigned    s_tex_used_hwm = 0;      // P2.8: high-water of live slots
unsigned    s_tex_dropped_total = 0; // P2.8: downloads rejected (table full)
int         s_cur = -1;              // bound texture slot (grTexSource)

bool s_warned_tex_full = false;

unsigned tex_key(unsigned tmu, unsigned addr) {
    return (tmu << 28) | (addr & 0x0FFFFFFFu);
}

unsigned tex_bytes_per_pixel(unsigned fmt) {
    // GR_TEXFMT_P_8 (5) and the other 8bpp formats are 1 byte; the rest 2.
    return (fmt == 0x5u || fmt <= 0x3u) ? 1u : 2u;
}

// P2.12 perf: key -> slot+1 open-addressing hash. g3x_frame_tex_source was a
// linear scan over up to kMaxTex slots per bind and tex_slot_alloc rescanned
// from 0 on every download — with the in-game table at several thousand live
// slots and hundreds of binds+downloads per frame that was real per-frame CPU
// (the round-14 field log shows ~500 draws/frame with continuous tile churn).
// Slots are never freed (no glide call releases one), so the table is
// insert-only and a monotonic allocation cursor preserves the exact
// first-free slot order of the old scan while making it O(1) amortized.
constexpr unsigned kTexHashBits = 14;                 // 16384 > 2*kMaxTex
constexpr unsigned kTexHashSize = 1u << kTexHashBits;
unsigned s_tex_hash[kTexHashSize] = {};               // 0 = empty; else slot+1
unsigned s_tex_alloc_cursor = 0;                      // first possibly-free slot

unsigned tex_hash(unsigned key) {
    return (key * 2654435761u) >> (32 - kTexHashBits);
}

int tex_slot_find(unsigned key) {
    unsigned h = tex_hash(key);
    for (;;) {
        const unsigned v = s_tex_hash[h];
        if (!v) return -1;
        const int slot = (int)v - 1;
        if (s_keys[slot] == key && s_tex[slot].used) return slot;
        h = (h + 1) & (kTexHashSize - 1);
    }
}

int tex_slot_alloc(unsigned key) {
    const int found = tex_slot_find(key);
    if (found >= 0) return found;
    while (s_tex_alloc_cursor < kMaxTex && s_tex[s_tex_alloc_cursor].used)
        ++s_tex_alloc_cursor;
    if (s_tex_alloc_cursor >= kMaxTex) return -1;
    return (int)s_tex_alloc_cursor;
}

// Insert a freshly allocated slot's key. No-op when the key is already in the
// table (re-download of a live texture through the tex_slot_find fast path).
void tex_hash_insert(unsigned key, int slot) {
    unsigned h = tex_hash(key);
    for (;;) {
        const unsigned v = s_tex_hash[h];
        if (!v) { s_tex_hash[h] = (unsigned)slot + 1; return; }
        const int s = (int)v - 1;
        if (s_keys[s] == key && s_tex[s].used) return;
        h = (h + 1) & (kTexHashSize - 1);
    }
}

unsigned s_tex_miss_total = 0;       // exit-summary diagnostic (round 6)

// P2.4 draw-path counters (surfaced via g3x_frame_draw_stats for the periodic
// frame lines — with per-call tags budget-quiet, these are the only window
// into what a gameplay session actually submits). Declared before the raster
// section: fill_rect_fast bumps s_n_fill (P2.7 fill diagnostic).
static unsigned s_n_vac = 0;          // grDrawVertexArrayContiguous fans
static unsigned s_n_parr = 0;         // grDrawVertexArray pointer-array draws
static unsigned s_n_parr_strip = 0;
static unsigned s_n_parr_fan = 0;
static unsigned s_n_skip_notex = 0;   // draws dropped: no texture bound
static unsigned s_n_space256 = 0;     // draws on sub-256-mip textures (P2.7:
                                      // the class the square-space scale fixes)
static unsigned s_n_rgb565 = 0;       // draws sampled from RGB_565 textures (P2.6)
static unsigned s_n_fill = 0;         // constant-fill quads (axis-aligned fast path)
static unsigned s_n_vc0 = 0;          // P2.8: textured quads with (near-)uniform
                                      // BLACK vertex color — the shadow/darken
                                      // class; opaque-black rects unless chroma
                                      // punches their transparent texels

// ---- raster ---------------------------------------------------------------
struct RVert { float x, y, u, v; uint32_t argb; };

// The D2 glide vertex: x,y @0 (f32), PARGB @8, oow @12, s,t @16/20 (0x1C stride).
inline void read_vert(RVert& out, const uint8_t* p) {
    memcpy(&out.x,    p + 0,  4);
    memcpy(&out.y,    p + 4,  4);
    memcpy(&out.argb, p + 8,  4);
    memcpy(&out.u,    p + 16, 4);   // s (ST0 @16)
    memcpy(&out.v,    p + 20, 4);   // t @20
}

// Top-left ownership rule (uniform form, y-down, interior = f>0 side): a pixel
// exactly on an edge belongs to the triangle whose interior is below/right of
// it — otherwise shared fan diagonals get drawn twice (visible with additive
// blend) or not at all (hairline seams with opaque).
inline bool owns_edge(const RVert& p, const RVert& q) {
    const float dy = q.y - p.y;
    if (dy != 0.0f) return dy < 0.0f;
    return (q.x - p.x) > 0.0f;
}

// Exact per-pixel inside test — the P2.1 semantics, verbatim. Only the span
// boundary pixels run this (see raster_tri); the interior is covered by the
// row interval math, so the per-pixel cost of the old full barycentric test
// disappears while tie/ownership behavior stays bit-identical.
inline bool inside_exact(int x, float py, const RVert& A, const RVert& B, const RVert& C,
                         float inv, bool own0, bool own1, bool own2,
                         float& wa, float& wb, float& wc) {
    const float px = (float)x + 0.5f;
    wa = ((C.x - B.x) * (py - B.y) - (C.y - B.y) * (px - B.x)) * inv;
    wb = ((A.x - C.x) * (py - C.y) - (A.y - C.y) * (px - C.x)) * inv;
    wc = ((B.x - A.x) * (py - A.y) - (B.y - A.y) * (px - A.x)) * inv;
    if (wa < 0.0f || wb < 0.0f || wc < 0.0f) return false;
    if ((wa == 0.0f && !own1) || (wb == 0.0f && !own2) || (wc == 0.0f && !own0)) return false;
    return true;
}

// Row fill for pixels [sx, ex). Vertex attributes are interpolated along the
// row from the exact values at sx (computed by the boundary fixup); texel
// sampling floor-WRAPS both axes via the +4096 trick: 4096 is a multiple of
// every texture dim (<= 256), and (int)(x + 4096) == floor(x) + 4096 for all
// x > -4096, so truncation equals floor and the mask is unaffected — this
// replaces two floorf() calls per pixel (the P2.1-P2.4 inner loop's worst
// scalar cost) with one float add and an int truncation.
static void fill_span(uint32_t* row, int sx, int ex,
                      float u, float v, float dudx, float dvdx,
                      bool textured, const Tex& t,
                      bool varying_vc,
                      const float* vc0, const float* dvc,
                      uint32_t fill_src, float tsx, float tsy) {
    const bool chroma = s_chroma_on && t.bpp == 1;
    const uint8_t* hit = s_chroma_hit;
    const uint32_t* pmult = s_pal_mult;
    const uint8_t* tdata = t.data;
    const unsigned tw = t.w, th = t.h;
    float cr = vc0[0], cg = vc0[1], cb = vc0[2];

    for (int x = sx; x < ex; ++x) {
        uint32_t src;
        if (!textured) {
            src = fill_src;
        } else if (t.bpp == 1 && tdata) {
            const unsigned tx = (unsigned)(int)(u * tsx + 4096.0f) & (tw - 1);
            const unsigned ty = (unsigned)(int)(v * tsy + 4096.0f) & (th - 1);
            const unsigned idx = tdata[(size_t)ty * tw + tx];
            if (chroma && hit[idx]) {
                u += dudx; v += dvdx;
                if (varying_vc) { cr += dvc[0]; cg += dvc[1]; cb += dvc[2]; }
                continue;
            }
            if (varying_vc) {
                // colorCombine SCALE_OTHER x LOCAL: texel * interpolated color
                unsigned vr = (unsigned)(0.5f + cr); if (vr > 255u) vr = 255u;
                unsigned vg = (unsigned)(0.5f + cg); if (vg > 255u) vg = 255u;
                unsigned vb = (unsigned)(0.5f + cb); if (vb > 255u) vb = 255u;
                const uint32_t p = s_palette[idx];
                src = (((p & 0xFFu) * vr + 255u) >> 8)
                    | (((((p >> 8) & 0xFFu) * vg + 255u) >> 8) << 8)
                    | (((((p >> 16) & 0xFFu) * vb + 255u) >> 8) << 16);
            } else {
                src = pmult[idx];
            }
        } else if (t.bpp == 2 && tdata) {
            // GR_TEXFMT_RGB_565 (format 0xa) — the in-game Smacker video quads
            // (GLSMACK_DrawSmackerQuad via gTexInfoBlocks0/1Glide, 256x64).
            // P2.5 and earlier sampled every non-P_8 texel as black.
            const unsigned tx = (unsigned)(int)(u * tsx + 4096.0f) & (tw - 1);
            const unsigned ty = (unsigned)(int)(v * tsy + 4096.0f) & (th - 1);
            const uint16_t p5 = ((const uint16_t*)tdata)[(size_t)ty * tw + tx];
            const unsigned r5 = (p5 >> 11) & 0x1Fu;
            const unsigned g6 = (p5 >> 5) & 0x3Fu;
            const unsigned b5 = p5 & 0x1Fu;
            src = 0xFF000000u
                | ((r5 << 3 | r5 >> 2) << 16)
                | ((g6 << 2 | g6 >> 4) << 8)
                | (b5 << 3 | b5 >> 2);
        } else {
            src = 0xFF000000u;   // other formats: still black, no chroma
        }
        row[x] = blend_px(row[x], src);
        u += dudx; v += dvdx;
        if (varying_vc) { cr += dvc[0]; cg += dvc[1]; cb += dvc[2]; }
    }
}

// P2.5 scanline triangle: the three directed edges confine the interior to
// E(x,y) >= 0 where E is the P2.1 cross form; along a scanline each E is
// affine in px, so the row's coverage is one px interval per edge. The
// interval endpoints are found by solving E=0 (one division per slanted
// edge), the interior is filled without any per-pixel edge test, and only the
// boundary pixels run the exact P2.1 test (inside_exact) so tie pixels and
// shared-edge ownership stay identical to P2.1-P2.4.
//
// Winding: the P2.1 barycentric test was orientation-agnostic (the sign of
// inv flips with the edge signs), and D2's fan quads ARE negative-area in
// y-down terms (fan#1: (0,0)(0,256)(256,256)(256,0)). So negative-area
// triangles are NOT skipped — B/C are swapped to normalize, which leaves
// every weight value bit-identical (E and inv both negate). Ownership ties
// however were decided on the ORIGINAL directed edges by P2.1-P2.4, so the
// swap inverts all three ownership flags — that reproduces the old decision
// per triangle exactly (shared diagonals still covered exactly once, by the
// same triangle as before; strips pre-normalize upstream and never swap).
static void raster_tri(const RVert& A0, const RVert& B0, const RVert& C0,
                       float tsx, float tsy) {
    if (!s_px) return;
    float area = (B0.x - A0.x) * (C0.y - A0.y) - (B0.y - A0.y) * (C0.x - A0.x);
    if (area == 0.0f) return;
    RVert A = A0, B = B0, C = C0;
    const bool swapped = area < 0.0f;
    if (swapped) { B = C0; C = B0; area = -area; }
    const float inv = 1.0f / area;
    const bool textured = !s_color_fill && s_cur >= 0;
    if (!textured && !s_color_fill) return;

    const Tex* t = textured ? &s_tex[s_cur] : nullptr;

    float minx = A.x < B.x ? (A.x < C.x ? A.x : C.x) : (B.x < C.x ? B.x : C.x);
    float miny = A.y < B.y ? (A.y < C.y ? A.y : C.y) : (B.y < C.y ? B.y : C.y);
    float maxx = A.x > B.x ? (A.x > C.x ? A.x : C.x) : (B.x > C.x ? B.x : C.x);
    float maxy = A.y > B.y ? (A.y > C.y ? A.y : C.y) : (B.y > C.y ? B.y : C.y);
    int bx0 = (int)minx;     if (bx0 < 0) bx0 = 0;
    int by0 = (int)miny;     if (by0 < 0) by0 = 0;
    int bx1 = (int)maxx + 1; if (bx1 > (int)s_w) bx1 = (int)s_w;
    int by1 = (int)maxy + 1; if (by1 > (int)s_h) by1 = (int)s_h;
    if (bx0 >= bx1 || by0 >= by1) return;

    bool own0 = owns_edge(A, B);         // wc == 0 -> pixel on edge A->B
    bool own1 = owns_edge(B, C);         // wa == 0 -> pixel on edge B->C
    bool own2 = owns_edge(C, A);         // wb == 0 -> pixel on edge C->A
    if (swapped) { own0 = !own0; own1 = !own1; own2 = !own2; }

    // constant-fill source (colorCombine mode 2)
    const uint32_t fill_src = const_fill_rgb();

    // vertex-color mode: uniform (multiplied-palette LUT) or per-pixel interp
    const bool uniform_vc = (A.argb == B.argb && B.argb == C.argb);
    if (textured && uniform_vc && !s_color_fill) {
        const uint32_t vc = A.argb & 0x00FFFFFFu;
        if (s_pal_mult_epoch != s_palette_epoch || s_pal_mult_vc != vc) pal_mult_build(vc);
    }
    float ac[3] = { (float)(A.argb & 0xFFu), (float)((A.argb >> 8) & 0xFFu), (float)((A.argb >> 16) & 0xFFu) };
    float bc[3] = { (float)(B.argb & 0xFFu), (float)((B.argb >> 8) & 0xFFu), (float)((B.argb >> 16) & 0xFFu) };
    float cc[3] = { (float)(C.argb & 0xFFu), (float)((C.argb >> 8) & 0xFFu), (float)((C.argb >> 16) & 0xFFu) };
    // per-px +x slope of each interpolated quantity (d(wa)/dpx etc. — affine)
    const float dudx = (A.u * (B.y - C.y) + B.u * (C.y - A.y) + C.u * (A.y - B.y)) * inv;
    const float dvdx = (A.v * (B.y - C.y) + B.v * (C.y - A.y) + C.v * (A.y - B.y)) * inv;
    float dvc[3] = {
        (ac[0] * (B.y - C.y) + bc[0] * (C.y - A.y) + cc[0] * (A.y - B.y)) * inv,
        (ac[1] * (B.y - C.y) + bc[1] * (C.y - A.y) + cc[1] * (A.y - B.y)) * inv,
        (ac[2] * (B.y - C.y) + bc[2] * (C.y - A.y) + cc[2] * (A.y - B.y)) * inv,
    };

    for (int y = by0; y < by1; ++y) {
        const float py = (float)y + 0.5f;
        float lo = -3.0e38f, hi = 3.0e38f;
        bool empty = false;
        // edge A->B  (its zero weight is wc)
        {
            const float dy = B.y - A.y;
            if (dy > 0.0f)      { const float xc = A.x + (B.x - A.x) * ((py - A.y) / dy); if (xc < hi) hi = xc; }
            else if (dy < 0.0f) { const float xc = A.x + (B.x - A.x) * ((py - A.y) / dy); if (xc > lo) lo = xc; }
            else {
                const float e = (B.x - A.x) * (py - A.y);
                if (e < 0.0f || (e == 0.0f && !own0)) empty = true;
            }
        }
        // edge B->C  (wa)
        {
            const float dy = C.y - B.y;
            if (dy > 0.0f)      { const float xc = B.x + (C.x - B.x) * ((py - B.y) / dy); if (xc < hi) hi = xc; }
            else if (dy < 0.0f) { const float xc = B.x + (C.x - B.x) * ((py - B.y) / dy); if (xc > lo) lo = xc; }
            else {
                const float e = (C.x - B.x) * (py - B.y);
                if (e < 0.0f || (e == 0.0f && !own1)) empty = true;
            }
        }
        // edge C->A  (wb)
        {
            const float dy = A.y - C.y;
            if (dy > 0.0f)      { const float xc = C.x + (A.x - C.x) * ((py - C.y) / dy); if (xc < hi) hi = xc; }
            else if (dy < 0.0f) { const float xc = C.x + (A.x - C.x) * ((py - C.y) / dy); if (xc > lo) lo = xc; }
            else {
                const float e = (A.x - C.x) * (py - C.y);
                if (e < 0.0f || (e == 0.0f && !own2)) empty = true;
            }
        }
        if (empty || lo > hi) continue;

        // candidate window: the rounded interval plus one pixel of slack per
        // side (division rounding is ~1e-4 px; the exact fixups below pin the
        // true boundary). [sx, ex) candidates, ex exclusive.
        int sx = (int)ceilf(lo - 0.5f) - 1;
        int ex = (int)ceilf(hi - 0.5f) + 2;
        if (sx < bx0) sx = bx0;
        if (ex > bx1) ex = bx1;

        float wa, wb, wc, t1, t2, t3;
        bool ok = false;
        for (int k = 0; k < 3 && sx < ex; ++k) {          // advance to first inside
            if (inside_exact(sx, py, A, B, C, inv, own0, own1, own2, wa, wb, wc)) { ok = true; break; }
            ++sx;
        }
        if (!ok) continue;
        for (int k = 0; k < 3 && sx > bx0; ++k) {         // retreat if rounded in too far
            if (!inside_exact(sx - 1, py, A, B, C, inv, own0, own1, own2, t1, t2, t3)) break;
            --sx; wa = t1; wb = t2; wc = t3;
        }
        for (int k = 0; k < 3 && ex > sx; ++k) {          // shrink to last inside
            if (inside_exact(ex - 1, py, A, B, C, inv, own0, own1, own2, t1, t2, t3)) break;
            --ex;
        }
        if (ex <= sx) continue;
        for (int k = 0; k < 3 && ex < bx1; ++k) {         // extend if rounded out early
            if (!inside_exact(ex, py, A, B, C, inv, own0, own1, own2, t1, t2, t3)) break;
            ++ex;
        }

        // attributes at the first covered pixel, then +slope per pixel
        float vc0[3] = {
            ac[0] * wa + bc[0] * wb + cc[0] * wc,
            ac[1] * wa + bc[1] * wb + cc[1] * wc,
            ac[2] * wa + bc[2] * wb + cc[2] * wc,
        };
        const float u0 = A.u * wa + B.u * wb + C.u * wc;
        const float v0 = A.v * wa + B.v * wb + C.v * wc;
        static const Tex k_no_tex = {};   // fill mode: hoists read it, never use it
        fill_span(s_px + (size_t)y * s_w, sx, ex, u0, v0, dudx, dvdx,
                  textured, textured ? *t : k_no_tex,
                  !uniform_vc && !s_color_fill, vc0, dvc, fill_src, tsx, tsy);
    }
}

// Fast path (P2.4, kept): axis-aligned constant-fill quads (fades, dim rects,
// loading screens — DrawSolidRectEx/DrawSolidRectAlpha) run FULL-SCREEN at
// 800x600; detect the rect shape at the quad level and fill scanlines
// directly. Blending applies per pixel through the hoisted blend class.
//
// P2.7 fill diagnostic: the round-9 field log shows ~14 black (constant RGB 0)
// LERP-alpha fills per in-game frame (constantColor=0x40/0x80/0xc0 = the
// SetDrawMode 0/1/2 dim family) and the user still reports "black bars" —
// log the first fill of each distinct (src, blend) class WITH its screen
// rect so the round-10 log pins exactly which rects those are.
bool fill_rect_fast(unsigned count, const RVert* v) {
    if (!s_color_fill || count != 4 || !s_px) return false;
    float xa = v[0].x, xb = v[0].x, ya = v[0].y, yb = v[0].y;
    bool two_x = false, two_y = false;
    for (unsigned i = 1; i < 4; ++i) {
        if (v[i].x != xa && v[i].x != xb) { if (two_x) return false; xb = v[i].x; two_x = true; }
        if (v[i].y != ya && v[i].y != yb) { if (two_y) return false; yb = v[i].y; two_y = true; }
    }
    if (!two_x || !two_y) return false;   // degenerate line
    for (unsigned i = 0; i < 4; ++i) {    // every vertex must sit on a corner
        if (v[i].x != xa && v[i].x != xb) return false;
        if (v[i].y != ya && v[i].y != yb) return false;
    }
    ++s_n_fill;
    const uint32_t fill_src = const_fill_rgb();
    {
        struct FillKey { uint32_t src; unsigned sf, df; };
        static FillKey s_seen[12]; static unsigned s_n = 0;
        const FillKey key = { fill_src, s_blend_sf, s_blend_df };
        bool seen = false;
        for (unsigned k = 0; k < s_n; ++k) {
            if (s_seen[k].src == key.src && s_seen[k].sf == key.sf && s_seen[k].df == key.df) { seen = true; break; }
        }
        if (!seen && s_n < 12) {
            s_seen[s_n++] = key;
            g3x_log("fill#%u: rect(%.0f,%.0f)..(%.0f,%.0f) src=0x%06x alpha=%u blend=(%u,%u)",
                    s_n, xa < xb ? xa : xb, ya < yb ? ya : yb, xa > xb ? xa : xb, ya > yb ? ya : yb,
                    fill_src, s_const_alpha, s_blend_sf, s_blend_df);
        }
    }
    const float minx = xa < xb ? xa : xb, maxx = xa > xb ? xa : xb;
    const float miny = ya < yb ? ya : yb, maxy = ya > yb ? ya : yb;
    // pixel centers strictly inside [min,max) — same coverage as the
    // barycentric path's top-left rule for axis-aligned integer rects
    int x0 = (int)ceilf(minx - 0.5f); if (x0 < 0) x0 = 0;
    int y0 = (int)ceilf(miny - 0.5f); if (y0 < 0) y0 = 0;
    int x1 = (int)ceilf(maxx - 0.5f); if (x1 > (int)s_w) x1 = (int)s_w;
    int y1 = (int)ceilf(maxy - 0.5f); if (y1 > (int)s_h) y1 = (int)s_h;
    for (int y = y0; y < y1; ++y) {
        uint32_t* row = s_px + (size_t)y * s_w;
        for (int x = x0; x < x1; ++x) row[x] = blend_px(row[x], fill_src);
    }
    return true;
}

// P2.7 — the UNIFORM coordinate rule (the P2.6 per-axis auto-detect is
// retired). Ground truth, decoded one level deeper into the game tree:
// DrawPalettedQuad/DrawPerspectiveImage scale raw sprite coords by
// gaPerspectiveImageSizeTable[aspect] spans {S,T} = (256,256) square,
// (256,256>>a) wide, (256>>|a|,256) tall. Reducing texel = coord/Span*dim for
// every aspect collapses to texel = coord * (2^largeLod / 256) on BOTH axes:
//   wide  (a >= 0): S span 256      -> texel_s = s*w/256 = s*2^lod/256 (w=2^lod)
//                   T span 256*h/w -> texel_t = t*h/(256*h/w) = t*w/256
//   tall  (a  < 0): S span 256*w/h -> texel_s = s*w/(256*w/h) = s*h/256 (h=2^lod)
//                   T span 256     -> texel_t = t*h/256
// i.e. coordinates ALWAYS live in the square mip space 2^largeLod x 2^largeLod
// (256 units = one full square), valid image in the [0,w)x[0,h) sub-rect.
// This is exactly the P2.2 scalar rule with lod = largeLodLog2 (which Tex
// stores). P2.6's extent heuristic mis-scaled the SHORT axis of non-square
// textures (dim/256 instead of 2^lod/256): 64x32 title buttons were crushed
// to half height (quad#5 field: rows 1..16 instead of 2..32), the 128x64
// mini-panel to half height (quad#12), 16x128 sprites to an eighth width
// (quad#14), 32x128/16x32/16x64 similarly. All 14 round-9 field quad classes
// pin 2^lod/256 on both axes with 1:1 texel:pixel mapping.
struct TexSpace { float sx, sy; };
TexSpace tex_space(const RVert* /*v*/, unsigned /*n*/, const Tex& t) {
    const float sc = (float)(1u << t.lod) / 256.0f;
    return { sc, sc };
}

// Field diagnostic (round 10): the first quads per distinct (dims) class,
// with raw vertices + the applied square-space scale — verifies the P2.7
// sampling rule directly in the field log without flooding. P2.8: the line
// also carries the blend pair + chroma state, because the two open field
// symptoms (in-game black rects, menu-label black boxes) are both
// "opaque-black unless chroma punches" classes and the log must show which
// blend/chroma regime each class drew under.
void quad_diag(unsigned count, const RVert* v, const Tex& t, float sc) {
    static unsigned s_w[14], s_h[14], s_n = 0;
    if (s_n >= 14) return;
    for (unsigned k = 0; k < s_n; ++k) {
        if (s_w[k] == t.w && s_h[k] == t.h) return;
    }
    s_w[s_n] = t.w; s_h[s_n] = t.h; ++s_n;
    RVert q[4] = { {}, {}, {}, {} };
    const unsigned n = count < 4 ? count : 4;
    for (unsigned i = 0; i < n; ++i) q[i] = v[i];
    g3x_log("quad#%u: tex %ux%u lod%u sc=%.4g blend=(%u,%u) ca=%u chm=%u "
            "v0(%.1f,%.1f|c%08x|s%.1f,t%.1f) "
            "v1(%.1f,%.1f|c%08x|s%.1f,t%.1f) v2(%.1f,%.1f|c%08x|s%.1f,t%.1f) "
            "v3(%.1f,%.1f|c%08x|s%.1f,t%.1f)",
            s_n, t.w, t.h, t.lod, sc, s_blend_sf, s_blend_df, s_const_alpha,
            (unsigned)s_chroma_on,
            q[0].x, q[0].y, q[0].argb, q[0].u, q[0].v,
            q[1].x, q[1].y, q[1].argb, q[1].u, q[1].v,
            q[2].x, q[2].y, q[2].argb, q[2].u, q[2].v,
            q[3].x, q[3].y, q[3].argb, q[3].u, q[3].v);
}

// P2.8 field diagnostic: the shadow/darken class — textured quads whose
// vertex color is (near-)uniform BLACK. Ground truth (D2Glide DrawShadow /
// DrawShadowQuad): non-blended shadows submit blend (ONE,ZERO) + all-vertex
// color 0, so the ONLY thing separating a soft shadow blob from a solid
// black rectangle is chromakey punching the texture's transparent texels.
// Menu labels draw the same way (SetDrawMode 5/7 -> opaque). Log the first
// 12 with texture dims, blend, chroma state and the screen rect so the
// round-11 log can be overlaid directly on the user's screenshots.
void vc0_diag(unsigned count, const RVert* v, const Tex& t) {
    static unsigned s_logged = 0;
    const uint32_t c0 = v[0].argb, c1 = v[1].argb, c2 = v[2].argb, c3 = v[3].argb;
    const bool three_black = (c0 == 0 && c1 == 0 && c2 == 0);
    if (!three_black) return;   // not the class (v0..v2 carry the fill color)
    ++s_n_vc0;
    if (s_logged >= 12) return;
    float xa = v[0].x, xb = v[0].x, ya = v[0].y, yb = v[0].y;
    for (unsigned i = 1; i < 4 && i < count; ++i) {
        if (v[i].x < xa) xa = v[i].x; if (v[i].x > xb) xb = v[i].x;
        if (v[i].y < ya) ya = v[i].y; if (v[i].y > yb) yb = v[i].y;
    }
    ++s_logged;
    g3x_log("vc0#%u: tex %ux%u lod%u rect(%.0f,%.0f)..(%.0f,%.0f) "
            "c=%08x/%08x/%08x/%08x blend=(%u,%u) ca=%u chm=%u fill=%u",
            s_logged, t.w, t.h, t.lod, xa, ya, xb, yb,
            c0, c1, c2, c3, s_blend_sf, s_blend_df, s_const_alpha,
            (unsigned)s_chroma_on, (unsigned)s_color_fill);
}

// P2.9 field diagnostic: the small-texture (font-glyph / UI-bit) quad class.
// The round-11 screenshots show the menu labels as glyph cores floating on
// their own black background bars — with the chroma fix the bars should
// vanish; these lines pin the glyph textures' CONTENT (top texel indices +
// their palette colors) and the quads' mapping so any residual label
// garbling can be decoded directly from the next field log.
void gly_diag(unsigned count, const RVert* v, const Tex& t, float sc) {
    static unsigned s_logged = 0;
    if (s_logged >= 10) return;
    const unsigned maxdim = t.w > t.h ? t.w : t.h;
    if (maxdim > 32 || t.bpp != 1 || !t.data) return;
    unsigned hist[256] = {};
    const unsigned n = t.w * t.h;
    for (unsigned i = 0; i < n; ++i) ++hist[t.data[i]];
    unsigned best[3] = {}, bcnt[3] = {};
    for (unsigned i = 0; i < 256; ++i) {
        if (hist[i] > bcnt[0]) {
            bcnt[2] = bcnt[1]; best[2] = best[1];
            bcnt[1] = bcnt[0]; best[1] = best[0];
            bcnt[0] = hist[i]; best[0] = i;
        } else if (hist[i] > bcnt[1]) {
            bcnt[2] = bcnt[1]; best[2] = best[1];
            bcnt[1] = hist[i]; best[1] = i;
        } else if (hist[i] > bcnt[2]) {
            bcnt[2] = hist[i]; best[2] = i;
        }
    }
    ++s_logged;
    g3x_log("gly#%u: tex %ux%u lod%u sc=%.4g rect(%.0f,%.0f)..(%.0f,%.0f) "
            "s0=%.1f t0=%.1f s2=%.1f t2=%.1f "
            "top=%u(x%u)/%u(x%u)/%u(x%u) pal[%u]=%08x pal[%u]=%08x pal[%u]=%08x "
            "blend=(%u,%u) ca=%u chm=%u",
            s_logged, t.w, t.h, t.lod, sc,
            v[0].x, v[0].y,
            v[count < 4 ? count - 1 : 2].x, v[count < 4 ? count - 1 : 2].y,
            v[0].u, v[0].v, v[2].u, v[2].v,
            best[0], bcnt[0], best[1], bcnt[1], best[2], bcnt[2],
            best[0], s_palette[best[0] & 0xFF],
            best[1], s_palette[best[1] & 0xFF],
            best[2], s_palette[best[2] & 0xFF],
            s_blend_sf, s_blend_df, s_const_alpha, (unsigned)s_chroma_on);
}

void gamma_identity() {
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < 256; ++i) s_gamma_lut[c][i] = (uint8_t)i;
}

struct GammaInit { GammaInit() { gamma_identity(); } } s_gamma_init;

} // namespace

// ---------------------------------------------------------------------------

void g3x_frame_reset(unsigned w, unsigned h) {
    if (!s_px || s_w != w || s_h != h) {
        delete[] s_px;
        s_px = new (std::nothrow) uint32_t[(size_t)w * h];
    }
    s_w = w; s_h = h;
    if (s_px) memset(s_px, 0, (size_t)w * h * 4);
}

void g3x_frame_clear(uint32_t color) {
    if (!s_px) return;
    if ((color & 0x00FFFFFFu) == 0) {           // observed: clear=0 every time
        memset(s_px, 0, (size_t)s_w * s_h * 4);
        return;
    }
    color |= 0xFF000000u;
    const size_t n = (size_t)s_w * s_h;
    for (size_t i = 0; i < n; ++i) s_px[i] = color;
}

uint8_t* g3x_frame_pixels() { return (uint8_t*)s_px; }
unsigned g3x_frame_width()  { return s_w; }
unsigned g3x_frame_height() { return s_h; }
unsigned g3x_frame_stride() { return s_w * 4; }

void g3x_frame_set_palette(const void* entries256) {
    if (entries256) memcpy(s_palette, entries256, sizeof(s_palette));
    ++s_palette_epoch;
    s_chroma_dirty = true;
}

void g3x_frame_set_chromakey(unsigned mode, unsigned value) {
    if (mode != s_chroma_mode || value != s_chroma_value) ++s_chroma_toggles;
    s_chroma_mode = mode; s_chroma_value = value;
    s_chroma_on = (mode == 1);
    s_chroma_dirty = true;
    // P2.8: the game flips chroma OFF around DrawSolidRect/DrawLightMap
    // batches and back ON after; the first 32 transitions show the exact
    // sequence (and whether it is active at label/shadow draws).
    static unsigned s_tlog = 0;
    if (s_tlog < 32) {
        ++s_tlog;
        g3x_log("chroma#%u: mode=%u value=0x%x (on=%u)", s_tlog, mode, value,
                (unsigned)s_chroma_on);
    }
}

void g3x_frame_set_blend(unsigned rgb_sf, unsigned rgb_df) {
    s_blend_sf = rgb_sf; s_blend_df = rgb_df;
    blend_reclass();
}

void g3x_frame_set_constant(unsigned argb) {
    s_constant = argb;
    s_const_alpha = argb & 0xFFu;   // game packs blend alpha in bits 7..0
}

void g3x_frame_set_color_fill(bool fill) {
    s_color_fill = fill;
}

void g3x_frame_set_gamma(float r, float g, float b) {
    const float gm[3] = { r, g, b };
    s_gamma_identity = (r == 1.0f && g == 1.0f && b == 1.0f);
    for (int c = 0; c < 3; ++c) {
        const float gamma = (gm[c] > 0.05f) ? gm[c] : 1.0f;
        const float inv = 1.0f / gamma;
        for (int i = 0; i < 256; ++i) {
            int out = (int)(pow((double)i / 255.0, (double)inv) * 255.0 + 0.5);
            if (out < 0) out = 0;
            if (out > 255) out = 255;
            s_gamma_lut[c][i] = (uint8_t)out;
        }
    }
}

bool g3x_frame_tex_download(unsigned tmu, unsigned addr, const GrTexInfo* info) {
    if (!info || !info->data) return false;
    unsigned w = 0, h = 0;
    g3x_tex_dims(info->largeLodLog2, (int)info->aspectRatioLog2, &w, &h);
    if (!w || !h) return false;
    const unsigned bpp = tex_bytes_per_pixel(info->format);
    // copy the whole mip chain when present (base = large lod comes first in
    // the glide layout); sampling only touches the base lod in P2.x.
    size_t bytes = 0;
    for (unsigned lod = info->smallLodLog2; lod <= info->largeLodLog2 && lod <= 11; ++lod) {
        unsigned lw = 0, lh = 0;
        g3x_tex_dims(lod, (int)info->aspectRatioLog2, &lw, &lh);
        bytes += (size_t)lw * lh * bpp;
    }
    if (!bytes) return false;

    const unsigned key = tex_key(tmu, addr);
    const int slot = tex_slot_alloc(key);
    if (slot < 0) {
        ++s_tex_dropped_total;
        if (!s_warned_tex_full) {
            s_warned_tex_full = true;
            g3x_log("frame: texture table full (%u) — downloads dropped", kMaxTex);
        } else if (s_tex_dropped_total % 500 == 0) {
            g3x_log("frame: texture table full — %u downloads dropped so far", s_tex_dropped_total);
        }
        return false;
    }
    Tex& t = s_tex[slot];
    uint8_t* nd = (uint8_t*)realloc(t.data, bytes);
    if (!nd) return false;
    memcpy(nd, info->data, bytes);
    t.data = nd; t.w = w; t.h = h; t.bpp = bpp; t.used = true;
    t.lod = info->largeLodLog2 > 11 ? 11 : info->largeLodLog2;
    s_keys[slot] = key;
    tex_hash_insert(key, slot);
    if (slot + 1 > (int)s_tex_used_hwm) s_tex_used_hwm = (unsigned)(slot + 1);
    return true;
}

void g3x_frame_tex_source(unsigned tmu, unsigned addr) {
    const unsigned key = tex_key(tmu, addr);
    // P2.12 perf rebind fast-out: the game re-binds the same texture across
    // consecutive draws constantly (tile strips share a download); the hash
    // lookup below is O(1), but this is O(0). Bind/miss counters unaffected:
    // the fast-out only fires when s_cur ALREADY holds the requested texture.
    if (s_cur >= 0 && s_tex[s_cur].used && s_keys[s_cur] == key) return;
    const int slot = tex_slot_find(key);
    if (slot >= 0) {
        const unsigned i = (unsigned)slot;
        {
            s_cur = (int)i;
            // Field diagnostic: log the first few DISTINCT (addr -> dims)
            // bindings so texture-size mistakes show up in the field log.
            static unsigned s_seen_addr[24];
            static unsigned s_seen_w[24], s_seen_h[24];
            static unsigned s_seen_n = 0;
            bool seen = false;
            for (unsigned k = 0; k < s_seen_n; ++k) {
                if (s_seen_addr[k] == addr && s_seen_w[k] == s_tex[i].w && s_seen_h[k] == s_tex[i].h) { seen = true; break; }
            }
            if (!seen && s_seen_n < 24) {
                s_seen_addr[s_seen_n] = addr;
                s_seen_w[s_seen_n] = s_tex[i].w;
                s_seen_h[s_seen_n] = s_tex[i].h;
                ++s_seen_n;
                g3x_log("frame: bind tmu=%u 0x%08x -> %ux%u (lod %u)", tmu, addr,
                        s_tex[i].w, s_tex[i].h, s_tex[i].lod);
            }
            return;
        }
    }
    s_cur = -1;
    ++s_tex_miss_total;
    // Field diagnostic (round 6): the first 8 DISTINCT missing addresses + a
    // running total. In-game ground/wall tiles binding nothing = black world,
    // so misses need to be visible per-address, not just once.
    static unsigned s_miss_addr[8];
    static unsigned s_miss_n = 0;
    bool known = false;
    for (unsigned k = 0; k < s_miss_n; ++k) {
        if (s_miss_addr[k] == addr) { known = true; break; }
    }
    if (!known && s_miss_n < 8) {
        s_miss_addr[s_miss_n++] = addr;
        g3x_log("frame: tex miss tmu=%u addr=0x%08x (no download; draws skipped; total %u)",
                tmu, addr, s_tex_miss_total);
    } else if (s_tex_miss_total == 100 || s_tex_miss_total == 1000 || s_tex_miss_total == 10000) {
        g3x_log("frame: tex miss total=%u (8 distinct logged above)", s_tex_miss_total);
    } else if (!known) {
        // distinct beyond 8: keep the count honest without spamming
        if (s_miss_n == 8 && s_tex_miss_total == 9) {
            g3x_log("frame: tex miss — more than 8 distinct addresses follow (total %u)", s_tex_miss_total);
        }
    }
}

unsigned g3x_frame_tex_miss_count() {
    return s_tex_miss_total;
}

void g3x_frame_draw_stats(unsigned out[9]) {
    out[0] = s_n_vac; out[1] = s_n_parr; out[2] = s_n_parr_strip;
    out[3] = s_n_parr_fan; out[4] = s_n_skip_notex;
    out[5] = s_n_space256; out[6] = s_n_rgb565; out[7] = s_n_fill;
    out[8] = s_n_vc0;
}

unsigned g3x_frame_chroma_toggles() {
    return s_chroma_toggles;
}

void g3x_frame_tex_table_stats(unsigned* used, unsigned* dropped) {
    if (used)    *used = s_tex_used_hwm;
    if (dropped) *dropped = s_tex_dropped_total;
}

void g3x_frame_draw_fan(unsigned count, const void* base, unsigned stride) {
    if (!s_px || !base || count < 3 || stride < 24) return;
    if (s_cur < 0 && !s_color_fill) { ++s_n_skip_notex; return; }
    ++s_n_vac;
    chroma_sync();
    RVert verts[64];
    if (count > 64) count = 64;
    for (unsigned i = 0; i < count; ++i) {
        read_vert(verts[i], (const uint8_t*)base + (size_t)i * stride);
    }
    if (!fill_rect_fast(count, verts)) {        // P2.4: fades/dim rects
        if (s_cur >= 0) vc0_diag(count, verts, s_tex[s_cur]);   // P2.8 shadows
    } else {
        return;   // P2.8: the fill fast path owns the quad — falling through
                  // would draw it a second time through the triangle loop
    }
    float tsx = 1.0f, tsy = 1.0f;
    if (s_cur >= 0) {
        const Tex& t = s_tex[s_cur];
        if (t.bpp == 2) ++s_n_rgb565;
        const TexSpace ts = tex_space(verts, count, t);
        tsx = ts.sx; tsy = ts.sy;
        if (tsx != 1.0f) ++s_n_space256;
        quad_diag(count, verts, t, tsx);
        gly_diag(count, verts, t, tsx);   // P2.9: font-glyph/UI-bit class
    }
    // Field diagnostic: the first fans' raw vertices (x,y | color | s,t) — the
    // ground truth for the s/t convention, quad geometry AND vertex colors
    // (ground lighting = 0xFFRRGGBB light-table tints; UI = 0xFFFFFFFF white).
    static unsigned s_dumped = 0;
    if (s_dumped < 8) {
        g3x_log("fan#%u: v0(%.1f,%.1f|c%08x|%.2f,%.2f) v1(%.1f,%.1f|c%08x|%.2f,%.2f) "
                "v2(%.1f,%.1f|c%08x|%.2f,%.2f) v3(%.1f,%.1f|c%08x|%.2f,%.2f)",
                s_dumped + 1,
                verts[0].x, verts[0].y, verts[0].argb, verts[0].u, verts[0].v,
                verts[1].x, verts[1].y, verts[1].argb, verts[1].u, verts[1].v,
                verts[2].x, verts[2].y, verts[2].argb, verts[2].u, verts[2].v,
                verts[3].x, verts[3].y, verts[3].argb, verts[3].u, verts[3].v);
        ++s_dumped;
    }
    for (unsigned i = 1; i + 1 < count; ++i) {
        raster_tri(verts[0], verts[i], verts[i + 1], tsx, tsy);
    }
}

void g3x_frame_draw_pointers(unsigned mode, unsigned count, void* const* pointers) {
    if (!s_px || !pointers || count < 3) return;
    if (s_cur < 0 && !s_color_fill) { ++s_n_skip_notex; return; }
    ++s_n_parr;
    chroma_sync();
    if (count > 64) count = 64;
    RVert verts[64];
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t* p = (const uint8_t*)pointers[i];
        if (!p) return;
        read_vert(verts[i], p);   // pointer-array vertices: fixed 0x1C layout
    }
    if (!fill_rect_fast(count, verts)) {        // P2.4: fades/dim rects
        if (s_cur >= 0) vc0_diag(count, verts, s_tex[s_cur]);   // P2.8 shadows
    } else {
        return;   // P2.8: same double-draw guard as the fan path
    }
    float tsx = 1.0f, tsy = 1.0f;
    if (s_cur >= 0) {
        const Tex& t = s_tex[s_cur];
        if (t.bpp == 2) ++s_n_rgb565;
        const TexSpace ts = tex_space(verts, count, t);
        tsx = ts.sx; tsy = ts.sy;
        if (tsx != 1.0f) ++s_n_space256;
        quad_diag(count, verts, t, tsx);
        gly_diag(count, verts, t, tsx);   // P2.9: font-glyph/UI-bit class
    }
    // Field diagnostic: the pointer-array path (ground tiles / wall strips).
    // First 4 draws log the primitive + first vertex colors so ground lighting
    // values show up in the field log.
    static unsigned s_pdumped = 0;
    if (s_pdumped < 4) {
        g3x_log("parr#%u: mode=%u count=%u c0=%08x c1=%08x c2=%08x c3=%08x",
                s_pdumped + 1, mode, count,
                verts[0].argb, verts[1].argb, verts[2].argb, verts[3].argb);
        ++s_pdumped;
    }
    if (mode == G3X_GR_TRIANGLE_FAN) {
        ++s_n_parr_fan;
        for (unsigned i = 1; i + 1 < count; ++i) {
            raster_tri(verts[0], verts[i], verts[i + 1], tsx, tsy);
        }
    } else if (mode == G3X_GR_TRIANGLE_STRIP) {
        ++s_n_parr_strip;
        // Triangle strip: consecutive triangles alternate winding; normalize
        // to the rasterizer's CCW (y-down) convention per triangle. Shared
        // edges between neighbours then fall on opposite directed edges, so
        // the top-left ownership rule still assigns each edge pixel exactly
        // once (no seams under opaque, no double-blend under additive).
        for (unsigned i = 0; i + 2 < count; ++i) {
            const RVert& a = verts[i];
            const RVert& b = verts[i + 1];
            const RVert& c = verts[i + 2];
            const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (area >= 0.0f) raster_tri(a, b, c, tsx, tsy);
            else              raster_tri(a, c, b, tsx, tsy);
        }
    }
    // other primitives: counted by the caller, not rasterized (as before)
}

// P2.5: grDrawPoint / grDrawLine. The game's only callers (D2Glide
// DrawLine/DrawPoint at Renderer/Glide.cpp:2296+, plus the radial-light dot
// helpers) always set colorCombine(2) (constant fill) + chromakey OFF first
// and pack the color as ScalePaletteValue|alpha, so the source pixel is the
// constant fill color through the CURRENT blend pair (mode 1 = opaque automap
// lines, mode 4 = alpha-scaled ones).
void g3x_frame_draw_point(float x, float y) {
    if (!s_px) return;
    const int px = (int)(x + 0.5f), py = (int)(y + 0.5f);
    if (px < 0 || py < 0 || px >= (int)s_w || py >= (int)s_h) return;
    uint32_t* row = s_px + (size_t)py * s_w;
    row[px] = blend_px(row[px], const_fill_rgb());
}

void g3x_frame_draw_line(float x0, float y0, float x1, float y1) {
    if (!s_px) return;
    int xa = (int)(x0 + 0.5f), ya = (int)(y0 + 0.5f);
    int xb = (int)(x1 + 0.5f), yb = (int)(y1 + 0.5f);
    int dx = xb - xa; if (dx < 0) dx = -dx;
    int dy = yb - ya; if (dy < 0) dy = -dy;
    const int sx = xa < xb ? 1 : -1, sy = ya < yb ? 1 : -1;
    int err = dx - dy;
    const uint32_t src = const_fill_rgb();
    for (;;) {
        if ((unsigned)xa < s_w && (unsigned)ya < s_h) {
            uint32_t* row = s_px + (size_t)ya * s_w;
            row[xa] = blend_px(row[xa], src);
        }
        if (xa == xb && ya == yb) break;
        const int e2 = err * 2;
        if (e2 > -dy) { err -= dy; xa += sx; }
        if (e2 <  dx) { err += dx; ya += sy; }
    }
}

// Gamma-correct (and, when the target differs, nearest-scale) the raster
// buffer into a BGRA8 staging buffer. Shared by the legacy compose path
// (scales to the swapchain extent) and the P3 slang path (native res, 1:1).
//
// 0.4.4: when the target's ratio differs from the source's, this now FITS +
// CENTERS with black bars instead of stretching — 800x600 into a 1920x1080
// window used to distort 4:3 content to 16:9, and the window presets make
// 16:9 client sizes routine. Same geometry the chain's scale=fit uses, so
// the legacy and chain paths agree. The 1:1 fast path is untouched; the bar
// fill is a few hundred KB of writes, noise next to the copy itself. Bars
// are plain zero fill: alpha is don't-care for an opaque present buffer.
static void gamma_scale_copy(uint8_t* dst, unsigned dw, unsigned dh) {
    if (s_gamma_identity && dw == s_w && dh == s_h) {
        std::memcpy(dst, s_px, (size_t)s_w * s_h * 4);
        return;
    }
    // fit + center: cross-multiply to compare ratios without floats
    unsigned rw = dw, rh = dh, ox = 0, oy = 0;
    const uint64_t src_x = (uint64_t)s_w * dh;
    const uint64_t dst_x = (uint64_t)dw * s_h;
    if (src_x != dst_x) {
        if (src_x > dst_x) {                        // source wider -> bars top/bottom
            rh = (unsigned)((uint64_t)dw * s_h / s_w);
            if (!rh) rh = 1;
            oy = (dh - rh) / 2;
        } else {                                    // source taller -> bars left/right
            rw = (unsigned)((uint64_t)dh * s_w / s_h);
            if (!rw) rw = 1;
            ox = (dw - rw) / 2;
        }
        uint32_t* d32 = (uint32_t*)dst;
        for (unsigned y = 0; y < oy; ++y)                     // top band
            std::memset(d32 + (size_t)y * dw, 0, (size_t)dw * 4);
        for (unsigned y = oy + rh; y < dh; ++y)               // bottom band
            std::memset(d32 + (size_t)y * dw, 0, (size_t)dw * 4);
        for (unsigned y = oy; y < oy + rh; ++y) {             // middle band edges
            if (ox) std::memset(d32 + (size_t)y * dw, 0, (size_t)ox * 4);
            if (dw > ox + rw)
                std::memset(d32 + (size_t)y * dw + ox + rw, 0,
                            (size_t)(dw - ox - rw) * 4);
        }
    }
    uint8_t* d = dst + ((size_t)oy * dw + ox) * 4;
    for (unsigned y = 0; y < rh; ++y) {
        const unsigned sy = y * s_h / rh;
        const uint32_t* row = s_px + (size_t)sy * s_w;
        for (unsigned x = 0; x < rw; ++x, d += 4) {
            const unsigned sx = x * s_w / rw;
            const uint32_t p = row[sx];
            d[0] = s_gamma_lut[2][p & 0xFF];          // B
            d[1] = s_gamma_lut[1][(p >> 8) & 0xFF];   // G
            d[2] = s_gamma_lut[0][(p >> 16) & 0xFF];  // R
            d[3] = 0xFF;
        }
        d = dst + ((size_t)(oy + y + 1) * dw + ox) * 4;   // next row start
    }
}

bool g3x_frame_present() {
    unsigned ext_w = 0, ext_h = 0;
    g3x_vk_present_extent(&ext_w, &ext_h);
    if (!ext_w || !ext_h || !s_px) return false;

    // P3: preset-chain path. The chain receives the gamma-corrected NATIVE
    // game resolution (no pre-scaling) so the preset's own scaling algorithms
    // see the true source size — 640x480 in, big-window out is the P3 exit
    // shape. The overlay is drawn at game res and filters along with the
    // frame (WYSIWYG); it still never touches the game's own framebuffer.
    if (g3x_slang_active()) {
        static uint8_t* slang_buf = nullptr;
        static unsigned slang_cap = 0;
        const size_t sneed = (size_t)s_w * s_h * 4;
        if (slang_cap < sneed) {
            delete[] slang_buf;
            slang_buf = new (std::nothrow) uint8_t[sneed];
            if (!slang_buf) { slang_cap = 0; return false; }
            slang_cap = (unsigned)sneed;
        }
        gamma_scale_copy(slang_buf, s_w, s_h);
        g3x_overlay_render(slang_buf, s_w, s_h);
        if (g3x_slang_present(slang_buf, s_w, s_h)) return true;
        g3x_log_once("slang-fell", "preset chain unavailable — unfiltered present this frame");
    }

    static uint8_t* compose = nullptr;
    static unsigned compose_cap = 0;
    const size_t need = (size_t)ext_w * ext_h * 4;
    if (compose_cap < need) {
        delete[] compose;
        compose = new (std::nothrow) uint8_t[need];
        if (!compose) { compose_cap = 0; return false; }
        compose_cap = (unsigned)need;
    }
    // P2.5 fast path lives inside gamma_scale_copy: 1:1 window + identity
    // gamma = straight copy (the dword layout already matches the swapchain's
    // BGRA byte order).
    gamma_scale_copy(compose, ext_w, ext_h);
    // P2.10: version tag — post-gamma, swapchain-resolution, drawn over
    // everything (including the LFB/Bink path); never enters the game's own
    // framebuffer, so grLfbLock reads and fade logic never see it.
    g3x_overlay_render(compose, ext_w, ext_h);
    return g3x_vk_present_pixels(compose, ext_w, ext_h);
}
