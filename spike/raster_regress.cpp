// raster_regress.cpp — the Route-B Stage 1 parity harness.
//
// One event stream, two backends, one hash per frame:
//   - the corpus (spike/routeb_corpus.h) or an fxparity capture
//     (spike/fxcap.h, recorded by the DLL's fxcapture=1 conf) feeds the
//     PRODUCTION raster (src/g3x_frame.cpp, unmodified public API);
//   - the record sink (g3x_frame.h) mirrors the post-dispatch primitives to
//     the GPU packer (spike/routeb_gpu.cpp) which bins them into 32x32 tiles
//     and runs spike/routeb.comp on a Vulkan device (Lavapipe in CI).
//
// Modes:
//   raster_regress cpu              corpus CPU hashes          (the pin gate)
//   raster_regress gpu <spv>        corpus GPU hashes          (Lavapipe)
//   raster_regress cmp <spv>        both, per-pixel diff scope (triage)
//   raster_regress replay <file>    capture through BOTH, gate semantics
//
// The CPU pin (spike/raster_hashes_stage1.txt) is read by three gates:
//   1. raster refactor gate: re-run after any g3x_frame.cpp/core change —
//      the lines must be byte-identical.
//   2. GPU gate (CI): `raster_regress gpu routeb.comp.spv` on Lavapipe must
//      produce the same lines (bit-exactness proof, ROUTE-B.md §6).
//   3. Field gate: the same pinned hashes the frame `ghash=` lines will
//      compare against in a later stage.
//
// The fake windows.h shim (spike/fakewin) satisfies vk_backend.h's
// <windows.h> include and the present-seam polls; the overlay/slang/shot/
// vk_backend entry points the frame TU links are stubbed below — the
// regression never presents, it only rasterizes and hashes.

#include "g3x_frame.h"
#include "g3x_types.h"
#include "g3x_log.h"

#include "routeb_corpus.h"
#include "routeb_collector.h"
#include "fxcap.h"

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

// ---- the drive: event stream -> production API -------------------------------

// 5-word harness vertex (x, y, u, v, argb) -> the 0x1C game layout.
static void pack28(uint8_t* dst28, const uint32_t* w5) {
    std::memcpy(dst28 + 0,  &w5[0], 4);   // x
    std::memcpy(dst28 + 4,  &w5[1], 4);   // y
    std::memcpy(dst28 + 8,  &w5[4], 4);   // PARGB
    std::memset(dst28 + 12, 0,      4);   // oow (unused by the raster)
    std::memcpy(dst28 + 16, &w5[2], 4);   // s
    std::memcpy(dst28 + 20, &w5[3], 4);   // t
    std::memset(dst28 + 24, 0,      4);   // stride pad
}

static float wordf(const std::vector<uint32_t>& w, size_t i) {
    float f; std::memcpy(&f, &w[i], 4); return f;
}

