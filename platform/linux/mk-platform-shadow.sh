#!/bin/sh
# platform/linux/mk-platform-shadow.sh — [d2linux M1]
#
# Generates "$2"/d2_platform.h: the tree's d2_platform.h with the handful of
# non-Windows stub declarations that clash with the SDK-faithful forms the
# linux build supplies (platform/linux/d2_linux_compat.h + tools/
# vp_host_api.h) removed or completed. Same pattern as tools/syntax-check.sh's
# temp shadow — its comment: "one in-built inconsistency that has never
# compiled anywhere" — scaled to a real build; there is more than one.
#
# The tree file is never edited. Every patched line sits inside #ifndef
# _WIN32, so the PE path is untouched by construction. The shadow is
# regenerated at configure time (platform/linux/build.cmake), so an upstream
# d2_platform.h regen (the R-syncs) re-applies the patches — and a regen that
# removes a patched line fails the build loudly instead of drifting silently.
#
#   Usage: mk-platform-shadow.sh <repo-root> <out-dir>
#
# The patch list:
#   1. `typedef void* PRTL_CRITICAL_SECTION_DEBUG;` — dropped. The Win32 SDK
#      (and Storm/_unnamespaced.h, which the tree shares) spell it
#      `struct _RTL_CRITICAL_SECTION_DEBUG*`; vp_host_api.h supplies that
#      struct-typed form. (syntax-check.sh's original one-line shadow.)
#   2. `struct CONTEXT { DWORD ContextFlags; };` — dropped. The compat header
#      provides the SDK-faithful alias CONTEXT = struct _CONTEXT, and
#      Fog/Debug.h defines the full _CONTEXT body for !defined(_WIN32); the
#      stub's 4-byte tag would clash with both.
#   3-5. The IMAGE_DOS_HEADER / IMAGE_NT_HEADERS / IMAGE_SECTION_HEADER stubs
#      — dropped. d2_crash.cpp's PE walk needs the REAL x86 layouts, which
#      vp_host_api.h supplies with member-for-member layouts.
#   6. `_LARGE_INTEGER` — completed: the decomp reads `.u.LowPart/.u.HighPart`
#      (winnt.h's anonymous member spelling, e.g. Fog/SMem.cpp's QPC math);
#      the stub only spelled `.s`. Both names, one anonymous struct type.
#   7. `struct CRITICAL_SECTION {...}` — retagged. The globals (Ghidra 1801,
#      globals.h) declare their lock objects with the SDK's struct-tag form
#      `_RTL_CRITICAL_SECTION*` while the stub spells a second, identical
#      tag `CRITICAL_SECTION` — two types for one lock, so
#      EnterCriticalSection(&_RTL_CRITICAL_SECTION global) fails to type.
#      The patch makes CRITICAL_SECTION an alias of the one _RTL tag
#      (forward-declared here, completed in tools/vp_host_api.h), which is
#      exactly what winnt.h does.
#   8. `typedef uint16_t WCHAR;` — retargeted to `wchar_t`. The build spells
#      -fshort-wchar (MSVC's 16-bit wchar_t is part of the ABI the decomp
#      models: W-path literals and LPWSTR must agree), and under that flag
#      GCC's 16-bit wchar_t is still a DISTINCT type from uint16_t, so every
#      L"..." argument fails to convert. winnt.h spells WCHAR as wchar_t.
#   9. `typedef void* HMODULE/HINSTANCE;` — the DECLARE_HANDLE form. Fog/
#      Debug.cpp:1672 dereferences HMODULE for the 'MZ' image check exactly
#      like the SDK shape (`struct HINSTANCE__ { int unused; }`), so the
#      handles get their real pointer-to-struct spelling.
#  11. The two stray non-Windows declaration groups (d2_platform.h's
#      `#ifndef _WIN32` blocks that spell SDK functions WITHOUT extern "C")
#      get the C linkage the rest of the surface already carries. The game
#      TUs' references and the shim's definitions must agree on one linkage;
#      the SDK's own is C. (CreateEventW's group ~line 637-647 and the
#      GetFileSize..timeGetTime group ~line 689-701.)
#  10. `typedef uintptr_t DWORD_PTR/ULONG_PTR;` — retargeted per-arch. MSVC's
#      32-bit spelling of the *_PTR integral family is `unsigned long` (same
#      TYPE as DWORD, not just same size): the decomp passes DWORD* into
#      ULONG_PTR* parameters (crashy.cpp's RaiseException thread-name raise),
#      which unifies on MSVC and must unify here. i386: unsigned long (4
#      bytes, same type as DWORD); x86-64 skeleton: uintptr_t as before.
#  13/14. `struct POINT/RECT` — dropped. [d2linux M2] the compat header now
#      owns both (identical spelling): the M2 structs (tagMSG/WINDOWPLACEMENT/
#      MONITORINFO) embed them BY VALUE and compat.h is force-included before
#      this file, so the stubs here would arrive too late and collide. The
#      LPPOINT/LPRECT typedefs STAY (they alias whatever POINT/RECT is).
#  15. `struct tagWINDOWPLACEMENT` — dropped. Same story: compat.h owns the
#      REAL layout (D2WinMain/D2GFX read showCmd + rcNormalPosition for the
#      fullscreen-restore math); the stub here would be a second definition.
#  16. `typedef void (*WNDPROC)(...)` — retargeted. [d2linux M2] the game's
#      class procs return LRESULT (D2Launch hands &WndProc to D2GFX_Initialize;
#      D2Win.cpp's gaUIWindowHandlers is a table of them) — a void-returning
#      WNDPROC fails every one of those assignments. winuser.h spells
#      LRESULT (__stdcall *)(HWND, UINT, WPARAM, LPARAM); the stdcall matters
#      on the i386 line (callee-clean) and is empty on the x86-64 skeleton.
set -eu

