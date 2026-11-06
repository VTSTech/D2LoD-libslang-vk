# vanilla-plus: fork-owned source/header filters. Included from the root
# CMakeLists before the source filters run; keep every filter here so the
# root file stays one include line away from upstream (docs/merge-upstream.md).
#
# A build directory inside the tree must not feed its own files back in, and
# the vendored libbacktrace (built as its own target below) must not leak its
# headers (config.h, internal.h) onto the game's include path. The prefix
# excludes match LITERALLY (string(FIND) == 0) instead of by regex: building
# a regex from the raw path fails to compile ("Nested *?+") when the checkout
# lives under a name with regex metacharacters, e.g. F:\Coding\Code\C++\.

# [vanilla-plus] literal prefix list filters, shared with the root CMakeLists:
# upstream's list(FILTER ... REGEX "^${D2_ROOT}/...") forms compile the raw
# path into a regex, which dies on checkout paths carrying metacharacters
# ("Nested *?+.", e.g. under a C++ directory) - these do the same job by
# string(FIND) prefix compare, and the root filter lines call them too.
function(d2_list_exclude_prefix out_list prefix)
    set(_kept "")
    foreach(entry IN LISTS ${out_list})
        string(FIND "${entry}" "${prefix}" _at)
        if(NOT _at EQUAL 0)
            list(APPEND _kept "${entry}")
        endif()
    endforeach()
    set(${out_list} "${_kept}" PARENT_SCOPE)
endfunction()

function(d2_list_keep_prefix out_list prefix)
    set(_kept "")
    foreach(entry IN LISTS ${out_list})
        string(FIND "${entry}" "${prefix}" _at)
        if(_at EQUAL 0)
            list(APPEND _kept "${entry}")
        endif()
    endforeach()
    set(${out_list} "${_kept}" PARENT_SCOPE)
endfunction()

foreach(list D2_SOURCES D2_HEADERS)
    list(FILTER ${list} EXCLUDE REGEX "/\\.git/")
    # Vendored C library (libbacktrace) and upstream's msvc8 oracle (asm_matching/)
    # are built as their own targets; their headers must not land on the game's
    # include path. Excluded by literal prefix (string(FIND) == 0), NOT by
    # list(FILTER REGEX): CMAKE_SOURCE_DIR can contain regex metacharacters
    # (e.g. F:\Coding\Code\C++\), which fails CMake regex compilation
    # ("Nested *?+") - the reason this filter is literal in the first place.
    # [R130] the vendored libs moved thirdparty/ -> 3rdparty/ at the tenth
    # sync; the libbacktrace prefix here follows, the belt stays on.
    d2_list_exclude_prefix(${list} "${CMAKE_BINARY_DIR}/")
    d2_list_exclude_prefix(${list} "${CMAKE_SOURCE_DIR}/3rdparty/libbacktrace/")
    d2_list_exclude_prefix(${list} "${CMAKE_SOURCE_DIR}/asm_matching/")
    # [R118] scripts/ holds host-side tools only. The root CMakeLists
    # globs *.cpp tree-wide, so R117's host test harness
    # (scripts/test_r117_fill.cpp, with its own main() and stub
    # globals) silently compiled INTO d2.exe - the owner's R117 build
    # died right after the static-init boot mark, before WinMain, in
    # every renderer. Excluded here so anything parked under scripts/
    # can never reach the game link again.
    d2_list_exclude_prefix(${list} "${CMAKE_SOURCE_DIR}/scripts/")
endforeach()
