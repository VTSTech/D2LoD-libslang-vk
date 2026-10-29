// routeb_gpu.cpp — the Route-B Stage 0 parity harness (GPU side).
//
// Packs the routeb corpus into SSBOs, runs spike/routeb.comp on the first
// available Vulkan compute device (Lavapipe in CI), and prints the same
// per-frame hash lines as the CPU run — `diff` against the CPU pin is the
// bit-exactness gate (ROUTE-B.md §6).
//
// Build (CI / any Linux box with a Vulkan loader):
//   glslangValidator -V spike/routeb.comp -o routeb.comp.spv
//   g++ -O2 -std=c++17 -DROUTEB_HAVE_GPU -I spike/fakewin -I src -I vendor/volk \
//       -I vendor/Vulkan-Headers/include \
//       spike/raster_regress.cpp spike/routeb_gpu.cpp src/g3x_frame.cpp \
//       vendor/volk/volk.c -ldl -lpthread -o raster_regress
// Run:
//   ./raster_regress gpu routeb.comp.spv > gpu_hashes.txt
//
// Fail-soft house style: no loader / no device / no compute queue prints the
// reason and exits 3 (the CI job treats that as red, not as skip).
#include "volk.h"

#include "routeb_corpus.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace routeb {

static uint64_t fnv1a_gpu(const uint32_t* px, unsigned n) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned i = 0; i < n; ++i) {
        h ^= (uint64_t)px[i];
        h *= 1099511628211ull;
    }
    return h;
}

static inline uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

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

