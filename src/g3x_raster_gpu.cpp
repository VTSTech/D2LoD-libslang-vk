// g3x_raster_gpu.cpp — Route-B Stage 2: the DLL-side GPU raster path (§7).
//
// Owns everything kernel-side inside the DLL: the record-sink consumer (the
// same stream shape the CI parity harness collects), the SHARED packer
// (spike/routeb_pack.h — one pack implementation for harness and DLL), the
// SSBO backing memory, the compute pipeline, the game-res intermediate image
// and the ghash readback. The present itself goes through the backend's
// g3x_vk_present_image seam: acquire -> [dispatch + copies recorded by this
// module] -> blit -> present, all in ONE backend-owned submission.
//
// Bit-exactness inheritance: this module packs the same primitive stream with
// the same packer into the same record layout the CI gates verify on Lavapipe
// (corpus pin + fxparity replay). The field ghash= option re-derives the
// same FNV-1a frame hash the harness pins, so a field session can be diffed
// against the corpus pins directly.
//
// Fail-soft house style: no routeb.comp.spv beside the DLL, no compute
// queue, any object creation failure -> raster=gpu disengages with a log
// line and the CPU raster carries the session. A present/dispatch failure
// mid-session costs that one frame (the game rolls on; next frame retries).
#include "g3x_raster_gpu.h"
#include "g3x_frame.h"
#include "g3x_log.h"
#include "vk_backend.h"
#include "../spike/routeb_pack.h"

#include <volk.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <new>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

constexpr VkDeviceSize kAlign = 256;     // region alignment (harness parity)
constexpr uint32_t kRegionCount = 8;     // bindings 0..7, order == the kernel's

struct Gpu {
    bool     active = false;      // sink attached, emit-only on
    bool     dev_ok = false;      // pipeline/image live (decided at start)
    uint32_t w = 0, h = 0;
    bool     ghash = false;
    uint64_t frames = 0;          // frames rendered on the GPU path
    uint64_t presented = 0;       // frames that reached the present seam

    // ---- sink state (shape-compatible with routeb::Collector's tables) ------
    routeb::Collector col;        // pal / chroma / tex content, pruned per frame
    routeb::FramePkg  fp;         // the frame currently accumulating
    bool              fp_open = false;

    // ---- pack output (reused) ----------------------------------------------
    routeb::FrameGPU g;

    // ---- Vulkan objects -----------------------------------------------------
    VkDevice         dev = VK_NULL_HANDLE;
    uint32_t         qfam = 0;
    VkBuffer         buf = VK_NULL_HANDLE;
    VkDeviceMemory   mem = VK_NULL_HANDLE;
    uint8_t*         map = nullptr;
    VkDeviceSize     buf_size = 0;
    VkDeviceSize     off[kRegionCount] = {};
    VkDeviceSize     size[kRegionCount] = {};
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkDescriptorPool      pool = VK_NULL_HANDLE;
    VkDescriptorSet       dset = VK_NULL_HANDLE;
    VkPipelineLayout      pl = VK_NULL_HANDLE;
    VkPipeline            pipe = VK_NULL_HANDLE;
    VkShaderModule        sm = VK_NULL_HANDLE;
    VkImage               img = VK_NULL_HANDLE;
    VkDeviceMemory        img_mem = VK_NULL_HANDLE;
    bool                  img_first = true;    // layout UNDEFINED on first use
    VkBuffer              rb = VK_NULL_HANDLE;
    VkDeviceMemory        rb_mem = VK_NULL_HANDLE;
    uint8_t*              rb_map = nullptr;
    uint64_t              ghash_prev = 0;
    bool                  ghash_have = false;
};

Gpu s;

void log_once_gpu(const char* tag, const char* msg) { g3x_log_once(tag, "%s", msg); }

// ---- the record sink (raster half + frame/clear api half) -------------------

void gp_frame(void*, uint32_t w, uint32_t h) {
    s.fp_open = true;
    s.fp.w = w; s.fp.h = h;
    s.fp.clear = 0;
    s.fp.prims.clear();                       // capacity retained
}

void gp_clear(void*, uint32_t color) {
    if (s.fp_open) s.fp.clear = color;
}

void gp_prim(void*, const G3xRouteBPrim* p) {
    if (s.fp_open) s.fp.prims.push_back(*p);
}

