#!/usr/bin/env bash
# vanilla-plus: host syntax check of the fork-owned TUs.
#
#   tools/syntax-check.sh [repo-root] [extra .cpp files...]
#
# Why: R70 shipped a compile break (an unqualified Engine::Gfx:: call that
# only resolves from TUs inside namespace D2Client) because every sandbox
# verifier was grep-based - the gates pin TEXT, not semantics. This script
# runs the fork-owned TUs through a C++ front end with the SAME flags as the
# Win11 build (tools/build-local-win11.ps1: -std=c++17 -w -fms-extensions,
# d2_platform.h + Redivivus/d2_hand.h force-included, CMake's D2_HEADERS
# include dirs) so a declaration/namespace/typo break dies here, before an
# overlay ships. It checks SEMANTICS of the fork code against the tree's
# headers; it does not produce objects and it is not a codegen check.
#
# Two host accommodations, both confined to THIS script - no repo file is
# edited:
#   1. tools/vp_host_shim.h is force-included FIRST on non-Windows hosts
#      (FILE guard reconciliation, calling-convention macros, errno_t, the
#      handful of Win32 SDK structs the decompiled headers import), and
#      tools/vp_host_api.h comes in AFTER d2_platform.h with the Win32
#      functions/constants the fork's own code calls that the non-Windows
#      path of d2_platform.h does not declare.
#   2. The tree's non-Windows path has one in-built inconsistency that has
#      never compiled anywhere: d2_platform.h typedefs
#      PRTL_CRITICAL_SECTION_DEBUG as void* while Storm/_unnamespaced.h (and
#      the Win32 SDK) say struct _RTL_CRITICAL_SECTION_DEBUG*. A TEMP SHADOW
#      COPY of d2_platform.h with that one line dropped is prepended to the
#      include path, so the SDK-faithful form from _unnamespaced.h stands.
#      The tree file on disk is untouched; the shadow lives in mktemp -d
#      (it also stubs <windows.h>, which two fork TUs include directly).
#
# Exit: 0 all TUs clean, 1 any TU failed, 2 no usable host compiler (the
# caller decides whether that is fatal; check-vanilla-plus.sh treats it as
# a skip so the gates stay runnable on a bare box).
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
# First arg is the repo root only if it IS one; otherwise every arg is a TU.
if [ "$#" -gt 0 ] && [ -d "$1" ]; then ROOT=$1; shift; fi
cd "$ROOT" || { echo "syntax-check: no such dir: $ROOT" >&2; exit 2; }

# Default: every fork-owned TU (the files the fork owns end to end - the
# merge playbook's "fork surface"). Extra args append one-off files.
TUS=(
    "D2Client/UI/VanillaPlusCore.cpp"
    "D2Client/UI/VanillaPlusOptions.cpp"
    "D2Game/PLAYER/VanillaPlusStash.cpp"
    "Redivivus/d2_crash.cpp"
)
if [ "$#" -gt 0 ]; then TUS=("$@"); fi

CXX=${CXX:-}
if [ -z "$CXX" ]; then
    for c in g++ clang++ c++; do
        if command -v "$c" >/dev/null 2>&1; then CXX="$c"; break; fi
    done
fi
if [ -z "$CXX" ] || ! command -v "$CXX" >/dev/null 2>&1; then
    echo "syntax-check: no host C++ compiler found (CXX, g++, clang++, c++)"
    exit 2
fi

# Include dirs exactly like the build: repo root plus every directory holding
# a header, minus what CMake filters out (vanilla-plus/cmake/sources.cmake):
# .git/, thirdparty/libbacktrace/ (vendored, own include set), asm_matching/
# (the msvc8 oracle - its shim_include would shadow <cstdint> with CRT
# headers a Linux host does not have), and any in-tree build dir.
mapfile -t INCDIRS < <(
    find . -name '*.h' \
        -not -path './.git/*' \
        -not -path './asm_matching/*' \
        -not -path './thirdparty/libbacktrace/*' \
        -not -path './build/*' \
        -printf '%h\n' | sort -u
)
INCS=("-I." "-I$ROOT")
for d in "${INCDIRS[@]}"; do INCS+=("-I$d"); done

