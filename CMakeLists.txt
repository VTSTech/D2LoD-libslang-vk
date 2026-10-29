# glide3xvk — our own glide3x.dll: Glide 3 on Vulkan, with the libslang-vk
# preset chain at present (see README.md).
#
# Standalone repo: this tree builds ONLY the wrapper DLL — the game just does
# LoadLibraryA("glide3x.dll") and resolves 45 stdcall exports; nothing else of
# it is needed here. Toolchain: cmake/mingw-i686.cmake (32-bit Windows only,
# deliberately: the whole point is loading inside the game's process).

cmake_minimum_required(VERSION 3.16)
project(glide3xvk LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "glide3xvk builds 32-bit Windows only — configure with "
        "-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake (or a native i686 mingw-w64 g++).")
endif()

# Build id for the log banner (same pattern as the game's REDIVIVUS_BUILD_ID).
if(NOT G3XVK_BUILD_ID)
    execute_process(COMMAND git describe --always --dirty --tags
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE G3XVK_BUILD_ID OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _g3xvk_describe_rc)
    if(NOT _g3xvk_describe_rc EQUAL 0 OR NOT G3XVK_BUILD_ID)
        set(G3XVK_BUILD_ID "unknown")
    endif()
endif()

# Short build id for strings with a length budget: the overlay tag line and
# toast draw it via the embedded 8x8 font, and the grGetString(GR_RENDERER)
# reply carries it. "glide3xvk 0.4.1 (d76cf3c-dirty)" is the worst-case shape
# at 30 chars. The long describe form ("0.3.1-6-gd76cf3c-dirty", 40 chars
# with the tag prefix) would overrun the toast/line budgets.
if(NOT G3XVK_BUILD_SHORT)
    execute_process(COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE G3XVK_BUILD_SHORT OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _g3xvk_short_rc)
    if(NOT _g3xvk_short_rc EQUAL 0 OR NOT G3XVK_BUILD_SHORT)
        set(G3XVK_BUILD_SHORT "unknown")
    elseif(G3XVK_BUILD_ID MATCHES "-dirty$")
        string(APPEND G3XVK_BUILD_SHORT "-dirty")
    endif()
endif()

set(G3XVK_SOURCES
    src/g3x_log.cpp
    src/g3x_state.cpp
    src/g3x_frame.cpp
    src/g3x_fxcap.cpp
    src/g3x_overlay.cpp
    src/g3x_slang.cpp
    src/g3x_shot.cpp
    src/g3x_hostver.cpp
    src/g3x_window.cpp
    src/g3x_controller.cpp
    src/vk_backend.cpp
    src/glide_exports.cpp
    vendor/volk/volk.c)

add_library(glide3x SHARED ${G3XVK_SOURCES})

# g3x_hostver reads the running exe's VERSIONINFO resource (GetFileVersionInfo
# family) — system import lib, no static-linking implications.

# ---- P3: libslang-vk (the preset chain at the present pass) -----------------
#
# Expects a libslang-vk checkout beside this repo (BUILD-32BIT.md recipe):
#   git clone https://github.com/VTSTech/libslang-vk ../libslang-vk
# Override the location with -DG3XVK_SLANGVK_DIR=<path>. When the checkout is
# missing, glide3x still builds — with the preset chain stubbed out (same API,
# always inactive) and a loud STATUS line here plus a log line at runtime.
set(G3XVK_SLANGVK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libslang-vk"
    CACHE PATH "libslang-vk checkout (static slangvk target gets linked)")
if(EXISTS "${G3XVK_SLANGVK_DIR}/CMakeLists.txt")
    # Normal (non-cache) sets: visible to add_subdirectory below, invisible to
    # the parent scope — the game tree's flags stay exactly as they were.
    set(SLANGVK_BUILD_SHARED OFF)
    set(SLANGVK_BUILD_TESTS OFF)
    set(SLANGVK_BUILD_EXAMPLES OFF)
    set(SLANGVK_BUILD_BENCHMARKS OFF)
    # glslang names its archives with the Debug postfix when the build type is
    # empty, which breaks the slangvk consumer graph (BUILD-32BIT.md); the
    # directory-scoped shadow below fixes that without touching the cache, so
    # a consumer configuring with an empty build type still links a Release
    # slangvk.
    set(CMAKE_BUILD_TYPE Release)
    add_subdirectory("${G3XVK_SLANGVK_DIR}" "${CMAKE_BINARY_DIR}/glide3xvk-slangvk" EXCLUDE_FROM_ALL)
    target_link_libraries(glide3x PRIVATE slangvk::slangvk)
    target_compile_definitions(glide3x PRIVATE G3XVK_HAVE_SLANGVK=1)
    message(STATUS "glide3xvk: libslang-vk at ${G3XVK_SLANGVK_DIR} — preset chain linked (P3)")
