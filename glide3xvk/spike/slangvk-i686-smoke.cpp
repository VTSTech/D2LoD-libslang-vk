// slangvk-i686-smoke.cpp — link-only smoke for libslang-vk as i686 Windows.
// Never executed in the sandbox (no 32-bit Windows ICD here): this TU exists to
// prove the full static closure (slangvk + glslang + SPIRV + volk + CRT) links
// under the d2w- mingw-w64-i686 toolchain, and that the P3-present-pass call
// pattern resolves against the real C API.
#include <slangvk/c.h>

int main() {
    slangvk_device *dev = nullptr;
    slangvk_preset *preset = nullptr;
    slangvk_chain *chain = nullptr;

    slangvk_device_options dev_opt = {};
    if (slangvk_device_create(&dev_opt, &dev) != SLANGVK_OK) return 1;

    slangvk_swapchain_frame out = {};
    if (slangvk_chain_create(dev, preset, 1280, 960,
                             VK_FORMAT_B8G8R8A8_UNORM, &chain) != SLANGVK_OK) return 2;

    slangvk_input_frame in = {};   /* view/format/width/height per contract */
    slangvk_frame_info fi = { 0, 16000, +1 };
    if (slangvk_chain_render_swapchain(chain, &in, &out, &fi) != SLANGVK_OK) return 3;
    return 0;
}