void gp_pal(void*, uint32_t ver, const uint32_t* pal256) {
    s.col.pal.emplace(ver, std::vector<uint32_t>(pal256, pal256 + 256));
}

void gp_chroma(void*, uint32_t ver, const uint8_t* hit) {
    s.col.chroma.emplace(ver, std::vector<uint8_t>(hit, hit + 256));
}

void gp_tex(void*, int32_t slot, uint32_t ver, const void* data, uint32_t bytes,
            uint32_t w, uint32_t h, uint32_t lod, uint32_t bpp) {
    routeb::TexSnap t;
    t.w = w; t.h = h; t.lod = lod; t.bpp = bpp;
    const uint8_t* d = (const uint8_t*)data;
    t.data.assign(d, d + bytes);
    s.col.tex.emplace(((uint64_t)(uint32_t)slot << 32) | ver, std::move(t));
}

G3xRouteBSink make_gpu_sink() {
    G3xRouteBSink s{};
    s.ud           = nullptr;
    s.prim         = &gp_prim;
    s.table_pal    = &gp_pal;
    s.table_chroma = &gp_chroma;
    s.table_tex    = &gp_tex;
    s.api_frame    = &gp_frame;
    s.api_clear    = &gp_clear;
    return s;    // remaining api handlers: fxcapture owns them when armed
}

// File-scope so stop() can detach conditionally (g3x_frame_routeb_detach):
// fxcapture and this path share the single sink slot, and a bare-null
// detach from whichever stops second disarmed the other mid-session.
const G3xRouteBSink s_gp_sink = make_gpu_sink();

// Cross-frame table pruning: content versions are monotonic per key, so a
// LATER frame can only reference the newest version of each key. Dropping
// the superseded entries after each pack bounds the maps to the live texture
// table (the CPU side holds the same bytes in its own cache).
void prune_tables() {
    if (s.col.pal.size() > 1)
        s.col.pal.erase(s.col.pal.begin(), std::prev(s.col.pal.end()));
    if (s.col.chroma.size() > 1)
        s.col.chroma.erase(s.col.chroma.begin(), std::prev(s.col.chroma.end()));
    if (s.col.tex.size() > 1) {
        std::map<uint64_t, routeb::TexSnap> nk;   // keys sort (slot, ver): keep
        for (auto it = s.col.tex.begin(); it != s.col.tex.end(); ++it) {
            auto nx = std::next(it);              // the LAST version per slot
            if (nx == s.col.tex.end() || (nx->first >> 32) != (it->first >> 32))
                nk.emplace(it->first, std::move(it->second));
        }
        s.col.tex = std::move(nk);
    }
}

// ---- Vulkan helpers ----------------------------------------------------------

VkPhysicalDevice s_phys_for_mem = VK_NULL_HANDLE;   // set at start()

uint32_t find_mem_type(VkMemoryRequirements mr, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(s_phys_for_mem, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((mr.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & want) == want)
            return i;
    return 0xFFFFFFFFu;
}

VkDeviceSize region_total() {
    VkDeviceSize t = 0;
    for (uint32_t i = 0; i < kRegionCount; ++i) t = t + s.size[i];
    return t;
}

void region_layout() {
    // Region ORDER == the kernel's binding order (routeb.comp):
    // 0 vxyuv, 1 vargb, 2 tris, 3 RECS, 4 HEAP, 5 RRECTS, 6 bins, 7 out.
    // Offsets chain THROUGH the 256-alignment exactly like the harness's
    // Region table (routeb_gpu.cpp) — an unaligned region start must push
    // the next one out, never overlap it.
    s.size[0] = (VkDeviceSize)s.g.vxyuv.size() * 4;
    s.size[1] = (VkDeviceSize)s.g.vargb.size() * 4;
    s.size[2] = (VkDeviceSize)s.g.tris.size() * 4;
    s.size[3] = (VkDeviceSize)s.g.recs.size() * 4;
    s.size[4] = (VkDeviceSize)s.g.heap.size();
    s.size[5] = (VkDeviceSize)s.g.rects.size() * 4;
    s.size[6] = (VkDeviceSize)s.g.bins.size() * 4;
    s.size[7] = (VkDeviceSize)s.g.w * s.g.h * 4;
    s.off[0] = 0;
    for (uint32_t i = 1; i < kRegionCount; ++i)
        s.off[i] = (s.off[i - 1] + s.size[i - 1] + kAlign - 1) & ~(VkDeviceSize)(kAlign - 1);
}

bool ensure_buffer(VkDeviceSize bytes) {
    if (s.buf && s.buf_size >= bytes) return true;
    if (s.map)      { vkUnmapMemory(s.dev, s.mem); s.map = nullptr; }
    if (s.buf)      { vkDestroyBuffer(s.dev, s.buf, nullptr); s.buf = VK_NULL_HANDLE; }
    if (s.mem)      { vkFreeMemory(s.dev, s.mem, nullptr); s.mem = VK_NULL_HANDLE; }
    s.buf_size = 0;
    VkBufferCreateInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = bytes; bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(s.dev, &bi, nullptr, &s.buf) != VK_SUCCESS) return false;
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(s.dev, s.buf, &mr);
    const uint32_t mt = find_mem_type(mr, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mt == 0xFFFFFFFFu) return false;
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size; ai.memoryTypeIndex = mt;
    if (vkAllocateMemory(s.dev, &ai, nullptr, &s.mem) != VK_SUCCESS) return false;
    if (vkBindBufferMemory(s.dev, s.buf, s.mem, 0) != VK_SUCCESS) return false;
    if (vkMapMemory(s.dev, s.mem, 0, VK_WHOLE_SIZE, 0, (void**)&s.map) != VK_SUCCESS)
        return false;
    s.buf_size = mr.size;
    return true;
}

