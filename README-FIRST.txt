d2linux - M14 overlay: the banner states the milestone shape
============================================================
commit 323e1803 (on top of the m13 video diagnostics, f8820cbd)

WHY
---
You built arch=native, got the M0 rainbow window and asked "Where is
M1?" - fair, the banner said "milestone 0" on BOTH arches. M1's
deliverable is link-time: 44 game TUs (Fog/Storm foundation + PKWARE +
the Win32 shim layer) compile and link i386 behind the shims, and
nothing calls them until M2's boot chain. The screen cannot show it -
now the banner does:

  native build: milestone 0 (skeleton + shims, native arch - no game TUs)
  i386 build:   milestone 1 (skeleton + shims + game slice: 44 TUs, i386)

The TU count is computed from the build's own source list, so step 2
(growing the slice) updates the banner automatically.

FILES: platform/linux/build.cmake, platform/linux/d2_linux_main.cpp,
docs/linux-port.md

APPLY: copy over the tree on branch linux, commit, push.
  git add -A && git commit -m "linux: the banner states the milestone shape"

SEE M1 ON YOUR BOX
------------------
  sudo apt-get install gcc-multilib g++-multilib libc6-dev-i386
  git submodule update --init --depth 1 external/SDL   # REQUIRED for i386:
      # the distro SDL3 package is x86-64-only; a 32-bit link needs the
      # vendored SDL built 32-bit from source
  cmake -S . -B build-linux -G Ninja \
        -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF
      # console flags: no :i386 X11 dev libraries exist; the i386 binary
      # is for the link proof + CI, not a desktop window
  cmake --build build-linux -j --target d2linux
  file build-linux/d2linux        # ELF 32-bit LSB executable, statically linked
  ./build-linux/d2linux --smoke   # milestone 1 banner + smoke OK

WHAT CHANGES THE PICTURE
------------------------
M2 - title screen offline: MPQ loading via the vendored Storm, the
game's composed 8-bit frame replacing the M0 pattern producer on the
presentation surface you just watched, input shim. Then M3 - playable
offline. The ladder is docs/linux-port.md.

SANDBOX VERIFICATION
--------------------
- native rebuild banners milestone 0; --smoke OK
- full i386 sysroot build links static ELF 32-bit, 15.4 MB, milestone-1
  banner baked (strings-verified; the sandbox cannot run 32-bit)
- check script E2E green at 323e1803
