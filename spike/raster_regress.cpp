// raster_regress.cpp — the Route-B Stage 0 parity harness (CPU side).
//
// Drives the PRODUCTION raster (src/g3x_frame.cpp, unmodified public API)
// over the deterministic routeb corpus and prints one hash per frame:
//
//   g++ -O2 -std=c++17 -I spike/fakewin -I src \
//       spike/raster_regress.cpp src/g3x_frame.cpp -o raster_regress
//   ./raster_regress cpu > hashes.txt
//
// The `cpu` run is the CPU pin. Three gates read it:
//   1. raster refactor gate: re-run after any g3x_frame.cpp change — the
//      lines must be byte-identical (g3x_raster_core.h extraction, Stage 0).
//   2. GPU gate (CI): `raster_regress gpu routeb.comp.spv` on Lavapipe must
//      produce the same lines (bit-exactness proof, ROUTE-B.md §6).
//   3. Field gate: the same pinned hashes the frame `ghash=` lines compare
//      against in a later stage.
//
// The fake windows.h shim (spike/fakewin) satisfies vk_backend.h's
// <windows.h> include and the present-seam polls; the overlay/slang/shot/
// vk_backend entry points the frame TU links are stubbed below — the
// regression never presents, it only rasterizes and hashes.

#include "g3x_frame.h"
#include "g3x_types.h"
#include "g3x_log.h"

#include "routeb_corpus.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---- stubs for the frame TU's out-of-module references (never called) ------

void g3x_log(const char*, ...) {}
void g3x_log_once(const char*, const char*, ...) {}
void g3x_overlay_fps_tick(uint64_t) {}
void g3x_overlay_render(uint8_t*, unsigned, unsigned) {}
void g3x_overlay_toast(const char*) {}
bool g3x_overlay_toggle() { return false; }
void g3x_shot_poll() {}
bool g3x_shot_take_pending() { return false; }
bool g3x_shot_submit(const uint8_t*, unsigned, unsigned, char* name, unsigned) {
    if (name) name[0] = 0;
    return false;
}
bool g3x_slang_active() { return false; }
bool g3x_slang_present(const uint8_t*, unsigned, unsigned) { return false; }
void g3x_slang_conf_panel_poll() {}
void g3x_vk_present_extent(unsigned* w, unsigned* h) { *w = 0; *h = 0; }
bool g3x_vk_present_pixels(const uint8_t*, unsigned, unsigned) { return false; }

// ---- hashing -----------------------------------------------------------------

static uint64_t fnv1a(const uint32_t* px, unsigned n) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned i = 0; i < n; ++i) {
        h ^= (uint64_t)px[i];
        h *= 1099511628211ull;
    }
    return h;
}

// ---- the CPU run ---------------------------------------------------------------

static void pack_vert(uint8_t* dst28, const routeb::RVert& v) {
    std::memcpy(dst28 + 0,  &v.x,    4);   // x
    std::memcpy(dst28 + 4,  &v.y,    4);   // y
    std::memcpy(dst28 + 8,  &v.argb, 4);   // PARGB
    std::memset(dst28 + 12, 0,       4);   // oow (unused by the raster)
    std::memcpy(dst28 + 16, &v.u,    4);   // s
    std::memcpy(dst28 + 20, &v.v,    4);   // t
    std::memset(dst28 + 24, 0,       4);   // stride pad (0x1C layout)
}

static int run_cpu(const routeb::Corpus& C) {
    // textures download once (the heap never moves across frames)
    for (const routeb::TexSpec& ts : C.tex) {
        GrTexInfo info;
        info.smallLodLog2    = (GrLOD_t)ts.lod;
        info.largeLodLog2    = (GrLOD_t)ts.lod;
        info.aspectRatioLog2 = (GrAspectRatio_t)ts.aspect;
        info.format          = (GrTextureFormat_t)ts.fmt;
        info.data            = (void*)ts.bytes.data();
        if (!g3x_frame_tex_download(0, ts.addr, &info)) {
            std::fprintf(stderr, "tex_download failed for addr 0x%08x\n", ts.addr);
            return 2;
        }
    }

    std::vector<uint8_t> vbuf;                  // 0x1C-stride game layout
    void* ptrs[64];

    for (unsigned f = 0; f < routeb::FRAMES; ++f) {
        g3x_frame_reset(routeb::W, routeb::H);
        g3x_frame_clear(C.clear[f]);
        g3x_frame_set_palette(C.palette[f].data());

        for (const routeb::DrawSpec& d : C.frame_draws[f]) {
            g3x_frame_set_blend(d.sf, d.df);
            g3x_frame_set_constant(d.constant);
            g3x_frame_set_color_fill(d.color_fill != 0);
            g3x_frame_set_chromakey(d.chroma_mode, d.chroma_value);
            if (d.tex_addr >= 0) g3x_frame_tex_source(0, (unsigned)d.tex_addr);

            vbuf.resize((size_t)d.count * 28);
            for (unsigned k = 0; k < d.count; ++k)
                pack_vert(&vbuf[(size_t)k * 28], C.verts[d.first + k]);
            for (unsigned k = 0; k < d.count; ++k)
                ptrs[k] = &vbuf[(size_t)k * 28];

            if (d.kind == routeb::KIND_FAN)
                g3x_frame_draw_fan(d.count, vbuf.data(), 28);
            else
                g3x_frame_draw_pointers(routeb::KIND_STRIP, d.count, ptrs);
        }

        const unsigned n = routeb::W * routeb::H;
        const uint64_t h = fnv1a((const uint32_t*)g3x_frame_pixels(), n);
        std::printf("frame %u %016llx\n", f, (unsigned long long)h);
    }
    return 0;
}

// ---- GPU side (built in the CI job / locally with Lavapipe) -----------------

#ifdef ROUTEB_HAVE_GPU
namespace routeb { int run_gpu_impl(const Corpus& C, const char* spv_path); }
#endif

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s cpu | %s gpu <routeb.comp.spv>\n",
                     argv[0], argv[0]);
        return 1;
    }
    routeb::Corpus C;
    C.build();

    if (std::strcmp(argv[1], "cpu") == 0) return run_cpu(C);
#ifdef ROUTEB_HAVE_GPU
    if (std::strcmp(argv[1], "gpu") == 0)
        return routeb::run_gpu_impl(C, argc > 2 ? argv[2] : "routeb.comp.spv");
#endif
    std::fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 1;
}
