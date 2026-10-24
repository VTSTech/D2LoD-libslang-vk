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

// Current present extent (swapchain, i.e. the window client area — which in
// windowed mode can differ from the game's requested render size). Returns
// 0,0 when no live swapchain.
void g3x_vk_present_extent(unsigned* w, unsigned* h);

// Present a ready-made BGRA8 frame: w*h*4 bytes, B8G8R8A8 byte order (what an
// ARGB dword looks like in little-endian memory — Glide's GR_LFBWRITEMODE_8888
// layout). w/h must match g3x_vk_present_extent. Used by the LFB/Bink path.
bool g3x_vk_present_pixels(const uint8_t* bgra, unsigned w, unsigned h);

// Tear everything down (grSstWinClose / grGlideShutdown / DLL detach).
void g3x_vk_shutdown();

// ---- P3 slang services (consumed by g3x_slang.cpp) -------------------------
//
// The preset chain renders into OUR swapchain: it needs the acquire step, the
// per-image views, the swap format/extent, the acquire semaphore, and a
// present that does not wait on our own render semaphore (the chain render is
// synchronous/fenced and leaves the image in PRESENT_SRC_KHR).

bool  g3x_vk_slang_ready();                        // live swapchain + sync objects
bool  g3x_vk_slang_acquire(uint32_t* idx);         // vkAcquireNextImageKHR (logs OODE + recreates)
void  g3x_vk_slang_present_acquired(uint32_t idx); // vkQueuePresentKHR, no wait semaphore
void* g3x_vk_slang_swap_view(uint32_t idx);        // VkImageView of swapchain image idx
int   g3x_vk_slang_swap_format();                  // VkFormat of the swapchain images
void  g3x_vk_slang_swap_extent(unsigned* w, unsigned* h);
void* g3x_vk_slang_acquire_semaphore();            // the binary semaphore acquire signaled

// 0.4.3 shader base resolution: one fenced one-shot submit that stretches the
// game-res source image into the N-times larger input image (linear filter).
// Both images are VkImage handles of the SAME VkDevice/queue the backend owns
// (slangvk adopted our context). The caller guarantees both images rest in
// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (slangvk_image's layout) and that
// no other submission is in flight on our command buffer/fence - true at the
// g3x_slang_present call site (slangvk_image_update just fenced-waited, the
// chain render has not been submitted yet, and the legacy path is idle).
// Returns false (soft, logged) when the backend is down.
bool g3x_vk_slang_blit_scaled(void* src_image, void* dst_image,
                              unsigned sw, unsigned sh, unsigned dw, unsigned dh);
