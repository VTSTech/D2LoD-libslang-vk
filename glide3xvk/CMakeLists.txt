# glide3xvk — our own glide3x.dll: Glide 3 on Vulkan, with the libslang-vk
# preset chain at present (P1 scaffold: device + swapchain + clear-black
# present; see README.md and PLAN.md sections 0/4/5).
#
# Built as a separate SHARED target — the game tree stays untouched. The root
# CMakeLists excludes this directory from the d2 glob and adds the subdirectory
# when present. Same toolchain as the game: cmake/mingw-i686.cmake (32-bit
# Windows only, deliberately: the whole point is loading inside d2.exe).

cmake_minimum_required(VERSION 3.16)
project(glide3xvk LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "glide3xvk builds 32-bit Windows only — configure with "
        "-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake (same as d2).")
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

set(G3XVK_SOURCES
    src/g3x_log.cpp
    src/g3x_state.cpp
    src/vk_backend.cpp
    src/glide_exports.cpp
    vendor/volk/volk.c)

add_library(glide3x SHARED ${G3XVK_SOURCES})

set_target_properties(glide3x PROPERTIES
    OUTPUT_NAME "glide3x"
    PREFIX ""            # no libglide3x.dll
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"  # lands beside d2.exe
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
string(REGEX MATCHALL "_[a-zA-Z]+@[0-9]+" _g3x_syms "${_g3x_def}")
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

# Our code may optimize freely (PLAN.md R5: only the decompiled game tree is
# pinned to -O0/-g). volk.c stays at its own pace; -O2 everywhere is fine.
target_compile_options(glide3x PRIVATE -O2 -w)

set_source_files_properties(src/glide_exports.cpp PROPERTIES
    COMPILE_DEFINITIONS "G3XVK_BUILD_ID=\"${G3XVK_BUILD_ID}\"")
