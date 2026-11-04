# d2linux - native Linux build path (platform/linux).
#
# Included by the top-level CMakeLists when CMAKE_SYSTEM_NAME is Linux; the
# parent returns right after, so none of the Windows/PE (d2.exe) logic runs.
# The Windows line keeps its own charter, gates and CI job - this file is the
# native-Linux track (docs/linux-port.md).
#
#   cmake -S . -B build-linux -G Ninja                    # i386 (default)
#   cmake -S . -B build-linux -G Ninja -DD2LINUX_ARCH=native
#
# Arch: the game's decompiled logic assumes the 32-bit MSVC x86 ABI
# (d2_platform.h models it; byte-matched struct offsets carry embedded
# pointers), so i386 is the ship target for game code. `native` exists so the
# host skeleton can be built without a multilib toolchain (CI, sandbox
# verification) - until game TUs are included (M1), both produce the same
# host-layer binary.
if(NOT DEFINED D2LINUX_ARCH)
    set(D2LINUX_ARCH "i386" CACHE STRING "d2linux arch: i386 (charter) or native")
endif()

message(STATUS "d2linux: arch=${D2LINUX_ARCH}")

if(D2LINUX_ARCH STREQUAL "i386")
    # The charter gate (PLAN.md §7 R1): a real -m32 compile+link probe.
    # NOT check_cxx_compiler_flag: it routes the flag through the child's
    # add_definitions() (Internal/CheckCompilerFlag sets
    # CMAKE_REQUIRED_DEFINITIONS), which reaches the COMPILE step only - the
    # child then links a 32-bit object against 64-bit crt objects and
    # false-fails on every working multilib toolchain (observed on
    # ubuntu-24.04 / cmake 3.28.3 / gcc 13.3, CI rounds 2026-11-04: the gate
    # failed while a plain `gcc -m32` linked fine in the very same job). The
    # CMAKE_FLAGS COMPILE_DEFINITIONS route used here seeds the child's
    # CMAKE_CXX_FLAGS instead, which rides both compile and link - the ground
    # truth. The game TUs are all C++; the SDL C TUs share this toolchain.
    if(NOT DEFINED D2LINUX_HAVE_M32)
        file(WRITE "${CMAKE_BINARY_DIR}/d2linux_m32_probe.cpp"
            "int main() { return 0; }\n")
        try_compile(D2LINUX_HAVE_M32
            "${CMAKE_BINARY_DIR}/d2linux_m32_probe_build"
            SOURCES "${CMAKE_BINARY_DIR}/d2linux_m32_probe.cpp"
            CMAKE_FLAGS "-DCOMPILE_DEFINITIONS:STRING=-m32"
            OUTPUT_VARIABLE D2LINUX_M32_LOG)
        set(D2LINUX_HAVE_M32 "${D2LINUX_HAVE_M32}" CACHE INTERNAL
            "d2linux: -m32 compile+link probe")
    endif()
    if(NOT D2LINUX_HAVE_M32)
        # Show the compiler's own verdict (crt1.o vs 32-bit headers vs
        # libgcc), not a bare flag result.
        string(REPLACE "\n" "\n    " _d2l_m32_tail "${D2LINUX_M32_LOG}")
        string(LENGTH "${_d2l_m32_tail}" _d2l_m32_len)
        if(_d2l_m32_len GREATER 2200)
            math(EXPR _d2l_m32_cut "${_d2l_m32_len} - 2200")
            string(SUBSTRING "${_d2l_m32_tail}" ${_d2l_m32_cut} 2200 _d2l_m32_tail)
        endif()
        message(FATAL_ERROR
            "d2linux: this toolchain cannot build/link -m32. Game code requires "
            "i386 (32-bit pointers, MSVC x86 ABI model, PLAN.md §7 R1). The "
            "compiler's own verdict:\n\n    ${_d2l_m32_tail}\n\n"
            "Fix (Debian/Ubuntu): sudo apt-get install gcc-multilib g++-multilib "
            "libc6-dev-i386, or configure a 32-bit cross toolchain. "
            "-DD2LINUX_ARCH=native builds the host skeleton on the host arch "
            "only (no game TUs).")
    endif()
    message(STATUS "d2linux: -m32 compile+link probe OK")
    add_compile_options(-m32)
    add_link_options(-m32)
