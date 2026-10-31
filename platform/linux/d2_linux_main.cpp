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
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

#include "d2_linux.h"
#include "d2_linux_video.h"
#include "d2_linux_audio.h"

namespace d2linux {

bool ParseArgs(int argc, char** argv, HostOptions* out) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (SDL_strcmp(a, "--smoke") == 0) {
            out->smoke = true;
        } else if (SDL_strcmp(a, "-h") == 0 || SDL_strcmp(a, "--help") == 0) {
            std::printf("usage: d2linux [--smoke]\n"
                        "  --smoke  headless self-test (dummy video/audio, %d frames, exit 0)\n",
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
    // Self-ID line, d2boot.log style: the first line names the build.
    std::printf("d2linux scaffold %s | Vanilla+ native Linux host, milestone 0%s\n",
                D2LINUX_BUILD_ID, opts.smoke ? " (smoke: dummy drivers)" : "");
}

}  // namespace d2linux

int main(int argc, char** argv) {
    d2linux::HostOptions opts;
    if (!d2linux::ParseArgs(argc, argv, &opts)) {
        return 2;
    }
    d2linux::PrintBanner(opts);

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

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::printf("d2linux: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    if (!d2linux::Video_Init(opts.window_w, opts.window_h, &window)) {
        SDL_Quit();
        return 1;
    }
    // Audio is non-fatal for the scaffold: a silent host is still a host.
    if (!d2linux::Audio_Init()) {
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