bool ensure_ghash_readback(VkDeviceSize bytes) {
    if (s.rb && s.rb_map) return true;
    VkBufferCreateInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = bytes; bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (vkCreateBuffer(s.dev, &bi, nullptr, &s.rb) != VK_SUCCESS) return false;
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(s.dev, s.rb, &mr);
    const uint32_t mt = find_mem_type(mr, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mt == 0xFFFFFFFFu) return false;
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size; ai.memoryTypeIndex = mt;
    if (vkAllocateMemory(s.dev, &ai, nullptr, &s.rb_mem) != VK_SUCCESS) return false;
    if (vkBindBufferMemory(s.dev, s.rb, s.rb_mem, 0) != VK_SUCCESS) return false;
    if (vkMapMemory(s.dev, s.rb_mem, 0, VK_WHOLE_SIZE, 0, (void**)&s.rb_map) != VK_SUCCESS)
        return false;
    return true;
}

// FNV-1a over the frame's ARGB dwords — the exact hash the harness pins.
uint64_t fnv1a_px(const uint32_t* px, unsigned n) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned i = 0; i < n; ++i) {
        h ^= (uint64_t)px[i];
        h *= 1099511628211ull;
    }
    return h;
}

// ---- the command-buffer callback: dispatch + copies, then the backend blits --

void record_dispatch(void*, void* cmdv, void*, unsigned, unsigned) {
    VkCommandBuffer cmd = (VkCommandBuffer)cmdv;

    // intermediate image into TRANSFER_DST for the out copy
    VkImageMemoryBarrier to_dst{}; to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_dst.image = s.img;
    to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_dst.subresourceRange.levelCount = 1;
    to_dst.subresourceRange.layerCount = 1;
    to_dst.oldLayout = s.img_first ? VK_IMAGE_LAYOUT_UNDEFINED
                                   : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.srcAccessMask = s.img_first ? 0u : VK_ACCESS_TRANSFER_READ_BIT; // blit read
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, s.img_first ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                          : VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &to_dst);
    s.img_first = false;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s.pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s.pl, 0, 1, &s.dset, 0, nullptr);
    const uint32_t tiles_x = (s.w + 31u) / 32u, tiles_y = (s.h + 31u) / 32u;
    const uint32_t push[4] = { s.w, s.h, tiles_x, tiles_y };
    vkCmdPushConstants(cmd, s.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, push);
    vkCmdDispatch(cmd, tiles_x, tiles_y, 1);

    // out region: compute write -> transfer read
    VkBufferMemoryBarrier bb{}; bb.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bb.buffer = s.buf;
    bb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bb.offset = s.off[7];
    bb.size = s.size[7];
    bb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &bb, 0, nullptr);

    VkBufferImageCopy cpy{};
    cpy.bufferOffset = s.off[7];
    cpy.bufferRowLength = 0; cpy.bufferImageHeight = 0;   // tightly packed
    cpy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.imageSubresource.layerCount = 1;
    cpy.imageExtent = {s.w, s.h, 1};
    vkCmdCopyBufferToImage(cmd, s.buf, s.img,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

    // intermediate -> TRANSFER_SRC: the backend's blit reads it next
    VkImageMemoryBarrier to_src = to_dst;
    to_src.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_src.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &to_src);

    if (s.ghash) {
        // out -> host-visible readback; valid after the backend's fence wait
        VkBufferCopy rbcpy{};
        rbcpy.srcOffset = s.off[7];
        rbcpy.dstOffset = 0;
        rbcpy.size = (VkDeviceSize)s.w * s.h * 4;
        vkCmdCopyBuffer(cmd, s.buf, s.rb, 1, &rbcpy);
    }
}

} // namespace

