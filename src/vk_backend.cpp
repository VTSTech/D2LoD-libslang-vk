#include "vk_backend.h"
#include "g3x_log.h"
#include "g3x_slang.h"

// volk first: it pulls in <vulkan/vulkan.h> with VK_NO_PROTOTYPES and provides
// every entry point as a function pointer loaded from vulkan-1.dll at runtime
// — no Vulkan SDK needed at build time, no static import of the loader.
// (windows.h already came in via vk_backend.h, LEAN_AND_MEAN + NOMINMAX.)
#include <volk.h>

#include <cstring>

G3xVkBackend g_vk;

namespace {

VkInstance       s_instance   = VK_NULL_HANDLE;
VkSurfaceKHR     s_surface    = VK_NULL_HANDLE;
VkPhysicalDevice s_phys       = VK_NULL_HANDLE;
VkDevice         s_device     = VK_NULL_HANDLE;
VkQueue          s_queue      = VK_NULL_HANDLE;
uint32_t         s_queue_fam  = 0;
VkSwapchainKHR   s_swapchain  = VK_NULL_HANDLE;
VkFormat         s_format     = VK_FORMAT_B8G8R8A8_UNORM;
VkExtent2D       s_extent     = {640, 480};
uint32_t         s_image_n    = 0;
VkImage          s_images[8]  = {};
VkImageView      s_views[8]   = {};   // P3: per-image views (the chain renders into these)
bool             s_sync_live  = false;
// 0.4.5 window presets: set by g3x_window after an OS-window resize so the
// next present recreates the swapchain from fresh caps WITHOUT waiting for
// the driver to raise OUT_OF_DATE (flaky on mixed-DPI-awareness windows).
bool             s_refresh_requested = false;
VkSemaphore      s_sem_acquire = VK_NULL_HANDLE;
VkSemaphore      s_sem_render  = VK_NULL_HANDLE;
VkFence          s_fence       = VK_NULL_HANDLE;
VkCommandPool    s_cmd_pool    = VK_NULL_HANDLE;
VkCommandBuffer  s_cmd         = VK_NULL_HANDLE;
uint64_t         s_frame       = 0;
// host-visible upload buffer for the LFB/Bink present path (extent-sized)
VkBuffer         s_staging      = VK_NULL_HANDLE;
VkDeviceMemory   s_staging_mem  = VK_NULL_HANDLE;
uint8_t*         s_staging_map  = nullptr;
VkDeviceSize     s_staging_size = 0;
// 0.4.6 PrintScreen: host-visible readback buffer for the presented-image
// capture (extent-sized, allocated on the first shot, regrown on resize), and
// whether the surface granted TRANSFER_SRC on the swapchain images at all.
VkBuffer         s_readback       = VK_NULL_HANDLE;
VkDeviceMemory   s_readback_mem   = VK_NULL_HANDLE;
uint8_t*         s_readback_map   = nullptr;
VkDeviceSize     s_readback_size  = 0;
bool             s_swap_src_ok    = false;

#define G3X_VK_CHECK(expr, what)                                              \
    do {                                                                      \
        VkResult _r = (expr);                                                 \
        if (_r != VK_SUCCESS) {                                               \
            g3x_log("vk fail: %s -> %d", what, (int)_r);                      \
            return false;                                                     \
        }                                                                     \
    } while (0)

void destroy_swapchain_objects() {
    if (s_device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(s_device);
    if (s_staging_map)      { vkUnmapMemory(s_device, s_staging_mem); s_staging_map = nullptr; }
    if (s_staging)          { vkDestroyBuffer(s_device, s_staging, nullptr); s_staging = VK_NULL_HANDLE; }
    if (s_staging_mem)      { vkFreeMemory(s_device, s_staging_mem, nullptr); s_staging_mem = VK_NULL_HANDLE; }
    s_staging_size = 0;
    if (s_readback_map)     { vkUnmapMemory(s_device, s_readback_mem); s_readback_map = nullptr; }
    if (s_readback)         { vkDestroyBuffer(s_device, s_readback, nullptr); s_readback = VK_NULL_HANDLE; }
    if (s_readback_mem)     { vkFreeMemory(s_device, s_readback_mem, nullptr); s_readback_mem = VK_NULL_HANDLE; }
    s_readback_size = 0;
    for (uint32_t i = 0; i < s_image_n; ++i) {
        if (s_views[i] != VK_NULL_HANDLE) { vkDestroyImageView(s_device, s_views[i], nullptr); s_views[i] = VK_NULL_HANDLE; }
    }
    if (s_swapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(s_device, s_swapchain, nullptr); s_swapchain = VK_NULL_HANDLE; }
    if (s_sem_acquire != VK_NULL_HANDLE) { vkDestroySemaphore(s_device, s_sem_acquire, nullptr); s_sem_acquire = VK_NULL_HANDLE; }
    if (s_sem_render != VK_NULL_HANDLE)  { vkDestroySemaphore(s_device, s_sem_render, nullptr);  s_sem_render  = VK_NULL_HANDLE; }
    if (s_fence != VK_NULL_HANDLE)       { vkDestroyFence(s_device, s_fence, nullptr);           s_fence       = VK_NULL_HANDLE; }
    if (s_cmd_pool != VK_NULL_HANDLE)    { vkDestroyCommandPool(s_device, s_cmd_pool, nullptr);  s_cmd_pool    = VK_NULL_HANDLE; }
    s_sync_live = false;
    s_image_n = 0;
}

bool create_sync_and_cmd() {
    VkSemaphoreCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi{}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    G3X_VK_CHECK(vkCreateSemaphore(s_device, &si, nullptr, &s_sem_acquire), "vkCreateSemaphore acquire");
    G3X_VK_CHECK(vkCreateSemaphore(s_device, &si, nullptr, &s_sem_render),  "vkCreateSemaphore render");
    G3X_VK_CHECK(vkCreateFence(s_device, &fi, nullptr, &s_fence), "vkCreateFence");
    VkCommandPoolCreateInfo pi{}; pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = s_queue_fam;
    G3X_VK_CHECK(vkCreateCommandPool(s_device, &pi, nullptr, &s_cmd_pool), "vkCreateCommandPool");
    VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = s_cmd_pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    G3X_VK_CHECK(vkAllocateCommandBuffers(s_device, &ai, &s_cmd), "vkAllocateCommandBuffers");
    s_sync_live = true;
    return true;
}

bool ensure_staging(VkDeviceSize bytes) {
    if (s_staging && s_staging_size >= bytes) return true;
    if (s_staging_map)      { vkUnmapMemory(s_device, s_staging_mem); s_staging_map = nullptr; }
    if (s_staging)          { vkDestroyBuffer(s_device, s_staging, nullptr); s_staging = VK_NULL_HANDLE; }
    if (s_staging_mem)      { vkFreeMemory(s_device, s_staging_mem, nullptr); s_staging_mem = VK_NULL_HANDLE; }
    s_staging_size = 0;

    VkBufferCreateInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    G3X_VK_CHECK(vkCreateBuffer(s_device, &bi, nullptr, &s_staging), "vkCreateBuffer staging");
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(s_device, s_staging, &mr);
    uint32_t mem_type = 0;
    {
        VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(s_phys, &mp);
        bool found = false;
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            if ((mr.memoryTypeBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                mem_type = i; found = true; break;
            }
        }
        if (!found) { g3x_log("vk fail: no host-visible memory type"); return false; }
    }
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size; ai.memoryTypeIndex = mem_type;
    G3X_VK_CHECK(vkAllocateMemory(s_device, &ai, nullptr, &s_staging_mem), "vkAllocateMemory staging");
    G3X_VK_CHECK(vkBindBufferMemory(s_device, s_staging, s_staging_mem, 0), "vkBindBufferMemory staging");
    G3X_VK_CHECK(vkMapMemory(s_device, s_staging_mem, 0, mr.size, 0, (void**)&s_staging_map),
                 "vkMapMemory staging");
    s_staging_size = mr.size;
    return true;
}

// host-visible readback buffer for the 0.4.6 PrintScreen presented-image
// capture. Mirror of ensure_staging with the mirror-image usage bit: the GPU
// writes INTO this buffer (CopyImageToBuffer target, TRANSFER_DST).
bool ensure_readback(VkDeviceSize bytes) {
    if (s_readback && s_readback_size >= bytes) return true;
    if (s_readback_map)     { vkUnmapMemory(s_device, s_readback_mem); s_readback_map = nullptr; }
    if (s_readback)         { vkDestroyBuffer(s_device, s_readback, nullptr); s_readback = VK_NULL_HANDLE; }
    if (s_readback_mem)     { vkFreeMemory(s_device, s_readback_mem, nullptr); s_readback_mem = VK_NULL_HANDLE; }
    s_readback_size = 0;

    VkBufferCreateInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    G3X_VK_CHECK(vkCreateBuffer(s_device, &bi, nullptr, &s_readback), "vkCreateBuffer readback");
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(s_device, s_readback, &mr);
    uint32_t mem_type = 0;
    {
        VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(s_phys, &mp);
        bool found = false;
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            if ((mr.memoryTypeBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                mem_type = i; found = true; break;
            }
        }
        if (!found) { g3x_log("vk fail: no host-visible memory type (readback)"); return false; }
    }
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size; ai.memoryTypeIndex = mem_type;
    G3X_VK_CHECK(vkAllocateMemory(s_device, &ai, nullptr, &s_readback_mem), "vkAllocateMemory readback");
    G3X_VK_CHECK(vkBindBufferMemory(s_device, s_readback, s_readback_mem, 0), "vkBindBufferMemory readback");
    G3X_VK_CHECK(vkMapMemory(s_device, s_readback_mem, 0, mr.size, 0, (void**)&s_readback_map),
                 "vkMapMemory readback");
    s_readback_size = mr.size;
    return true;
}

bool create_swapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    G3X_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys, s_surface, &caps),
                 "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    s_extent = caps.currentExtent;
    if (s_extent.width == 0xFFFFFFFFu) { s_extent.width = g_vk.width; }
    if (s_extent.height == 0xFFFFFFFFu) { s_extent.height = g_vk.height; }

