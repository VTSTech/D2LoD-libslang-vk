// g3x_fxcap.cpp — Route-B Stage 1 fxparity capture writer (DLL side).
//
// Implements the api half of G3xRouteBSink (g3x_frame.h) and streams each
// call to fxcapture.cap as it happens (spike/fxcap.h format). The raster
// half (primitives/tables) is intentionally left unwired: the file records
// only the public call stream, and a replay re-derives the primitive stream
// through the production dispatcher — the file cannot drift from what the
// CPU actually drew.
//
// Field usage: [glide3xvk] fxcapture=1 (and optionally fxcap_frames=N,
// default 10) in glide3xvk.ini, then play. The log names the file; drop it
// into spike/ as fxparity.cap (plus its hash pin) to arm the CI replay gate.
#include "g3x_fxcap.h"
#include "g3x_frame.h"
#include "g3x_log.h"
#include "../spike/fxcap.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

struct Cap {
    std::FILE* f = nullptr;
    unsigned frames = 0;
    unsigned max_frames = 10;
    unsigned events = 0;
    char path[512] = "";
};

Cap s_cap;

void cap_event(const fxcap::Event& e) {
    if (!s_cap.f) return;
    if (fxcap::save_event(s_cap.f, e)) ++s_cap.events;
}

// ---- api-level handlers (the file's content) -------------------------------

void cap_frame(void*, uint32_t w, uint32_t h) {
    if (!s_cap.f) return;
    if (s_cap.frames >= s_cap.max_frames) { g3x_fxcap_stop(); return; }
    ++s_cap.frames;
    fxcap::Event e; e.tag = fxcap::EV_FRAME;
    fxcap::put(e.words, w); fxcap::put(e.words, h);
    cap_event(e);
}

void cap_clear(void*, uint32_t color) {
    fxcap::Event e; e.tag = fxcap::EV_CLEAR;
    fxcap::put(e.words, color);
    cap_event(e);
}

void cap_palette(void* ud, const uint32_t* pal256) {
    fxcap::Event e; e.tag = fxcap::EV_PALETTE;
    for (unsigned i = 0; i < 256; ++i) fxcap::put(e.words, pal256[i]);
    cap_event(e);
}

void cap_texdown(void*, uint32_t tmu, uint32_t addr, uint32_t lod, int32_t aspect,
                 uint32_t fmt, const void* data, uint32_t bytes,
                 uint32_t w, uint32_t h, uint32_t bpp) {
    fxcap::Event e; e.tag = fxcap::EV_TEXDOWN;
    fxcap::put(e.words, tmu); fxcap::put(e.words, addr);
    fxcap::put(e.words, lod); fxcap::put(e.words, (uint32_t)aspect);
    fxcap::put(e.words, fmt); fxcap::put(e.words, bytes);
    fxcap::put(e.words, w);   fxcap::put(e.words, h); fxcap::put(e.words, bpp);
    const size_t n = bytes;
    e.blob.resize(n);
    if (n) std::memcpy(e.blob.data(), data, n);
    cap_event(e);
}

void cap_texsrc(void*, uint32_t tmu, uint32_t addr) {
    fxcap::Event e; e.tag = fxcap::EV_TEXSRC;
    fxcap::put(e.words, tmu); fxcap::put(e.words, addr);
    cap_event(e);
}

void cap_blend(void*, uint32_t sf, uint32_t df) {
    fxcap::Event e; e.tag = fxcap::EV_BLEND;
    fxcap::put(e.words, sf); fxcap::put(e.words, df);
    cap_event(e);
}

void cap_constant(void*, uint32_t argb) {
    fxcap::Event e; e.tag = fxcap::EV_CONSTANT;
    fxcap::put(e.words, argb);
    cap_event(e);
}

void cap_colorfill(void*, uint32_t flag) {
    fxcap::Event e; e.tag = fxcap::EV_COLORFILL;
    fxcap::put(e.words, flag);
    cap_event(e);
}

void cap_chromakey(void*, uint32_t mode, uint32_t value) {
    fxcap::Event e; e.tag = fxcap::EV_CHROMAKEY;
    fxcap::put(e.words, mode); fxcap::put(e.words, value);
    cap_event(e);
}