SRC="$1/d2_platform.h"
OUT="$2"
mkdir -p "$OUT"
TMP="$OUT/d2_platform.h.tmp"

apply() {
    # apply <label> <sed-expr> <expected-matches> [z]
    # The optional 4th arg runs sed in -z (NUL-separated) mode, for multiline
    # replacements spelled with \n escapes.
    label=$1; expr=$2; want=$3; zmode=${4:-}
    n=$(grep -cF -- "$label" "$SRC" || true)
    if [ "$n" -ne "$want" ]; then
        echo "mk-platform-shadow: pattern count changed for: $label" >&2
        echo "  expected $want occurrence(s) in d2_platform.h, found $n." >&2
        echo "  An upstream regen changed the stub; re-audit platform/linux/" >&2
        echo "  mk-platform-shadow.sh against the new d2_platform.h." >&2
        exit 1
    fi
    if [ "$zmode" = "z" ]; then
        sed -z -i "$expr" "$TMP"
    else
        sed -i "$expr" "$TMP"
    fi
}

cp "$SRC" "$TMP"

# 1. PRTL_CRITICAL_SECTION_DEBUG — see above. (1 occurrence, line-anchored.)
apply "typedef void* PRTL_CRITICAL_SECTION_DEBUG;" \
      "/^typedef void\* PRTL_CRITICAL_SECTION_DEBUG;$/d" 1
# 2. CONTEXT stub — see above.
apply "struct CONTEXT { DWORD ContextFlags; };" \
      "/^struct CONTEXT { DWORD ContextFlags; };$/d" 1
# 3-5. IMAGE_* stubs — see above.
apply "struct IMAGE_DOS_HEADER { WORD e_magic; };" \
      "/^struct IMAGE_DOS_HEADER { WORD e_magic; };$/d" 1
apply "struct IMAGE_NT_HEADERS { DWORD Signature; };" \
      "/^struct IMAGE_NT_HEADERS { DWORD Signature; };$/d" 1
apply "struct IMAGE_SECTION_HEADER { char Name[8]; };" \
      "/^struct IMAGE_SECTION_HEADER { char Name\[8\]; };$/d" 1
# 6. _LARGE_INTEGER .u member — see above.
apply "} s; long long QuadPart; };" "/} s; long long QuadPart; };/s||} s, u; long long QuadPart; };|" 1
# 7. CRITICAL_SECTION retag — see above. (The stub line and its typedef sit on
#    consecutive lines; replace both with the one-tag form, typedef pair kept.)
apply "struct CRITICAL_SECTION { void* DebugInfo;" \
      "/^struct CRITICAL_SECTION { void\* DebugInfo;/,+1c\\
struct _RTL_CRITICAL_SECTION;\\
typedef _RTL_CRITICAL_SECTION CRITICAL_SECTION;\\
typedef CRITICAL_SECTION* LPCRITICAL_SECTION;" 1
# 8. WCHAR -> wchar_t — see above. (Line 256 in the tree file; the two
#    adjacent typedefs move together so the scalar block stays one piece.)
apply "typedef uint16_t WCHAR;" \
      "/^typedef uint16_t WCHAR;$/s||typedef wchar_t WCHAR;|" 1
