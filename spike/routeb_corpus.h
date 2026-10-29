// routeb_corpus.h — the Route-B Stage 1 corpus: OVERLAPPING primitives and
// mid-frame rebind storms, emitted as the fxparity API event stream
// (spike/fxcap.h) that BOTH parity gates consume (ROUTE-B.md §6).
//
// Stage 0 proved the kernel's math bit-exact on a deliberately single-writer
// scene (no two triangles shared a pixel, so unordered compute writes could
// not race). Stage 1's kernel is TILED: each 32x32 screen tile owns its
// pixels exclusively and walks its bin's primitive list in submission order,
// so overlapping additive/alpha geometry blends exactly like the CPU's
// painter order. The corpus therefore now deliberately draws:
//   - overlapping quads (placement jitter crosses the 128x96 cell borders,
//     and every third cell stacks a second quad on the first — an OPAQUE
//     quad UNDER an ADD quad, the ordering-sensitive pair), which Stage 0's
//     corpus could not express;
//   - rebind storms: palette re-downloads, chromakey toggles and texture
//     RE-DOWNLOADS (same TMU address, NEW texel content — the (slot,version)
//     snapshot path) land mid-frame between draws;
//   - the automap primitives (grDrawPoint / grDrawLine) the Stage 0 corpus
//     skipped, plus tex_source misses (a bind with no download: the draw is
//     skipped on BOTH backends by construction);
// on top of everything Stage 0 already covered: all five blend classes,
// chroma on/off, uniform vs varying vertex color, negative winding,
// degenerate triangles, frame-edge clipping, texel wrap (+4096 floor trick),
// the fill_rect fast path AND its declined (non-axis) fall-through.
//
// Determinism: every "random" choice comes from one fixed-seed LCG, so the
// stream — and both backends' hashes — are reproducible forever.
#pragma once

#include "fxcap.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace routeb {

static constexpr unsigned W = 640, H = 480, FRAMES = 16;

// g3x_types.h primitive modes (kept numeric so the corpus stands alone).
static constexpr unsigned KIND_STRIP = 4u;
static constexpr unsigned KIND_FAN   = 5u;

struct Corpus {
    std::vector<fxcap::Event> ev;

    // ---- event builders ----------------------------------------------------
    void ev2(uint32_t tag, uint32_t a, uint32_t b) {
        fxcap::Event e; e.tag = tag;
        fxcap::put(e.words, a); fxcap::put(e.words, b);
        ev.push_back(std::move(e));
    }
    void ev1(uint32_t tag, uint32_t a) {
        fxcap::Event e; e.tag = tag;
        fxcap::put(e.words, a);
        ev.push_back(std::move(e));
    }
    void ev_palette(const uint32_t* pal) {
        fxcap::Event e; e.tag = fxcap::EV_PALETTE;
        for (unsigned i = 0; i < 256; ++i) fxcap::put(e.words, pal[i]);
        ev.push_back(std::move(e));
    }
    void ev_texdown(uint32_t tmu, uint32_t addr, uint32_t lod, int aspect,
                    uint32_t fmt, const std::vector<uint8_t>& bytes,
                    uint32_t w, uint32_t h, uint32_t bpp) {
        fxcap::Event e; e.tag = fxcap::EV_TEXDOWN;
        fxcap::put(e.words, tmu); fxcap::put(e.words, addr);
        fxcap::put(e.words, lod); fxcap::put(e.words, (uint32_t)aspect);
        fxcap::put(e.words, fmt); fxcap::put(e.words, (uint32_t)bytes.size());
        fxcap::put(e.words, w);   fxcap::put(e.words, h);
        fxcap::put(e.words, bpp);
        e.blob = bytes;
        ev.push_back(std::move(e));
    }
    void ev_drawfan(uint32_t count, const uint32_t* v5) {   // v5 = count*5 words
        fxcap::Event e; e.tag = fxcap::EV_DRAWFAN;
        fxcap::put(e.words, count);
        for (uint32_t i = 0; i < count * 5u; ++i) fxcap::put(e.words, v5[i]);
        ev.push_back(std::move(e));
    }
    void ev_drawptrs(uint32_t mode, uint32_t count, const uint32_t* v5) {
        fxcap::Event e; e.tag = fxcap::EV_DRAWPTRS;
        fxcap::put(e.words, mode); fxcap::put(e.words, count);
        for (uint32_t i = 0; i < count * 5u; ++i) fxcap::put(e.words, v5[i]);
        ev.push_back(std::move(e));
    }
    static void vert5(uint32_t* out, float x, float y, float u, float v, uint32_t argb) {
        std::memcpy(&out[0], &x, 4);
        std::memcpy(&out[1], &y, 4);
        std::memcpy(&out[2], &u, 4);
        std::memcpy(&out[3], &v, 4);
        out[4] = argb;
    }