else()
    target_compile_definitions(glide3x PRIVATE G3XVK_HAVE_SLANGVK=0)
    message(STATUS "glide3xvk: libslang-vk NOT FOUND at ${G3XVK_SLANGVK_DIR} — "
        "building WITHOUT the preset chain (clone it beside the repo, see BUILD-32BIT.md)")
endif()

target_link_libraries(glide3x PRIVATE version)

set_target_properties(glide3x PROPERTIES
    OUTPUT_NAME "glide3x"
    PREFIX ""            # no libglide3x.dll
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"  # lands at the build dir root
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    C_STANDARD 99 C_EXTENSIONS ON)

target_include_directories(glide3x PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/volk"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/Vulkan-Headers/include")

target_compile_definitions(glide3x PRIVATE
    VK_USE_PLATFORM_WIN32_KHR
    WIN32_LEAN_AND_MEAN
    NOMINMAX)

# Exports: glide3x.def lists the literal decorated names the game resolves
# ("_grBufferSwap@4" — underscore INCLUDED). Two toolchain quirks to defeat:
#   1. GNU ld keeps def-file export names verbatim but searches symbols with an
#      extra leading '_' — solved by __asm__("__name@N") labels (one extra '_'
#      gets stripped when ld writes the table, see glide_exports.cpp).
#   2. CMake archives our objects (objects.a) and nothing references them, so
#      def lookups find nothing — solved by -Wl,-u flags forcing every symbol
#      undefined before the archive scan.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/glide3x.def" _g3x_def)
# Strip ';' comment lines first — the header comment cites "_grBufferSwap@4"
# as an example, which the naive regex would count as a 46th export.
string(REGEX REPLACE ";[^\n]*" "" _g3x_code "${_g3x_def}")
string(REGEX MATCHALL "_[a-zA-Z]+@[0-9]+" _g3x_syms "${_g3x_code}")
list(LENGTH _g3x_syms _g3x_n)
if(NOT _g3x_n EQUAL 45)
    message(FATAL_ERROR "glide3x.def must list exactly 45 exports (found ${_g3x_n})")
endif()
set(_g3x_force)
foreach(s IN LISTS _g3x_syms)
    string(SUBSTRING "${s}" 1 -1 _sym)          # grXxx@N
    list(APPEND _g3x_force "-Wl,-u,__${_sym}")  # force __grXxx@N undefined
endforeach()

target_link_options(glide3x PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/glide3x.def"
    ${_g3x_force}
    -static -static-libgcc -static-libstdc++)  # self-contained DLL, house style

# Our code may optimize freely — nothing else in this repo pins flags.
# volk.c stays at its own pace; -O2 everywhere is fine.
#
# P2.12 perf: -msse2 -mfpmath=sse. The i686 default is x87 for ALL float math
# (stack-based 80-bit, slow int conversions, no SIMD, no auto-vectorization)
# and the raster is float-heavy — every fill_span/raster_tri inner loop pays
# 1.5-3x. SSE2 is IEEE-754 single precision, deterministic, and universal on
# any rig that runs a Vulkan driver (Pentium 4 / Athlon 64, 2003+).
# glide3x target only: the game tree and the libslang-vk subbuild keep their
# own flags.
target_compile_options(glide3x PRIVATE -O2 -msse2 -mfpmath=sse -w)

set_source_files_properties(src/glide_exports.cpp PROPERTIES
    COMPILE_DEFINITIONS "G3XVK_BUILD_ID=\"${G3XVK_BUILD_ID}\"")
# The overlay tag line embeds the short build id (see g3x_overlay.cpp).
set_source_files_properties(src/g3x_overlay.cpp PROPERTIES
    COMPILE_DEFINITIONS "G3XVK_BUILD_ID=\"${G3XVK_BUILD_ID}\";G3XVK_BUILD_SHORT=\"${G3XVK_BUILD_SHORT}\"")
