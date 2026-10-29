// routeb_collector.h — the record-sink consumer shared by the Route-B
// parity harness (spike/raster_regress.cpp) and the GPU packer
// (spike/routeb_gpu.cpp). Collects the post-dispatch primitive stream and
// the versioned table content the frame module emits (g3x_frame.h).
#pragma once

#include "g3x_frame.h"

#include <cstdint>
#include <map>
#include <vector>

namespace routeb {

struct TexSnap { uint32_t w, h, lod, bpp; std::vector<uint8_t> data; };

struct FramePkg {
    uint32_t w = 0, h = 0, clear = 0;
    std::vector<G3xRouteBPrim> prims;
};

struct Collector {
    std::vector<FramePkg> frames;
    std::map<uint32_t, std::vector<uint32_t>> pal;      // ver -> 256 ARGB
    std::map<uint32_t, std::vector<uint8_t>>  chroma;   // ver -> 256 hits
    std::map<uint64_t, TexSnap> tex;                    // (slot<<32|ver) -> snap

    static void SPrim(void* ud, const G3xRouteBPrim* p) {
        ((Collector*)ud)->frames.back().prims.push_back(*p);
    }
    static void SPal(void* ud, uint32_t ver, const uint32_t* pal256) {
        ((Collector*)ud)->pal.emplace(ver,
            std::vector<uint32_t>(pal256, pal256 + 256));
    }
    static void SChroma(void* ud, uint32_t ver, const uint8_t* hit) {
        ((Collector*)ud)->chroma.emplace(ver,
            std::vector<uint8_t>(hit, hit + 256));
    }
    static void STex(void* ud, int32_t slot, uint32_t ver, const void* data,
                     uint32_t bytes, uint32_t w, uint32_t h,
                     uint32_t lod, uint32_t bpp) {
        TexSnap t;
        t.w = w; t.h = h; t.lod = lod; t.bpp = bpp;
        const uint8_t* d = (const uint8_t*)data;
        t.data.assign(d, d + bytes);
        ((Collector*)ud)->tex.emplace(((uint64_t)(uint32_t)slot << 32) | ver,
                                      std::move(t));
    }
    static void SFrame(void* ud, uint32_t w, uint32_t h) {
        Collector* c = (Collector*)ud;
        c->frames.emplace_back();
        c->frames.back().w = w;
        c->frames.back().h = h;
    }
    static void SClear(void* ud, uint32_t color) {
        Collector* c = (Collector*)ud;
        if (!c->frames.empty()) c->frames.back().clear = color;
    }

    G3xRouteBSink sink() {
        G3xRouteBSink s{};
        s.ud           = this;
        s.prim         = &SPrim;
        s.table_pal    = &SPal;
        s.table_chroma = &SChroma;
        s.table_tex    = &STex;
        s.api_frame    = &SFrame;
        s.api_clear    = &SClear;
        return s;    // remaining api handlers: null (drive() owns those paths)
    }
};

} // namespace routeb