// Drives the production API in event order. When `hashes` is non-null, one
// FNV-1a hash per frame is written there (hashed just BEFORE the next
// reset — the frame boundary both backends agree on). When `snap` is
// non-null, the same boundary copies the frame's pixels out (cmp mode's
// per-pixel diff feed). Returns frame count.
static unsigned drive(const std::vector<fxcap::Event>& ev, uint64_t* hashes,
                      uint32_t** snap = nullptr) {
    unsigned f = 0;
    bool open = false;
    std::vector<uint8_t> vbuf;
    std::vector<void*> ptrs;

    auto hash_open = [&](void) {
        if (!open) return;
        const unsigned n = g3x_frame_width() * g3x_frame_height();
        if (hashes)
            hashes[f - 1] = fnv1a((const uint32_t*)g3x_frame_pixels(), n);
        if (snap && snap[f - 1])
            std::memcpy(snap[f - 1], g3x_frame_pixels(), (size_t)n * 4);
    };

    for (const fxcap::Event& e : ev) {
        switch (e.tag) {
            case fxcap::EV_FRAME:
                hash_open();
                g3x_frame_reset(e.words[0], e.words[1]);
                ++f; open = true;
                break;
            case fxcap::EV_CLEAR:
                g3x_frame_clear(e.words[0]);
                break;
            case fxcap::EV_PALETTE:
                g3x_frame_set_palette(e.words.data());
                break;
            case fxcap::EV_TEXDOWN: {
                GrTexInfo info;
                info.smallLodLog2    = (GrLOD_t)e.words[2];
                info.largeLodLog2    = (GrLOD_t)e.words[2];
                info.aspectRatioLog2 = (GrAspectRatio_t)(int32_t)e.words[3];
                info.format          = (GrTextureFormat_t)e.words[4];
                // GrTexInfo.data is void* (the game uploads mutable buffers);
                // the replay's copy is const, so it walks through a cast.
                info.data            = (void*)e.blob.data();
                g3x_frame_tex_download(e.words[0], e.words[1], &info);
                break;
            }
            case fxcap::EV_TEXSRC:
                g3x_frame_tex_source(e.words[0], e.words[1]);
                break;
            case fxcap::EV_BLEND:
                g3x_frame_set_blend(e.words[0], e.words[1]);
                break;
            case fxcap::EV_CONSTANT:
                g3x_frame_set_constant(e.words[0]);
                break;
            case fxcap::EV_COLORFILL:
                g3x_frame_set_color_fill(e.words[0] != 0);
                break;
            case fxcap::EV_CHROMAKEY:
                g3x_frame_set_chromakey(e.words[0], e.words[1]);
                break;
            case fxcap::EV_DRAWFAN: {
                const uint32_t count = e.words[0];
                vbuf.resize((size_t)count * 28);
                for (uint32_t k = 0; k < count; ++k)
                    pack28(&vbuf[(size_t)k * 28], &e.words[1 + (size_t)k * 5]);
                g3x_frame_draw_fan(count, vbuf.data(), 28);
                break;
            }
            case fxcap::EV_DRAWPTRS: {
                const uint32_t mode = e.words[0], count = e.words[1];
                vbuf.resize((size_t)count * 28);
                for (uint32_t k = 0; k < count; ++k)
                    pack28(&vbuf[(size_t)k * 28], &e.words[2 + (size_t)k * 5]);
                ptrs.resize(count);
                for (uint32_t k = 0; k < count; ++k) ptrs[k] = &vbuf[(size_t)k * 28];
                g3x_frame_draw_pointers(mode, count, ptrs.data());
                break;
            }
            case fxcap::EV_POINT:
                g3x_frame_draw_point(wordf(e.words, 0), wordf(e.words, 1));
                break;
            case fxcap::EV_LINE:
                g3x_frame_draw_line(wordf(e.words, 0), wordf(e.words, 1),
                                    wordf(e.words, 2), wordf(e.words, 3));
                break;
            default:
                std::fprintf(stderr, "drive: unknown event tag %u\n", e.tag);
                std::exit(2);
        }
    }
    hash_open();
    return f;
}

// ---- GPU side (built in the CI job / locally with Lavapipe) -----------------

#ifdef ROUTEB_HAVE_GPU
namespace routeb {
struct Collector;
int run_gpu_impl(const Collector& C, const char* spv_path,
                 uint64_t* hashes_out, uint32_t** sink);
}
#endif

static unsigned count_frames(const std::vector<fxcap::Event>& ev) {
    unsigned n = 0;
    for (const fxcap::Event& e : ev) if (e.tag == fxcap::EV_FRAME) ++n;
    return n;
}

