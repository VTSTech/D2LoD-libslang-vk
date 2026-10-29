// routeb_gpu.cpp — the Route-B Stage 1 parity harness (GPU side).
//
// Consumes the record sink's primitive stream (the POST-DISPATCH primitives
// the production dispatcher emitted, ROUTE-B.md §4), bins them into 32x32
// screen tiles, packs the SSBO record, and runs spike/routeb.comp on the
// first available Vulkan compute device (Lavapipe in CI). Per-frame hashes
// feed the bit-exactness gate against the CPU run (ROUTE-B.md §6).
//
// The packer NEVER re-derives dispatcher decisions: what arrives in the
// sink is what the CPU drew, already strip-normalized / shape-resolved /
// line-expanded. Binning only chooses WHICH workgroup stores a pixel —
// each pixel keeps exactly one writer, in submission order per tile.
//
// Table content (palettes, chroma tables, texels) is versioned by the frame
// module; the packer snapshots each version once per frame into a shared
// heap, so mid-frame rebind storms replay exactly.
//
// Build (CI / any Linux box with a Vulkan loader):
//   glslangValidator -V spike/routeb.comp -o routeb.comp.spv
//   g++ -O2 -std=c++17 -DROUTEB_HAVE_GPU -I spike/fakewin -I src -I vendor/volk \
//       -I vendor/Vulkan-Headers/include \
//       spike/raster_regress.cpp spike/routeb_gpu.cpp src/g3x_frame.cpp \
//       vendor/volk/volk.c -ldl -lpthread -o raster_regress
//
// Fail-soft house style: no loader / no device / no compute queue prints the
// reason and exits 3 (the CI job treats that as red, not as skip).
#include "volk.h"

#include "g3x_frame.h"
#include "routeb_pack.h"

#include <cinttypes>
#include <cmath>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

namespace routeb {

// ---- hashing ----------------------------------------------------------------

static uint64_t fnv1a_gpu(const uint32_t* px, unsigned n) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned i = 0; i < n; ++i) {
        h ^= (uint64_t)px[i];
        h *= 1099511628211ull;
    }
    return h;
}

// ---- Vulkan plumbing (mostly as Stage 0) -------------------------------------

static constexpr VkDeviceSize ALIGN = 256;
struct Region { VkDeviceSize off, size; };

static void die3(const char* why) {
    std::fprintf(stderr, "raster_regress gpu: %s\n", why);
    std::exit(3);
}

static void die3_vk(const char* what, VkResult r) {
    std::fprintf(stderr, "raster_regress gpu: %s failed (VkResult %d)\n", what, (int)r);
    if (r == VK_ERROR_INCOMPATIBLE_DRIVER)
        std::fprintf(stderr,
            "  no ICDs found — if VK_DRIVER_FILES/VK_ICD_FILENAMES is set it must\n"
            "  name an existing json (mesa >= 24.3 ships lvp_icd.json, no arch\n"
            "  suffix; older mesas lvp_icd.x86_64.json). Check /usr/share/vulkan/icd.d/\n");
    std::exit(3);
}

