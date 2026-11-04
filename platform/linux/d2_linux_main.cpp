// d2linux - native Linux host, milestone 0 scaffold (branch `linux`).
//
// What this binary IS: the SDL3 host skeleton of the native-Linux port -
// window + renderer + 8-bit palettized presentation + audio device stream,
// with a headless --smoke self-test (SDL dummy drivers, exit code 0) that CI
// runs. What it is NOT: the game. The decompiled game logic is not compiled
// here yet; the port plan, shim inventory and milestone ladder are in
// docs/linux-port.md. The Windows PE line (d2.exe) is untouched and remains
// the shipped artifact on main.
//
// Design anchor (docs/linux-port.md): the game renders 8-bit palettized
// frames and its lighting lives in palette ramps, so the host presents one
// INDEX8 surface through one streaming texture - the DirectDraw shim in M2
// drives exactly this path with the game's own frames.
//
// Failures carry their own evidence (the -m32-gate philosophy at runtime):
// a video-less run prints which backends THIS binary carries, what the
// session offers, and the fix that follows - --list-video prints the same
// report without running.
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

#include "d2_linux.h"
#include "d2_linux_video.h"
#include "d2_linux_audio.h"

namespace d2linux {
namespace {

const char* EnvOr(const char* name, const char* fallback) {
    const char* v = SDL_getenv(name);
    return (v && *v) ? v : fallback;
}

// True when the kernel self-identifies as WSL - the display advice differs
// (Windows 11 WSLg provides the session; plain WSL2 needs an X server on the
// Windows side with DISPLAY set).
bool RunningUnderWSL() {
    FILE* f = std::fopen("/proc/sys/kernel/osrelease", "r");
    if (!f) {
        return false;
    }
    char buf[128] = {0};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] >= 'A' && buf[i] <= 'Z') {
            buf[i] = static_cast<char>(buf[i] + 32);
        }
    }
    return std::strstr(buf, "microsoft") != nullptr;
}

// The compiled-in driver enumeration is the ground truth about what THIS
// binary can do (SDL_video.h/SDL_audio.h bootstrappers; safe before SDL_Init).
// The vendored SDL builds these to match the dev libraries present at
// configure time - a dummy-only list is a build-time fact, not a runtime one.
void PrintCompiledDrivers() {
    std::printf("  video drivers compiled in:");
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
        std::printf(" %s", SDL_GetVideoDriver(i));
    }
    std::printf("\n  audio drivers compiled in:");
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        std::printf(" %s", SDL_GetAudioDriver(i));
    }
    std::printf("\n");
}

bool HasVideoDriver(const char* want) {
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
        const char* d = SDL_GetVideoDriver(i);
        if (d && SDL_strcmp(d, want) == 0) {
            return true;
        }
    }
    return false;
}

void PrintSessionEnv() {
    std::printf("  DISPLAY=%s\n", EnvOr("DISPLAY", "(unset)"));
    std::printf("  WAYLAND_DISPLAY=%s\n", EnvOr("WAYLAND_DISPLAY", "(unset)"));
    std::printf("  XDG_SESSION_TYPE=%s\n", EnvOr("XDG_SESSION_TYPE", "(unset)"));
    std::printf("  SDL_VIDEODRIVER=%s\n", EnvOr("SDL_VIDEODRIVER", "(unset)"));
    if (RunningUnderWSL()) {
        std::printf("  note: WSL kernel detected - Windows 11 WSLg provides the display\n"
                    "    session; plain WSL2 needs an X server on Windows with DISPLAY set\n");
    }
}

}  // namespace

// --list-video: the first move for any display problem. Works without
// SDL_Init (bootstrapper enumeration); exit 0.
void PrintVideoReport() {
    std::printf("d2linux video report:\n");
    PrintCompiledDrivers();
    PrintSessionEnv();
}

