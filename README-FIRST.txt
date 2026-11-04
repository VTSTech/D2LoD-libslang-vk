d2linux - M13 overlay: the video failure is self-diagnosing
============================================================
commit f8820cbd (on top of the m12 gate-fix, 2ddd6675)

WHAT THIS FIXES
---------------
Your desktop run died with a bare "SDL_Init failed: No available video
device". Two different causes hide behind that line, and the old binary
could not tell them apart:

  1. The BINARY is console-only. CI's i386 artifact is built with
     SDL_X11=OFF SDL_WAYLAND=OFF by design (no :i386 X11/Wayland dev
     libraries exist), so its only video driver is SDL's "dummy" - it
     cannot open a window on ANY machine, desktop or not. Same for any
     build that went through check-build-linux.sh's console retry.
  2. The SHELL has no display session (SSH without -X, WSL without
     WSLg, a bare server console). The driver exists but there is no X
     server to connect to.

QUICK CHECKS ON YOUR BOX (before applying anything)
---------------------------------------------------
  echo "DISPLAY='$DISPLAY' WAYLAND_DISPLAY='$WAYLAND_DISPLAY' XDG_SESSION_TYPE='$XDG_SESSION_TYPE'"
  ldd ./d2linux | grep -Ei 'libX11|libwayland' || echo "no X11/Wayland linkage (console-only binary)"

If you ran the CI artifact: that is cause 1, guaranteed. If DISPLAY is
empty in that echo: cause 2. If DISPLAY is set AND ldd shows libX11:
install the dev set below, rebuild SDL (fresh build dir), rerun.

WHAT THE OVERLAY CHANGES
------------------------
- platform/linux/build.cmake   after the vendored SDL3 configures, cmake
  now reports the compiled video backends ("d2linux: SDL3 video
  backends (vendored): x11 offscreen dummy"); console-only-by-request
  is a neutral STATUS line, a misconfigured dummy-only state is a loud
  WARNING with the fix.
- platform/linux/d2_linux_main.cpp
  SDL_Init splits: video is fatal + self-diagnosing (drivers, session
  env, WSL note, one of four verdicts); audio is best-effort, so a
  soundcard-less box no longer dies inside a combined init. New flag:
      ./d2linux --list-video
  prints the compiled video/audio drivers + the session environment and
  exits - the first move for any display problem.
- platform/linux/d2_linux.h        HostOptions.list_video
- tools/check-build-linux.sh       every CI build logs its backend report
- docs/linux-port.md               "Troubleshooting: No available video
                                   device" section with the four verdicts

APPLY
-----
From the repo root (branch linux), with this zip's files over the tree:

  git apply --stat d2linux-m13-video-diagnostics.patch   # optional sanity
  # or just copy the 5 files over their paths, then:
  git add -A && git commit -m "linux: the video failure is self-diagnosing"

The check script now also runs --list-video, so apply BEFORE pushing if
you want CI's first round to be green on an up-to-date tree.

VERIFY (your box)
-----------------
  cmake -S . -B build-native -G Ninja -DD2LINUX_ARCH=native \
        -DCMAKE_BUILD_TYPE=Release     # fresh dir; install dev pkgs first if
                                       # the configure warns
  cmake --build build-native -j --target d2linux
  ./build-native/d2linux --list-video   # want x11 (or wayland) in the list
  ./build-native/d2linux                # wants a display session
  ./build-native/d2linux --smoke        # unchanged, exit 0

If --list-video still says only "offscreen dummy evdev": the dev
libraries were missing at configure time (see the cmake warning), or you
reused a stale build dir - rm -rf the build dir and reconfigure.

Dev set (docs/linux-port.md, Build):
  sudo apt-get install libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
    libxi-dev libxfixes-dev libxss-dev libxtst-dev libwayland-dev \
    libxkbcommon-dev libdecor-0-dev libegl1-mesa-dev libasound2-dev

NOTE: libxtst-dev (XTEST) is a HARD dependency of SDL3's X11 configure;
without it configure fatal-errors with "Couldn't find dependency package
for XTEST". libdecor-0-dev is SOFT (bare windows on GNOME Wayland
without it).

SANDBOX VERIFICATION (this exact commit)
----------------------------------------
- console-only configure: STATUS "offscreen dummy (console-only by
  request...)"; no warning (by design for CI)
- configure WITHOUT the flags on a partial-X11 box (ext headers
  extracted into a prefix): STATUS "x11 offscreen dummy"
- i386 configure via the sysroot recipe: -m32 probe OK, report fires,
  configure done
- runtime: --smoke exit 0 (both builds); --list-video exit 0 pre-init;
  console-only real run -> verdict "no desktop backend compiled in";
  x11 build without DISPLAY -> verdict "no display session"; dead
  DISPLAY=:99 -> verdict "force the backend"; broken audio driver ->
  "audio subsystem unavailable - continuing without", host keeps
  running
- tools/check-build-linux.sh repo HEAD: OK, backend report + smoke
  verdict in the log (banner f8820cbd)
