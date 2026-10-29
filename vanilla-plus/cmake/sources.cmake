# vanilla-plus: fork-owned source/header filters. Included from the root
# CMakeLists right after the GLOB_RECURSE; keep every filter here so the root
# file stays one include line away from upstream (docs/merge-upstream.md).
#
# A build directory inside the tree must not feed its own files back in, and
# the vendored libbacktrace (built as its own target below) must not leak its
# headers (config.h, internal.h) onto the game's include path. The prefix
# excludes match LITERALLY (string(FIND) == 0) instead of by regex: building
# a regex from the raw path fails to compile ("Nested *?+") when the checkout
# lives under a name with regex metacharacters, e.g. F:\Coding\Code\C++\.
foreach(list D2_SOURCES D2_HEADERS)
    list(FILTER ${list} EXCLUDE REGEX "/\\.git/")
    # Vendored C library (libbacktrace) and upstream's msvc8 oracle (asm_matching/)
    # are built as their own targets; their headers must not land on the game's
    # include path. Excluded by literal prefix (string(FIND) == 0), NOT by
    # list(FILTER REGEX): CMAKE_SOURCE_DIR can contain regex metacharacters
    # (e.g. F:\Coding\Code\C++\), which fails CMake regex compilation
    # ("Nested *?+") - the reason this filter is literal in the first place.
    set(_kept "")
    foreach(entry IN LISTS ${list})
        string(FIND "${entry}" "${CMAKE_BINARY_DIR}/" _in_build)
        string(FIND "${entry}" "${CMAKE_SOURCE_DIR}/thirdparty/libbacktrace/" _in_backtrace)
        string(FIND "${entry}" "${CMAKE_SOURCE_DIR}/asm_matching/" _in_asm)
        if(NOT _in_build EQUAL 0 AND NOT _in_backtrace EQUAL 0 AND NOT _in_asm EQUAL 0)
            list(APPEND _kept "${entry}")
        endif()
    endforeach()
    set(${list} "${_kept}")
endforeach()
unset(entry)
unset(_in_build)
unset(_in_backtrace)