    // ---- texture pool --------------------------------------------------------
    struct TexDef { uint32_t addr, lod, w, h, bpp, fmt; int aspect; };
    static constexpr unsigned NTEX = 6;
    TexDef tex[NTEX];

    static void tex_dims(unsigned lod, int aspect, uint32_t* w, uint32_t* h) {
        if (aspect >= 0) { *w = 1u << lod; *h = 1u << (lod - (aspect > (int)lod ? (int)lod : aspect)); }
        else             { *w = 1u << (lod + (-aspect > (int)lod ? -(int)lod : aspect)); *h = 1u << lod; }
    }
    static uint32_t tex_bytes(unsigned fmt) { return (fmt == 5u || fmt <= 3u) ? 1u : 2u; }

    void make_texbytes(const TexDef& t, uint32_t seed, std::vector<uint8_t>& out) {
        uint32_t s = seed;
        auto nb = [&s]() { s = s * 1664525u + 1013904223u; return (uint8_t)(s >> 16); };
        out.resize((size_t)t.w * t.h * t.bpp);
        for (size_t i = 0; i < out.size(); ++i) {
            uint8_t b = nb();
            if (t.bpp == 1) b &= 0x0Fu;          // sprite-ish: indices 0..15, 0 = chroma-0
            out[i] = b;
        }
    }

