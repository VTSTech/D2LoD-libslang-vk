# BUILD-32BIT.md — libslang-vk as 32-bit Windows (the P1 gate)

> Status: **SPIKE PASSED (2026-10-24, agent sandbox) — R1 RETIRED.** libslang-vk
> 0.4.06 builds as i686 Windows with the exact d2 toolchain: C++23 + gcc-13,
> zero source changes, zero fallbacks exercised. Everything downstream (linking
> libslang-vk into glide3x.dll, the P3 preset chain) is unblocked.

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

# 3. Configure 32-bit, static slangvk target, vendored deps (verified flags;
#    note: the static lib is the DEFAULT target — there is no SLANGVK_STATIC
#    option, disable the shared variant instead. CMAKE_BUILD_TYPE=Release is
#    REQUIRED — with an empty build type glslang emits Debug-postfixed archive
#    names (libSPIRVd.a) that break the ninja graph; see Results.)
cmake -S libslang-vk -B libslang-vk/build-i686 -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=D2LoD-libslang-vk/cmake/mingw-i686.cmake \
    -DD2_TOOL_PREFIX=d2w- \
    -DCMAKE_BUILD_TYPE=Release \
    -DSLANGVK_BUILD_SHARED=OFF -DSLANGVK_BUILD_TESTS=OFF \
    -DSLANGVK_BUILD_EXAMPLES=OFF -DSLANGVK_BUILD_BENCHMARKS=OFF

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

## Results (2026-10-24, agent sandbox)

**Verdict: PASS.** libslang-vk 0.4.06 (d208ac5) builds as i686 Windows with the
exact d2 toolchain — no source changes, no fallback needed.

| Checklist item | Result |
|---|---|
| Configure | Clean first try, 6.5 s incl. dep fetch. Two recipe corrections below. |
| C++23 gaps on gcc-13 i686 | **None.** All 76 targets compile at `c++23`, 0 errors, no fallback exercised. |
| Vendored volk + VMA | Clean. Static `libvolk.a` 205 KB; VMA impl TU fine; no 32-bit pointer-width complaints. |
| glslang (embedded) | 16.6.0 builds i686; auto-selects `OSDependent/Windows`; PCH auto-disabled on cross (fine). **2 warnings total** (`-Wmaybe-uninitialized` on `TType::longVector`/`coopvecNV` — known upstream noise, benign). Build: ~2 min for 76 targets (ninja). |
| Artifact | `libslangvk.a` **1,819,150 B**, members `pe-i386`; **27 exported `_slangvk_*` C symbols**, incl. `_slangvk_device_create`, `_slangvk_chain_create`, `_slangvk_chain_render_swapchain`, `_slangvk_preset_load`, `_slangvk_device_get_handles` (COFF keeps an internal leading underscore; the extern name is plain `slangvk_*` cdecl). |
| Smoke (link) | Consumer TU `spike/slangvk-i686-smoke.cpp` exercising the P3 present-pass pattern links against the full closure → `pei-i386` exe importing **`KERNEL32.dll` + `msvcrt.dll` only** (`-static`), the same import profile as `glide3x.dll`. Link-only in-sandbox: executing needs 32-bit Windows Vulkan (field test; wine smoke on the VM later). |

Static closure sizes (Release): `libglslang.a` 7.46 MB (in 16.6.0 the
MachineIndependent / GenericCodeGen / SPIRV archives are 800-byte stubs
forwarding into it), `libslangvk.a` 1.82 MB, `libvolk.a` 0.21 MB,
resource-limits 0.04 MB → smoke exe 8.47 MB fully static. Expect glide3x.dll to
grow by roughly this order once slangvk links in (P3) — still trivial next to a
348 MB d2.exe.

### Recipe corrections (vs. the draft above)

1. `-DSLANGVK_STATIC=ON` does not exist — the static `slangvk` target is the
   default; turn OFF `SLANGVK_BUILD_SHARED` (plus tests/examples/benchmarks).
2. `-DCMAKE_BUILD_TYPE=Release` is **required**: with an empty build type,
   glslang names its outputs with the Debug postfix (`libSPIRVd.a`) while the
   consumer graph expects `libSPIRV.a` → `ninja: error: ... missing and no
   known rule to make it`. Release also re-enables glslang PCH.

### What this unblocks / what stays open

- **Unblocked — and used (0.4.0):** P3 is wired — `glide3xvk/CMakeLists.txt`
  adds the sibling libslang-vk checkout with exactly these flags
  (`-DG3XVK_SLANGVK_DIR`, default `../libslang-vk`; fail-soft stub when the
  checkout is absent), and `g3x_slang.cpp` drives
  `slangvk_chain_render_swapchain` at the present pass. CI checks out
  `VTSTech/libslang-vk` pinned to `d208ac5`. The recipe below remains the
  authoritative way to reproduce/upgrade the linked build.
- **Open (field):** runtime behavior of the i686 artifact on real 32-bit
  Windows Vulkan — covered by the P1 field test (VTSTech, Win11 + RX 560) or a
  later wine smoke on the Ubuntu VM (sandbox lavapipe is 64-bit; a 32-bit
  Windows exe cannot run here). For the preset chain specifically: round 14
  exercises it on the field rig.
