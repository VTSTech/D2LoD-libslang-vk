d2linux M1 — the shim layer round (branch `linux`, applies on 9aca9569)
========================================================================

WHAT THIS IS
  M1 of the native-Linux port (PLAN.md §5): the foundation slice of game TUs
  (44 files — Fog/Storm foundation + PKWARE) compiles and links i386 behind
  the shim layer. Sandbox-verified: static-i386 link green, native smoke
  green, PE gates 380/380. The i386 --smoke run is CI's ring — push and read
  the linux-native job.

APPLY (from the repo root, on branch linux @ 9aca9569)
  1. unzip this overlay over the tree (paths are tree-relative).
  2. The build-time script needs its exec bit recorded (git modes cannot
     travel through zips — PLAN.md §0):
       git update-index --chmod=+x platform/linux/mk-platform-shadow.sh
     CI is unaffected either way (build.cmake invokes it via `sh`).
  3. Commit as your own (house convention):
       git add -A
       git commit   # suggested subject:
                    # "linux: d2linux M1 — game TUs compile + link i386
                    #  behind the shim layer"

BUILD (locally)
  git submodule update --init --depth 1 external/SDL   # once
  sudo apt-get install gcc-multilib g++-multilib       # M1's i386 charter
  cmake -S . -B build-linux -G Ninja -DD2LINUX_ARCH=i386
  cmake --build build-linux -j --target d2linux
  ./build-linux/d2linux --smoke

WHAT CHANGED
  NEW  platform/linux/: d2_linux_compat.h (forced prelude), d2_linux_api.h
       (the M1 surface), mk-platform-shadow.sh + configure-time shadow
       (12 d2_platform.h stub patches), shim_win32/shim_registry/shim_crt
       (the shim layer), d2_linux_crash.cpp (the crash twin),
       d2_linux_globals_step1.cpp (STEP-1 weak globals -> globals.cpp at
       step 2), INVENTORY.md (the inventory pass + ledger)
  MOD  platform/linux/build.cmake (the slice list + flags + CI arch),
       .github/workflows/build.yml (i386 + multilib), PLAN.md +
       docs/linux-port.md (M1 status), tools/vp_host_api.h (extern "C" wrap)
  EDIT (rule-1 guarded, PE-identical): Bnclient/BnSend.h, support/d2_crt_impl.cpp,
       support/d2_crt_support.cpp, D2Net/in_addr.h, Fog/Src/Safesock.h

VERIFICATION DONE
  - the 44-TU slice compiles + links i386: build-linux/d2linux, ELF 32-bit,
    statically linked
  - native (skeleton + shims) build: d2linux --smoke OK, exit 0
  - PE gates: 380/380 (tools/check-vanilla-plus.sh)
  - CI: linux-native now builds i386 + multilib and smokes the charter binary

NEXT (step 2, toward M2)
  globals.cpp + the boot chain (D2Launch/D2Client/D2Multi/D2Common) — the
  step-1 weak globals yield to the strong ones automatically. Then the
  Storm/MPQ wiring onto the M0 presentation surface (title screen offline).
