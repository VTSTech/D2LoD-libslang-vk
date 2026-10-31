#!/usr/bin/env bash
# Build the d2linux native host from a clean clone, the way a stranger would,
# and smoke it headless.
#
#   tools/check-build-linux.sh [repo-url-or-path] [ref]
#
# Mirror of tools/check-build.sh for the linux track (docs/linux-port.md).
# Needs only public tools: git, cmake, ninja (or make), and a C++17 compiler.
# Builds with -DD2LINUX_ARCH=native by default (host skeleton; no multilib
# needed). When the game-code milestone switches to i386, run with
# D2LINUX_ARCH=i386 and gcc-multilib installed. Exits 0 when the build
# produced d2linux and its headless --smoke passed under the SDL dummy
# video/audio drivers.
set -euo pipefail

SRC=${1:-$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)}
REF=${2:-HEAD}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

for tool in git cmake; do
  command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 2; }
done
GENERATOR="Unix Makefiles"
command -v ninja >/dev/null && GENERATOR=Ninja
JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

git clone -q "$SRC" "$WORK/src"
git -C "$WORK/src" checkout -q "$REF"
# A plain clone does not fetch submodule content; the vendored SDL3 gitlink
# (external/SDL) is fetched from the URL recorded in .gitmodules.
if [ -f "$WORK/src/.gitmodules" ] && [ ! -f "$WORK/src/external/SDL/CMakeLists.txt" ]; then
  git -C "$WORK/src" submodule update --init --depth 1 external/SDL
fi
echo "source: $SRC @ $(git -C "$WORK/src" rev-parse --short HEAD)"
echo "compiler: $(c++ --version | head -1)"

# SDL3 hard-errors at configure time when a requested backend's dev
# libraries are absent (sdlchecks.cmake). CI installs the X11/Wayland dev
# set (build.yml); in bare environments (containers, sandboxes) the first
# configure fails and we retry console-only, which keeps the dummy drivers
# the smoke uses. D2LINUX_SDL_FLAGS passes extra SDL options through.
SDL_CONSOLE="-DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF"

if ! cmake -S "$WORK/src" -B "$WORK/build" -G "$GENERATOR" \
     -DD2LINUX_ARCH="${D2LINUX_ARCH:-native}" ${D2LINUX_SDL_FLAGS:-} \
     >"$WORK/configure.log" 2>&1; then
  echo "configure failed; retrying SDL console-only (dummy drivers)"
  cmake -S "$WORK/src" -B "$WORK/build" -G "$GENERATOR" \
    -DD2LINUX_ARCH="${D2LINUX_ARCH:-native}" ${D2LINUX_SDL_FLAGS:-} $SDL_CONSOLE \
    >"$WORK/configure.log" 2>&1 || { cat "$WORK/configure.log"; exit 1; }
fi

set +e
cmake --build "$WORK/build" -j "$JOBS" --target d2linux >"$WORK/build.log" 2>&1
RC=$?
set -e

ERRORS=$(grep -c ' error: ' "$WORK/build.log" || true)
echo "compile errors: $ERRORS"

if [ "$RC" -ne 0 ] || [ ! -f "$WORK/build/d2linux" ]; then
  { grep -E ' error: |undefined reference' "$WORK/build.log" || tail -40 "$WORK/build.log"; } | head -40 >&2
  echo "FAIL: no d2linux" >&2
  exit 1
fi

if command -v file >/dev/null; then
  file "$WORK/build/d2linux" | sed 's/^/binary: /'
fi

# Headless smoke: --smoke forces the SDL dummy video/audio drivers itself;
# the exit code and the verdict line are the check.
if ! "$WORK/build/d2linux" --smoke >"$WORK/smoke.log" 2>&1; then
  tail -40 "$WORK/smoke.log" >&2
  echo "FAIL: d2linux --smoke exited non-zero" >&2
  exit 1
fi
head -2 "$WORK/smoke.log" | sed 's/^/smoke: /'
grep -q 'd2linux smoke OK' "$WORK/smoke.log" \
  || { echo "FAIL: no smoke verdict line" >&2; exit 1; }
echo "OK: d2linux built and smoke passed"
