#include "g3x_frame.h"
#include "g3x_log.h"
#include "vk_backend.h"

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
// Precomputed post-palette color match (identical rule to the P2.3 per-pixel
// compare, evaluated once per palette/chroma change instead of per pixel).
uint8_t s_chroma_hit[256];
bool    s_chroma_dirty = true;

inline void chroma_sync() {
    if (!s_chroma_dirty) return;
    const uint32_t key = s_chroma_value & 0x00FFFFFFu;
    for (int i = 0; i < 256; ++i)
        s_chroma_hit[i] = (uint8_t)((s_palette[i] & 0x00FFFFFFu) == key);
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
constexpr unsigned kMaxTex = 4096;   // in-game tile churn needs headroom (943+ dl/round-5)
Tex         s_tex[kMaxTex];
unsigned    s_keys[kMaxTex];         // (tmu<<28) | (addr & 0x0FFFFFFF)
int         s_cur = -1;              // bound texture slot (grTexSource)

bool s_warned_tex_full = false;

unsigned tex_key(unsigned tmu, unsigned addr) {
    return (tmu << 28) | (addr & 0x0FFFFFFFu);
}

unsigned tex_bytes_per_pixel(unsigned fmt) {
    // GR_TEXFMT_P_8 (5) and the other 8bpp formats are 1 byte; the rest 2.
    return (fmt == 0x5u || fmt <= 0x3u) ? 1u : 2u;
}

int tex_slot_alloc(unsigned key) {
    for (unsigned i = 0; i < kMaxTex; ++i) {
        if (!s_tex[i].used || s_keys[i] == key) return (int)i;
    }
    return -1;
}

unsigned s_tex_miss_total = 0;       // exit-summary diagnostic (round 6)

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
                      uint32_t fill_src) {
    const bool chroma = s_chroma_on;
    const uint8_t* hit = s_chroma_hit;
    const uint32_t* pmult = s_pal_mult;
    const uint8_t* tdata = t.data;
    const unsigned tw = t.w, th = t.h;
    const float tscale = (float)(1u << t.lod) / 256.0f;
    float cr = vc0[0], cg = vc0[1], cb = vc0[2];

    for (int x = sx; x < ex; ++x) {
        uint32_t src;
        if (!textured) {
            src = fill_src;
        } else if (t.bpp == 1 && tdata) {
            const unsigned tx = (unsigned)(int)(u * tscale + 4096.0f) & (tw - 1);
            const unsigned ty = (unsigned)(int)(v * tscale + 4096.0f) & (th - 1);
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
        } else {
            src = 0xFF000000u;   // non-P_8: P2.1 sampled black, no chroma
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
static void raster_tri(const RVert& A0, const RVert& B0, const RVert& C0) {
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
                  !uniform_vc && !s_color_fill, vc0, dvc, fill_src);
    }
}

// Fast path (P2.4, kept): axis-aligned constant-fill quads (fades, dim rects,
// loading screens — DrawSolidRectEx/DrawSolidRectAlpha) run FULL-SCREEN at
// 800x600; detect the rect shape at the quad level and fill scanlines
// directly. Blending applies per pixel through the hoisted blend class.
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
    const float minx = xa < xb ? xa : xb, maxx = xa > xb ? xa : xb;
    const float miny = ya < yb ? ya : yb, maxy = ya > yb ? ya : yb;
    // pixel centers strictly inside [min,max) — same coverage as the
    // barycentric path's top-left rule for axis-aligned integer rects
    int x0 = (int)ceilf(minx - 0.5f); if (x0 < 0) x0 = 0;
    int y0 = (int)ceilf(miny - 0.5f); if (y0 < 0) y0 = 0;
    int x1 = (int)ceilf(maxx - 0.5f); if (x1 > (int)s_w) x1 = (int)s_w;
    int y1 = (int)ceilf(maxy - 0.5f); if (y1 > (int)s_h) y1 = (int)s_h;
    const uint32_t fill_src = const_fill_rgb();
    for (int y = y0; y < y1; ++y) {
        uint32_t* row = s_px + (size_t)y * s_w;
        for (int x = x0; x < x1; ++x) row[x] = blend_px(row[x], fill_src);
    }
    return true;
}

