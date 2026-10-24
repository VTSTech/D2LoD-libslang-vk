// glide3xvk — Vulkan backend skeleton (P1).
//
// P1 contract (PLAN.md section 5): grSstWinOpen boots the device + swapchain;
// grBufferSwap acquires, clears black, presents. Nothing is rasterized yet.
// P3 inserts libslang-vk between the (future) game-frame image and the
// swapchain — the seam is marked TODO(P3) below.
//
// Fail-soft: if anything Vulkan fails, the backend marks itself dead, the
// game keeps running (grSstWinOpen still returns a context), and the log
// explains why. D2 then shows a black screen — which IS the P1 exit state.
#pragma once

#include "g3x_types.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

struct G3xVkBackend {
    bool        alive = false;
    HWND        hwnd = nullptr;
    unsigned    width = 0, height = 0;
};

extern G3xVkBackend g_vk;

// Boot instance/device/swapchain for a game window. Safe to call again after
// a grSstWinClose (re-open). Returns false on failure (fail-soft, logged).
bool g3x_vk_init(HWND hwnd, unsigned width, unsigned height);

// Recreate the swapchain (mode change / out-of-date). Returns false soft.
bool g3x_vk_resize(unsigned width, unsigned height);

// One present: acquire -> clear to black -> present.
// swap_interval: 0 = immediate, 1 = FIFO vsync (D2 passes bVSync as 0/1).
void g3x_vk_present(unsigned swap_interval);

// Tear everything down (grSstWinClose / grGlideShutdown / DLL detach).
void g3x_vk_shutdown();
