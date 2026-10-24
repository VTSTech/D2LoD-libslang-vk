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
unsigned s_chroma_mode = 0;
unsigned s_chroma_value = 0;
unsigned s_blend_sf = 4, s_blend_df = 0;   // GR_BLEND rgb pair; default ONE,ZERO
uint32_t s_constant = 0xFFu;         // grConstantColorValue, raw as passed
unsigned s_const_alpha = 0xFFu;      // game packs the blend alpha in bits 7..0
bool     s_color_fill = false;       // colorCombine mode 2 = constant fill
uint8_t  s_gamma_lut[3][256];        // per channel; identity until set_gamma

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

inline uint32_t shade_blend(uint32_t dst, uint32_t src) {
    // src channels already multiplied by vertex color by the caller.
    // SRC_ALPHA / ONE_MINUS_SRC_ALPHA read the CONSTANT's alpha byte: the game
    // always pairs these blends with grAlphaCombine(LOCAL_CONSTANT) and packs
    // the alpha in bits 7..0 of grConstantColorValue (grey in bits 15..8).
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

inline uint32_t sample_texel(const Tex& t, float u, float v, bool* skip) {
    *skip = false;
    if (t.bpp != 1 || !t.data || !t.w || !t.h) return 0xFF000000u;
    // The 2^lod coordinate space: both axes scale by 2^lod/256 (see header
    // note). For 256x256 squares this is the identity; for tall 128x256 the
    // game's S stops at 128 and for wide 256x64 its T stops at 64 — texels.
    const float scale = (float)(1u << t.lod) / 256.0f;
    const float fu = u > 0.0f ? u * scale : 0.0f;
    const float fv = v > 0.0f ? v * scale : 0.0f;
    const unsigned tx = (unsigned)fu & (t.w - 1);
    const unsigned ty = (unsigned)fv & (t.h - 1);
    const unsigned idx = t.data[(size_t)ty * t.w + tx];
    if (s_chroma_mode == 1) {
        // Voodoo chroma semantics: the compare happens on the POST-PALETTE
        // COLOR, and D2 parks pure blue (chroma value 0x000000ff as passed) on
        // its transparent palette entry. P2.1's additional raw-index rule
        // (idx == 255) also ate font/text ink texels that legitimately sit at
        // the top of the palette — that erased button and menu text, and
        // hollowed bright cores out of the copyright line. Color rule only.
        if ((s_palette[idx] & 0x00FFFFFFu) == (s_chroma_value & 0x00FFFFFFu)) {
            *skip = true;
            return 0;
        }
    }
    return s_palette[idx];
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

void raster_tri(const RVert& a, const RVert& b, const RVert& c) {
    if (!s_px) return;
    const bool textured = !s_color_fill && s_cur >= 0;
    if (!textured && !s_color_fill) return;
    const Tex* t = textured ? &s_tex[s_cur] : nullptr;
    // colorCombine mode 2: output = grConstantColorValue — gray lives in bits
    // 15..8 (RENDERER_GLIDE_ScalePaletteValue << 8), so replicate it to RGB.
    const unsigned fill_gray = (s_constant >> 8) & 0xFFu;
    const uint32_t fill_src = fill_gray | (fill_gray << 8) | (fill_gray << 16);
    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (area == 0.0f) return;
    const float inv = 1.0f / area;
    const float minx = a.x < b.x ? (a.x < c.x ? a.x : c.x) : (b.x < c.x ? b.x : c.x);
    const float miny = a.y < b.y ? (a.y < c.y ? a.y : c.y) : (b.y < c.y ? b.y : c.y);
    const float maxx = a.x > b.x ? (a.x > c.x ? a.x : c.x) : (b.x > c.x ? b.x : c.x);
    const float maxy = a.y > b.y ? (a.y > c.y ? a.y : c.y) : (b.y > c.y ? b.y : c.y);
    int x0 = (int)minx;     if (x0 < 0) x0 = 0;
    int y0 = (int)miny;     if (y0 < 0) y0 = 0;
    int x1 = (int)maxx + 1; if (x1 > (int)s_w) x1 = (int)s_w;
    int y1 = (int)maxy + 1; if (y1 > (int)s_h) y1 = (int)s_h;
    const bool own0 = owns_edge(a, b);   // wc == 0 -> pixel on edge a->b
    const bool own1 = owns_edge(b, c);   // wa == 0 -> pixel on edge b->c
    const bool own2 = owns_edge(c, a);   // wb == 0 -> pixel on edge c->a

    for (int y = y0; y < y1; ++y) {
        const float py = (float)y + 0.5f;
        uint32_t* row = s_px + (size_t)y * s_w;
        for (int x = x0; x < x1; ++x) {
            const float px = (float)x + 0.5f;
            // barycentrics, each from its own cross so edge zeros are exact:
            //   wa = weight of a (0 on edge b->c), wb = weight of b (0 on c->a),
            //   wc = weight of c (0 on a->b)
            const float wa = ((c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x)) * inv;
            const float wb = ((a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x)) * inv;
            const float wc = ((b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x)) * inv;
            if (wa < 0.0f || wb < 0.0f || wc < 0.0f) continue;
            if ((wa == 0.0f && !own1) || (wb == 0.0f && !own2) || (wc == 0.0f && !own0)) continue;
            const float u = a.u * wa + b.u * wb + c.u * wc;
            const float v = a.v * wa + b.v * wb + c.v * wc;
            uint32_t vc;
            if (a.argb == b.argb && b.argb == c.argb) {
                vc = a.argb;
            } else {
                const unsigned cr = (unsigned)(0.5f + (a.argb & 0xFF) * wa + (b.argb & 0xFF) * wb + (c.argb & 0xFF) * wc);
                const unsigned cg = (unsigned)(0.5f + ((a.argb >> 8) & 0xFF) * wa + ((b.argb >> 8) & 0xFF) * wb + ((c.argb >> 8) & 0xFF) * wc);
                const unsigned cb = (unsigned)(0.5f + ((a.argb >> 16) & 0xFF) * wa + ((b.argb >> 16) & 0xFF) * wb + ((c.argb >> 16) & 0xFF) * wc);
                vc = (cr > 255 ? 255 : cr) | ((cg > 255 ? 255 : cg) << 8) | ((cb > 255 ? 255 : cb) << 16);
            }
            uint32_t src;
            if (s_color_fill) {
                src = fill_src;   // constant fill: vertex color and texture unused
            } else {
                bool skip;
                const uint32_t tex = sample_texel(*t, u, v, &skip);
                if (skip) continue;
                // colorCombine SCALE_OTHER x LOCAL: texel * vertex color
                const unsigned tr = tex & 0xFF, tg = (tex >> 8) & 0xFF, tb = (tex >> 16) & 0xFF;
                const unsigned vr = vc & 0xFF, vg = (vc >> 8) & 0xFF, vb = (vc >> 16) & 0xFF;
                src = ((tr * vr + 255) >> 8) |
                      (((tg * vg + 255) >> 8) << 8) |
                      (((tb * vb + 255) >> 8) << 16);
            }
            row[x] = shade_blend(row[x], src);
        }
    }
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
    const size_t n = (size_t)s_w * s_h;
    for (size_t i = 0; i < n; ++i) s_px[i] = color | 0xFF000000u;
}

uint8_t* g3x_frame_pixels() { return (uint8_t*)s_px; }
unsigned g3x_frame_width()  { return s_w; }
unsigned g3x_frame_height() { return s_h; }
unsigned g3x_frame_stride() { return s_w * 4; }

void g3x_frame_set_palette(const void* entries256) {
    if (entries256) memcpy(s_palette, entries256, sizeof(s_palette));
}

void g3x_frame_set_chromakey(unsigned mode, unsigned value) {
    s_chroma_mode = mode; s_chroma_value = value;
}

void g3x_frame_set_blend(unsigned rgb_sf, unsigned rgb_df) {
    s_blend_sf = rgb_sf; s_blend_df = rgb_df;
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

void g3x_frame_draw_fan(unsigned count, const void* base, unsigned stride) {
    if (!s_px || !base || count < 3 || stride < 24) return;
    if (s_cur < 0 && !s_color_fill) return;
    RVert verts[64];
    if (count > 64) count = 64;
    for (unsigned i = 0; i < count; ++i) {
        read_vert(verts[i], (const uint8_t*)base + (size_t)i * stride);
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
        raster_tri(verts[0], verts[i], verts[i + 1]);
    }
}

void g3x_frame_draw_pointers(unsigned mode, unsigned count, void* const* pointers) {
    if (!s_px || !pointers || count < 3) return;
    if (s_cur < 0 && !s_color_fill) return;
    if (count > 64) count = 64;
    RVert verts[64];
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t* p = (const uint8_t*)pointers[i];
        if (!p) return;
        read_vert(verts[i], p);   // pointer-array vertices: fixed 0x1C layout
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
        for (unsigned i = 1; i + 1 < count; ++i) {
            raster_tri(verts[0], verts[i], verts[i + 1]);
        }
    } else if (mode == G3X_GR_TRIANGLE_STRIP) {
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
