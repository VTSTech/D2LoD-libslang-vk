# BUILD-32BIT.md — libslang-vk as 32-bit Windows (the P1 gate)

> Status: **SPIKE PENDING** — this file is the template; fill in the Results
> section as the spike runs. Everything downstream (linking libslang-vk into
> glide3x.dll, the P3 preset chain) waits on this.

## Why this is the gate

glide3x.dll must be 32-bit (i686) because the game loads it. libslang-vk is
C++23 (gcc 13 in our toolchain). Nobody has built libslang-vk as i686 Windows
yet — that is the single biggest unknown in PLAN.md (risk R1).

## Recipe

```sh
# 1. Clone libslang-vk beside the fork (or reuse the agent sandbox clone)
git clone https://github.com/VTSTech/libslang-vk

# 2. Same toolchain as d2 (scripts/setup-toolchain.sh recipe)
export PATH=/home/z/.local/opt/venv/bin:/home/z/.local/opt/mingw-wrappers:$PATH

# 3. Configure 32-bit, static slangvk, vendored volk/VMA
cmake -S libslang-vk -B libslang-vk/build-i686 \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-i686.cmake \
    -DD2_TOOL_PREFIX=d2w- \
    -DSLANGVK_STATIC=ON -DSLANGVK_BUILD_TESTS=OFF -DSLANGVK_BUILD_BENCHMARKS=OFF

# 4. Build + record every error verbatim
cmake --build libslang-vk/build-i686 2>&1 | tee /tmp/slangvk-i686.log
```

## What to record in Results

- [ ] Configure result (header/feature errors at configure vs compile time)
- [ ] C++23 feature gaps on gcc-13 i686 (list each `-std=c++2b` diagnostic)
- [ ] Vendored volk + VMA compile clean? (32-bit pointer-width warnings)
- [ ] glslang (embedded preset compiler) — does it build i686? Size?
- [ ] Artifact: `libslangvk.a` size, exported C API present
      (`objdump -p` / `nm` for `slangvk_device_create`, `slangvk_chain_create`,
      `slangvk_chain_render_swapchain`)
- [ ] Smoke: headless slangvk_device_create on lavapipe x86? (lavapipe is
      64-bit in the sandbox — a 32-bit Linux build with -m32 may substitute)

## Fallbacks (in order, per PLAN.md P1)

1. **C++20 with pinned features** — if the C++23 usage is narrow, back-port
   the library build (keep the API identical).
2. **Reduced core** — 32-bit C shim around the subset glide3x needs
   (`slangvk_chain_*` only; skip editor/preview paths).
3. **clang for OUR DLL only** — llvm-mingw is fine for glide3xvk (fresh
   library code); the header-drift problem only affects the *game* tree,
   which keeps its mingw gcc build. Two toolchains, clean seam.

## Results

_(fill in)_