// ---- lifecycle ---------------------------------------------------------------

bool g3x_raster_gpu_active() { return s.active; }

void g3x_raster_gpu_set_ghash(bool on) { s.ghash = on; }

void g3x_raster_gpu_start(unsigned w, unsigned h) {
    if (s.active) return;
    s.w = w; s.h = h;
    s.fp.prims.reserve(16384);

    void* phys = nullptr; void* dev = nullptr; unsigned qfam = 0;
    if (!g3x_vk_routeb_device(&phys, &dev, &qfam)) {
        log_once_gpu("gpu-nodev", "raster=gpu: no Vulkan device (backend down) — CPU raster stays");
        return;
    }
    s_phys_for_mem = (VkPhysicalDevice)phys;
    s.dev = (VkDevice)dev; s.qfam = qfam;

    // routeb.comp.spv beside the DLL — the SAME file the CI routeb job
    // compiled and hash-verified, so the DLL runs exactly the gated kernel.
    char path[600] = "";
#ifdef _WIN32
    {
        char exe[MAX_PATH];
        if (GetModuleFileNameA(nullptr, exe, MAX_PATH) > 0) {
            char* slash = strrchr(exe, '\\');
            if (slash) *slash = '\0';
            snprintf(path, sizeof(path) - 1, "%s\\routeb.comp.spv", exe);
            path[sizeof(path) - 1] = '\0';
        }
    }
#endif
    if (!path[0]) snprintf(path, sizeof(path) - 1, "routeb.comp.spv");
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        g3x_log("raster=gpu: %s not found — CPU raster stays (the CI artifact "
                "ships the file next to glide3x.dll)", path);
        return;
    }
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    std::vector<uint32_t> spv((size_t)sz / 4);
    const size_t got = fread(spv.data(), 4, spv.size(), fp);
    fclose(fp);
    if (got != spv.size() || spv.empty()) {
        log_once_gpu("gpu-spv", "raster=gpu: short .spv read — CPU raster stays");
        return;
    }
    VkShaderModuleCreateInfo smci{}; smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = spv.size() * 4; smci.pCode = spv.data();
    if (vkCreateShaderModule(s.dev, &smci, nullptr, &s.sm) != VK_SUCCESS) {
        log_once_gpu("gpu-sm", "raster=gpu: vkCreateShaderModule failed — CPU raster stays");
        return;
    }

    VkDescriptorSetLayoutBinding lb[kRegionCount];
    for (uint32_t i = 0; i < kRegionCount; ++i) {
        lb[i].binding = i; lb[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lb[i].descriptorCount = 1; lb[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dlci{}; dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlci.bindingCount = kRegionCount; dlci.pBindings = lb;
    if (vkCreateDescriptorSetLayout(s.dev, &dlci, nullptr, &s.dsl) != VK_SUCCESS) return;
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kRegionCount};
    VkDescriptorPoolCreateInfo pci{}; pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1; pci.poolSizeCount = 1; pci.pPoolSizes = &ps;
    if (vkCreateDescriptorPool(s.dev, &pci, nullptr, &s.pool) != VK_SUCCESS) return;
    VkDescriptorSetAllocateInfo dai{}; dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = s.pool; dai.descriptorSetCount = 1; dai.pSetLayouts = &s.dsl;
    if (vkAllocateDescriptorSets(s.dev, &dai, &s.dset) != VK_SUCCESS) return;
    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16};
    VkPipelineLayoutCreateInfo plci{}; plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1; plci.pSetLayouts = &s.dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(s.dev, &plci, nullptr, &s.pl) != VK_SUCCESS) return;
    VkPipelineShaderStageCreateInfo st{}; st.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st.stage = VK_SHADER_STAGE_COMPUTE_BIT; st.module = s.sm; st.pName = "main";
    VkComputePipelineCreateInfo cp{}; cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp.stage = st; cp.layout = s.pl;
    if (vkCreateComputePipelines(s.dev, VK_NULL_HANDLE, 1, &cp, nullptr, &s.pipe) != VK_SUCCESS) {
        log_once_gpu("gpu-pipe", "raster=gpu: vkCreateComputePipelines failed — CPU raster stays");
        return;
    }

    // game-res intermediate (BGRA8 bytes == the kernel's ARGB dword layout)
    VkImageCreateInfo ii{}; ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_B8G8R8A8_UNORM;
    ii.extent = {s.w, s.h, 1};
    ii.mipLevels = 1; ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(s.dev, &ii, nullptr, &s.img) != VK_SUCCESS) return;
    VkMemoryRequirements mr{}; vkGetImageMemoryRequirements(s.dev, s.img, &mr);
    uint32_t mt = find_mem_type(mr, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mt == 0xFFFFFFFFu) mt = find_mem_type(mr, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (mt == 0xFFFFFFFFu) return;
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size; ai.memoryTypeIndex = mt;
    if (vkAllocateMemory(s.dev, &ai, nullptr, &s.img_mem) != VK_SUCCESS) return;
    if (vkBindImageMemory(s.dev, s.img, s.img_mem, 0) != VK_SUCCESS) return;

    if (s.ghash && !ensure_ghash_readback((VkDeviceSize)s.w * s.h * 4)) {
        g3x_log("raster=gpu: ghash readback unavailable — continuing without it");
        s.ghash = false;
    }

    s.dev_ok = true;
    g3x_frame_routeb_sink(&s_gp_sink);
    g3x_frame_gpu_emit_only(true);
    s.active = true;
    g3x_log("raster=gpu: kernel live (%ux%u, %ux%u tiles) — CPU raster idle "
            "(ghash=%s)", s.w, s.h, (s.w + 31u) / 32u, (s.h + 31u) / 32u,
            s.ghash ? "on" : "off");
}

