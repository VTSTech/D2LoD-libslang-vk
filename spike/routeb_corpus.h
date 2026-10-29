// routeb_corpus.h — the deterministic synthetic scene BOTH parity gates run.
//
// Stage 0 (ROUTE-B.md §6): one corpus, two backends, one hash per frame.
//   - CPU side: spike/raster_regress.cpp drives the g3x_frame public API
//     (the production raster) over this scene and hashes the frame buffer.
//   - GPU side: the same binary packs the same scene into SSBOs and runs
//     spike/routeb.comp (the compute port of the raster) on a Vulkan device
//     (Lavapipe in CI), then hashes.
// Identical hashes = the kernel is bit-exact against the CPU raster.
//
// Scene constraints (Stage 0 is deliberately binless, see ROUTE-B.md §3.2):
// every pixel is written by AT MOST one triangle, so the unordered compute
// writes cannot race. The scene still covers the shared-diagonal ownership
// rule (every quad is a 2-triangle fan/strip — its diagonal is a shared
// edge), all five blend classes against a varied clear color, chroma on/off,
// uniform- vs varying-vertex-color paths, negative-area fans, degenerate
// triangles, frame-edge clipping, texel wrap (the +4096 floor trick), the
// fill_rect fast path and its triangle fall-through, and palette epochs.
//
// grDrawPoint/grDrawLine are NOT in the corpus: Stage 0 keeps them on the
// CPU raster (automap-only callers; no kernel support yet).
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace routeb {

static constexpr unsigned W = 640, H = 480, FRAMES = 12;
static constexpr unsigned COLS = 8, ROWS = 6;        // 80x80 cells

// g3x_types.h primitive modes (kept numeric so the corpus stands alone).
static constexpr unsigned KIND_STRIP = 4u;
static constexpr unsigned KIND_FAN   = 5u;

struct RVert { float x, y, u, v; uint32_t argb; };

struct DrawSpec {
    unsigned kind;            // KIND_FAN / KIND_STRIP
    unsigned count;           // 4 (quads throughout)
    unsigned first;           // first RVert in verts[]
    // state the CPU driver pushes before the draw (in this order):
    uint32_t sf, df;          // GR_BLEND rgb pair
    uint32_t constant;        // grConstantColorValue dword (alpha in bits 7..0)
    uint32_t chroma_mode;     // 0 off, 1 on
    uint32_t chroma_value;    // chroma key (palette RGB)
    uint32_t color_fill;      // colorCombine mode 2
    int      tex_addr;        // texture to bind (tex[].addr), -1 = leave
};

struct TriSpec { uint32_t a, b, c, rec; };   // rec = flat DrawSpec index

struct TexSpec {
    unsigned   addr, lod, w, h, bpp, fmt;
    int        aspect;
    std::vector<uint8_t> bytes;              // base lod first, row-major
};

struct Corpus {
    std::vector<RVert> verts;                // all frames, concatenated
    std::vector<std::vector<DrawSpec>> frame_draws;
    std::vector<std::vector<TriSpec>>  frame_tris;
    std::vector<uint32_t>              clear;        // [frame]
    std::vector<std::array<uint32_t, 256>> palette;  // [frame]
    std::vector<uint32_t>              chroma_key;   // [frame] (mode-1 value)
    std::vector<uint32_t>              hit_index;    // [frame] (which palette
                                                     //  entry the key matches)
    std::vector<TexSpec> tex;
    std::vector<uint8_t> heap;               // concatenated texel bytes
    std::vector<uint32_t> tex_off;           // [tex] byte offset into heap

    // The CPU raster's chroma_sync() rule, host-computed for the GPU side:
    // hit[i] = (palette[i] & 0xFFFFFF) == key, then hit[0] = 1 (P2.9).
    void chroma_table(unsigned f, uint8_t out[256]) const {
        const uint32_t key = chroma_key[f] & 0x00FFFFFFu;
        for (unsigned i = 0; i < 256; ++i)
            out[i] = (uint8_t)((palette[f][i] & 0x00FFFFFFu) == key);
        out[0] = 1;
    }

    void build() {
        uint32_t s = 0x6732766bu;            // 'g3xvk'
        auto nxt = [&s]() { s = s * 1664525u + 1013904223u; return s; };
        auto pick = [&](uint32_t n) { return nxt() % n; };

        // ---- texture pool (downloaded once; the heap never moves) ----------
        // T0 P8 64x64, T1 P8 128x64 (wide aspect), T2 P8 256x256, T3 565 64x64.
        tex.clear(); heap.clear(); tex_off.clear();
        const unsigned lods[4] = { 6, 7, 8, 6 };
        const int      asp[4]  = { 0, 1, 0, 0 };
        const unsigned fmts[4] = { 5, 5, 5, 0xa };      // GR_TEXFMT_P_8 / RGB_565
        for (unsigned t = 0; t < 4; ++t) {
            TexSpec ts;
            ts.addr = 0x01000000u + t * 0x00100000u;
            ts.lod = lods[t]; ts.aspect = asp[t]; ts.fmt = fmts[t];
            if (asp[t] >= 0) { ts.w = 1u << ts.lod; ts.h = 1u << (ts.lod - asp[t]); }
            else             { ts.w = 1u << (ts.lod + (-asp[t])); ts.h = 1u << ts.lod; }
            ts.bpp = (fmts[t] == 5) ? 1u : 2u;
            tex_off.push_back((uint32_t)heap.size());
            const size_t n = (size_t)ts.w * ts.h * ts.bpp;
            ts.bytes.resize(n);
            for (size_t i = 0; i < n; ++i) {
                uint8_t b = (uint8_t)(nxt() & 0xFF);
                if (fmts[t] == 5 && (nxt() & 0xFF) < 38) b = 0;  // ~15% chroma-0
                ts.bytes[i] = b;
            }
            heap.insert(heap.end(), ts.bytes.begin(), ts.bytes.end());
            tex.push_back(ts);
        }

        // ---- frames ---------------------------------------------------------
        verts.clear(); frame_draws.clear(); frame_tris.clear();
        clear.clear(); palette.clear(); chroma_key.clear(); hit_index.clear();
        for (unsigned f = 0; f < FRAMES; ++f) {
            // clear: full-dword LCG (dst side of every blend class)
            clear.push_back(0xFF000000u | (nxt() & 0x00FFFFFFu));

            // palette: pal[0] reserved black (the DC6 rule); regen every 3rd
            // frame so the uniform-VC LUT epoch path is exercised mid-run.
            std::array<uint32_t, 256> pal;
            pal[0] = 0xFF000000u;
            for (unsigned i = 1; i < 256; ++i)
                pal[i] = 0xFF000000u | (nxt() & 0x00FFFFFFu);
            if (f % 3 != 0 && !palette.empty()) pal = palette.back();
            palette.push_back(pal);

            // chroma key: an actual palette entry's RGB (so the hit table is
            // non-trivial) on some frames, a guaranteed-miss on others.
            const uint32_t hi = 1u + pick(255u);
            hit_index.push_back(hi);
            chroma_key.push_back((f % 4 == 2) ? 0x0FFFFFFFu : (pal[hi] & 0x00FFFFFFu));

            frame_draws.push_back({});
            frame_tris.push_back({});

            const unsigned base_vert = (unsigned)verts.size();
            const unsigned base_rec = (unsigned)frame_draws.back().size();

            for (unsigned cy = 0; cy < ROWS; ++cy) {
                for (unsigned cx = 0; cx < COLS; ++cx) {
                    const unsigned type = pick(6u);   // 0..5, see header comment

                    // ---- placement: strictly non-overlapping 80x80 cells ----
                    float x0 = (float)(cx * 80) + 2.0f + (float)pick(3u);
                    float y0 = (float)(cy * 80) + 2.0f + (float)pick(3u);
                    float qw = 64.0f + (float)pick(12u);
                    float qh = 64.0f + (float)pick(12u);
                    if (pick(2u)) x0 += 0.5f;                   // fractional edges
                    if (pick(2u)) y0 += 0.25f;
                    // frame-edge cells only: partial clipping off the frame
                    if (cx == 0     && pick(5u) == 0) x0 -= 30.0f;
                    if (cy == 0     && pick(7u) == 0) y0 -= 30.0f;
                    if (cx == COLS - 1 && pick(11u) == 0) x0 += 30.0f;
                    if (cy == ROWS - 1 && pick(13u) == 0) y0 += 30.0f;
                    const float x1 = x0 + qw, y1 = y0 + qh;

                    // ---- blend state (all five classes rotate) -------------
                    static const uint32_t pairs[5][2] = {
                        { 4, 0 },   // OPAQUE  ONE,ZERO
                        { 4, 4 },   // ADD     ONE,ONE
                        { 1, 5 },   // LERP    SRC_ALPHA, ONE_MINUS_SRC_ALPHA
                        { 0, 2 },   // MUL     ZERO,SRC_COLOR
                        { 2, 4 },   // GEN     SRC_COLOR,ONE (blend_gen path)
                    };
                    const unsigned bp = pick(5u);
                    const uint32_t sf = pairs[bp][0], df = pairs[bp][1];
                    const uint32_t alpha = pick(256u);
                    const uint32_t fillrgb = pick(0x1000000u);
                    const uint32_t constant = (fillrgb << 8) | alpha;

                    // ---- vertices -------------------------------------------
                    const unsigned vi = (unsigned)verts.size();
                    const unsigned tex_i = pick(4u);
                    const TexSpec& ts = tex[tex_i];
                    const float span = (float)(150u + pick(200u));
                    const float u0 = (float)(pick(512u)) - 128.0f;   // wraps
                    const float v0 = (float)(pick(512u)) - 128.0f;
                    const uint32_t vc_u = 0xFF000000u | (nxt() & 0x00FFFFFFu);
                    uint32_t vc[4];
                    if (type == 1) {                    // varying-VC class
                        for (unsigned k = 0; k < 4; ++k)
                            vc[k] = 0xFF000000u | (nxt() & 0x00FFFFFFu);
                    } else {
                        for (unsigned k = 0; k < 4; ++k) vc[k] = vc_u;
                    }
                    const bool flip = (pick(4u) == 0);  // negative-area fans

                    RVert q[4];
                    q[0] = { x0, y0, u0,           v0,           vc[0] };
                    q[1] = { x1, y0, u0 + span,    v0,           vc[1] };
                    q[2] = { x1, y1, u0 + span,    v0 + span,    vc[2] };
                    q[3] = { x0, y1, u0,           v0 + span,    vc[3] };
                    if (type == 4) {                    // axis-aligned fill quad:
                        for (RVert& r : q) { r.u = r.v = 0.0f; }   // whole px
                    }
                    if (type == 5 && (pick(2u))) {      // non-axis fill quad:
                        q[1].x += 1.5f;                 // fill_rect declines it,
                        q[1].y += 0.5f;                 // triangles take over
                    }
                    if (flip) { const RVert t = q[1]; q[1] = q[2]; q[2] = t; }
                    if (type == 5 && pick(9u) == 0) {   // degenerate triangle
                        q[2].y = q[1].y = q[0].y;
                    }
                    verts.insert(verts.end(), q, q + 4);

                    // ---- draw + state ----------------------------------------
                    DrawSpec d;
                    d.kind = (pick(2u) ? KIND_FAN : KIND_STRIP);
                    d.count = 4;
                    d.first = vi;
                    d.sf = sf; d.df = df;
                    d.constant = constant;
                    d.color_fill = (type == 4 || type == 5) ? 1u : 0u;
                    if (d.color_fill) {
                        d.chroma_mode = 0u; d.chroma_value = 0u;
                        d.tex_addr = -1;
                    } else {
                        // chroma ON only for the dedicated class (mode 1, the
                        // game's own on/off batching); texel wrap via the
                        // out-of-range u0 above
                        d.chroma_mode = (type == 2) ? 1u : 0u;
                        d.chroma_value = chroma_key.back();
                        d.tex_addr = (int)ts.addr;
                    }
                    frame_draws.back().push_back(d);

                    // ---- triangle expansion (GPU side) ----------------------
                    // Replicates the production expansion rules exactly:
                    //   fan   (g3x_frame_draw_fan):    (v0, vi, vi+1)
                    //   strip (g3x_frame_draw_pointers): per-tri winding
                    //     normalize — area>=0 keeps (a,b,c), else (a,c,b).
                    if (d.kind == KIND_FAN) {
                        for (unsigned i = 1; i + 1 < d.count; ++i)
                            frame_tris.back().push_back({ vi, vi + i, vi + i + 1,
                                                          base_rec + (unsigned)frame_draws.back().size() - 1 });
                    } else {
                        for (unsigned i = 0; i + 2 < d.count; ++i) {
                            const RVert& a = verts[vi + i];
                            const RVert& b = verts[vi + i + 1];
                            const RVert& c = verts[vi + i + 2];
                            const float area = (b.x - a.x) * (c.y - a.y)
                                             - (b.y - a.y) * (c.x - a.x);
                            if (area >= 0.0f)
                                frame_tris.back().push_back({ vi + i, vi + i + 1, vi + i + 2,
                                                              base_rec + (unsigned)frame_draws.back().size() - 1 });
                            else
                                frame_tris.back().push_back({ vi + i, vi + i + 2, vi + i + 1,
                                                              base_rec + (unsigned)frame_draws.back().size() - 1 });
                        }
                    }
                    (void)base_vert; (void)ts;
                }
            }
        }
    }
};

} // namespace routeb