int run_gpu_impl(const Corpus& C, const char* spv_path, uint32_t** sink = nullptr) {
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

    // ---- resource layout: one buffer, 8 aligned regions ---------------------
    const uint32_t nverts   = (uint32_t)C.verts.size();
    uint32_t ntris_max = 0, nrecs_max = 0;
    for (unsigned f = 0; f < FRAMES; ++f) {
        if (C.frame_tris[f].size()  > ntris_max) ntris_max  = (uint32_t)C.frame_tris[f].size();
        if (C.frame_draws[f].size() > nrecs_max) nrecs_max = (uint32_t)C.frame_draws[f].size();
    }
    const size_t heap_words = (C.heap.size() + 3) / 4;

    Region r_vxyuv{0, (VkDeviceSize)nverts * 16}, r_vargb, r_tris, r_recs,
           r_heap, r_pal{0, 1024}, r_chroma{0, 1024}, r_out{0, (VkDeviceSize)W * H * 4 + 4096};
    auto align256 = [](VkDeviceSize s) { return (s + ALIGN - 1) & ~(VkDeviceSize)(ALIGN - 1); };
    r_vargb.off  = align256(r_vxyuv.off + r_vxyuv.size);
    r_vargb.size = (VkDeviceSize)nverts * 4;
    r_tris.off   = align256(r_vargb.off + r_vargb.size);
    r_tris.size  = (VkDeviceSize)ntris_max * 16;
    r_recs.off   = align256(r_tris.off + r_tris.size);
    r_recs.size  = (VkDeviceSize)nrecs_max * 52;
    r_heap.off   = align256(r_recs.off + r_recs.size);
    r_heap.size  = (VkDeviceSize)heap_words * 4;
    r_pal.off    = align256(r_heap.off + r_heap.size);
    r_chroma.off = align256(r_pal.off + r_pal.size);
    r_out.off    = align256(r_chroma.off + r_chroma.size);
    const VkDeviceSize total = align256(r_out.off + r_out.size);

    VkBuffer buf; 
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = total; bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
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

    // ---- descriptors ---------------------------------------------------------
    const Region* regs[8] = { &r_vxyuv, &r_vargb, &r_tris, &r_recs, &r_heap, &r_pal, &r_chroma, &r_out };
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
    VkDescriptorPool pool; vkCreateDescriptorPool(dev, &pci, nullptr, &pool);
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = pool; dai.descriptorSetCount = 1; dai.pSetLayouts = &dsl;
    VkDescriptorSet dset; vkAllocateDescriptorSets(dev, &dai, &dset);
    VkWriteDescriptorSet wr[8];
    VkDescriptorBufferInfo bi[8];
    for (uint32_t i = 0; i < 8; ++i) {
        bi[i].buffer = buf; bi[i].offset = regs[i]->off; bi[i].range = regs[i]->size;
        wr[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[i].dstSet = dset; wr[i].dstBinding = i; wr[i].descriptorCount = 1;
        wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wr[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(dev, 8, wr, 0, nullptr);

    // ---- pipeline ------------------------------------------------------------
    std::FILE* fp = std::fopen(spv_path, "rb");
    if (!fp) die3("cannot open .spv (compile with glslangValidator -V routeb.comp)");
    std::fseek(fp, 0, SEEK_END); long sz = std::ftell(fp); std::fseek(fp, 0, SEEK_SET);
    std::vector<uint32_t> spv((size_t)sz / 4);
    if (std::fread(spv.data(), 4, spv.size(), fp) != spv.size()) die3("short .spv read");
    std::fclose(fp);
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spv.size() * 4; smci.pCode = spv.data();
    VkShaderModule sm; vkCreateShaderModule(dev, &smci, nullptr, &sm);
    VkPipelineShaderStageCreateInfo st{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    st.stage = VK_SHADER_STAGE_COMPUTE_BIT; st.module = sm; st.pName = "main";
    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    VkPipelineLayout pl; vkCreatePipelineLayout(dev, &plci, nullptr, &pl);
    VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cp.stage = st; cp.layout = pl;
    VkPipeline pipe; vkCreateComputePipelines(dev, nullptr, 1, &cp, nullptr, &pipe);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence; vkCreateFence(dev, &fci, nullptr, &fence);
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = qf;
    VkCommandPool cpool; vkCreateCommandPool(dev, &cpi, nullptr, &cpool);
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cpool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer cmd; vkAllocateCommandBuffers(dev, &cai, &cmd);

    // persistent host-side packs (verts/tris shape is frame-stable except tris)
    uint32_t* vxyuv = (uint32_t*)(mapped + r_vxyuv.off);
    uint32_t* vargb = (uint32_t*)(mapped + r_vargb.off);
    for (uint32_t i = 0; i < nverts; ++i) {
        float f4[4] = { C.verts[i].x, C.verts[i].y, C.verts[i].u, C.verts[i].v };
        std::memcpy(&vxyuv[i * 4], f4, 16);
        vargb[i] = C.verts[i].argb;
    }
    uint32_t* heapw = (uint32_t*)(mapped + r_heap.off);
    for (size_t i = 0; i < heap_words; ++i) {
        uint32_t w = 0;
        for (size_t b = 0; b < 4 && i * 4 + b < C.heap.size(); ++b)
            w |= (uint32_t)C.heap[i * 4 + b] << (8 * b);
        heapw[i] = w;
    }

    std::vector<uint32_t> recwords((size_t)nrecs_max * 13, 0);
    uint8_t  chroma_tab[256];

    for (unsigned f = 0; f < FRAMES; ++f) {
        // tris + records for this frame
        uint32_t* trisw = (uint32_t*)(mapped + r_tris.off);
        const auto& tris = C.frame_tris[f];
        for (size_t i = 0; i < tris.size(); ++i) {
            trisw[i * 4 + 0] = tris[i].a; trisw[i * 4 + 1] = tris[i].b;
            trisw[i * 4 + 2] = tris[i].c; trisw[i * 4 + 3] = tris[i].rec;
        }
        const auto& draws = C.frame_draws[f];
        for (size_t d = 0; d < draws.size(); ++d) {
            const DrawSpec& ds = draws[d];
            uint32_t* R = &recwords[d * 13];
            R[0] = ds.sf; R[1] = ds.df; R[2] = ds.constant & 0xFFu;
            R[3] = ds.color_fill; R[4] = (ds.chroma_mode == 1u) ? 1u : 0u;
            R[5] = 0; R[6] = 0; R[7] = 0; R[8] = 0; R[9] = 1u;   // tex fields: unused on fill
            if (ds.tex_addr >= 0) {
                for (size_t t = 0; t < C.tex.size(); ++t) {
                    if ((int)C.tex[t].addr == ds.tex_addr) {
                        R[5] = C.tex_off[t]; R[6] = C.tex[t].w; R[7] = C.tex[t].h;
                        R[8] = C.tex[t].lod; R[9] = C.tex[t].bpp;
                        break;
                    }
                }
            }
            R[10] = (ds.constant >> 8) & 0x00FFFFFFu;
            const float sc = (float)(1u << R[8]) / 256.0f;
            R[11] = fbits(sc); R[12] = fbits(sc);
        }
        std::memcpy(mapped + r_recs.off, recwords.data(), draws.size() * 52);

        uint32_t* palw = (uint32_t*)(mapped + r_pal.off);
        std::memcpy(palw, C.palette[f].data(), 1024);
        C.chroma_table(f, chroma_tab);
        uint32_t* chw = (uint32_t*)(mapped + r_chroma.off);
        for (unsigned i = 0; i < 256; ++i) chw[i] = chroma_tab[i];

        uint32_t* outw = (uint32_t*)(mapped + r_out.off);
        for (unsigned i = 0; i < W * H; ++i) outw[i] = C.clear[f];

        // record + dispatch
        vkResetFences(dev, 1, &fence);
        vkResetCommandPool(dev, cpool, 0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmd, &bi);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, nullptr);
        uint32_t dims[4] = { W, H, 0u, 0u };
        vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, dims);
        vkCmdDispatch(cmd, (uint32_t)tris.size(), H, 1);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &si, fence);
        vkWaitForFences(dev, 1, &fence, VK_TRUE, 60ull * 1000ull * 1000ull * 1000ull);
        VkMappedMemoryRange inv{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        inv.memory = mem; inv.offset = 0; inv.size = VK_WHOLE_SIZE;
        vkInvalidateMappedMemoryRanges(dev, 1, &inv);

        const uint64_t h = fnv1a_gpu(outw, W * H);
        if (sink && sink[f]) std::memcpy(sink[f], outw, (size_t)W * H * 4);
        std::printf("frame %u %016" PRIx64 "\n", f, h);
    }

    vkDeviceWaitIdle(dev);
    return 0;
}

} // namespace routeb