// cmp / replay — run BOTH backends over one event stream and report per
// frame: differing-pixel count plus the first few (x, y, cpu, gpu) tuples.
// The gate (hash diff) says THAT it diverged; this says WHERE.
static int run_cmp(const std::vector<fxcap::Event>& ev, const char* spv_path,
                   bool gate_semantics) {
    const unsigned nf = count_frames(ev);
    if (!nf) { std::fprintf(stderr, "no frames in the stream\n"); return 2; }
    std::vector<uint64_t> ch(nf), gh(nf);
#ifdef ROUTEB_HAVE_GPU
    routeb::Collector col;
    const G3xRouteBSink sk = col.sink();
    g3x_frame_routeb_sink(&sk);
    std::vector<uint32_t*> ca(nf, nullptr), ga(nf, nullptr);
    // pixel sinks must exist BEFORE the drive: the CPU side snapshots at
    // each frame boundary (the GPU side fills in run_gpu_impl below)
    for (unsigned f = 0; f < nf; ++f) {
        ca[f] = (uint32_t*)std::malloc((size_t)routeb::W * routeb::H * 4 * 2);
        ga[f] = (uint32_t*)std::malloc((size_t)routeb::W * routeb::H * 4 * 2);
    }
    const unsigned nd = drive(ev, ch.data(), ca.data());
    g3x_frame_routeb_sink(nullptr);
    if (nd != nf || nd != col.frames.size()) {
        std::fprintf(stderr, "frame count mismatch: drive %u vs stream %u vs collector %zu\n",
                     nd, nf, col.frames.size());
        return 2;
    }
    routeb::run_gpu_impl(col, spv_path, gh.data(), ga.data());
#else
    (void)spv_path;
    drive(ev, ch.data());
    std::fprintf(stderr, "cmp/replay needs ROUTEB_HAVE_GPU\n");
    return 1;
#endif

    unsigned rx0 = 0, ry0 = 0, rx1 = 0, ry1 = 0;
    const bool region = std::getenv("ROUTEB_REGION") != nullptr;
    if (region) {
        std::sscanf(std::getenv("ROUTEB_REGION"), "%u,%u,%u,%u", &rx0, &ry0, &rx1, &ry1);
        std::printf("region window [%u,%u)..[%u,%u)\n", rx0, ry0, rx1, ry1);
    }
    unsigned bad = 0;
#ifdef ROUTEB_HAVE_GPU
    for (unsigned f = 0; f < nf; ++f) {
        const unsigned w = col.frames[f].w, h = col.frames[f].h;
        const uint32_t* a = ca[f];
        const uint32_t* b = ga[f];
        if (ch[f] == gh[f] && !region) {
            std::printf("frame %u OK %016llx\n", f, (unsigned long long)ch[f]);
            continue;
        }
        ++bad;
        unsigned ndf = 0, first = 0xFFFFFFFFu;
        for (unsigned i = 0; i < w * h; ++i) {
            const unsigned x = i % w, y = i / w;
            const bool inwin = !region || (x >= rx0 && x < rx1 && y >= ry0 && y < ry1);
            if (a[i] != b[i]) { if (first == 0xFFFFFFFFu && inwin) first = i; if (inwin) ++ndf; }
        }
        std::printf("frame %u: cpu %016llx gpu %016llx — %u px differ",
                    f, (unsigned long long)ch[f], (unsigned long long)gh[f], ndf);
        if (first != 0xFFFFFFFFu) {
            std::printf(", first:");
            for (unsigned k = 0, i = first; k < 6 && k < ndf; ++k) {
                while (i < w * h && a[i] == b[i]) ++i;
                if (i >= w * h) break;
                std::printf(" (%u,%u) c%08x/g%08x", i % w, i / w, a[i], b[i]);
                ++i;
            }
        }
        std::printf("\n");
    }
#endif
    if (gate_semantics) {
        if (bad) std::printf("REPLAY: %u/%u frames diverged — kernel vs CPU\n", bad, nf);
        else     std::printf("REPLAY: %u frames, CPU == GPU bit-exact\n", nf);
    }
    return bad ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: %s cpu | %s gpu <spv> | %s cmp <spv> | %s replay <cap> [spv]"
            " | %s cap <out.cap>\n",
            argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }
    const std::string mode = argv[1];

    std::vector<fxcap::Event> ev;
    if (mode == "replay") {
        if (argc < 3) { std::fprintf(stderr, "replay needs a capture file\n"); return 1; }
        if (!fxcap::load(argv[2], ev)) {
            std::fprintf(stderr, "replay: cannot load %s\n", argv[2]);
            return 2;
        }
        std::printf("replay: %zu events from %s\n", ev.size(), argv[2]);
    } else {
        routeb::Corpus C;
        C.build();
        ev = std::move(C.ev);
    }

    if (mode == "cap") {
        // Corpus -> capture file: exercises the exact fxwriter/fxreader path
        // the DLL capture and the replay gate use, and hands you a reference
        // .cap for CI before the first real field capture exists.
        if (argc < 3) { std::fprintf(stderr, "cap needs an output path\n"); return 1; }
        std::FILE* fp = std::fopen(argv[2], "wb");
        if (!fp) { std::fprintf(stderr, "cap: cannot open %s\n", argv[2]); return 2; }
        if (!fxcap::save_begin(fp)) { std::fclose(fp); return 2; }
        for (const fxcap::Event& e : ev)
            if (!fxcap::save_event(fp, e)) { std::fclose(fp); return 2; }
        std::fclose(fp);
        std::printf("cap: wrote %zu events -> %s\n", ev.size(), argv[2]);
        return 0;
    }
    if (mode == "cpu") {
        std::vector<uint64_t> hs(routeb::FRAMES);
        const unsigned nf = drive(ev, hs.data());
        for (unsigned f = 0; f < nf; ++f)
            std::printf("frame %u %016llx\n", f, (unsigned long long)hs[f]);
        return 0;
    }
    if (mode == "gpu") {
#ifdef ROUTEB_HAVE_GPU
        routeb::Collector col;
        const G3xRouteBSink sk = col.sink();
        g3x_frame_routeb_sink(&sk);
        drive(ev, nullptr);
        g3x_frame_routeb_sink(nullptr);
        std::vector<uint64_t> hs(routeb::FRAMES);
        const int nf = routeb::run_gpu_impl(col, argc > 2 ? argv[2] : "routeb.comp.spv",
                                            hs.data(), nullptr);
        for (unsigned f = 0; f < (unsigned)nf; ++f)
            std::printf("frame %u %016llx\n", f, (unsigned long long)hs[f]);
        return 0;
#else
        std::fprintf(stderr, "gpu needs ROUTEB_HAVE_GPU\n");
        return 1;
#endif
    }
    if (mode == "cmp")
        return run_cmp(ev, argc > 2 ? argv[2] : "routeb.comp.spv", false);
    if (mode == "replay")
        return run_cmp(ev, argc > 3 ? argv[3] : "routeb.comp.spv", true);
    std::fprintf(stderr, "unknown mode '%s'\n", mode.c_str());
    return 1;
}
