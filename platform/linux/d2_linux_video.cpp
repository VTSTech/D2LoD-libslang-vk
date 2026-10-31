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

// Palette: 128-shade gray ramp (0-127) + hue sweep (128-255). This is the
// carrier the game's light ramps will live in; nothing in the pattern math
// below depends on the specific colors.
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

}  // namespace

bool Video_Init(int w, int h, SDL_Window** out_window) {
    g_video.w = w;
    g_video.h = h;

    g_video.window = SDL_CreateWindow("d2linux (Vanilla+ native Linux host)", w, h,
                                      SDL_WINDOW_RESIZABLE);
    if (!g_video.window) {
        std::printf("d2linux: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g_video.renderer = SDL_CreateRenderer(g_video.window, nullptr);
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

    *out_window = g_video.window;
    return true;
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

    // Palette lookup + upload + present. (M2: fold the conversion into the
    // texture lock and drop the per-frame intermediate surface.)
    SDL_Surface* rgb = SDL_ConvertSurface(g_video.staging, SDL_PIXELFORMAT_XRGB8888);
    if (!rgb) {
        std::printf("d2linux: SDL_ConvertSurface failed: %s\n", SDL_GetError());
        return;
    }
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(g_video.texture, nullptr, &pixels, &pitch)) {
        const Uint8* src = (const Uint8*)rgb->pixels;
        Uint8* dst = (Uint8*)pixels;
        const size_t copy = (size_t)g_video.w * 4;
        for (int y = 0; y < g_video.h; ++y) {
            SDL_memcpy(dst + (size_t)y * pitch, src + (size_t)y * rgb->pitch, copy);
        }
        SDL_UnlockTexture(g_video.texture);
    }
    SDL_DestroySurface(rgb);

    SDL_RenderTexture(g_video.renderer, g_video.texture, nullptr, nullptr);
    SDL_RenderPresent(g_video.renderer);
}

void Video_Shutdown() {
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
    if (g_video.window) {
        SDL_DestroyWindow(g_video.window);
        g_video.window = nullptr;
    }
}

}  // namespace d2linux
