// routeb_pack.h — the Route-B GPU packer (host side, ROUTE-B.md §4/§7).
//
// Consumes one frame of the record-sink stream (FramePkg + the versioned
// table snapshots in Collector) and produces the exact SSBO record the
// compute kernel (spike/routeb.comp) consumes: per-prim 16-word records,
// vertex pools, tri/rect buffers, the deduped texel/palette/chroma heap and
// the flattened 32x32-tile bins. PURE host code — zero Vulkan — so the
// parity harness (spike/routeb_gpu.cpp) and the DLL's GPU raster path
// (src/g3x_raster_gpu.cpp) pack from ONE implementation and cannot drift.
//
// The packer NEVER re-derives dispatcher decisions: what arrives in the
// sink is what the CPU drew, already strip-normalized / shape-resolved /
// line-expanded. Binning only chooses WHICH workgroup stores a pixel —
// each pixel keeps exactly one writer, in submission order per tile.
//
// Table content (palettes, chroma tables, texels) is versioned by the frame
// module; the packer snapshots each version once per frame into a shared
// heap, so mid-frame rebind storms replay exactly.
#pragma once

#include "routeb_collector.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

namespace routeb {

static inline uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }


// ---- per-frame GPU pack ------------------------------------------------------

struct FrameGPU {
    uint32_t w = 0, h = 0, tiles_x = 0, tiles_y = 0;
    uint32_t ntris = 0, nrects = 0, nprims = 0;
    std::vector<float>    vxyuv;   // 4 floats per vertex
    std::vector<uint32_t> vargb;   // 1 word per vertex
    std::vector<uint32_t> tris;    // uvec4 (va, vb, vc, rec)
    std::vector<uint32_t> rects;   // 2 x uvec4 per rect/point prim
    std::vector<uint32_t> recs;    // 16 words per prim
    std::vector<uint8_t>  heap;    // texel bytes + palettes + chroma tables
    std::vector<uint32_t> bins;    // [NT counts][NT offsets][entries]
};

// min/max ternary chains, exactly raster_tri's forms (not min()/max()).
static inline float tmin3(float a, float b, float c) {
    return a < b ? (a < c ? a : c) : (b < c ? b : c);
}
static inline float tmax3(float a, float b, float c) {
    return a > b ? (a > c ? a : c) : (b > c ? b : c);
}