# 9. HMODULE/HINSTANCE -> DECLARE_HANDLE form — see above.
apply "typedef void* HMODULE;" \
      "/^typedef void\* HMODULE;$/c\
struct HINSTANCE__ { int unused; };\
typedef struct HINSTANCE__* HINSTANCE;\
typedef HINSTANCE HMODULE;" 1
apply "typedef void* HINSTANCE;" "/^typedef void\* HINSTANCE;$/d" 1
# 10. DWORD_PTR/ULONG_PTR per-arch — see above. (GNU sed expands \n in the
#     replacement text to real newlines.)
apply "typedef uintptr_t DWORD_PTR;" \
      "s|typedef uintptr_t DWORD_PTR;|#if defined(__i386__)\\ntypedef unsigned long DWORD_PTR;\\n#else\\ntypedef uintptr_t DWORD_PTR;\\n#endif|" 1
apply "typedef uintptr_t ULONG_PTR;" \
      "s|typedef uintptr_t ULONG_PTR;|#if defined(__i386__)\\ntypedef unsigned long ULONG_PTR;\\n#else\\ntypedef uintptr_t ULONG_PTR;\\n#endif|" 1
# 11. C linkage for the stray non-Windows SDK declaration groups (the game
#     TUs' references and the shim's definitions must agree on one linkage;
#     the SDK's own is C). Line-anchored on unique declarations; GNU sed
#     expands the \n escapes to real newlines.
# 11a. CreateEventW/WaitForMultipleObjects/SendMessageA group.
apply "HANDLE CreateEventW(LPSECURITY_ATTRIBUTES" \
      's|^HANDLE CreateEventW(LPSECURITY_ATTRIBUTES|extern "C" {\nHANDLE CreateEventW(LPSECURITY_ATTRIBUTES|' 1
apply "LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);" \
      's|LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);|LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);\n}  // extern "C"|' 1
# 11b. GetFileSize..timeGetTime group.
apply "BOOL WriteFile(HANDLE hFile" \
      's|^BOOL WriteFile(HANDLE hFile|extern "C" {\nBOOL WriteFile(HANDLE hFile|' 1
apply "DWORD timeGetTime(void);" \
      's|DWORD timeGetTime(void);|DWORD timeGetTime(void);\n}  // extern "C"|' 1
# 13/14. POINT/RECT structs — see above. (Their LPPOINT/LPRECT typedefs and
#        every later use fall through to the compat header's shapes.)
apply "struct POINT { long x; long y; };" \
      "/^struct POINT { long x; long y; };$/d" 1
apply "struct RECT { long left; long top; long right; long bottom; };" \
      "/^struct RECT { long left; long top; long right; long bottom; };$/d" 1
# 15. tagWINDOWPLACEMENT — see above. (Struct + typedef, consecutive lines.)
apply "struct tagWINDOWPLACEMENT { UINT length;" \
      "/^struct tagWINDOWPLACEMENT { UINT length;/,+1d" 1
# 16. WNDPROC retarget — see above.
apply "typedef void (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);" \
      "s|typedef void (\*WNDPROC)(HWND, UINT, WPARAM, LPARAM);|typedef LRESULT (__stdcall *WNDPROC)(HWND, UINT, WPARAM, LPARAM);|" 1
# 12. The IsBad*/CreateEventA/window-function block (the bare declarations
#     after the first extern "C" close, before the #ifndef _WIN32 end).
apply "BOOL IsBadCodePtr(FARPROC lpfn);" \
      's|^BOOL IsBadCodePtr(FARPROC lpfn);|extern "C" {\nBOOL IsBadCodePtr(FARPROC lpfn);|' 1
apply "int ReleaseDC(HWND hWnd, HDC hDC);" \
      's|int ReleaseDC(HWND hWnd, HDC hDC);|int ReleaseDC(HWND hWnd, HDC hDC);\n}  // extern "C"|' 1

# The shadow self-identifies so nobody mistakes it for the tree file.
sed -i "1i // GENERATED at configure time by platform/linux/mk-platform-shadow.sh — do not edit." "$TMP"
sed -i "2i // Patch list and rationale in that script; source of truth: d2_platform.h." "$TMP"
mv "$TMP" "$OUT/d2_platform.h"
echo "d2linux: platform shadow -> $OUT/d2_platform.h (16 patches applied)"