    // format: prefer BGRA8/RGBA8 UNORM, whatever the surface supports first
    uint32_t nf = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nf, nullptr);
    VkSurfaceFormatKHR fmts[16] = {};
    if (nf == 0) { g3x_log("vk fail: surface has no formats"); return false; }
    if (nf > 16) nf = 16;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nf, fmts);
    s_format = fmts[0].format;
    for (uint32_t i = 0; i < nf; ++i) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            s_format = fmts[i].format;
            break;
        }
    }

    // P2.12 perf: +2 (was +1). The field log shows images=2 — the minimum.
    // The chain render is synchronous per frame, but FIFO pacing blocks
    // present until the previous image retires; one more swapchain image
    // gives the queue headroom so acquire stalls the raster thread less.
    uint32_t image_count = caps.minImageCount + 2;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) image_count = caps.maxImageCount;

    // P3: the preset chain draws as a render pass -> COLOR_ATTACHMENT usage is
    // required; TRANSFER_DST stays for the legacy unfiltered copy path. 0.4.6
    // PrintScreen: +TRANSFER_SRC when the surface allows it, so the PRESENTED
    // image can be read back for a slang-FILTERED shot (see
    // g3x_vk_slang_readback_acquired). The spec minimum for swapchain usage is
    // COLOR_ATTACHMENT only — every real implementation also grants
    // TRANSFER_SRC — but the check keeps a bizarre surface from killing the
    // whole swapchain for a screenshot's sake.
    s_swap_src_ok = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // always supported
    if (g3x_slang_vsync_pref() == 0) {
        uint32_t npm = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys, s_surface, &npm, nullptr);
        VkPresentModeKHR modes[8] = {}; if (npm > 8) npm = 8;
        vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys, s_surface, &npm, modes);
        for (uint32_t i = 0; i < npm; ++i) {
            if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) { present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR; break; }
        }
    }

    VkSwapchainCreateInfoKHR ci{}; ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = s_surface;
    ci.minImageCount = image_count;
    ci.imageFormat = s_format;
    ci.imageColorSpace = fmts[0].colorSpace;
    ci.imageExtent = s_extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    (s_swap_src_ok ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = present_mode;
    ci.clipped = VK_TRUE;
    G3X_VK_CHECK(vkCreateSwapchainKHR(s_device, &ci, nullptr, &s_swapchain), "vkCreateSwapchainKHR");

    G3X_VK_CHECK(vkGetSwapchainImagesKHR(s_device, s_swapchain, &s_image_n, nullptr),
                 "vkGetSwapchainImagesKHR");
    if (s_image_n > 8) s_image_n = 8;
    G3X_VK_CHECK(vkGetSwapchainImagesKHR(s_device, s_swapchain, &s_image_n, s_images),
                 "vkGetSwapchainImagesKHR imgs");

    g3x_log("vk: swapchain %ux%u fmt=%d images=%u (%s) readback=%s", s_extent.width, s_extent.height,
            (int)s_format, s_image_n,
            present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "FIFO",
            s_swap_src_ok ? "on" : "off");
    for (uint32_t i = 0; i < s_image_n; ++i) {
        VkImageViewCreateInfo vi{}; vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = s_images[i]; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = s_format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1; vi.subresourceRange.layerCount = 1;
        G3X_VK_CHECK(vkCreateImageView(s_device, &vi, nullptr, &s_views[i]),
                     "vkCreateImageView");
    }
    if (!ensure_staging((VkDeviceSize)s_extent.width * s_extent.height * 4)) return false;
    if (!s_sync_live && !create_sync_and_cmd()) return false;
    // P3: let the preset chain retarget (slangvk_chain_resize) to the new extent.
    g3x_slang_swapchain_resized(s_extent.width, s_extent.height, (int)s_format);
    return true;
}

} // namespace

