// d2linux video host - the presentation half of the milestone-0 skeleton.
//
// Why INDEX8 + palette: the game's 2D pipeline is 8-bit palettized and its
// lighting works by palette ramps, so the native host keeps that model: the
// game (M2) writes indices into one surface, the host uploads one converted
// frame per present. This file already exercises that exact path with a
// synthetic pattern, so the M2 DirectDraw shim replaces only the index
// producer, not the presentation plumbing.
//
// SDL3 notes (verified against the vendored 3.4.18 headers):
//   - SDL_CreateSurfacePalette attaches the palette and returns it; the
//     surface keeps a reference, nothing to free separately.
//   - SDL_ConvertSurface(sfc, format) does the palette lookup for us; M2
//     moves the conversion into the texture lock (zero extra allocs) once
//     the real frame producer is in.
//   - SDL_SetRenderLogicalPresentation + LETTERBOX keeps 4:3 on resize.
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "d2_linux_video.h"

namespace d2linux {

namespace {

struct VideoState {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Surface*  staging  = nullptr;  // INDEX8, the future game-frame target
    SDL_Texture*  texture  = nullptr;  // XRGB8888 streaming upload target
    int w = 0;
    int h = 0;
};

VideoState g_video;

int g_presentCount = 0;

// The palette: 128-shade gray ramp (0-127) + hue sweep (128-255). This is the
// carrier the game's light ramps will live in; nothing in the pattern math
// below depends on the specific colors. (The game's own palette replaces it
// through Video_SetPalette once the DirectDraw shim attaches one.)
void BuildPalette(SDL_Palette* palette) {
    SDL_Color colors[256];
    for (int i = 0; i < 128; ++i) {
        colors[i].r = (Uint8)i;
        colors[i].g = (Uint8)i;
        colors[i].b = (Uint8)i;
        colors[i].a = 0xFF;
    }
    for (int i = 128; i < 256; ++i) {
        // Hue sweep around the wheel: three 120-degree phase-shifted waves.
        const double t = (i - 128) * 6.283185307 / 128.0;
        colors[i].r = (Uint8)(128 + 127 * std::sin(t));
        colors[i].g = (Uint8)(128 + 127 * std::sin(t + 2.094395102));
        colors[i].b = (Uint8)(128 + 127 * std::sin(t + 4.188790205));
        colors[i].a = 0xFF;
    }
    if (!SDL_SetPaletteColors(palette, colors, 0, 256)) {
        std::printf("d2linux: SDL_SetPaletteColors failed: %s\n", SDL_GetError());
    }
}

// [d2linux M2] AttachPresentation: the presentation plumbing (renderer +
// INDEX8 staging + palette + streaming texture) for ONE window. Video_Init
// uses it for the M0 skeleton window; Video_Attach uses it for the game's
// window the user32 shim created (D2GFX_CreateWindow's CreateWindowExA).
static bool AttachPresentation(SDL_Window* window, int w, int h) {
    g_video.window   = window;
    g_video.w        = w;
    g_video.h        = h;

    g_video.renderer = SDL_CreateRenderer(window, nullptr);
    if (!g_video.renderer) {
        std::printf("d2linux: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(g_video.renderer, 1);
    // Letterbox keeps the game's 4:3 framing when the window is resized.
    SDL_SetRenderLogicalPresentation(g_video.renderer, w, h,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    g_video.staging = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_INDEX8);
    if (!g_video.staging) {
        std::printf("d2linux: SDL_CreateSurface(INDEX8) failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_Palette* palette = SDL_CreateSurfacePalette(g_video.staging);
    if (!palette) {
        std::printf("d2linux: SDL_CreateSurfacePalette failed: %s\n", SDL_GetError());
        return false;
    }
    BuildPalette(palette);

    g_video.texture = SDL_CreateTexture(g_video.renderer, SDL_PIXELFORMAT_XRGB8888,
                                        SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!g_video.texture) {
        std::printf("d2linux: SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

// [d2linux M2 round 2] the shared present: staging (INDEX8) → streaming
// texture (XRGB8888) → renderer. M2 folds the 8→32-bit conversion into the
// texture lock — one pass over the frame, zero intermediate surfaces (the
// SDL_ConvertSurface round-trip the M0 path used is gone).
static void PresentStaging() {
    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(g_video.texture, nullptr, &pixels, &pitch)) {
        std::printf("d2linux: SDL_LockTexture failed: %s\n", SDL_GetError());
        return;
    }
    const Uint8* idx = (const Uint8*)g_video.staging->pixels;
    const int spitch = g_video.staging->pitch;
    const SDL_Palette* spal = SDL_GetSurfacePalette(g_video.staging);
    const SDL_Color* pal = spal ? spal->colors : nullptr;
    for (int y = 0; y < g_video.h; ++y) {
        const Uint8* row = idx + (size_t)y * spitch;
        Uint32* dst = (Uint32*)((Uint8*)pixels + (size_t)y * pitch);
        for (int x = 0; x < g_video.w; ++x) {
            const SDL_Color& c = pal[row[x]];   // XRGB8888, little-endian
            dst[x] = 0xFF000000u | ((Uint32)c.r << 16) | ((Uint32)c.g << 8) | (Uint32)c.b;
        }
    }
    SDL_UnlockTexture(g_video.texture);
    SDL_RenderTexture(g_video.renderer, g_video.texture, nullptr, nullptr);
    SDL_RenderPresent(g_video.renderer);
    ++g_presentCount;
}

}  // namespace

bool Video_Init(int w, int h, SDL_Window** out_window) {
    SDL_Window* window = SDL_CreateWindow("d2linux (Vanilla+ native Linux host)", w, h,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::printf("d2linux: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    if (!AttachPresentation(window, w, h)) {
        return false;
    }

    *out_window = window;
    return true;
}

// [d2linux M2] the game's window (user32 shim CreateWindowExA) becomes the
// presentation host. The DirectDraw shim's Flip drives Video_Frame from here
// on; until that shim exists nothing attaches, so the M0 window stays the
// only presenter.
bool Video_Attach(SDL_Window* window) {
    if (!window || g_video.renderer) {
        return false;   // already hosting (the M0 window, or double-create)
    }
    int wf = 0, hf = 0;
    SDL_GetWindowSizeInPixels(window, &wf, &hf);
    return AttachPresentation(window, wf, hf);
}

// [d2linux M2] tears the presentation plumbing down on DestroyWindow. The
// window itself is NOT destroyed here — the user32 shim owns its lifecycle.
// Paired with Video_Attach: only the hosting window's plumbing goes; a
// DestroyWindow of an unattached window changes nothing, and NULL tears
// down whatever is hosted (Video_Shutdown's path).
void Video_Detach(SDL_Window* window) {
    if (window && g_video.window != window) {
        return;   // not mine — the M0 window keeps its plumbing
    }
    if (g_video.texture) {
        SDL_DestroyTexture(g_video.texture);
        g_video.texture = nullptr;
    }
    if (g_video.staging) {
        SDL_DestroySurface(g_video.staging);
        g_video.staging = nullptr;
    }
    if (g_video.renderer) {
        SDL_DestroyRenderer(g_video.renderer);
        g_video.renderer = nullptr;
    }
    g_video.window = nullptr;
}

void Video_Frame(int frame) {
    if (!g_video.staging || !g_video.texture) {
        return;
    }

    // M0 pattern producer - the M2 DirectDraw shim puts the game's composed
    // 8-bit frame here instead. (x*y moire + diagonal sweep, animated.)
    Uint8* idx = (Uint8*)g_video.staging->pixels;
    const int stride = g_video.staging->pitch;
    for (int y = 0; y < g_video.h; ++y) {
        Uint8* row = idx + (size_t)y * stride;
        for (int x = 0; x < g_video.w; ++x) {
            row[x] = (Uint8)(((x * y) >> 6) + frame);
        }
    }

    PresentStaging();
}

bool Video_SetPalette(const void* entries256) {
    if (!g_video.staging || !entries256) {
        return false;
    }
    SDL_Palette* palette = SDL_GetSurfacePalette(g_video.staging);
    if (!palette) {
        return false;
    }
    const Uint8* e = (const Uint8*)entries256;  // PALETTEENTRY: r,g,b,peFlags
    SDL_Color colors[256];
    for (int i = 0; i < 256; ++i) {
        colors[i].r = e[i * 4 + 0];
        colors[i].g = e[i * 4 + 1];
        colors[i].b = e[i * 4 + 2];
        colors[i].a = 0xFF;
    }
    return SDL_SetPaletteColors(palette, colors, 0, 256);
}

bool Video_PresentIndexed(const uint8_t* indices, int pitch, int w, int h) {
    if (!g_video.staging || !g_video.texture || !indices || pitch <= 0 ||
        w <= 0 || h <= 0) {
        return false;
    }
    Uint8* dst = (Uint8*)g_video.staging->pixels;
    const int dpitch = g_video.staging->pitch;

    if (w == g_video.w && h == g_video.h) {
        // The exact fit: row copy, both pitches honored.
        const int copy = (w < pitch) ? w : pitch;
        for (int y = 0; y < h; ++y) {
            std::memcpy(dst + (size_t)y * dpitch, indices + (size_t)y * pitch,
                        (size_t)copy);
        }
    } else {
        // Mode smaller than the hosted window (the smoke self-test's shape:
        // the M0 host owns an 800x600 presentation, the DD mode is 640x480):
        // center the frame on black — the fullscreen-exclusive analogue, a
        // centered letterboxed mode. Larger-than-host frames clip.
        std::memset(dst, 0, (size_t)dpitch * g_video.h);
        const int ox = (g_video.w - w) / 2;
        const int oy = (g_video.h - h) / 2;
        int copy = w;
        if (ox + w > g_video.w) copy = g_video.w - ox;
        int rows = h;
        if (oy + h > g_video.h) rows = g_video.h - oy;
        if (copy > 0 && rows > 0) {
            for (int y = 0; y < rows; ++y) {
                std::memcpy(dst + (size_t)(oy + y) * dpitch + ox,
                            indices + (size_t)y * pitch, (size_t)copy);
            }
        }
    }

    PresentStaging();
    return true;
}

int Video_PresentCount() {
    return g_presentCount;
}

void Video_Shutdown() {
    Video_Detach(nullptr);
    if (g_video.window) {
        SDL_DestroyWindow(g_video.window);
        g_video.window = nullptr;
    }
}

}  // namespace d2linux