# Temp shadow dir with the reconciled d2_platform.h (see header comment).
# Both the -I prepend and the force-include must point at the SAME copy:
# #pragma once is keyed on file identity, so the real + shadow files would
# otherwise both parse (D2Client.h and friends include d2_platform.h by
# name, which the -I prepend resolves to the shadow).
SHADOW=$(mktemp -d)
trap 'rm -rf "$SHADOW"' EXIT
# Drop d2_platform.h's one-line PRTL stub (conflicts with _unnamespaced.h)
# AND its one-line IMAGE_* stubs (layout-wrong for d2_crash.cpp's PE walk,
# which reads e_lfanew / FileHeader / section Characteristics) AND its one-line
# CONTEXT stub + LPCONTEXT typedef (vp_host_shim.h provides the tag-_CONTEXT
# alias the SDK spells; Fog/Debug.h completes the body). The faithful
# replacements live in vp_host_api.h / vp_host_shim.h.
sed -e '/typedef void\* PRTL_CRITICAL_SECTION_DEBUG;/d' \
    -e '/^struct IMAGE_DOS_HEADER { WORD e_magic; };$/d' \
    -e '/^struct IMAGE_NT_HEADERS { DWORD Signature; };$/d' \
    -e '/^struct IMAGE_SECTION_HEADER { char Name\[8\]; };$/d' \
    -e '/^struct CONTEXT { DWORD ContextFlags; };$/d' \
    -e '/^typedef CONTEXT\* LPCONTEXT;$/d' \
    Source/D2Headers/d2_platform.h > "$SHADOW/d2_platform.h"
# [tenth sync] upstream's headers include the platform header by its rooted
# post-restructure path ("Source/D2Headers/d2_platform.h"), which would bypass
# the shadow above and parse the stub-bearing real file a second time
# (redefinition of D2_IsNaN et al). Mirror the reconciled copy at the rooted
# path inside the shadow so -I$SHADOW (first on the search path) serves both
# spellings from one physical file.
mkdir -p "$SHADOW/Source/D2Headers"
cp "$SHADOW/d2_platform.h" "$SHADOW/Source/D2Headers/d2_platform.h"
# Some fork TUs include <windows.h> directly (VanillaPlusStash.cpp,
# d2_crash.cpp). A stub in the shadow (first on the include path) routes them
# to d2_platform.h's alias surface + vp_host_api.h's fill-in, both already
# force-parsed by then, and to Fog/Debug.h - the tree's OWN non-Windows
# fallback for the full exception/CONTEXT struct bodies. Anything those TUs
# use beyond that surfaces as an error and gets added to the shims - never
# hidden.
cat > "$SHADOW/windows.h" <<'EOF'
#pragma once
#include "d2_platform.h"
#include "Fog/Debug.h"
EOF
# d2_crash.cpp's module walker; its API surface lives in vp_host_api.h.
cat > "$SHADOW/tlhelp32.h" <<'EOF'
#pragma once
#include "d2_platform.h"
EOF
PLATFORM_HDR="$ROOT/Source/D2Headers/d2_platform.h"
if ! cmp -s Source/D2Headers/d2_platform.h "$SHADOW/d2_platform.h"; then
    INCS=("-I$SHADOW" "${INCS[@]}")
    PLATFORM_HDR="$SHADOW/d2_platform.h"
fi

# Same force-include chain and flags as the real build; -fsyntax-only keeps
# it parse+semantic only (seconds per TU, no objects, no linker).
FLAGS=(-fsyntax-only -std=c++17 -w -fms-extensions
       -include tools/vp_host_shim.h
       -include "$PLATFORM_HDR"
       -include tools/vp_host_api.h
       -include Redivivus/d2_hand.h)
if [ ! -f Redivivus/d2_hand.h ]; then
    FLAGS=(-fsyntax-only -std=c++17 -w -fms-extensions
           -include tools/vp_host_shim.h
           -include "$PLATFORM_HDR"
           -include tools/vp_host_api.h)
fi

FAILED=0
for tu in "${TUS[@]}"; do
    if [ ! -f "$tu" ]; then
        printf 'syntax %-30s MISSING\n' "$tu"
        FAILED=$((FAILED + 1))
        continue
    fi
    if err=$("$CXX" "${FLAGS[@]}" "${INCS[@]}" "$tu" 2>&1); then
        printf 'syntax %-30s ok\n' "$tu"
    else
        printf 'syntax %-30s FAILED\n' "$tu"
        # VP_SYNTAX_FULL=1 disables the 30-line cap: a big decompiled TU can
        # produce 30+ lines of 64-bit-host noise (precision casts, SDK gaps)
        # BEFORE a real parse error ("function-definition is not allowed")
        # further down - the cap hid exactly that class in the 1792 merge
        # (Rev 93). scripts/merge_site_syntax_sweep.sh always sets it; human
        # runs keep the readable cap.
        if [ "${VP_SYNTAX_FULL:-0}" = "1" ]; then
            printf '%s\n' "$err" | sed 's/^/    /'
        else
            printf '%s\n' "$err" | sed 's/^/    /' | head -30
        fi
        FAILED=$((FAILED + 1))
    fi
done

if [ "$FAILED" -eq 0 ]; then
    echo "syntax-check: ${#TUS[@]}/${#TUS[@]} TUs clean ($CXX)"
    exit 0
fi
echo "syntax-check: $FAILED FAILED - fix before shipping an overlay"
exit 1