bool g3x_vk_init(HWND hwnd, unsigned width, unsigned height) {
    if (g_vk.alive) { // re-open after close: reset first
        if (hwnd == g_vk.hwnd && width == g_vk.width && height == g_vk.height) return true;
        g3x_vk_shutdown();
    }
    g_vk.hwnd = hwnd; g_vk.width = width; g_vk.height = height;

    if (volkInitialize() != VK_SUCCESS) {
        g3x_log("vk: vulkan-1.dll (32-bit) not loadable — fail-soft, no present. "
                "Install/repair GPU driver or use the DirectDraw path (-w).");
        return false;
    }
    g3x_log("vk: loader ok, api %u.%u", VK_VERSION_MAJOR(volkGetInstanceVersion()),
            VK_VERSION_MINOR(volkGetInstanceVersion()));

    VkApplicationInfo app{}; app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "D2 glide3xvk"; app.apiVersion = VK_API_VERSION_1_1;
    const char* exts[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
    VkInstanceCreateInfo ii{}; ii.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ii.pApplicationInfo = &app;
    ii.enabledExtensionCount = 2; ii.ppEnabledExtensionNames = exts;
    G3X_VK_CHECK(vkCreateInstance(&ii, nullptr, &s_instance), "vkCreateInstance");
    volkLoadInstance(s_instance);

    VkWin32SurfaceCreateInfoKHR sci{}; sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sci.hinstance = GetModuleHandleA(nullptr); sci.hwnd = hwnd;
    G3X_VK_CHECK(vkCreateWin32SurfaceKHR(s_instance, &sci, nullptr, &s_surface),
                 "vkCreateWin32SurfaceKHR");

    uint32_t nd = 0;
    vkEnumeratePhysicalDevices(s_instance, &nd, nullptr);
    if (nd == 0) { g3x_log("vk fail: no physical devices"); return false; }
    VkPhysicalDevice devs[8] = {}; if (nd > 8) nd = 8;
    vkEnumeratePhysicalDevices(s_instance, &nd, devs);
    s_phys = devs[0]; // first with swapchain ext; prefer discrete
    for (uint32_t i = 0; i < nd; ++i) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(devs[i], &p);
        uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qn, nullptr);
        VkQueueFamilyProperties qf[8] = {}; if (qn > 8) qn = 8;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qn, qf);
        bool graphics = false;
        for (uint32_t q = 0; q < qn; ++q) if (qf[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphics = true; s_queue_fam = q; break; }
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], s_queue_fam, s_surface, &present);
        if (graphics && present) {
            s_phys = devs[i];
            if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) break; // prefer discrete
        }
        VkPhysicalDeviceProperties pp{}; vkGetPhysicalDeviceProperties(s_phys, &pp);
        g3x_log("vk: gpu[%u] %s type=%d", i, p.deviceName, (int)p.deviceType);
    }
    {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(s_phys, &p);
        g3x_log("vk: picked %s", p.deviceName);
    }

    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qc{}; qc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qc.queueFamilyIndex = s_queue_fam; qc.queueCount = 1; qc.pQueuePriorities = &prio;
    const char* dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo di{}; di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qc;
    di.enabledExtensionCount = 1; di.ppEnabledExtensionNames = dev_exts;
    G3X_VK_CHECK(vkCreateDevice(s_phys, &di, nullptr, &s_device), "vkCreateDevice");
    volkLoadDevice(s_device);
    vkGetDeviceQueue(s_device, s_queue_fam, 0, &s_queue);

    if (!create_swapchain()) return false;

    g_vk.alive = true;
    s_frame = 0;
    g3x_log("vk: backend ready (%ux%u)", g_vk.width, g_vk.height);
    if (s_extent.width != g_vk.width || s_extent.height != g_vk.height) {
        g3x_log("vk: windowed scale — window %ux%u, game renders %ux%u (present scales)",
                s_extent.width, s_extent.height, g_vk.width, g_vk.height);
    }

    // P3: hand the live context to the libslang-vk preset chain. Fail-soft:
    // without d2slangvk.conf (or on any slang failure) the legacy unfiltered
    // present below stays the whole story.
    g3x_slang_init(s_instance, s_phys, s_device, s_queue, s_queue_fam);
    return true;
}

