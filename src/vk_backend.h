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

// 0.4.5 window presets: after an OS-window resize, recreate the swapchain on
// the NEXT present instead of relying on the driver raising OUT_OF_DATE
// (mixed-DPI-awareness windows are exactly where that signal is flaky). The
// flag is consumed inside acquire_image; calling it when Vulkan is down is a
// harmless no-op.
void g3x_vk_request_swapchain_refresh();

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

// ---- Route-B Stage 2: GPU raster services (consumed by g3x_raster_gpu.cpp) --
//
// The kernel module renders on OUR device/queue: its dispatch is recorded
// into the present-path command buffer by g3x_vk_present_image, so kernel
// work, the out->image copy and the present are ONE fenced submission on
// the game thread (the same serialization the legacy present path uses).

// The backend's physical device, logical device and queue family. False when
// the backend is down (raster=gpu fails soft to the CPU raster).
bool g3x_vk_routeb_device(void** phys, void** device, unsigned* queue_fam);

// One present built around a caller-recorded dispatch: acquire -> begin ->
// swapchain image UNDEFINED->TRANSFER_DST -> record(ud, cmd, swapchain image,
// extent) [the module records bind/push/dispatch, the out-buffer dependency,
// the buffer->image copy and leaves src_image in TRANSFER_SRC] -> blit
// src_image (sw x sh, NEAREST — the CPU present path's integer-sampling look)
// into the swapchain extent -> present. Fail-soft: false on any failure (the
// frame is lost, the game rolls on, the next frame retries).
typedef void (*g3x_vk_present_record_fn)(void* ud, void* cmd,
                                         void* swapchain_image,
                                         unsigned dw, unsigned dh);
bool g3x_vk_present_image(void* ud, g3x_vk_present_record_fn record,
                          void* src_image, unsigned sw, unsigned sh);

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

// 0.4.6 PrintScreen: read back the JUST-RENDERED swapchain image (index as
// acquired by g3x_vk_slang_acquire, chain render already submitted and
// fence-waited, image resting in PRESENT_SRC_KHR) into a persistent
// host-visible buffer and return the mapped BGRA8 pixels — the EXACT
// presented raster: slang-FILTERED, overlay boxes included, swapchain extent
// (query with g3x_vk_slang_swap_extent). The image makes one extra lap
// PRESENT_SRC -> TRANSFER_SRC -> copy -> PRESENT_SRC in its own fenced
// submit, so the caller presents immediately afterwards with
// g3x_vk_slang_present_acquired as usual. The pointer is stable until the
// NEXT readback (or shutdown) — encode or copy before returning. Requires
// VK_IMAGE_USAGE_TRANSFER_SRC_BIT on the swapchain (granted at creation when
// the surface allows; otherwise returns nullptr, once-logged — the shot key
// fails soft). Returns nullptr (soft, logged) on any other failure too.
const uint8_t* g3x_vk_slang_readback_acquired(uint32_t idx);