elseif(NOT D2LINUX_ARCH STREQUAL "native")
    message(FATAL_ERROR "d2linux: D2LINUX_ARCH must be 'i386' or 'native', got '${D2LINUX_ARCH}'")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build id (same scheme as the PE line's d2_crash bake): short hash baked at
# configure time, overridable for local builds without git.
if(NOT D2LINUX_BUILD_ID)
    execute_process(COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE D2LINUX_BUILD_ID OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _d2l_git_rc)
    if(NOT _d2l_git_rc EQUAL 0 OR NOT D2LINUX_BUILD_ID)
        set(D2LINUX_BUILD_ID "unknown")
    endif()
endif()

# ---------------------------------------------------------------------------
# [d2linux] Which video backends did SDL actually configure? The answer is in
# the generated SDL_build_config.h (SDL_VIDEO_DRIVER_*), and it is the
# difference between a binary that opens a window and one that only ever
# smokes headless. CI's i386 job is console-only BY DESIGN (no :i386 X11/
# Wayland dev libraries exist); a local desktop build that lands dummy-only
# is missing the dev libraries - say so at configure time, not at first
# SDL_Init. The runtime twin of this report is `./d2linux --list-video`.
# (System-package SDL3 is skipped: its backend set is the packager's choice
# and the config header's location varies per distro; --list-video reports
# it just as well at runtime.)
# ---------------------------------------------------------------------------
function(d2linux_report_video_backends _sdl3_binary_dir _context)
    # SDL writes the config in two steps (CMakeLists: configure_file the
    # intermediate during configure, file(GENERATE) the final per-config
    # copy during generate) - at report time only the intermediate exists.
    # The recursive glob is the fallback for an SDL that changes the scheme.
    set(_d2l_cfg_h "${_sdl3_binary_dir}/CMakeFiles/SDL_build_config.h.intermediate")
    if(NOT EXISTS "${_d2l_cfg_h}")
        file(GLOB_RECURSE _d2l_cfgs "${_sdl3_binary_dir}/SDL_build_config.h")
        if(NOT _d2l_cfgs)
            return()
        endif()
        list(GET _d2l_cfgs 0 _d2l_cfg_h)
    endif()
    file(READ "${_d2l_cfg_h}" _d2l_cfg)
    set(_d2l_found "")
    foreach(_b X11 WAYLAND KMSDRM VIVANTE DIRECTFB RPI OFFSCREEN DUMMY)
        if(_d2l_cfg MATCHES "#define SDL_VIDEO_DRIVER_${_b} 1")
            string(TOLOWER "${_b}" _d2l_bl)
            list(APPEND _d2l_found "${_d2l_bl}")
        endif()
    endforeach()
    if(NOT _d2l_found)
        return()
    endif()
    list(JOIN _d2l_found " " _d2l_found_s)
    list(FIND _d2l_found "x11" _d2l_i_x11)
    list(FIND _d2l_found "wayland" _d2l_i_wl)
    if(_d2l_i_x11 GREATER -1 OR _d2l_i_wl GREATER -1)
        message(STATUS "d2linux: SDL3 video backends (${_context}): ${_d2l_found_s}")
    elseif(NOT SDL_X11 AND NOT SDL_WAYLAND)
        # Both desktop options were explicitly turned off (CI's i386 job, the
        # check script's console retry): by request, not a misconfiguration.
        message(STATUS "d2linux: SDL3 video backends (${_context}): ${_d2l_found_s} "
            "(console-only by request: SDL_X11=OFF SDL_WAYLAND=OFF - the binary "
            "runs --smoke headless only)")
    else()
        message(WARNING
            "d2linux: SDL3 configured WITHOUT a desktop video backend "
            "(${_context}; compiled: ${_d2l_found_s}). d2linux builds but only "
            "ever runs headless: ./d2linux dies with 'No available video device' "
            "on any machine. If a desktop run is wanted: install the X11/Wayland "
            "dev set (docs/linux-port.md, Build) and reconfigure on a FRESH "
            "build dir (rm -rf), or pass the console flags knowingly "
            "(-DSDL_X11=OFF -DSDL_WAYLAND=OFF, what CI does).")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# SDL3. Resolution order:
#   1. -DD2_SDL3_DIR=<path>  - a checkout to build in-tree (sandbox/verify)
#   2. external/SDL          - the git submodule (preferred; see docs)
#   3. find_package(SDL3)    - system package (Debian trixie+, current Fedora/Arch)
# Static preferred: the PE line ships self-contained exes; d2linux follows
# the same ethos. SDL is zlib-licensed, so static linking is unrestricted.
# ---------------------------------------------------------------------------
set(D2_SDL3_SOURCE "")
if(D2_SDL3_DIR AND EXISTS "${D2_SDL3_DIR}/CMakeLists.txt")
    set(D2_SDL3_SOURCE "${D2_SDL3_DIR}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/SDL/CMakeLists.txt")
    set(D2_SDL3_SOURCE "${CMAKE_SOURCE_DIR}/external/SDL")
endif()

if(D2_SDL3_SOURCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${D2_SDL3_SOURCE}" "${CMAKE_BINARY_DIR}/SDL3" EXCLUDE_FROM_ALL)
    if(TARGET SDL3::SDL3-static)
        set(D2_SDL3_TARGET SDL3::SDL3-static)
    else()
        set(D2_SDL3_TARGET SDL3::SDL3)
    endif()
    message(STATUS "d2linux: SDL3 vendored from ${D2_SDL3_SOURCE} -> ${D2_SDL3_TARGET}")
    d2linux_report_video_backends(
        "${CMAKE_BINARY_DIR}/SDL3" "vendored")
else()
    find_package(SDL3 CONFIG QUIET)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR
            "d2linux: no SDL3 found. Vendor it as the submodule (preferred):\n"
            "  git submodule add --depth 1 -b release-3.4.18 "
            "https://github.com/libsdl-org/SDL external/SDL\n"
            "or point -DD2_SDL3_DIR= at a checkout, or install the system "
            "package (Debian/Ubuntu: libsdl3-dev; Fedora: SDL3-devel).")
    endif()
    set(D2_SDL3_TARGET SDL3::SDL3)
    message(STATUS "d2linux: SDL3 system package -> ${D2_SDL3_TARGET}")
endif()

add_executable(d2linux
    platform/linux/d2_linux_main.cpp
    platform/linux/d2_linux_video.cpp
    platform/linux/d2_linux_audio.cpp)
target_compile_features(d2linux PRIVATE cxx_std_17)
target_compile_definitions(d2linux PRIVATE D2LINUX_BUILD_ID="${D2LINUX_BUILD_ID}")
target_include_directories(d2linux PRIVATE "${CMAKE_SOURCE_DIR}/platform/linux")
target_link_libraries(d2linux PRIVATE ${D2_SDL3_TARGET})
target_compile_options(d2linux PRIVATE -Wall -Wextra)

# ---------------------------------------------------------------------------
# [d2linux M1] game logic joins the build (PLAN.md §5 M1): the Fog/Storm
# foundation slice + PKWARE, compiled i386 against the shim layer. The slice
# list is explicit, not globbed — the plan grows it 5-10 TUs per step, and an
# explicit list is the step ledger. Each family has its named call sites in
# the slice (inventory: platform/linux/INVENTORY.md).
#   step 1 (this list): Fog foundation (memory/log/crash/registry/file),
#   Storm foundation (registry/compression/system/window), PKWARE, the
#   globals TU, the PE line's portable CRT impl/support, and the shim layer.
#   step 2: the boot chain (D2Launch/D2Client/D2Multi/D2Common) toward M2.
#
# i386-only: game TUs assume the 32-bit MSVC x86 ABI (PLAN.md §7 R1 — the
# decomp's casts treat pointers as 32-bit ints); a 64-bit build both fails to
# compile and would corrupt game state. The `native` arch therefore builds the
# host skeleton + shim layer only (the M0 shape), as the charter says.
# ---------------------------------------------------------------------------
set(D2LINUX_GAME_SOURCES
    PKWARE/PKWARE.cpp
    Fog/SMem.cpp
    Fog/Memory.cpp
    Fog/BitBuffer.cpp
    Fog/File.cpp
    Fog/Fog.cpp
    Fog/Save.cpp
    Fog/Debug.cpp
    Fog/Src/DataArrays.cpp
    Fog/Src/ErrorManager.cpp
    Fog/Src/LogManager.cpp
    Fog/Src/String/FogString.cpp
    Fog/Src/AsyncData.cpp
    Fog/Src/Safesock.cpp
    Fog/Source/SFile.cpp
    Fog/Source/SSComp.cpp
    Storm/Source/SSignature.cpp
    Fog/3rdparty/crashy.cpp
    Fog/3rdparty/crashy/Report.cpp
    Storm/Source/SReg.cpp
    Fog/Source/SSignature.cpp
    Storm/Source/SErr.cpp
    Storm/Source/SComp.cpp
    Storm/Source/SBig.cpp
    Storm/Source/SMem.cpp
    Storm/Source/SSTR.cpp
    Storm/Source/SSystem.cpp
    Storm/Source/SEVT.cpp
    Storm/Source/SLog.cpp
    Storm/Source/SFile.cpp
    Storm/WindowHandle.cpp
    Storm/3rdparty/zlib/adler32.cpp
    Storm/3rdparty/zlib/crc32.cpp
    Storm/3rdparty/zlib/deflate.cpp
    Storm/3rdparty/zlib/inffast.cpp
    Storm/3rdparty/zlib/inflate.cpp
    Storm/3rdparty/zlib/inftrees.cpp
    Storm/3rdparty/zlib/trees.cpp
    Storm/3rdparty/zlib/uncompr.cpp
    Storm/3rdparty/zlib/zutil.cpp
    support/d2_crt_impl.cpp
    support/d2_crt_support.cpp
    globals/Fog.cpp
    globals/Storm.cpp
    # globals/globals.cpp is STEP 2: the 39K-line central-data TU embeds the
    # game's command dispatch tables (D2Client::CMD_* and friends), whose
    # closure is the whole client. Step 1 links without it; the few symbols
    # the foundation slice externs (ExceptionList, the s_* string constants)
    # get weak definitions in platform/linux/d2_linux_globals_step1.cpp that
    # yield to globals.cpp's strong ones when step 2 brings it in.
)

# The shim layer (this track's code) and the crash twin.
set(D2LINUX_SHIM_SOURCES
    platform/linux/d2_linux_shim_win32.cpp
    platform/linux/d2_linux_shim_registry.cpp
    platform/linux/d2_linux_globals_step1.cpp
    platform/linux/d2_linux_shim_crt.cpp
    platform/linux/d2_linux_crash.cpp
)

# The platform shadow: d2_platform.h with the non-Windows stubs that clash
# with the SDK-faithful compat layer removed (tools/syntax-check.sh's pattern;
# the patch list lives in the script). Generated at configure time so an
# upstream d2_platform.h regen re-applies the patches automatically.
set(D2LINUX_PLATFORM_SHADOW_DIR "${CMAKE_BINARY_DIR}/platform-shadow")
find_program(D2LINUX_SH sh)
add_custom_command(
    OUTPUT "${D2LINUX_PLATFORM_SHADOW_DIR}/d2_platform.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${D2LINUX_PLATFORM_SHADOW_DIR}"
    COMMAND ${D2LINUX_SH} "${CMAKE_SOURCE_DIR}/platform/linux/mk-platform-shadow.sh"
            "${CMAKE_SOURCE_DIR}" "${D2LINUX_PLATFORM_SHADOW_DIR}"
    DEPENDS "${CMAKE_SOURCE_DIR}/d2_platform.h"
            "${CMAKE_SOURCE_DIR}/platform/linux/mk-platform-shadow.sh"
    COMMENT "d2linux: generating the platform shadow (d2_platform.h patches)"
    VERBATIM)
add_custom_target(d2linux_platform_shadow
    DEPENDS "${D2LINUX_PLATFORM_SHADOW_DIR}/d2_platform.h")

# Symbolization note: the vendored libbacktrace (thirdparty/libbacktrace) is
# the PE line's i686-mingw configuration (pecoff.c, hand-written config.h) and
# carries no ELF backend. The crash twin symbolizes through dladdr() against
# the -rdynamic executable instead; an ELF libbacktrace configuration (its own
# config.h + elf.c vendored) is the documented upgrade when the report needs
# inlined/frame-precise DWARF names.

# ---------------------------------------------------------------------------
# Game TU flags: the PE charter's compile shape (-w, -fms-extensions, the
# forced-include chain), plus the linux additions (-fshort-wchar: MSVC's
# 16-bit wchar_t is part of the ABI the decomp models). D2LINUX_GAME marks
# the guarded decomp edits (BnSend.h, d2_crt_impl.cpp, d2_crt_support.cpp).
# ---------------------------------------------------------------------------
target_sources(d2linux PRIVATE ${D2LINUX_SHIM_SOURCES})
if(D2LINUX_ARCH STREQUAL "i386")
    target_sources(d2linux PRIVATE ${D2LINUX_GAME_SOURCES})
else()
    message(STATUS "d2linux: arch=native - game TUs stay out (i386 charter, PLAN.md R1); "
                   "the skeleton + shim layer build and the smoke runs as in M0")
endif()

# The shadow dir leads the include path: game headers say #include
# "d2_platform.h" and must resolve to the SAME file the forced include used
# (#pragma once dedupes per path; the tree file would double-parse).
set(D2LINUX_INCLUDE_DIRS "${D2LINUX_PLATFORM_SHADOW_DIR}" "${CMAKE_SOURCE_DIR}")
file(GLOB_RECURSE D2LINUX_GAME_HEADERS CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    "${CMAKE_SOURCE_DIR}/*.h")
foreach(h IN LISTS D2LINUX_GAME_HEADERS)
    string(FIND "${h}" "${CMAKE_SOURCE_DIR}/asm_matching/" _in_asm)
    string(FIND "${h}" "${CMAKE_SOURCE_DIR}/thirdparty/" _in_bt)
    string(FIND "${h}" "${CMAKE_SOURCE_DIR}/vanilla-plus/" _in_vp)
    string(FIND "${h}" "${CMAKE_SOURCE_DIR}/external/" _in_ext)
    string(FIND "${h}" "${CMAKE_SOURCE_DIR}/platform/" _in_plat)
    if(NOT _in_asm EQUAL 0 AND NOT _in_bt EQUAL 0 AND NOT _in_vp EQUAL 0
       AND NOT _in_ext EQUAL 0 AND NOT _in_plat EQUAL 0)
        get_filename_component(_dir "${h}" DIRECTORY)
        list(APPEND D2LINUX_INCLUDE_DIRS "${_dir}")
    endif()
endforeach()
list(REMOVE_DUPLICATES D2LINUX_INCLUDE_DIRS)
target_include_directories(d2linux PRIVATE ${D2LINUX_INCLUDE_DIRS})

set(D2LINUX_GAME_FLAGS -w -fms-extensions -fshort-wchar)
# The forced-include chain rides target_compile_options (which honors the
# SHELL: prefix — set_property(SOURCE COMPILE_OPTIONS) would pass the spaces
# through Ninja's response files as literal quotes). It applies to every
# d2linux source: the host skeleton already lives with d2_platform's shapes
# (the M0 binary did), and the FILE-guard trick in the compat header keeps
# glibc's stdio compatible everywhere.
target_compile_options(d2linux PRIVATE
    "SHELL:-include ${CMAKE_SOURCE_DIR}/platform/linux/d2_linux_compat.h"
    "SHELL:-include ${D2LINUX_PLATFORM_SHADOW_DIR}/d2_platform.h"
    "SHELL:-include ${CMAKE_SOURCE_DIR}/tools/vp_host_api.h"
    "SHELL:-include ${CMAKE_SOURCE_DIR}/platform/linux/d2_linux_api.h")

foreach(src IN LISTS D2LINUX_GAME_SOURCES)
    set_property(SOURCE "${src}" PROPERTY COMPILE_OPTIONS
        ${D2LINUX_GAME_FLAGS} ${D2LINUX_FORCED_INCLUDES})
    set_property(SOURCE "${src}" PROPERTY COMPILE_DEFINITIONS D2LINUX_GAME=1)
    set_property(SOURCE "${src}" PROPERTY OBJECT_DEPENDS
        "${D2LINUX_PLATFORM_SHADOW_DIR}/d2_platform.h"
        "${CMAKE_SOURCE_DIR}/platform/linux/d2_linux_compat.h"
        "${CMAKE_SOURCE_DIR}/tools/vp_host_api.h"
        "${CMAKE_SOURCE_DIR}/platform/linux/d2_linux_api.h")
endforeach()

# The shim layer compiles with the same forced includes (its declarations live
# there) but keeps -Wall -Wextra: this is house code, not decomp.
foreach(src IN LISTS D2LINUX_SHIM_SOURCES)
    set_property(SOURCE "${src}" PROPERTY COMPILE_OPTIONS ${D2LINUX_FORCED_INCLUDES})
    set_property(SOURCE "${src}" PROPERTY OBJECT_DEPENDS
        "${D2LINUX_PLATFORM_SHADOW_DIR}/d2_platform.h"
        "${CMAKE_SOURCE_DIR}/platform/linux/d2_linux_compat.h"
        "${CMAKE_SOURCE_DIR}/tools/vp_host_api.h"
        "${CMAKE_SOURCE_DIR}/platform/linux/d2_linux_api.h")
endforeach()
target_compile_definitions(d2linux PRIVATE D2LINUX_GAME=1)

# support/d2_crt_impl.cpp + d2_crt_support.cpp are listed in the game sources;
# d2_crt_impl's D2LINUX_GAME-gated filter block needs the define too (already
# set target-wide above). The asm TU the PE build carries (d2_link_support.s)
# is mingw-only: not in the linux source list.

add_dependencies(d2linux d2linux_platform_shadow)
target_link_libraries(d2linux PRIVATE pthread dl)
# -rdynamic: dladdr() resolves the game's own symbols in the crash report.
target_link_options(d2linux PRIVATE -rdynamic)
