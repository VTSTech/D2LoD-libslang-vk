d2linux - M2 fixups overlay: the banner tells the truth + the warnings go quiet
===============================================================================
on top of your "M2" commit (d38789a2); supersedes the first cut of this overlay (baked strings broke the build)

WHY THIS ROUND
--------------
Two things from your console capture:

1. The banner said "milestone 0" on every build you will ever run natively.
   That was the banner conflating two different facts into one token: the
   PROJECT stage and the BINARY shape. The i386 charter (PLAN.md R1) keeps
   all game TUs out of the 64-bit binary FOREVER - so "native arch - no game
   TUs" was describing your binary, but it was printed as "milestone 0",
   which reads as the port being stalled at M0. It never was: M0 (window),
   M1 (44-TU i386 slice, CI artifact) and M2 round 1 (the user32 surface -
   your --smoke's "user32 self-test OK" line is that round's gate) are all
   in your binary already.

2. The warning noise: FD_SETSIZE/FD_* "redefined" (5x per TU), ~40x
   "initialized and declared 'extern'" from the weak-string constants, and
   the -Wformat-truncation notes in the crash twin. All three are intended
   behavior or pure keyword hygiene - silenced at the source this round.

WHAT THE OVERLAY CHANGES (5 files)
----------------------------------
- platform/linux/build.cmake        D2LINUX_MILESTONE replaced by TWO defines:
                                    D2LINUX_PROJECT_STAGE (hand-written,
                                    BUMPED EVERY CODE ROUND - the linux twin
                                    of the PE-side version self-ID) and
                                    D2LINUX_BINARY_SHAPE (computed from arch
                                    + game TU count).
- platform/linux/d2_linux_main.cpp  PrintBanner: 3 lines now -
                                      d2linux scaffold <hash> | Vanilla+ native Linux host
                                      project: M2 round 1 - user32 surface real (...); next: DirectDraw ...
                                      binary: native x86-64 skeleton - game TUs are i386-only (charter R1); ...
                                    The second line is the one that answers
                                    "where are we"; it changes every round.
- platform/linux/d2_linux_api.h     the winsock FD_* macro map #undef's
                                    glibc's definitions first (the
                                    redefinition IS the intent; now it
                                    doesn't warn).
- platform/linux/d2_linux_globals_step1.cpp
                                    D2_WEAK_STR drops the pointless "extern"
                                    before the initializer (same external
                                    linkage, same weak symbol, no warning).
- platform/linux/d2_linux_crash.cpp -Wformat-truncation silenced around the
                                    two functions whose snprintf truncation
                                    is the SPEC (PE module fields are 256
                                    bytes; the behavior matches Windows).

ROUND 2 OF THIS OVERLAY (your build failure)
--------------------------------------------
The first cut baked the banner strings with ';' separators - CMake reads
';' as a LIST separator, so target_compile_definitions emitted broken -D
fragments on every TU ("missing terminating " character"). My probe tested
gcc quoting but not the CMake layer; that's on me. Fixed:

- banner strings now use ' / ' separators (no ';' anywhere)
- configure-time FATAL_ERROR guard: a banner string with ';' fails cmake
  with the reason, never the compiler

VERIFIED THIS TIME (with real cmake + GNU Makefiles, your generator)
--------------------------------------------------------------------
- cmake -S . -B /tmp/bannertest -G "Unix Makefiles" -DD2LINUX_ARCH=native
  configures; flags.make shows ONE well-formed -DD2LINUX_PROJECT_STAGE=...
  fragment per TU (no list split);
- the two TUs that failed on your box (d2_linux_main.cpp, d2_linux_audio.cpp)
  compile with ZERO warnings through the full forced-include chain;
- the compiled object carries the new banner strings;
- platform shadow regenerates clean (16 patches).

Your VM build stays the gate of record.

Your VM build is the gate of record, as usual.

APPLY
-----
From the repo root (branch linux):

  unzip -o d2linux-m2-banner-warnings-overlay.zip   # 5 files over their paths (overwrites the first cut)
  git add -A && git commit -m "linux: banner states project stage vs binary shape; warning families quiet"

RECONFIGURE, then build (the banner defines are baked at configure time):

  cmake -S . -B build -G Ninja -DD2LINUX_ARCH=native   # or your existing dir; cmake . works too
  cmake --build build
  ./build/d2linux            # rainbow window, new 3-line banner
  ./build/d2linux --smoke    # expect: banner + user32 self-test OK + smoke OK, zero warnings

NEXT (M2 round 2)
------------------
d2_linux_shim_ddraw.cpp - the DirectDraw COM shim: CreateSurface / Lock /
Unlock / Blt / Flip / SetPalette onto the Video_Attach staging surface.
First real game pixel on screen; the M0 rainbow producer retires. Then
Storm/MPQ routing and the boot-chain slice (globals.cpp step 2 + D2Launch/
D2Client). Exit per PLAN.md: title screen renders, menu navigable.
