// fxcap.h — the fxparity capture format (Route-B Stage 1, ROUTE-B.md §6).
//
// A capture is the game's exact public frame-API call stream, serialized as
// a little-endian u32 word stream. A replay feeds the same calls back into
// the production g3x_frame API in the same order — the CPU side of the
// fxparity gate is therefore THE PRODUCTION RASTER, not a reimplementation.
//
// File layout:
//   u32 MAGIC ('FXCP'), u32 VERSION
//   events..., each: u32 tag, u32 word_count, words..., then (only EV_PALETTE
//   and EV_TEXDOWN) u32 byte_count + byte payload padded to a word boundary.
//
// Per-event payload words (floats travel as IEEE bits):
//   EV_FRAME     w h                                  (g3x_frame_reset)
//   EV_CLEAR     color                                (g3x_frame_clear)
//   EV_PALETTE   256 words (ARGB dwords)              (g3x_frame_set_palette)
//   EV_TEXDOWN   tmu addr lod aspect(fmt i32) fmt bytes w h bpp + texel bytes
//                (g3x_frame_tex_download; bytes = the full mip-chain dump)
//   EV_TEXSRC    tmu addr                             (g3x_frame_tex_source)
//   EV_BLEND     sf df                                (g3x_frame_set_blend)
//   EV_CONSTANT  argb                                 (g3x_frame_set_constant)
//   EV_COLORFILL flag                                (g3x_frame_set_color_fill)
//   EV_CHROMAKEY mode value                          (g3x_frame_set_chromakey)
//   EV_DRAWFAN   count, count*5 verts (x y u v argb)  (g3x_frame_draw_fan)
//   EV_DRAWPTRS  mode count, count*5 verts            (g3x_frame_draw_pointers)
//   EV_POINT     x y (float bits)                     (g3x_frame_draw_point)
//   EV_LINE      x0 y0 x1 y1 (float bits)             (g3x_frame_draw_line)
//
// Draw vertices are stored in the harness's 20-byte used-layout (x, y, u, v,
// argb) regardless of the original stride; the replay repacks to 0x1C.
//
// The raster-level primitive stream (G3xRouteBPrim) is deliberately NOT in
// the file: a replay re-derives it through the production dispatcher via the
// record sink, so the file cannot drift from what the CPU actually drew.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace fxcap {

enum : uint32_t {
    MAGIC   = 0x46584350u,   // "FXCP"
    VERSION = 1,
    EV_FRAME     = 1,
    EV_CLEAR     = 2,
    EV_PALETTE   = 3,
    EV_TEXDOWN   = 4,
    EV_TEXSRC    = 5,
    EV_BLEND     = 6,
    EV_CONSTANT  = 7,
    EV_COLORFILL = 8,
    EV_CHROMAKEY = 9,
    EV_DRAWFAN   = 10,
    EV_DRAWPTRS  = 11,
    EV_POINT     = 12,
    EV_LINE      = 13,
};

struct Event {
    uint32_t tag = 0;
    std::vector<uint32_t> words;   // payload words (floats as IEEE bits)
    std::vector<uint8_t>  blob;    // texel bytes (EV_TEXDOWN only)
};

// ---- serialization helpers -------------------------------------------------

inline void put(std::vector<uint32_t>& w, uint32_t v) { w.push_back(v); }

inline void putf(std::vector<uint32_t>& w, float f) {
    uint32_t u; std::memcpy(&u, &f, 4);
    w.push_back(u);
}

// Byte payload: length word, then ceil(bytes/4) words, zero-padded.
inline void put_blob_words(std::vector<uint32_t>& w, const void* p, size_t bytes) {
    const uint8_t* b = (const uint8_t*)p;
    w.push_back((uint32_t)bytes);
    uint32_t cur = 0; unsigned nb = 0;
    for (size_t i = 0; i < bytes; ++i) {
        cur |= (uint32_t)b[i] << (8 * nb);
        if (++nb == 4) { w.push_back(cur); cur = 0; nb = 0; }
    }
    if (nb) w.push_back(cur);
}

// Serialize one event (tag included) into a word vector.
inline void event_words(const Event& e, std::vector<uint32_t>& out) {
    out.clear();
    out.push_back(e.tag);
    out.push_back((uint32_t)e.words.size());
    out.insert(out.end(), e.words.begin(), e.words.end());
    if (e.tag == EV_TEXDOWN)
        put_blob_words(out, e.blob.data(), e.blob.size());
}

inline bool save_begin(FILE* f) {
    if (!f) return false;
    uint32_t hdr[2] = { MAGIC, VERSION };
    return std::fwrite(hdr, 4, 2, f) == 2;
}

inline bool save_event(FILE* f, const Event& e) {
    std::vector<uint32_t> w;
    event_words(e, w);
    return std::fwrite(w.data(), 4, w.size(), f) == w.size();
}

// ---- parsing ---------------------------------------------------------------

struct Reader {
    const uint32_t* w = nullptr;
    size_t n = 0, i = 0;
    bool ok = true;

    uint32_t u32() {
        if (!ok || i >= n) { ok = false; return 0; }
        return w[i++];
    }
    float f32() { const uint32_t v = u32(); float f; std::memcpy(&f, &v, 4); return f; }
    void words(uint32_t count, std::vector<uint32_t>& out) {
        out.clear();
        for (uint32_t k = 0; k < count && ok; ++k) out.push_back(u32());
    }
    void blob(std::vector<uint8_t>& out) {
        out.clear();
        if (!ok) return;
        const uint32_t bytes = u32();
        out.reserve(bytes);
        const size_t nw = (bytes + 3) / 4;
        for (size_t k = 0; k < nw && ok; ++k) {
            const uint32_t v = u32();
            for (unsigned b = 0; b < 4 && out.size() < bytes; ++b)
                out.push_back((uint8_t)((v >> (8 * b)) & 0xFFu));
        }
    }
};

// Load a whole capture. Returns false (empty vector) on any structural error.
inline bool load(const char* path, std::vector<Event>& ev) {
    ev.clear();
    std::FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;
    std::fseek(fp, 0, SEEK_END);
    const long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz < 8 || (size_t)sz % 4) { std::fclose(fp); return false; }
    std::vector<uint32_t> w((size_t)sz / 4);
    const size_t got = std::fread(w.data(), 4, w.size(), fp);
    std::fclose(fp);
    if (got != w.size() || w[0] != MAGIC || w[1] != VERSION) return false;

    Reader r{ w.data(), w.size(), 2, true };
    std::vector<uint32_t> tmp;
    while (r.ok && r.i < r.n) {
        Event e;
        e.tag = r.u32();
        const uint32_t nw = r.u32();
        if (!r.ok || nw > (1u << 26)) return false;
        r.words(nw, e.words);
        if (e.tag == EV_TEXDOWN) r.blob(e.blob);
        if (!r.ok) return false;
        ev.push_back(std::move(e));
    }
    return r.ok;
}

} // namespace fxcap
