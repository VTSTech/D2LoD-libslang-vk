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
    include(CheckCXXCompilerFlag)
    include(CheckCCompilerFlag)
    check_cxx_compiler_flag("-m32" D2LINUX_HAVE_M32_CXX)
    check_c_compiler_flag("-m32" D2LINUX_HAVE_M32_C)
    if(NOT D2LINUX_HAVE_M32_CXX OR NOT D2LINUX_HAVE_M32_C)
        message(FATAL_ERROR
            "d2linux: this toolchain cannot build -m32. Game code requires i386 "
            "(32-bit pointers, MSVC x86 ABI model): install the multilib "
            "toolchain (Debian/Ubuntu: gcc-multilib g++-multilib) or configure "
            "a 32-bit cross toolchain. -DD2LINUX_ARCH=native builds the host "
            "skeleton on the host arch only.")
    endif()
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