// The SDL_Init(SDL_INIT_VIDEO) failure verdict: the binary states its own
// evidence (backends, request, session) and the fix that follows from the
// combination - the runtime twin of the -m32 configure gate's verdict.
void PrintVideoVerdict() {
    std::printf("d2linux: SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
    std::printf("d2linux video diagnosis:\n");
    PrintCompiledDrivers();
    PrintSessionEnv();

    const bool x11 = HasVideoDriver("x11");
    const bool wayland = HasVideoDriver("wayland");
    const bool kmsdrm = HasVideoDriver("kmsdrm");
    const char* display = EnvOr("DISPLAY", "");
    const char* wl_display = EnvOr("WAYLAND_DISPLAY", "");

    if (!x11 && !wayland && !kmsdrm) {
        std::printf(
            "  verdict: no desktop or console video backend is compiled into this\n"
            "    binary - only SDL's dummy/offscreen test drivers - so it cannot\n"
            "    open a window on ANY machine, desktop or not. A CI artifact is\n"
            "    console-only by design (SDL_X11=OFF SDL_WAYLAND=OFF: no :i386\n"
            "    X11/Wayland dev libraries exist); a local build landing here means\n"
            "    SDL configured without the dev libraries. Fix: install the X11/\n"
            "    Wayland dev set (docs/linux-port.md, Build) and reconfigure on a\n"
            "    FRESH build dir (rm -rf), e.g.:\n"
            "      cmake -S . -B build-native -G Ninja -DD2LINUX_ARCH=native\n");
        return;
    }
    if (!x11 && !wayland) {
        std::printf(
            "  verdict: this binary is KMS/DRM-only - it draws on the Linux console\n"
            "    (a real VT), not inside a desktop session. Run it from a TTY (may\n"
            "    need video-group/root for DRM master), or rebuild with the X11/\n"
            "    Wayland dev set for a desktop window.\n");
        return;
    }
    if (*display == '\0' && *wl_display == '\0') {
        std::printf(
            "  verdict: a desktop backend is compiled in, but this shell has no\n"
            "    display session (DISPLAY/WAYLAND_DISPLAY unset). Run from inside\n"
            "    a desktop session; over SSH use 'ssh -X' (or run it on the\n"
            "    machine's console); on a bare TTY the KMS path needs a\n"
            "    kmsdrm-enabled build: SDL_VIDEODRIVER=kmsdrm ./d2linux\n");
        return;
    }
    std::printf(
        "  verdict: both backend and session exist - force the backend the\n"
        "    session actually runs and compare the error:\n"
        "      SDL_VIDEODRIVER=x11 ./d2linux      (X11 / XWayland sessions)\n"
        "      SDL_VIDEODRIVER=wayland ./d2linux  (native Wayland)\n"
        "    On a Wayland-only session without XWayland the x11 attempt fails\n"
        "    while the wayland attempt connects (GNOME also needs libdecor-0\n"
        "    for decorations).\n");
}

bool ParseArgs(int argc, char** argv, HostOptions* out) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (SDL_strcmp(a, "--smoke") == 0) {
            out->smoke = true;
        } else if (SDL_strcmp(a, "--list-video") == 0) {
            out->list_video = true;
        } else if (SDL_strcmp(a, "-h") == 0 || SDL_strcmp(a, "--help") == 0) {
            std::printf("usage: d2linux [--smoke] [--list-video]\n"
                        "  --smoke       headless self-test (dummy video/audio, %d frames, exit 0)\n"
                        "  --list-video  print the video/audio drivers compiled into this binary\n"
                        "                and the display-session environment, then exit\n",
                        out->smoke_frames);
            return false;
        } else {
            std::printf("d2linux: unknown argument '%s' (-h for usage)\n", a);
            return false;
        }
    }
    return true;
}

void PrintBanner(const HostOptions& opts) {
    // Self-ID line, d2boot.log style: the first line names the build AND the
    // milestone shape (D2LINUX_MILESTONE, baked by build.cmake with the
    // computed game-slice TU count). M1 is a link-time milestone - the game
    // TUs join the binary but nothing calls them until M2's boot chain - so
    // the screen cannot show it; the banner does.
    std::printf("d2linux scaffold %s | Vanilla+ native Linux host, %s%s\n",
                D2LINUX_BUILD_ID, D2LINUX_MILESTONE,
                opts.smoke ? " (smoke: dummy drivers)" : "");
}

}  // namespace d2linux

int main(int argc, char** argv) {
    d2linux::HostOptions opts;
    if (!d2linux::ParseArgs(argc, argv, &opts)) {
        return 2;
    }
    d2linux::PrintBanner(opts);

    if (opts.list_video) {
        d2linux::PrintVideoReport();
        return 0;
    }

    if (opts.smoke) {
        // Before SDL_Init: force the dummy backends so the self-test runs
        // headless (CI, containers). SDL reads these as environment variables
        // (SDL_hints.c: SDL_getenv("SDL_VIDEODRIVER")), so set the env vars -
        // a same-named hint alone is not what the driver picker consults.
        SDL_setenv_unsafe("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
        SDL_SetHint("SDL_VIDEODRIVER", "dummy");
        SDL_SetHint("SDL_AUDIODRIVER", "dummy");
    }

    // Video is fatal and self-diagnosing; audio is best-effort - splitting
    // the SDL_Init call means a soundcard-less box fails on video only (if
    // at all), instead of a combined failure with no clue which side died.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        d2linux::PrintVideoVerdict();
        return 1;
    }
    const bool audio_subsystem = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!audio_subsystem) {
        std::printf("d2linux: SDL audio subsystem unavailable (%s) - continuing without\n",
                    SDL_GetError());
    }

    SDL_Window* window = nullptr;
    if (!d2linux::Video_Init(opts.window_w, opts.window_h, &window)) {
        SDL_Quit();
        return 1;
    }
    // Audio is non-fatal for the scaffold: a silent host is still a host.
    if (audio_subsystem && !d2linux::Audio_Init()) {
        std::printf("d2linux: audio unavailable (%s) - continuing without\n", SDL_GetError());
    }

    int frame = 0;
    bool quit = false;
    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                quit = true;
            }
        }
        if (quit) {
            break;
        }
        d2linux::Video_Frame(frame++);
        if (opts.smoke && frame >= opts.smoke_frames) {
            break;
        }
        SDL_Delay(16);  // ~60 fps cap; M2 replaces this with the game's frame pacing
    }

    if (opts.smoke) {
        // The CI-verdict line: tools/check-build-linux.sh greps for this.
        std::printf("d2linux smoke OK (%d frames, dummy video/audio)\n", frame);
    }

    d2linux::Audio_Shutdown();
    d2linux::Video_Shutdown();
    SDL_Quit();
    return 0;
}
