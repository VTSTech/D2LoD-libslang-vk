d2linux M1 CI round 3 — the gate reflects the toolchain, not add_definitions
(branch `linux`, applies on top of your round-2 commit, e.g. f71a415)
========================================================================

WHAT THIS IS
  Your round-2 log cracked the case. Read it back: the fail-fast verify
  step PASSED (plain `gcc -m32` compiles and links fine on the runner),
  the raw diagnosis probe SUCCEEDED (no FAILED marker, no ld errors) --
  and yet check_cxx_compiler_flag("-m32") said Failed. The module lied.

ROOT CAUSE (confirmed by inspecting the try_compile children)
  check_cxx_compiler_flag passes the flag through the child project's
  add_definitions() (Internal/CheckCompilerFlag sets
  CMAKE_REQUIRED_DEFINITIONS). add_definitions reaches the COMPILE step
  only: the child compiled a 32-bit object and then linked it against
  64-bit crt objects -- link.txt of the generated child literally has no
  -m32. That fails on every WORKING multilib toolchain. cmake 3.28.3
  (the runner) and 3.31.6 behave the same; it is not your toolchain, it
  never was. The CMAKE_FLAGS COMPILE_DEFINITIONS route seeds the child's
  CMAKE_CXX_FLAGS instead, which rides both compile and link -- that is
  the probe that succeeded in your own log.

THE FIX
  platform/linux/build.cmake -- the gate is the raw try_compile probe
  (compile+link, cached); check_cxx_compiler_flag is gone. When the
  toolchain genuinely cannot do i386, the FATAL_ERROR still carries the
  compiler's own verdict.

  .github/workflows/build.yml -- under i386, SDL now configures
  console-only up front (D2LINUX_SDL_FLAGS pass-through + the same flags
  in the in-tree artifact step). Runners have no :i386 X11 dev libraries,
  so the full-SDL attempt was a deterministic per-run failure that the
  console retry had to rescue every time; the smoke forces the SDL dummy
  drivers anyway. Real backend configure stays the default for local
  native-arch builds.

APPLY (from the repo root, on branch linux)
  1. unzip this overlay over the tree (paths are tree-relative).
  2. Commit as your own:
       git add -A && git commit
     suggested subject:
       "linux: M1 CI round 3 -- the -m32 gate reflects the toolchain,
        not add_definitions"

WHAT TO EXPECT FROM THE NEXT CI RUN
  - "Verify the i386 toolchain": OK (it already passed for you).
  - Configure: first attempt succeeds (console-only), no retry noise.
  - Build: the 44-TU i386 slice + shims + static SDL3 -> ELF 32-bit.
  - Smoke: `d2linux smoke OK (3 frames, dummy video/audio)` -- this is
    the first CI run that actually reaches and runs the i386 smoke, so
    this is the M1 exit ring: if anything in the slice trips at RUNTIME
    on the runner (it linked clean here), the smoke is where it shows.

VERIFICATION DONE (sandbox, this round)
  - Failure path: fresh clone with no multilib at all -> gate fails with
    the compiler's verdict (Scrt1.o/crti.o not found, skipping
    incompatible libstdc++/libm/libgcc), exit 1. The gate still catches
    genuinely broken environments.
  - Happy path: full 44-TU i386 build through the check script with the
    new console-first flow -> configure OK on the FIRST attempt, "compile
    errors: 0", ELF 32-bit statically linked. (The i386 --smoke run stays
    CI's ring; this container refuses static-i386 execution.)
  - Native arch end-to-end: configure -> build -> --smoke OK, exit 0.

NEXT (step 2, toward M2)
  globals.cpp + the boot chain (D2Launch/D2Client/D2Multi/D2Common) --
  the step-1 weak globals yield to the strong ones automatically. Then
  the Storm/MPQ wiring onto the M0 presentation surface (title screen
  offline). A green CI run on this push closes M1.