int run_gpu_impl(const Collector& C, const char* spv_path,
                 uint64_t* hashes_out, uint32_t** sink) {
    if (volkInitialize() != VK_SUCCESS) die3("no Vulkan loader (libvulkan.so.1)");

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance inst;
    VkResult ir;
    if ((ir = vkCreateInstance(&ici, nullptr, &inst)) != VK_SUCCESS)
        die3_vk("vkCreateInstance", ir);
    volkLoadInstance(inst);

    uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, nullptr);
    if (!nd) die3("no physical devices (install mesa-vulkan-drivers for Lavapipe)");
    std::vector<VkPhysicalDevice> pds(nd);
    vkEnumeratePhysicalDevices(inst, &nd, pds.data());
    VkPhysicalDevice pd = pds[0];

    uint32_t qf = 0xFFFFFFFFu;
    uint32_t nfam = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nfam, nullptr);
    std::vector<VkQueueFamilyProperties> fams(nfam);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nfam, fams.data());
    for (uint32_t i = 0; i < nfam; ++i)
        if (fams[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { qf = i; break; }
    if (qf == 0xFFFFFFFFu) die3("no compute queue family");

    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = qf; qci.queueCount = 1; qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    VkDevice dev;
    if (vkCreateDevice(pd, &dci, nullptr, &dev) != VK_SUCCESS) die3("vkCreateDevice failed");
    volkLoadDevice(dev);   // device-level entry points

    VkQueue queue; vkGetDeviceQueue(dev, qf, 0, &queue);

    // ---- pack every frame on the host first ---------------------------------
    std::vector<FrameGPU> packs(C.frames.size());
    for (size_t f = 0; f < C.frames.size(); ++f)
        if (!pack_frame(C.frames[f], C, packs[f]))
            die3("pack failed: primitive references an unknown table version");

    // one buffer, per-frame region chains; size once at the max
    size_t max_words = 0;
    std::vector<std::array<Region, 8>> regs(packs.size());
    for (size_t f = 0; f < packs.size(); ++f) {
        const FrameGPU& g = packs[f];
        const uint32_t NT = g.tiles_x * g.tiles_y;
        auto align256 = [](VkDeviceSize s) { return (s + ALIGN - 1) & ~(VkDeviceSize)(ALIGN - 1); };
        // Region ORDER == the kernel's binding order (routeb.comp):
        // 0 vxyuv, 1 vargb, 2 tris, 3 RECS, 4 HEAP, 5 RRECTS, 6 bins, 7 out.
        Region& r_vxyuv = regs[f][0]; Region& r_vargb = regs[f][1];
        Region& r_tris  = regs[f][2]; Region& r_recs  = regs[f][3];
        Region& r_heap  = regs[f][4]; Region& r_rects = regs[f][5];
        Region& r_bins  = regs[f][6]; Region& r_out   = regs[f][7];
        r_vxyuv.off = 0;                       r_vxyuv.size = (VkDeviceSize)g.vxyuv.size() * 4;
        r_vargb.off = align256(r_vxyuv.off + r_vxyuv.size); r_vargb.size = (VkDeviceSize)g.vargb.size() * 4;
        r_tris.off  = align256(r_vargb.off + r_vargb.size); r_tris.size  = (VkDeviceSize)g.tris.size() * 4;
        r_recs.off  = align256(r_tris.off + r_tris.size);   r_recs.size  = (VkDeviceSize)g.recs.size() * 4;
        r_heap.off  = align256(r_recs.off + r_recs.size);   r_heap.size  = (VkDeviceSize)g.heap.size();
        r_rects.off = align256(r_heap.off + r_heap.size);   r_rects.size = (VkDeviceSize)g.rects.size() * 4;
        r_bins.off  = align256(r_rects.off + r_rects.size); r_bins.size  = (VkDeviceSize)g.bins.size() * 4;
        r_out.off   = align256(r_bins.off + r_bins.size);   r_out.size   = (VkDeviceSize)g.w * g.h * 4 + 4096;
        const VkDeviceSize total = align256(r_out.off + r_out.size);
        max_words = max_words < (size_t)(total / 4) ? (size_t)(total / 4) : max_words;
        (void)NT;
    }
    if (!max_words) die3("no frames to render");

    VkBuffer buf;
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = (VkDeviceSize)max_words * 4; bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &bci, nullptr, &buf) != VK_SUCCESS) die3("vkCreateBuffer failed");
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, buf, &mr);
    uint32_t mem_type = 0xFFFFFFFFu;
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((mr.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
               (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            mem_type = i; break;
        }
    if (mem_type == 0xFFFFFFFFu) die3("no host-visible coherent memory type");
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size + ALIGN; mai.memoryTypeIndex = mem_type;
    VkDeviceMemory mem;
    if (vkAllocateMemory(dev, &mai, nullptr, &mem) != VK_SUCCESS) die3("vkAllocateMemory failed");
    if (vkBindBufferMemory(dev, buf, mem, 0) != VK_SUCCESS) die3("vkBindBufferMemory failed");
    uint8_t* mapped = nullptr;
    if (vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, (void**)&mapped) != VK_SUCCESS) die3("vkMapMemory failed");

    // ---- descriptors (8 storage-buffer bindings) ------------------------------
    VkDescriptorSetLayoutBinding lb[8];
    for (uint32_t i = 0; i < 8; ++i) {
        lb[i].binding = i; lb[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lb[i].descriptorCount = 1; lb[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dlci.bindingCount = 8; dlci.pBindings = lb;
    VkDescriptorSetLayout dsl;
    if (vkCreateDescriptorSetLayout(dev, &dlci, nullptr, &dsl) != VK_SUCCESS) die3("layout failed");
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8};
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = 1; pci.poolSizeCount = 1; pci.pPoolSizes = &ps;
    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(dev, &pci, nullptr, &pool) != VK_SUCCESS) die3("vkCreateDescriptorPool failed");
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = pool; dai.descriptorSetCount = 1; dai.pSetLayouts = &dsl;
    VkDescriptorSet dset;
    if (vkAllocateDescriptorSets(dev, &dai, &dset) != VK_SUCCESS) die3("vkAllocateDescriptorSets failed");
    VkWriteDescriptorSet wr[8];
    VkDescriptorBufferInfo bi[8];
    for (uint32_t i = 0; i < 8; ++i) {
        bi[i].buffer = buf; bi[i].offset = regs[0][i].off; bi[i].range = regs[0][i].size;
        wr[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[i].dstSet = dset; wr[i].dstBinding = i; wr[i].descriptorCount = 1;
        wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wr[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(dev, 8, wr, 0, nullptr);

    // ---- pipeline --------------------------------------------------------------
    std::FILE* fp = std::fopen(spv_path, "rb");
    if (!fp) die3("cannot open .spv (compile with glslangValidator -V routeb.comp)");
    std::fseek(fp, 0, SEEK_END); long sz = std::ftell(fp); std::fseek(fp, 0, SEEK_SET);
    std::vector<uint32_t> spv((size_t)sz / 4);
    if (std::fread(spv.data(), 4, spv.size(), fp) != spv.size()) die3("short .spv read");
    std::fclose(fp);
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spv.size() * 4; smci.pCode = spv.data();
    VkShaderModule sm;
    if (vkCreateShaderModule(dev, &smci, nullptr, &sm) != VK_SUCCESS) die3("vkCreateShaderModule failed");
    VkPipelineShaderStageCreateInfo st{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    st.stage = VK_SHADER_STAGE_COMPUTE_BIT; st.module = sm; st.pName = "main";
    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    VkPipelineLayout pl;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &pl) != VK_SUCCESS) die3("vkCreatePipelineLayout failed");
    VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cp.stage = st; cp.layout = pl;
    VkPipeline pipe;
    if (vkCreateComputePipelines(dev, nullptr, 1, &cp, nullptr, &pipe) != VK_SUCCESS)
        die3("vkCreateComputePipelines failed (shader rejected by the driver)");

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence; vkCreateFence(dev, &fci, nullptr, &fence);
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = qf;
    VkCommandPool cpool; vkCreateCommandPool(dev, &cpi, nullptr, &cpool);
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cpool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer cmd; vkAllocateCommandBuffers(dev, &cai, &cmd);

    // ---- per-frame dispatch -----------------------------------------------------
    for (size_t f = 0; f < packs.size(); ++f) {
        const FrameGPU& g = packs[f];
        const Region* R = regs[f].data();

        std::memcpy(mapped + R[0].off, g.vxyuv.data(), (size_t)g.vxyuv.size() * 4);
        std::memcpy(mapped + R[1].off, g.vargb.data(), (size_t)g.vargb.size() * 4);
        std::memcpy(mapped + R[2].off, g.tris.data(),  (size_t)g.tris.size() * 4);
        std::memcpy(mapped + R[3].off, g.recs.data(),  (size_t)g.recs.size() * 4);
        std::memcpy(mapped + R[4].off, g.heap.data(),  g.heap.size());
        std::memcpy(mapped + R[5].off, g.rects.data(), (size_t)g.rects.size() * 4);
        std::memcpy(mapped + R[6].off, g.bins.data(),  (size_t)g.bins.size() * 4);

        // out init: the CPU's reset+clear rule, verbatim
        uint32_t* outw = (uint32_t*)(mapped + R[7].off);
        const uint32_t clrv = C.frames[f].clear;
        const uint32_t init = (clrv & 0x00FFFFFFu) == 0 ? 0u : clrv | 0xFF000000u;
        for (unsigned i = 0; i < g.w * g.h; ++i) outw[i] = init;

        // descriptor refresh for this frame's offsets
        VkWriteDescriptorSet wr2[8];
        VkDescriptorBufferInfo bi2[8];
        for (uint32_t i = 0; i < 8; ++i) {
            bi2[i].buffer = buf; bi2[i].offset = R[i].off; bi2[i].range = R[i].size;
            wr2[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr2[i].dstSet = dset; wr2[i].dstBinding = i; wr2[i].descriptorCount = 1;
            wr2[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wr2[i].pBufferInfo = &bi2[i];
        }
        vkUpdateDescriptorSets(dev, 8, wr2, 0, nullptr);

        vkResetFences(dev, 1, &fence);
        vkResetCommandPool(dev, cpool, 0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmd, &bi);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, nullptr);
        const uint32_t push[4] = { g.w, g.h, g.tiles_x, g.tiles_y };
        vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, push);
        vkCmdDispatch(cmd, g.tiles_x, g.tiles_y, 1);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &si, fence);
        vkWaitForFences(dev, 1, &fence, VK_TRUE, 60ull * 1000ull * 1000ull * 1000ull);
        VkMappedMemoryRange inv{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        inv.memory = mem; inv.offset = 0; inv.size = VK_WHOLE_SIZE;
        vkInvalidateMappedMemoryRanges(dev, 1, &inv);

        if (std::getenv("ROUTEB_DEBUG")) {
            std::printf("DBG gpu NT=%u bins63=%u off63=%u e0=%u heap0=%08x heap256=%08x\n",
                        outw[1000], outw[1001], outw[1002], outw[1003],
                        outw[1004], outw[1005]);
        }
        const uint64_t h = fnv1a_gpu(outw, g.w * g.h);
        if (hashes_out) hashes_out[f] = h;
        if (sink && sink[f]) std::memcpy(sink[f], outw, (size_t)g.w * g.h * 4);
    }

    vkDeviceWaitIdle(dev);
    return (int)packs.size();
}

} // namespace routeb
