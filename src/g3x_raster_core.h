// g3x_raster_core.h — the Route-B pure raster core (Stage 0, ROUTE-B.md §4).
//
// The P2.x scanline rasterizer's MATH, extracted verbatim from g3x_frame.cpp
// into a host-agnostic header so two backends can share it:
//   - CPU: g3x_frame.cpp instantiates it over its static state (FrameHost)
//     — the production behavior, pinned by spike/raster_regress `cpu` runs.
//   - GPU: spike/routeb.comp is the GLSL rendering of the same expressions
//     (kept line-for-line in lockstep; CI proves it by hash on Lavapipe).
//
// House rules preserved here: zero windows.h, zero Vulkan, pure math over a
// Host contract. Every expression below is byte-for-byte the production
// form — changing an expression here CHANGES FIELD OUTPUT and must re-pin
// the regression hashes (and one day the field `ghash=` lines).
//
// Host contract (the state the math reads; FrameHost implements it against
// the frame module's statics, the GPU record implements it in the kernel):
//   unsigned      width() / height()      frame dims
//   uint32_t*     px()                    the ARGB dword sink (w*h)
//   uint32_t      constant()              grConstantColorValue, raw
//   unsigned      const_alpha()           s_const_alpha (bits 7..0)
//   bool          color_fill()            colorCombine mode 2
//   unsigned      blend_sf()/blend_df()   GR_BLEND rgb pair
//   BlendClass    blend_class()           hoisted class (blend_class_of)
//   bool          chroma_on()             grChromakeyMode == 1
//   const uint8_t*  chroma_hit()          256-entry table (chroma_sync'd)
//   const uint32_t* palette()             256 ARGB dwords as downloaded
//   const uint32_t* pal_mult()            uniform-VC LUT (host-memoized)
//   void          ensure_pal_mult(vc)     build pal_mult for vc if stale
//   bool          has_tex()               a texture is bound
//   TexView       tex_view()              {data,w,h,lod,bpp} of the bind
#pragma once

#include <cmath>
#include <cstdint>