void g3x_raster_gpu_stop() {
    if (!s.active) return;
    g3x_frame_gpu_emit_only(false);
    g3x_frame_routeb_detach(&s_gp_sink);
    s.active = false;
    s.fp_open = false;
    g3x_log("raster=gpu: detached (rendered %llu frames)",
            (unsigned long long)s.frames);
}

// The per-frame dispatch body: pack + upload + dispatch + present whatever
// the sink collected this frame. Runs with the window closed; returns
// whether the kernel drew and presented it.
static bool end_frame_dispatch() {
    if (s.fp.prims.empty()) return false;    // Bink/LFB frame: CPU compose path

    if (!routeb::pack_frame(s.fp, s.col, s.g)) {
        log_once_gpu("gpu-pack", "raster=gpu: pack failed (unknown table version) — frame lost");
        s.fp.prims.clear();
        return false;
    }
    prune_tables();
    s.frames++;

    region_layout();
    if (!ensure_buffer(region_total())) {
        log_once_gpu("gpu-mem", "raster=gpu: SSBO alloc failed — frame lost");
        s.fp.prims.clear();
        return false;
    }
    for (uint32_t i = 0; i < kRegionCount; ++i)
        if (s.size[i]) {
            const void* src = nullptr;
            switch (i) {
                case 0: src = s.g.vxyuv.data(); break;
                case 1: src = s.g.vargb.data(); break;
                case 2: src = s.g.tris.data();  break;
                case 3: src = s.g.recs.data();  break;
                case 4: src = s.g.heap.data();  break;
                case 5: src = s.g.rects.data(); break;
                case 6: src = s.g.bins.data();  break;
            }
            if (src) memcpy(s.map + s.off[i], src, (size_t)s.size[i]);
        }
    // out init: the CPU's reset+clear rule, verbatim (harness parity)
    const uint32_t clrv = s.fp.clear;
    const uint32_t init = (clrv & 0x00FFFFFFu) == 0 ? 0u : clrv | 0xFF000000u;
    uint32_t* outw = (uint32_t*)(s.map + s.off[7]);
    for (unsigned i = 0; i < s.g.w * s.g.h; ++i) outw[i] = init;

    // descriptor refresh for this frame's offsets
    VkWriteDescriptorSet wr[kRegionCount];
    VkDescriptorBufferInfo bi[kRegionCount];
    for (uint32_t i = 0; i < kRegionCount; ++i) {
        bi[i].buffer = s.buf; bi[i].offset = s.off[i]; bi[i].range = s.size[i];
        wr[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[i].dstSet = s.dset; wr[i].dstBinding = i; wr[i].descriptorCount = 1;
        wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wr[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(s.dev, kRegionCount, wr, 0, nullptr);

    const bool ok = g3x_vk_present_image(&s, &record_dispatch, (void*)s.img,
                                         s.w, s.h);
    s.fp.prims.clear();
    if (!ok) return false;

    if (s.ghash && s.rb_map) {
        const uint64_t h = fnv1a_px((const uint32_t*)s.rb_map, s.w * s.h);
        if (!s.ghash_have || h != s.ghash_prev || (s.frames % 120u) == 0) {
            g3x_log("frame %llu: ghash=%016llx%s",
                    (unsigned long long)s.frames, (unsigned long long)h,
                    s.ghash_have && h != s.ghash_prev ? " (changed)" : "");
            s.ghash_prev = h;
            s.ghash_have = true;
        }
    }
    return true;
}

bool g3x_raster_gpu_end_frame() {
    if (!s.active || !s.dev_ok) return false;
    s.fp_open = false;
    s.presented++;
    const bool ok = end_frame_dispatch();
    // Frame-window reopen — field bug, first raster=gpu session: api_frame
    // (the window-open event) fires per grSstWinOpen because it IS the cap
    // format's frame boundary, but the live loop opens a context ONCE and
    // then presents N times. The window closed at the first present —
    // usually primitive-less, the game clears before it draws — and every
    // later draw hit gp_prim's fp_open guard and was dropped: "detached
    // (rendered 0 frames)", slang presented a black out-image all session.
    // The replay harness never saw this: it drives one frame per EV_FRAME,
    // i.e. one window per pack. The real per-swap frame boundary is HERE:
    // reopen the window whatever this frame did (drawn / Bink-LFB / lost)
    // so the next frame's draws collect. w/h stay as set at context open.
    s.fp_open = true;
    s.fp.clear = 0;
    s.fp.prims.clear();                      // capacity retained
    return ok;
}

void g3x_raster_gpu_shutdown() {
    g3x_raster_gpu_stop();
    if (s.dev == VK_NULL_HANDLE) return;
    if (s.map)    { vkUnmapMemory(s.dev, s.mem); s.map = nullptr; }
    if (s.buf)    { vkDestroyBuffer(s.dev, s.buf, nullptr); s.buf = VK_NULL_HANDLE; }
    if (s.mem)    { vkFreeMemory(s.dev, s.mem, nullptr); s.mem = VK_NULL_HANDLE; }
    if (s.rb_map) { vkUnmapMemory(s.dev, s.rb_mem); s.rb_map = nullptr; }
    if (s.rb)     { vkDestroyBuffer(s.dev, s.rb, nullptr); s.rb = VK_NULL_HANDLE; }
    if (s.rb_mem) { vkFreeMemory(s.dev, s.rb_mem, nullptr); s.rb_mem = VK_NULL_HANDLE; }
    if (s.img)    { vkDestroyImage(s.dev, s.img, nullptr); s.img = VK_NULL_HANDLE; }
    if (s.img_mem){ vkFreeMemory(s.dev, s.img_mem, nullptr); s.img_mem = VK_NULL_HANDLE; }
    if (s.pipe)   { vkDestroyPipeline(s.dev, s.pipe, nullptr); s.pipe = VK_NULL_HANDLE; }
    if (s.pl)     { vkDestroyPipelineLayout(s.dev, s.pl, nullptr); s.pl = VK_NULL_HANDLE; }
    if (s.pool)   { vkDestroyDescriptorPool(s.dev, s.pool, nullptr); s.pool = VK_NULL_HANDLE; }
    if (s.dsl)    { vkDestroyDescriptorSetLayout(s.dev, s.dsl, nullptr); s.dsl = VK_NULL_HANDLE; }
    if (s.sm)     { vkDestroyShaderModule(s.dev, s.sm, nullptr); s.sm = VK_NULL_HANDLE; }
    s.dev = VK_NULL_HANDLE;
    s.dev_ok = false;
    s.buf_size = 0;
}
