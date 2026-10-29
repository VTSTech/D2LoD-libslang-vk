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
#include <cstdlib>
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

static int run_cpu(const routeb::Corpus& C, uint32_t** sink = nullptr) {
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
        if (sink && sink[f]) std::memcpy(sink[f], g3x_frame_pixels(), (size_t)n * 4);
        std::printf("frame %u %016llx\n", f, (unsigned long long)h);
    }
    return 0;
}

// ---- GPU side (built in the CI job / locally with Lavapipe) -----------------

#ifdef ROUTEB_HAVE_GPU
namespace routeb { int run_gpu_impl(const Corpus& C, const char* spv_path, uint32_t** sink = nullptr); }
#endif

// cmp — the divergence scope: runs BOTH backends over the same corpus and
// reports, per frame, the differing-pixel count plus the first few (x, y,
// cpu dword, gpu dword) tuples. The gate (hash diff) says THAT it diverged;
// this says WHERE — which cells (fill / textured / chroma / varying-VC) and
// which dwords, so the offending kernel expression is one grep away.
static int run_cmp(const routeb::Corpus& C, const char* spv_path) {
#ifdef ROUTEB_HAVE_GPU
    static uint32_t* ca[routeb::FRAMES];
    static uint32_t* ga[routeb::FRAMES];
    for (unsigned f = 0; f < routeb::FRAMES; ++f) {
        ca[f] = (uint32_t*)std::malloc((size_t)routeb::W * routeb::H * 4);
        ga[f] = (uint32_t*)std::malloc((size_t)routeb::W * routeb::H * 4);
    }
    run_cpu(C, ca);
    routeb::run_gpu_impl(C, spv_path, ga);
    unsigned rx0 = 0, ry0 = 0, rx1 = routeb::W, ry1 = routeb::H;
    if (const char* rgn = std::getenv("ROUTEB_REGION")) {
        std::sscanf(rgn, "%u,%u,%u,%u", &rx0, &ry0, &rx1, &ry1);
        std::printf("region window [%u,%u)..[%u,%u)\n", rx0, ry0, rx1, ry1);
    }
    for (unsigned f = 0; f < routeb::FRAMES; ++f) {
        const uint32_t* a = ca[f];
        const uint32_t* b = ga[f];
        unsigned nd = 0, first = 0xFFFFFFFFu;
        for (unsigned i = 0; i < routeb::W * routeb::H; ++i) {
            const unsigned x = i % routeb::W, y = i / routeb::W;
            const bool inwin = x >= rx0 && x < rx1 && y >= ry0 && y < ry1;
            if (a[i] != b[i]) { if (first == 0xFFFFFFFFu && inwin) first = i; if (inwin) ++nd; }
        }
        if (rx0 || ry0 || rx1 != routeb::W || ry1 != routeb::H) {
            // region mode: dump every differing pixel in the window
            std::printf("frame %u: %u px differ in window; dump:\n", f, nd);
            unsigned shown = 0;
            for (unsigned y = ry0; y < ry1 && shown < 24; ++y)
                for (unsigned x = rx0; x < rx1 && shown < 24; ++x) {
                    const unsigned i = y * routeb::W + x;
                    if (a[i] != b[i]) {
                        std::printf("  (%u,%u) c%08x/g%08x\n", x, y, a[i], b[i]);
                        ++shown;
                    }
                }
            continue;
        }
        if (!nd) { std::printf("frame %u OK\n", f); continue; }
        std::printf("frame %u: %u px differ, first:", f, nd);
        for (unsigned k = 0, i = first; k < 6 && k < nd; ++k) {
            while (i < routeb::W * routeb::H && a[i] == b[i]) ++i;
            if (i >= routeb::W * routeb::H) break;
            std::printf(" (%u,%u) c%08x/g%08x", i % routeb::W, i / routeb::W, a[i], b[i]);
            ++i;
        }
        std::printf("\n");
    }
    return 0;
#else
    (void)C; (void)spv_path;
    std::fprintf(stderr, "cmp needs ROUTEB_HAVE_GPU\n");
    return 1;
#endif
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s cpu | %s gpu <routeb.comp.spv> | %s cmp <routeb.comp.spv>\n",
                     argv[0], argv[0], argv[0]);
        return 1;
    }
    routeb::Corpus C;
    C.build();

    if (std::strcmp(argv[1], "cpu") == 0) return run_cpu(C);
    if (std::strcmp(argv[1], "cmp") == 0)
        return run_cmp(C, argc > 2 ? argv[2] : "routeb.comp.spv");
#ifdef ROUTEB_HAVE_GPU
    if (std::strcmp(argv[1], "gpu") == 0)
        return routeb::run_gpu_impl(C, argc > 2 ? argv[2] : "routeb.comp.spv");
#endif
    std::fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 1;
}