bool g3x_vk_resize(unsigned width, unsigned height) {
    if (!g_vk.alive && s_device == VK_NULL_HANDLE) return false;
    // Minimized (or cloaked) windows report a 0x0 currentExtent: there is no
    // swapchain to make yet. Keep the old one alive and bail; the retry in
    // acquire_image brings it back the moment the window has a real size.
    if (s_phys != VK_NULL_HANDLE && s_surface != VK_NULL_HANDLE) {
        VkSurfaceCapabilitiesKHR caps{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys, s_surface, &caps) == VK_SUCCESS
            && caps.currentExtent.width == 0 && caps.currentExtent.height == 0) {
            g3x_log_once("swap-min", "surface extent 0x0 (minimized?) — swapchain kept, retrying");
            return false;
        }
    }
    g_vk.width = width; g_vk.height = height;
    destroy_swapchain_objects();
    if (!create_swapchain()) { g_vk.alive = false; return false; }
    g_vk.alive = true;
    return true;
}

void g3x_vk_request_swapchain_refresh() { s_refresh_requested = true; }

namespace {

bool acquire_image(uint32_t* idx) {
    // 0.4.5 window presets: an OS resize lands here as an explicit refresh
    // request, consumed before the acquire — the swapchain (and the preset
    // chain via g3x_slang_swapchain_resized) retargets to the new extent on
    // the very frame after the resize, no driver OUT_OF_DATE signal needed.
    if (s_refresh_requested) {
        s_refresh_requested = false;
        if (g_vk.hwnd) {
            g3x_log_once("swap-refresh", "swapchain refresh requested (window preset resized)");
            g3x_vk_resize(g_vk.width, g_vk.height);
        }
    }
    // A recreate can fail transiently (minimized 0x0 extent, mode change in
    // flight): retry here every frame until it succeeds — a present outage is
    // never permanent (the old code went dead at the first failed recreate).
    if ((!g_vk.alive || !s_sync_live) && g_vk.hwnd && s_device != VK_NULL_HANDLE) {
        if (!g3x_vk_resize(g_vk.width, g_vk.height)) return false;
    }
    if (!g_vk.alive || !s_sync_live) return false;
    VkResult r = vkAcquireNextImageKHR(s_device, s_swapchain, UINT64_MAX,
                                       s_sem_acquire, VK_NULL_HANDLE, idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        g3x_log_once("swap-oode", "swapchain out of date, recreating");
        g3x_vk_resize(g_vk.width, g_vk.height);
        return false;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        g3x_log_once("swap-acq", "vkAcquireNextImageKHR -> %d", (int)r);
        return false;
    }
    return true;
}

// barriers to TRANSFER_DST, caller records the transfer op, then this closes
void transfer_barriers_to_dst(uint32_t idx) {
    VkImageMemoryBarrier bar{}; bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bar.image = s_images[idx];
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bar.subresourceRange.levelCount = 1;
    bar.subresourceRange.layerCount = 1;
    bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.srcAccessMask = 0;
    bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(s_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);
}

void submit_and_present(uint32_t idx) {
    VkImageMemoryBarrier bar{}; bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bar.image = s_images[idx];
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bar.subresourceRange.levelCount = 1;
    bar.subresourceRange.layerCount = 1;
    bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bar.dstAccessMask = 0;
    vkCmdPipelineBarrier(s_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);
    vkEndCommandBuffer(s_cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo sub{}; sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub.waitSemaphoreCount = 1; sub.pWaitSemaphores = &s_sem_acquire;
    sub.pWaitDstStageMask = &wait_stage;
    sub.commandBufferCount = 1; sub.pCommandBuffers = &s_cmd;
    sub.signalSemaphoreCount = 1; sub.pSignalSemaphores = &s_sem_render;
    vkQueueSubmit(s_queue, 1, &sub, s_fence);
    vkWaitForFences(s_device, 1, &s_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(s_device, 1, &s_fence);

    VkPresentInfoKHR pi{}; pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &s_sem_render;
    pi.swapchainCount = 1; pi.pSwapchains = &s_swapchain;
    pi.pImageIndices = &idx;
    VkResult r = vkQueuePresentKHR(s_queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        g3x_log_once("swap-oode2", "present out of date, recreating");
        g3x_vk_resize(g_vk.width, g_vk.height);
    }
}

} // namespace

void g3x_vk_present(unsigned swap_interval) {
    ++s_frame;
    uint32_t idx = 0;
    if (!acquire_image(&idx)) return;

    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(s_cmd, &bi);
    transfer_barriers_to_dst(idx);

    // P1 frame content: solid black. P3 replaces this command buffer with the
    // libslang-vk chain render (slangvk_chain_render_swapchain) — see PLAN.md.
    VkClearColorValue black{}; black.float32[3] = 1.f;
    VkImageSubresourceRange rng{};
    rng.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; rng.levelCount = 1; rng.layerCount = 1;
    vkCmdClearColorImage(s_cmd, s_images[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &black, 1, &rng);

    (void)swap_interval; // FIFO is always on in P1; immediate mode is P2 polish
    submit_and_present(idx);
}

void g3x_vk_present_extent(unsigned* w, unsigned* h) {
    if (w) *w = (g_vk.alive && s_sync_live) ? s_extent.width : 0;
    if (h) *h = (g_vk.alive && s_sync_live) ? s_extent.height : 0;
}

bool g3x_vk_present_pixels(const uint8_t* bgra, unsigned w, unsigned h) {
    ++s_frame;
    if (!g_vk.alive || !s_sync_live) return false;
    if (w != s_extent.width || h != s_extent.height) return false; // stale compose buffer
    VkDeviceSize bytes = (VkDeviceSize)w * h * 4;
    if (!ensure_staging(bytes) || !s_staging_map) return false;

    if (s_format == VK_FORMAT_B8G8R8A8_UNORM) {
        memcpy(s_staging_map, bgra, (size_t)bytes);
    } else {
        // rare surface format (e.g. RGBA8): swizzle BGRA -> RGBA while uploading
        const uint8_t* s = bgra; uint8_t* d = s_staging_map;
        for (VkDeviceSize i = 0; i < w * h; ++i, s += 4, d += 4) {
            d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
        }
    }

    uint32_t idx = 0;
    if (!acquire_image(&idx)) return false;
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(s_cmd, &bi);
    transfer_barriers_to_dst(idx);

    VkBufferImageCopy cpy{};
    cpy.bufferOffset = 0;
    cpy.bufferRowLength = 0; cpy.bufferImageHeight = 0;   // tightly packed
    cpy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.imageSubresource.layerCount = 1;
    cpy.imageExtent = {s_extent.width, s_extent.height, 1};
    vkCmdCopyBufferToImage(s_cmd, s_staging, s_images[idx],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

    submit_and_present(idx);
    return true;
}

void g3x_vk_shutdown() {
    g3x_slang_shutdown();   // chains -> presets -> image -> adopt-release, BEFORE the device goes
    if (s_device != VK_NULL_HANDLE) destroy_swapchain_objects();
    if (s_device != VK_NULL_HANDLE) { vkDestroyDevice(s_device, nullptr); s_device = VK_NULL_HANDLE; }
    if (s_surface != VK_NULL_HANDLE && s_instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(s_instance, s_surface, nullptr); s_surface = VK_NULL_HANDLE;
    }
    if (s_instance != VK_NULL_HANDLE) { vkDestroyInstance(s_instance, nullptr); s_instance = VK_NULL_HANDLE; }
    g_vk.alive = false;
    g3x_log("vk: shutdown (frames presented: %llu)", (unsigned long long)s_frame);
}

// ---- P3 slang services -----------------------------------------------------

bool g3x_vk_slang_ready() { return g_vk.alive && s_sync_live; }

bool g3x_vk_slang_acquire(uint32_t* idx) { return acquire_image(idx); }

void g3x_vk_slang_present_acquired(uint32_t idx) {
    // The chain render already left the image in PRESENT_SRC_KHR and fenced
    // its own submission, so the present waits on nothing.
    VkPresentInfoKHR pi{}; pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.swapchainCount = 1; pi.pSwapchains = &s_swapchain;
    pi.pImageIndices = &idx;
    VkResult r = vkQueuePresentKHR(s_queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        g3x_log_once("swap-oode3", "present (slang) out of date, recreating");
        g3x_vk_resize(g_vk.width, g_vk.height);
    }
}

void* g3x_vk_slang_swap_view(uint32_t idx) {
    return (idx < s_image_n) ? (void*)s_views[idx] : nullptr;
}

// ---- 0.4.6 PrintScreen: presented-image readback ----------------------------
//
// Reads back the JUST-RENDERED swapchain image so a shot is the exact
// presented raster — slang-FILTERED pixels, overlay boxes included (they draw
// into the chain's input on the CPU side) — instead of the pre-shader chain
// input the old CPU capture points saw. Runs BETWEEN the chain render (which
// fence-waits internally — "the chain render is synchronous per frame") and
// the present itself, on the game thread: the image makes one extra lap
// PRESENT_SRC -> TRANSFER_SRC -> copy -> PRESENT_SRC in its own fenced
// submit, so present still sees the layout it requires and the map is stable
// the moment this returns (the copy is a sub-millisecond PCIe read; the
// encode+write keeps the same at-most-one-frame-hitch trade as before).
//
// Needs VK_IMAGE_USAGE_TRANSFER_SRC_BIT on the swapchain — requested at
// creation only when the surface's supportedUsageFlags grants it (every real
// implementation does; the spec minimum is COLOR_ATTACHMENT only). Without
// the bit this returns nullptr and once-logs; the shot key fails soft (press
// logged, no file, game rolls on).
const uint8_t* g3x_vk_slang_readback_acquired(uint32_t idx) {
    if (!g_vk.alive || !s_sync_live || idx >= s_image_n) return nullptr;
    if (!s_swap_src_ok) {
        g3x_log_once("shot-src", "shot: swapchain lacks TRANSFER_SRC usage — readback unavailable");
        return nullptr;
    }
    const VkDeviceSize bytes = (VkDeviceSize)s_extent.width * s_extent.height * 4;
    if (!ensure_readback(bytes) || !s_readback_map) return nullptr;

    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(s_cmd, &bi) != VK_SUCCESS) return nullptr;

    // PRESENT_SRC -> TRANSFER_SRC. srcAccessMask MUST be 0 coming out of
    // PRESENT_SRC (no access is guaranteed under it); ALL_COMMANDS covers the
    // chain's render into this image (the same idiom the blit service uses).
    VkImageMemoryBarrier to_src{}; to_src.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_src.image = s_images[idx];
    to_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_src.subresourceRange.levelCount = 1;
    to_src.subresourceRange.layerCount = 1;
    to_src.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_src.srcAccessMask = 0;
    to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(s_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_src);

    VkBufferImageCopy cpy{};
    cpy.bufferOffset = 0;
    cpy.bufferRowLength = 0; cpy.bufferImageHeight = 0;   // tightly packed
    cpy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.imageSubresource.layerCount = 1;
    cpy.imageExtent = {s_extent.width, s_extent.height, 1};
    vkCmdCopyImageToBuffer(s_cmd, s_images[idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           s_readback, 1, &cpy);

    // TRANSFER_SRC -> PRESENT_SRC: the present still gets the layout it
    // requires; ALL_COMMANDS destination scope mirrors the blit's leave
    // barriers.
    VkImageMemoryBarrier to_present = to_src;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_present.dstAccessMask = 0;
    vkCmdPipelineBarrier(s_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);
    if (vkEndCommandBuffer(s_cmd) != VK_SUCCESS) return nullptr;

    VkSubmitInfo sub{}; sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub.commandBufferCount = 1; sub.pCommandBuffers = &s_cmd;
    if (vkQueueSubmit(s_queue, 1, &sub, s_fence) != VK_SUCCESS) return nullptr;
    if (vkWaitForFences(s_device, 1, &s_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) return nullptr;
    vkResetFences(s_device, 1, &s_fence);

    // Rare non-BGRA surface (e.g. RGBA8): swizzle in place so the PNG encoder
    // keeps its BGRA contract (the same swap present_pixels does on upload).
    if (s_format != VK_FORMAT_B8G8R8A8_UNORM) {
        uint8_t* p = s_readback_map;
        for (VkDeviceSize i = 0; i < (VkDeviceSize)s_extent.width * s_extent.height; ++i, p += 4) {
            uint8_t t = p[0]; p[0] = p[2]; p[2] = t;
        }
    }
    return s_readback_map;
}

int g3x_vk_slang_swap_format() { return (int)s_format; }

void g3x_vk_slang_swap_extent(unsigned* w, unsigned* h) {
    if (w) *w = s_extent.width;
    if (h) *h = s_extent.height;
}

void* g3x_vk_slang_acquire_semaphore() { return (void*)s_sem_acquire; }

bool g3x_vk_slang_blit_scaled(void* src_image, void* dst_image,
                              unsigned sw, unsigned sh, unsigned dw, unsigned dh) {
    if (!g_vk.alive || !s_sync_live || !src_image || !dst_image ||
        !sw || !sh || !dw || !dh) return false;

    // Layout dance in one submit: SHADER_READ_ONLY -> TRANSFER both ways.
    // Access masks cover the previous frame's shader reads (src) and blit
    // writes (dst WAW), so no knowledge of the chain's exact read stages is
    // needed. The fence wait after the submit makes every write visible to
    // the chain's own (later, separately fenced) submission.
    VkImageMemoryBarrier base{}; base.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    base.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    base.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    base.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    base.subresourceRange.levelCount = 1;
    base.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier to_src = base;
    to_src.image = (VkImage)src_image;
    to_src.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_src.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkImageMemoryBarrier to_dst = base;
    to_dst.image = (VkImage)dst_image;
    to_dst.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkImageMemoryBarrier back_src = to_src;
    back_src.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    back_src.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    back_src.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    back_src.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    VkImageMemoryBarrier back_dst = to_dst;
    back_dst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    back_dst.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    back_dst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    back_dst.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {(int32_t)sw, (int32_t)sh, 1};
    blit.dstOffsets[1] = {(int32_t)dw, (int32_t)dh, 1};

    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(s_cmd, &bi) != VK_SUCCESS) return false;
    VkImageMemoryBarrier enter[2] = {to_src, to_dst};
    vkCmdPipelineBarrier(s_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                         2, enter);
    vkCmdBlitImage(s_cmd, (VkImage)src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   (VkImage)dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);
    VkImageMemoryBarrier leave[2] = {back_src, back_dst};
    vkCmdPipelineBarrier(s_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr,
                         2, leave);
    if (vkEndCommandBuffer(s_cmd) != VK_SUCCESS) return false;

    VkSubmitInfo sub{}; sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub.commandBufferCount = 1; sub.pCommandBuffers = &s_cmd;
    if (vkQueueSubmit(s_queue, 1, &sub, s_fence) != VK_SUCCESS) return false;
    if (vkWaitForFences(s_device, 1, &s_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) return false;
    vkResetFences(s_device, 1, &s_fence);
    return true;
}