// P2.4 draw-path counters (surfaced via g3x_frame_draw_stats for the periodic
// frame lines — with per-call tags budget-quiet, these are the only window
// into what a gameplay session actually submits).
static unsigned s_n_vac = 0;          // grDrawVertexArrayContiguous fans
static unsigned s_n_parr = 0;         // grDrawVertexArray pointer-array draws
static unsigned s_n_parr_strip = 0;
static unsigned s_n_parr_fan = 0;
static unsigned s_n_skip_notex = 0;   // draws dropped: no texture bound

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
    s_chroma_mode = mode; s_chroma_value = value;
    s_chroma_on = (mode == 1);
    s_chroma_dirty = true;
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
        static unsigned s_tex_dropped = 0;
        ++s_tex_dropped;
        if (!s_warned_tex_full) {
            s_warned_tex_full = true;
            g3x_log("frame: texture table full (%u) — downloads dropped", kMaxTex);
        } else if (s_tex_dropped % 500 == 0) {
            g3x_log("frame: texture table full — %u downloads dropped so far", s_tex_dropped);
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
    return true;
}

void g3x_frame_tex_source(unsigned tmu, unsigned addr) {
    const unsigned key = tex_key(tmu, addr);
    for (unsigned i = 0; i < kMaxTex; ++i) {
        if (s_tex[i].used && s_keys[i] == key) {
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

void g3x_frame_draw_stats(unsigned out[5]) {
    out[0] = s_n_vac; out[1] = s_n_parr; out[2] = s_n_parr_strip;
    out[3] = s_n_parr_fan; out[4] = s_n_skip_notex;
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
    if (fill_rect_fast(count, verts)) return;   // P2.4: fades/dim rects
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
        raster_tri(verts[0], verts[i], verts[i + 1]);
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
    if (fill_rect_fast(count, verts)) return;   // P2.4: fades/dim rects
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
            raster_tri(verts[0], verts[i], verts[i + 1]);
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
            if (area >= 0.0f) raster_tri(a, b, c);
            else              raster_tri(a, c, b);
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

bool g3x_frame_present() {
    unsigned ext_w = 0, ext_h = 0;
    g3x_vk_present_extent(&ext_w, &ext_h);
    if (!ext_w || !ext_h || !s_px) return false;

    static uint8_t* compose = nullptr;
    static unsigned compose_cap = 0;
    const size_t need = (size_t)ext_w * ext_h * 4;
    if (compose_cap < need) {
        delete[] compose;
        compose = new (std::nothrow) uint8_t[need];
        if (!compose) { compose_cap = 0; return false; }
        compose_cap = (unsigned)need;
    }
    // P2.5 fast path: 1:1 window + identity gamma = straight copy (the dword
    // layout already matches the swapchain's BGRA byte order).
    if (s_gamma_identity && ext_w == s_w && ext_h == s_h) {
        memcpy(compose, s_px, (size_t)s_w * s_h * 4);
        return g3x_vk_present_pixels(compose, ext_w, ext_h);
    }
    uint8_t* dst = compose;
    for (unsigned y = 0; y < ext_h; ++y) {
        const unsigned sy = y * s_h / ext_h;
        const uint32_t* row = s_px + (size_t)sy * s_w;
        for (unsigned x = 0; x < ext_w; ++x, dst += 4) {
            const unsigned sx = x * s_w / ext_w;
            const uint32_t p = row[sx];
            dst[0] = s_gamma_lut[2][p & 0xFF];          // B
            dst[1] = s_gamma_lut[1][(p >> 8) & 0xFF];   // G
            dst[2] = s_gamma_lut[0][(p >> 16) & 0xFF];  // R
            dst[3] = 0xFF;
        }
    }
    return g3x_vk_present_pixels(compose, ext_w, ext_h);
}