// Vertex buffers arrive as count * 20B (x, y, u, v, argb) — the sink's
// packed form. Copy straight into 5-word payloads.
void cap_drawfan(void*, uint32_t count, const void* verts) {
    fxcap::Event e; e.tag = fxcap::EV_DRAWFAN;
    fxcap::put(e.words, count);
    const uint8_t* v = (const uint8_t*)verts;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t w5[5];
        std::memcpy(w5, v + (size_t)i * 20, 20);
        for (unsigned k = 0; k < 5; ++k) fxcap::put(e.words, w5[k]);
    }
    cap_event(e);
}

void cap_drawptrs(void*, uint32_t mode, uint32_t count, const void* verts) {
    fxcap::Event e; e.tag = fxcap::EV_DRAWPTRS;
    fxcap::put(e.words, mode); fxcap::put(e.words, count);
    const uint8_t* v = (const uint8_t*)verts;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t w5[5];
        std::memcpy(w5, v + (size_t)i * 20, 20);
        for (unsigned k = 0; k < 5; ++k) fxcap::put(e.words, w5[k]);
    }
    cap_event(e);
}

void cap_point(void*, float x, float y) {
    fxcap::Event e; e.tag = fxcap::EV_POINT;
    fxcap::putf(e.words, x); fxcap::putf(e.words, y);
    cap_event(e);
}

void cap_line(void*, float x0, float y0, float x1, float y1) {
    fxcap::Event e; e.tag = fxcap::EV_LINE;
    fxcap::putf(e.words, x0); fxcap::putf(e.words, y0);
    fxcap::putf(e.words, x1); fxcap::putf(e.words, y1);
    cap_event(e);
}

G3xRouteBSink make_sink() {
    G3xRouteBSink s{};
    s.ud            = nullptr;
    s.prim          = nullptr;      // raster level: derived on replay
    s.table_pal     = nullptr;
    s.table_chroma  = nullptr;
    s.table_tex     = nullptr;
    s.api_frame     = &cap_frame;
    s.api_clear     = &cap_clear;
    s.api_palette   = &cap_palette;
    s.api_texdown   = &cap_texdown;
    s.api_texsrc    = &cap_texsrc;
    s.api_blend     = &cap_blend;
    s.api_constant  = &cap_constant;
    s.api_colorfill = &cap_colorfill;
    s.api_chromakey = &cap_chromakey;
    s.api_drawfan   = &cap_drawfan;
    s.api_drawptrs  = &cap_drawptrs;
    s.api_point     = &cap_point;
    s.api_line      = &cap_line;
    return s;
}

void cap_path(char* out, unsigned cap) {
#ifdef _WIN32
    char exe[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe, MAX_PATH) > 0) {
        char* slash = strrchr(exe, '\\');
        if (slash) *slash = '\0';
        snprintf(out, cap - 1, "%s\\fxcapture.cap", exe);
        out[cap - 1] = '\0';
        return;
    }
#endif
    snprintf(out, cap - 1, "fxcapture.cap");
    out[cap - 1] = '\0';
}

} // namespace

void g3x_fxcap_start(unsigned max_frames) {
    if (s_cap.f) return;                     // already recording
    Cap c;
    c.max_frames = max_frames ? max_frames : 10;
    cap_path(c.path, sizeof(c.path));
    c.f = std::fopen(c.path, "wb");
    if (!c.f) {
        g3x_log("fxcapture: cannot open %s — capture off", c.path);
        return;
    }
    if (!fxcap::save_begin(c.f)) {
        std::fclose(c.f);
        g3x_log("fxcapture: header write failed — capture off");
        return;
    }
    s_cap = c;
    static const G3xRouteBSink sink = make_sink();
    g3x_frame_routeb_sink(&sink);
    g3x_log("fxcapture: recording %u frames -> %s", s_cap.max_frames, s_cap.path);
}

void g3x_fxcap_stop() {
    if (!s_cap.f) return;
    g3x_frame_routeb_sink(nullptr);
    std::fclose(s_cap.f);
    g3x_log("fxcapture: wrote frames=%u events=%u -> %s",
            s_cap.frames, s_cap.events, s_cap.path);
    s_cap = Cap{};
}