    void build() {
        uint32_t s = 0x73743176u;            // 'st1v' — Stage 1
        auto nxt  = [&s]() { s = s * 1664525u + 1013904223u; return s; };
        auto pick = [&](uint32_t n) { return nxt() % n; };

        // ---- texture pool: P8 64x64, P8 128x64, P8 256x256, 565 64x64,
        //      P8 32x32, 565 64x32 ------------------------------------------------
        const unsigned lods[NTEX] = { 6, 7, 8, 6, 5, 6 };
        const int      asp[NTEX]  = { 0, 1, 0, 0, 0, 1 };
        const unsigned fmts[NTEX] = { 5, 5, 5, 0xa, 5, 0xa };
        for (unsigned t = 0; t < NTEX; ++t) {
            tex[t].addr = 0x01000000u + t * 0x00100000u;
            tex[t].lod = lods[t]; tex[t].aspect = asp[t]; tex[t].fmt = fmts[t];
            tex_dims(lods[t], asp[t], &tex[t].w, &tex[t].h);
            tex[t].bpp = tex_bytes(fmts[t]);
        }

        // palettes: two alternating bodies so mid-frame re-downloads change
        // the LUT meaning; pal[0] stays the reserved DC6 black.
        uint32_t palA[256], palB[256];
        palA[0] = palB[0] = 0xFF000000u;
        for (unsigned i = 1; i < 256; ++i) {
            palA[i] = 0xFF000000u | (nxt() & 0x00FFFFFFu);
            palB[i] = 0xFF000000u | (nxt() & 0x00FFFFFFu);
        }
        // guarantee chroma keys that actually hit: index 7 of each body
        palA[7] = 0xFF013370u; palB[7] = 0xFF703301u;
        uint32_t chromaA = 0x013370u, chromaB = 0x703301u;

        // texture content, two versions for the re-download storm
        std::vector<uint8_t> tbytes[NTEX][2];
        for (unsigned t = 0; t < NTEX; ++t) {
            make_texbytes(tex[t], 0x1000u + t * 7u,     tbytes[t][0]);
            make_texbytes(tex[t], 0x2000u + t * 11u,    tbytes[t][1]);
        }

        for (unsigned f = 0; f < FRAMES; ++f) {
            ev2(fxcap::EV_FRAME, W, H);
            ev1(fxcap::EV_CLEAR, 0xFF000000u | (nxt() & 0x00FFFFFFu));

            const bool storm = (f % 4) == 1;      // rebind-storm frames
            const bool useB  = (f % 3) == 2;      // alternate palette bodies
            ev_palette(useB ? palB : palA);

            // chroma key on for the frame's textured draws on most frames
            const uint32_t key = useB ? chromaB : chromaA;
            ev2(fxcap::EV_CHROMAKEY, (f % 5 == 4) ? 0u : 1u, key);

            // texture downloads: all six at frame 0; the storm re-downloads
            // T0 and T2 with the OTHER content mid-frame (below)
            if (f == 0) {
                for (unsigned t = 0; t < NTEX; ++t)
                    ev_texdown(0, tex[t].addr, tex[t].lod, tex[t].aspect, tex[t].fmt,
                               tbytes[t][0], tex[t].w, tex[t].h, tex[t].bpp);
            }

            // ---- draws ---------------------------------------------------------
            // 6x5 cells of 128x96, jitter up to +-40px so neighbours overlap;
            // every 3rd cell stacks a second quad 6px lower-right (an OPAQUE
            // quad UNDER an ADD quad — the painter-order stress pair).
            uint32_t v5[4 * 5];
            for (unsigned cy = 0; cy < 5; ++cy) {
                for (unsigned cx = 0; cx < 6; ++cx) {
                    float x0 = (float)(cx * 128) + (float)pick(40u);
                    float y0 = (float)(cy * 96) + (float)pick(40u);
                    float qw = 64.0f + (float)pick(48u);
                    float qh = 64.0f + (float)pick(32u);
                    if (pick(2u)) x0 += 0.5f;                  // fractional edges
                    if (pick(2u)) y0 += 0.25f;
                    if (cx == 0     && pick(6u) == 0) x0 -= 28.0f;   // frame clips
                    if (cy == 0     && pick(8u) == 0) y0 -= 28.0f;
                    if (cx == 5     && pick(10u) == 0) x0 += 28.0f;
                    if (cy == 4     && pick(12u) == 0) y0 += 28.0f;
                    const float x1 = x0 + qw, y1 = y0 + qh;

                    const unsigned type = pick(8u);   // see header comment
                    const unsigned tex_i = pick(NTEX);
                    const TexDef& td = tex[tex_i];
                    const unsigned tver = storm && (tex_i == 0 || tex_i == 2) ? 1u : 0u;
                    const bool fillq = (type == 4 || type == 5);

                    // blend classes rotate (all five)
                    static const uint32_t pairs[5][2] = {
                        { 4, 0 },   // OPAQUE
                        { 4, 4 },   // ADD
                        { 1, 5 },   // LERP
                        { 0, 2 },   // MUL
                        { 2, 4 },   // GEN
                    };
                    const unsigned bp = pick(5u);
                    const uint32_t alpha = pick(256u);
                    const uint32_t fillrgb = pick(0x1000000u);
                    const uint32_t constant = (fillrgb << 8) | alpha;

                    const float span = (float)(96u + pick(160u));
                    const float u0 = (float)(pick(512u)) - 128.0f;    // wraps
                    const float v0 = (float)(pick(512u)) - 128.0f;
                    const uint32_t vc_u = 0xFF000000u | (nxt() & 0x00FFFFFFu);
                    uint32_t vc[4];
                    if (type == 1) {
                        for (unsigned k = 0; k < 4; ++k)
                            vc[k] = 0xFF000000u | (nxt() & 0x00FFFFFFu);
                    } else {
                        for (unsigned k = 0; k < 4; ++k) vc[k] = vc_u;
                    }

                    uint32_t* w5 = v5;
                    vert5(&w5[0],  x0, y0, u0,        v0,        vc[0]);
                    vert5(&w5[5],  x1, y0, u0 + span, v0,        vc[1]);
                    vert5(&w5[10], x1, y1, u0 + span, v0 + span, vc[2]);
                    vert5(&w5[15], x0, y1, u0,        v0 + span, vc[3]);
                    if (fillq) {                       // fill quads: no texels
                        for (unsigned k = 0; k < 20; ++k) w5[k] = 0;
                        vert5(&w5[0],  x0, y0, 0, 0, 0);
                        vert5(&w5[5],  x1, y0, 0, 0, 0);
                        vert5(&w5[10], x1, y1, 0, 0, 0);
                        vert5(&w5[15], x0, y1, 0, 0, 0);
                    }
                    if (type == 5 && pick(2u)) {       // declined fill: non-axis
                        float nx, ny;
                        std::memcpy(&nx, &w5[5], 4); nx += 1.5f;   // v1.x
                        std::memcpy(&ny, &w5[6], 4); ny += 0.5f;   // v1.y
                        std::memcpy(&w5[5], &nx, 4);
                        std::memcpy(&w5[6], &ny, 4);
                    }
                    if (type == 7 && pick(9u) == 0) {  // degenerate: all y equal
                        w5[6]  = w5[1];                // v1.y = v0.y
                        w5[11] = w5[1];                // v2.y = v0.y
                    }

                    // state pushes, in the drive() order
                    ev2(fxcap::EV_BLEND, pairs[bp][0], pairs[bp][1]);
                    ev1(fxcap::EV_CONSTANT, constant);
                    ev1(fxcap::EV_COLORFILL, fillq ? 1u : 0u);
                    if (fillq) {
                        // the game's rect batches run chroma-off (P2.8 log);
                        // T-excluded: keep the frame's own chroma decision
                    } else {
                        // texture: chroma-class cells keep chroma ON with the
                        // frame key; texel wrap via the out-of-range u0
                        if (type == 2) ev2(fxcap::EV_CHROMAKEY, 1u, key);
                    }
                    if (!fillq) {
                        if (pick(24u) == 0) {
                            // the bind miss: an address with no download —
                            // s_cur goes -1, the draw is skipped on both sides
                            ev2(fxcap::EV_TEXSRC, 0, 0x0DEAD000u + pick(4u));
                        } else {
                            ev2(fxcap::EV_TEXSRC, 0, td.addr);
                        }
                    }
                    const unsigned mode = (type == 6) ? KIND_STRIP : KIND_FAN;
                    if (type == 6) {
                        // zig-zag strip order (TL,TR,BL,BR)
                        uint32_t z[20];
                        std::memcpy(z + 0,  &w5[0],  20);
                        std::memcpy(z + 5,  &w5[5],  20);
                        std::memcpy(z + 10, &w5[15], 20);
                        std::memcpy(z + 15, &w5[10], 20);
                        ev_drawptrs(KIND_STRIP, 4, z);
                    } else if (type == 7 && pick(4u) == 0) {
                        // reversed fan (v0,v3,v2,v1) — same two triangles,
                        // opposite winding
                        uint32_t r[20];
                        std::memcpy(r + 0,  &w5[0],  20);
                        std::memcpy(r + 5,  &w5[15], 20);
                        std::memcpy(r + 10, &w5[10], 20);
                        std::memcpy(r + 15, &w5[5],  20);
                        ev_drawfan(4, r);
                    } else {
                        ev_drawfan(4, w5);
                    }

                    if (cx % 3 == 0 && !fillq) {
                        // the layered pair: the SAME spot again under ADD —
                        // two quads share dozens of pixels, order decides
                        ev2(fxcap::EV_BLEND, 4u, 4u);
                        ev1(fxcap::EV_CONSTANT, (pick(0x1000000u) << 8) | pick(256u));
                        ev2(fxcap::EV_TEXSRC, 0, td.addr);
                        uint32_t l5[20];
                        const float ox = 6.0f, oy = 6.0f;
                        vert5(&l5[0],  x0 + ox, y0 + oy, u0,        v0,        vc[0]);
                        vert5(&l5[5],  x1 + ox, y0 + oy, u0 + span, v0,        vc[1]);
                        vert5(&l5[10], x1 + ox, y1 + oy, u0 + span, v0 + span, vc[2]);
                        vert5(&l5[15], x0 + ox, y1 + oy, u0,        v0 + span, vc[3]);
                        ev_drawfan(4, l5);
                    }
                }
            }

            // ---- rebind storm, mid-frame --------------------------------------
            if (storm) {
                ev_palette(useB ? palA : palB);                 // NEW palette body
                ev2(fxcap::EV_CHROMAKEY, 1u, useB ? chromaA : chromaB);
                for (unsigned t = 0; t < NTEX; ++t) {
                    if (t == 0 || t == 2)                       // NEW texel content,
                        ev_texdown(0, tex[t].addr, tex[t].lod, tex[t].aspect, tex[t].fmt,
                                   tbytes[t][1], tex[t].w, tex[t].h, tex[t].bpp);
                }
                for (unsigned cy = 0; cy < 5; ++cy) {           // draws on the NEW state
                    for (unsigned cx = 0; cx < 6; ++cx) {
                        const unsigned tex_i = pick(NTEX);
                        const TexDef& td = tex[tex_i];
                        ev2(fxcap::EV_BLEND, 1u, 5u);           // LERP over everything
                        ev1(fxcap::EV_CONSTANT, (pick(0x1000000u) << 8) | pick(256u));
                        ev1(fxcap::EV_COLORFILL, 0u);
                        ev2(fxcap::EV_TEXSRC, 0, td.addr);
                        const float x0 = (float)(cx * 128 + 32) + (float)pick(24u);
                        const float y0 = (float)(cy * 96 + 24) + (float)pick(24u);
                        const float span = 48.0f + (float)pick(40u);
                        const float u0 = (float)(pick(256u)) - 64.0f;
                        const float v0 = (float)(pick(256u)) - 64.0f;
                        uint32_t s5[20];
                        const uint32_t vcc = 0xFF000000u | (nxt() & 0x00FFFFFFu);
                        vert5(&s5[0],  x0,        y0,        u0,        v0,        vcc);
                        vert5(&s5[5],  x0+span,   y0,        u0 + span, v0,        vcc);
                        vert5(&s5[10], x0+span,   y0+span,   u0 + span, v0 + span, vcc);
                        vert5(&s5[15], x0,        y0+span,   u0,        v0 + span, vcc);
                        ev_drawfan(4, s5);
                    }
                }
            }

            // ---- automap primitives --------------------------------------------
            ev2(fxcap::EV_BLEND, pick(2u) ? 4u : 1u, pick(2u) ? 0u : 5u);
            ev1(fxcap::EV_CONSTANT, (pick(0x1000000u) << 8) | pick(256u));
            ev1(fxcap::EV_COLORFILL, 1u);
            ev2(fxcap::EV_CHROMAKEY, 0u, 0u);
            for (unsigned p = 0; p < 8; ++p) {
                fxcap::Event e; e.tag = fxcap::EV_POINT;
                fxcap::putf(e.words, (float)(pick(W)));
                fxcap::putf(e.words, (float)(pick(H)));
                ev.push_back(std::move(e));
            }
            for (unsigned l = 0; l < 4; ++l) {
                fxcap::Event e; e.tag = fxcap::EV_LINE;
                fxcap::putf(e.words, (float)(pick(W)));
                fxcap::putf(e.words, (float)(pick(H)));
                fxcap::putf(e.words, (float)(pick(W)));
                fxcap::putf(e.words, (float)(pick(H)));
                ev.push_back(std::move(e));
            }
        }
    }
};

} // namespace routeb