namespace g3xrc {

struct RVert { float x, y, u, v; uint32_t argb; };

struct TexView {
    const uint8_t* data;
    unsigned w, h, lod, bpp;
};

struct TexSpace { float sx, sy; };

// ---- blend classes ----------------------------------------------------------
// The five GR_BLEND pairs D2 actually submits, hoisted out of the inner loops
// (P2.5). Fast-path lane math is PROVEN equal to the original shade_blend:
//   (x*255+255)>>8 == x for all x in 0..255, so ONE/ZERO terms collapse;
//   the LERP/MUL forms keep the exact (x*y+255)>>8 rounding the fxtest pins.
enum BlendClass { BC_OPAQUE, BC_ADD, BC_LERP, BC_MUL, BC_GEN };

inline BlendClass blend_class_of(unsigned sf, unsigned df) {
    if      (sf == 4 && df == 0) return BC_OPAQUE;
    else if (sf == 4 && df == 4) return BC_ADD;
    else if (sf == 1 && df == 5) return BC_LERP;
    else if (sf == 0 && df == 2) return BC_MUL;
    else                         return BC_GEN;
}

// Constant fill source. The game builds grConstantColorValue as
// ScalePaletteValue(v) | alpha = palette_entry << 8 | alpha, and a palette
// entry is 0xFFRRGGBB — so the constant dword is [R:24-31][G:16-23][B:8-15]
// [alpha:0-7]. Shifting right by 8 lands R,G,B exactly in the framebuffer's
// 0xAARRGGBB lanes.
template <class H> inline uint32_t const_fill_rgb(const H& st) {
    return (st.constant() >> 8) & 0x00FFFFFFu;
}

// Generic fallback — the original shade_blend body, verbatim.
template <class H> inline uint32_t blend_gen(const H& st, uint32_t dst, uint32_t src) {
    const unsigned sf = st.blend_sf(), df = st.blend_df();
    const unsigned s[3] = { src & 0xFF, (src >> 8) & 0xFF, (src >> 16) & 0xFF };
    const unsigned d[3] = { dst & 0xFF, (dst >> 8) & 0xFF, (dst >> 16) & 0xFF };
    unsigned out[3];
    for (int i = 0; i < 3; ++i) {
        unsigned sc, dc;
        switch (sf) {
            case 0:  sc = 0; break;                          // ZERO
            case 1:  sc = st.const_alpha(); break;           // SRC_ALPHA
            case 2:  sc = s[i]; break;                       // SRC_COLOR
            case 4:  sc = 255; break;                        // ONE
            case 5:  sc = 255 - st.const_alpha(); break;     // ONE_MINUS_SRC_ALPHA
            default: sc = 255; break;
        }
        switch (df) {
            case 0:  dc = 0; break;                          // ZERO
            case 1:  dc = st.const_alpha(); break;           // SRC_ALPHA
            case 2:  dc = s[i]; break;                       // SRC_COLOR (dst * src)
            case 3:  dc = (dst >> 24) & 0xFF; break;         // DST_ALPHA
            case 4:  dc = 255; break;                        // ONE
            case 5:  dc = 255 - st.const_alpha(); break;
            default: dc = 0; break;
        }
        unsigned v = (s[i] * sc + 255) >> 8;
        v += (d[i] * dc + 255) >> 8;
        out[i] = v > 255 ? 255 : v;
    }
    return out[0] | (out[1] << 8) | (out[2] << 16) | 0xFF000000u;
}

template <class H> inline uint32_t blend_px(const H& st, uint32_t dst, uint32_t src) {
    switch (st.blend_class()) {
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
            const unsigned a = st.const_alpha(), ia = 255u - a;
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
            return blend_gen(st, dst, src);
    }
}

// ---- edges ------------------------------------------------------------------
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
// boundary pixels run this; the interior is covered by the row interval math,
// so tie/ownership behavior stays bit-identical.
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

// P2.7 the UNIFORM coordinate rule: coordinates ALWAYS live in the square mip
// space 2^largeLod x 2^largeLod (256 units = one full square), valid image in
// the [0,w)x[0,h) sub-rectangle — texel = coord * (2^lod / 256) on BOTH axes.
inline TexSpace tex_space_lod(unsigned lod) {
    const float sc = (float)(1u << lod) / 256.0f;
    return { sc, sc };
}

// ---- span fill --------------------------------------------------------------
// Row fill for pixels [sx, ex). Vertex attributes are interpolated along the
// row from the exact values at sx (computed by the boundary fixup); texel
// sampling floor-WRAPS both axes via the +4096 trick: 4096 is a multiple of
// every texture dim (<= 256), and (int)(x + 4096) == floor(x) + 4096 for all
// x > -4096, so truncation equals floor and the mask is unaffected.
//
// The kernel (routeb.comp) mirrors this function expression-for-expression.
template <class H>
inline void fill_span(H& st, uint32_t* row, int sx, int ex,
                      float u, float v, float dudx, float dvdx,
                      bool textured, const TexView& t,
                      bool varying_vc,
                      const float* vc0, const float* dvc,
                      uint32_t fill_src, float tsx, float tsy) {
    const bool chroma = st.chroma_on() && t.bpp == 1;
    const uint8_t* hit = st.chroma_hit();
    const uint32_t* pmult = st.pal_mult();
    const uint32_t* pal = st.palette();
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
                const uint32_t p = pal[idx];
                src = (((p & 0xFFu) * vr + 255u) >> 8)
                    | (((((p >> 8) & 0xFFu) * vg + 255u) >> 8) << 8)
                    | (((((p >> 16) & 0xFFu) * vb + 255u) >> 8) << 16);
            } else {
                src = pmult[idx];
            }
        } else if (t.bpp == 2 && tdata) {
            // GR_TEXFMT_RGB_565 (format 0xa) — the in-game Smacker video quads.
            const unsigned tx = (unsigned)(int)(u * tsx + 4096.0f) & (tw - 1);
            const unsigned ty = (unsigned)(int)(v * tsy + 4096.0f) & (th - 1);
            const uint16_t p5 = (uint16_t)(tdata[2 * ((size_t)ty * tw + tx)] |
                                           (tdata[2 * ((size_t)ty * tw + tx) + 1] << 8));
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
        row[x] = blend_px(st, row[x], src);
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
// Winding: negative-area triangles are NOT skipped — B/C are swapped to
// normalize, which leaves every weight value bit-identical (E and inv both
// negate). Ownership ties however are decided on the ORIGINAL directed edges,
// so the swap inverts all three ownership flags. Strips pre-normalize
// upstream (g3x_frame_draw_pointers) and never swap.
//
// THE kernel port (routeb.comp): one invocation per (triangle, row) runs the
// body of this row loop — identical expressions, identical order — so every
// float result is bit-identical (IEEE single on both sides, precise/no-FMA
// enforced in the GLSL; ROUTE-B.md §2).
template <class H>
inline void raster_tri(H& st, const RVert& A0, const RVert& B0, const RVert& C0,
                       float tsx, float tsy) {
    if (!st.px()) return;
    float area = (B0.x - A0.x) * (C0.y - A0.y) - (B0.y - A0.y) * (C0.x - A0.x);
    if (area == 0.0f) return;
    RVert A = A0, B = B0, C = C0;
    const bool swapped = area < 0.0f;
    if (swapped) { B = C0; C = B0; area = -area; }
    const float inv = 1.0f / area;
    const bool textured = !st.color_fill() && st.has_tex();
    if (!textured && !st.color_fill()) return;

    const TexView t = textured ? st.tex_view() : TexView{};

    float minx = A.x < B.x ? (A.x < C.x ? A.x : C.x) : (B.x < C.x ? B.x : C.x);
    float miny = A.y < B.y ? (A.y < C.y ? A.y : C.y) : (B.y < C.y ? B.y : C.y);
    float maxx = A.x > B.x ? (A.x > C.x ? A.x : C.x) : (B.x > C.x ? B.x : C.x);
    float maxy = A.y > B.y ? (A.y > C.y ? A.y : C.y) : (B.y > C.y ? B.y : C.y);
    int bx0 = (int)minx;     if (bx0 < 0) bx0 = 0;
    int by0 = (int)miny;     if (by0 < 0) by0 = 0;
    int bx1 = (int)maxx + 1; if (bx1 > (int)st.width()) bx1 = (int)st.width();
    int by1 = (int)maxy + 1; if (by1 > (int)st.height()) by1 = (int)st.height();
    if (bx0 >= bx1 || by0 >= by1) return;

    bool own0 = owns_edge(A, B);         // wc == 0 -> pixel on edge A->B
    bool own1 = owns_edge(B, C);         // wa == 0 -> pixel on edge B->C
    bool own2 = owns_edge(C, A);         // wb == 0 -> pixel on edge C->A
    if (swapped) { own0 = !own0; own1 = !own1; own2 = !own2; }

    // constant-fill source (colorCombine mode 2)
    const uint32_t fill_src = const_fill_rgb(st);

    // vertex-color mode: uniform (multiplied-palette LUT) or per-pixel interp
    const bool uniform_vc = (A.argb == B.argb && B.argb == C.argb);
    if (textured && uniform_vc && !st.color_fill()) {
        st.ensure_pal_mult(A.argb & 0x00FFFFFFu);
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

    uint32_t* px = st.px();
    const unsigned w = st.width();
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
        fill_span(st, px + (size_t)y * w, sx, ex, u0, v0, dudx, dvdx,
                  textured, t,
                  !uniform_vc && !st.color_fill(), vc0, dvc, fill_src, tsx, tsy);
    }
}

// ---- fill rect (the P2.4 fast path, split for the host) ---------------------

// Shape test: axis-aligned quad with every vertex on a corner (the
// DrawSolidRectEx/DrawSolidRectAlpha form). Pure geometry; the host keeps the
// stats + diagnostics, the exec below does the pixels.
inline bool fill_rect_shape(const RVert* v, unsigned count,
                            float& xa, float& xb, float& ya, float& yb) {
    if (count != 4) return false;
    xa = v[0].x; xb = v[0].x; ya = v[0].y; yb = v[0].y;
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
    return true;
}

// Pixel fill for the detected rect. Same coverage as the barycentric path's
// top-left rule for axis-aligned integer rects: pixel centers strictly
// inside [min,max).
template <class H>
inline void fill_rect_exec(H& st, float xa, float xb, float ya, float yb) {
    if (!st.px()) return;
    const uint32_t fill_src = const_fill_rgb(st);
    const float minx = xa < xb ? xa : xb, maxx = xa > xb ? xa : xb;
    const float miny = ya < yb ? ya : yb, maxy = ya > yb ? ya : yb;
    const unsigned w = st.width(), h = st.height();
    int x0 = (int)ceilf(minx - 0.5f); if (x0 < 0) x0 = 0;
    int y0 = (int)ceilf(miny - 0.5f); if (y0 < 0) y0 = 0;
    int x1 = (int)ceilf(maxx - 0.5f); if (x1 > (int)w) x1 = (int)w;
    int y1 = (int)ceilf(maxy - 0.5f); if (y1 > (int)h) y1 = (int)h;
    for (int y = y0; y < y1; ++y) {
        uint32_t* row = st.px() + (size_t)y * w;
        for (int x = x0; x < x1; ++x) row[x] = blend_px(st, row[x], fill_src);
    }
}

} // namespace g3xrc