static bool pack_frame(const FramePkg& fp, const Collector& C, FrameGPU& g) {
    g = FrameGPU{};
    g.w = fp.w; g.h = fp.h;
    g.tiles_x = (fp.w + 31u) / 32u;
    g.tiles_y = (fp.h + 31u) / 32u;
    const uint32_t NT = g.tiles_x * g.tiles_y;
    std::vector<std::vector<uint32_t>> lists(NT);

    std::map<uint32_t, uint32_t> palOff;     // ver -> word offset in heap
    std::map<uint32_t, uint32_t> chromaOff;  // ver -> word offset in heap
    std::map<uint64_t, uint32_t> texOff;     // (slot<<32|ver) -> byte offset
    auto align4 = [&g]() { while (g.heap.size() & 3u) g.heap.push_back(0); };
    auto push_word = [&g](uint32_t v) {
        g.heap.push_back((uint8_t)(v & 0xFFu));
        g.heap.push_back((uint8_t)((v >> 8) & 0xFFu));
        g.heap.push_back((uint8_t)((v >> 16) & 0xFFu));
        g.heap.push_back((uint8_t)((v >> 24) & 0xFFu));
    };

    for (const G3xRouteBPrim& p : fp.prims) {
        const uint32_t rec = (uint32_t)(g.recs.size() / 16);
        g.recs.resize(g.recs.size() + 16, 0);
        uint32_t* R = &g.recs[(size_t)rec * 16];
        R[0] = p.sf; R[1] = p.df; R[2] = p.const_alpha;
        R[3] = p.color_fill; R[4] = p.chroma_on;
        R[10] = (p.constant >> 8) & 0x00FFFFFFu;   // const_fill_rgb
        R[11] = fbits(p.tsx); R[12] = fbits(p.tsy);

        // palette snapshot (word block, shared via dedupe)
        {
            auto it = C.pal.find(p.pal_ver);
            if (it == C.pal.end()) return false;
            auto o = palOff.find(p.pal_ver);
            if (o == palOff.end()) {
                align4();
                o = palOff.emplace(p.pal_ver, (uint32_t)(g.heap.size() / 4)).first;
                for (uint32_t w32 : it->second) push_word(w32);
            }
            R[13] = o->second;
        }
        // chroma snapshot (256 words of 0/1, shared via dedupe)
        {
            auto it = C.chroma.find(p.chroma_ver);
            if (it == C.chroma.end()) return false;
            auto o = chromaOff.find(p.chroma_ver);
            if (o == chromaOff.end()) {
                align4();
                o = chromaOff.emplace(p.chroma_ver, (uint32_t)(g.heap.size() / 4)).first;
                for (uint8_t h : it->second) push_word(h ? 1u : 0u);
            }
            R[14] = o->second;
        }
        const bool has_tex = (p.kind == 0u && p.tex_slot >= 0);
        if (has_tex) {
            const uint64_t key = ((uint64_t)(uint32_t)p.tex_slot << 32) | p.tex_ver;
            auto it = C.tex.find(key);
            if (it == C.tex.end()) return false;
            auto o = texOff.find(key);
            if (o == texOff.end()) {
                align4();
                o = texOff.emplace(key, (uint32_t)g.heap.size()).first;
                g.heap.insert(g.heap.end(), it->second.data.begin(),
                              it->second.data.end());
            }
            R[5] = o->second; R[6] = p.tex_w; R[7] = p.tex_h;
            R[8] = p.tex_lod; R[9] = p.tex_bpp;
        }
        R[15] = p.kind | (has_tex ? (1u << 8) : 0u);

        if (p.kind == 0) {
            // ---- triangle: three verts, one tris entry, raster_tri bbox ----
            // The bin entry carries the TRIS-BUFFER index (not the prim/rec
            // index — rects and points consume rec numbers without consuming
            // tris slots); the rec rides in the tris entry's .w for the kernel.
            const uint32_t ti = (uint32_t)(g.tris.size() / 4);
            const uint32_t va = (uint32_t)g.vargb.size();
            const float xyuv[12] = { p.ax, p.ay, p.au, p.av,
                                     p.bx, p.by, p.bu, p.bv,
                                     p.cx, p.cy, p.cu, p.cv };
            g.vxyuv.insert(g.vxyuv.end(), xyuv, xyuv + 12);
            g.vargb.push_back(p.aargb);
            g.vargb.push_back(p.bargb);
            g.vargb.push_back(p.cargb);
            g.tris.push_back(va); g.tris.push_back(va + 1);
            g.tris.push_back(va + 2); g.tris.push_back(rec);

            const float minx = tmin3(p.ax, p.bx, p.cx);
            const float miny = tmin3(p.ay, p.by, p.cy);
            const float maxx = tmax3(p.ax, p.bx, p.cx);
            const float maxy = tmax3(p.ay, p.by, p.cy);
            int bx0 = (int)minx;     if (bx0 < 0) bx0 = 0;
            int by0 = (int)miny;     if (by0 < 0) by0 = 0;
            int bx1 = (int)maxx + 1; if (bx1 > (int)fp.w) bx1 = (int)fp.w;
            int by1 = (int)maxy + 1; if (by1 > (int)fp.h) by1 = (int)fp.h;
            for (int ty = by0 >> 5; ty <= (by1 - 1) >> 5; ++ty)
                for (int tx = bx0 >> 5; tx <= (bx1 - 1) >> 5; ++tx)
                    lists[(uint32_t)ty * g.tiles_x + (uint32_t)tx].push_back((ti << 2) | 0u);
        } else if (p.kind == 1) {
            // ---- fill rect: one rects entry, fill_rect_exec bounds --------
            const uint32_t ri = (uint32_t)(g.rects.size() / 8);   // rects-buffer index
            g.rects.push_back(fbits(p.ax)); g.rects.push_back(fbits(p.bx));
            g.rects.push_back(fbits(p.ay)); g.rects.push_back(fbits(p.by));
            g.rects.push_back(rec); g.rects.push_back(0);
            g.rects.push_back(0);   g.rects.push_back(0);

            const float minx = p.ax < p.bx ? p.ax : p.bx;
            const float maxx = p.ax > p.bx ? p.ax : p.bx;
            const float miny = p.ay < p.by ? p.ay : p.by;
            const float maxy = p.ay > p.by ? p.ay : p.by;
            int x0 = (int)ceilf(minx - 0.5f); if (x0 < 0) x0 = 0;
            int y0 = (int)ceilf(miny - 0.5f); if (y0 < 0) y0 = 0;
            int x1 = (int)ceilf(maxx - 0.5f); if (x1 > (int)fp.w) x1 = (int)fp.w;
            int y1 = (int)ceilf(maxy - 0.5f); if (y1 > (int)fp.h) y1 = (int)fp.h;
            // tile RANGE of the pixel bounds (the write area is a superset —
            // the kernel re-derives the exact bounds per row anyway)
            for (int ty = y0 >> 5; ty <= (y1 - 1) >> 5; ++ty)
                for (int tx = x0 >> 5; tx <= (x1 - 1) >> 5; ++tx)
                    lists[(uint32_t)ty * g.tiles_x + (uint32_t)tx].push_back((ri << 2) | 1u);
        } else {
            // ---- point pixel: one rects entry, one tile -------------------
            const uint32_t ri = (uint32_t)(g.rects.size() / 8);   // rects-buffer index
            g.rects.push_back(fbits(p.ax)); g.rects.push_back(fbits(p.ay));
            g.rects.push_back(0);           g.rects.push_back(0);
            g.rects.push_back(rec); g.rects.push_back(0);
            g.rects.push_back(0);   g.rects.push_back(0);

            const int px = (int)(p.ax + 0.5f), py = (int)(p.ay + 0.5f);
            if (px >= 0 && py >= 0 && px < (int)fp.w && py < (int)fp.h)
                lists[((uint32_t)py >> 5) * g.tiles_x + (uint32_t)(px >> 5)]
                    .push_back((ri << 2) | 2u);
        }
    }

    g.ntris  = (uint32_t)(g.tris.size() / 4);
    g.nrects = (uint32_t)(g.rects.size() / 8);
    g.nprims = (uint32_t)(g.recs.size() / 16);

    // ---- flatten the bins: [NT counts][NT offsets][entries] ----------------
    g.bins.assign((size_t)2 * NT, 0);
    uint32_t acc = 0;
    for (uint32_t t = 0; t < NT; ++t) {
        g.bins[t] = (uint32_t)lists[t].size();   // the count the kernel reads
        g.bins[NT + t] = acc;
        acc += (uint32_t)lists[t].size();
    }
    g.bins.resize((size_t)2 * NT + acc);
    for (uint32_t t = 0; t < NT; ++t) {
        const uint32_t base = g.bins[NT + t];
        for (uint32_t k = 0; k < lists[t].size(); ++k)
            g.bins[(size_t)2 * NT + base + k] = lists[t][k];
    }

    if (std::getenv("ROUTEB_DEBUG")) {
        uint32_t tot = 0;
        for (uint32_t t = 0; t < NT; ++t) tot += (uint32_t)lists[t].size();
        std::printf("DBG frame %ux%u tiles %ux%u NT=%u prims=%u tris=%u rects=%u"
                    " entries=%u bins_words=%zu heap=%zu\n",
                    g.w, g.h, g.tiles_x, g.tiles_y, NT, g.nprims, g.ntris,
                    g.nrects, tot, g.bins.size(), g.heap.size());
        std::printf("DBG counts[0..7]:");
        for (uint32_t t = 0; t < 8 && t < NT; ++t) std::printf(" %u", g.bins[t]);
        std::printf("\n");
        const uint32_t dbg_tile = (std::getenv("ROUTEB_TILE")
            ? (uint32_t)std::atoi(std::getenv("ROUTEB_TILE")) : 63u);
        std::printf("DBG tile %u: cnt=%u off=%u entries:", dbg_tile,
                    g.bins[dbg_tile], g.bins[NT + dbg_tile]);
        for (uint32_t k = 0; k < g.bins[dbg_tile]; ++k)
            std::printf(" %u", g.bins[(size_t)2 * NT + g.bins[NT + dbg_tile] + k]);
        std::printf("\n");
        for (uint32_t k = 0; k < g.bins[dbg_tile] && k < 2; ++k) {
            const uint32_t e = g.bins[(size_t)2 * NT + g.bins[NT + dbg_tile] + k];
            const uint32_t kind = e & 3u, idx = e >> 2;
            std::printf("DBG entry kind=%u idx=%u rec:", kind, idx);
            if (kind == 0u) {
                for (uint32_t w = 0; w < 4; ++w) std::printf(" %u", g.tris[idx * 4 + w]);
                const uint32_t r0 = g.tris[idx * 4 + 3];
                std::printf("\nDBG   rec16:");
                for (uint32_t w = 0; w < 16; ++w) std::printf(" %u", g.recs[r0 * 16 + w]);
                const uint32_t va = g.tris[idx * 4 + 0];
                std::printf("\nDBG   vA xyuv %.3f %.3f %.3f %.3f argb=%08x",
                    g.vxyuv[va*4+0], g.vxyuv[va*4+1], g.vxyuv[va*4+2], g.vxyuv[va*4+3],
                    g.vargb[va]);
            } else {
                for (uint32_t w = 0; w < 8; ++w) std::printf(" %u", g.rects[idx * 8 + w]);
            }
            std::printf("\n");
        }
    }
    return true;
}

} // namespace routeb
