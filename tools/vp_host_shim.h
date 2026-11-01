// vanilla-plus: host-side prelude for tools/syntax-check.sh ONLY.
//
// tools/syntax-check.sh runs the fork-owned TUs through the HOST compiler
// (g++ -fsyntax-only, same flags as the Win11 build) so a compile break dies
// sandbox-side before an overlay ships. d2_platform.h already carries a
// non-Windows typedef path ("On other platforms, provides minimal compatible
// typedefs"), but a glibc host still needs three small holes plugged before
// that path is valid. This header is force-included FIRST (before
// d2_platform.h) by the script; no normal build ever sees it. Everything is
// no-op under _WIN32 so an accidental MinGW run stays byte-identical.
//
//  1. FILE: d2_platform.h claims the MSVC CRT layout (struct _iobuf, guarded
//     by _FILE_DEFINED - the MSVC/mingw guard). glibc uses its own guards
//     (__FILE_defined / __io_FILE_defined) and would typedef FILE to
//     _IO_FILE - a conflicting redeclaration. Pre-defining glibc's guards
//     skips both its struct and its typedef; d2_platform.h's typedef stands
//     and every later FILE* stays pointer-consistent.
//  2. Calling conventions: the tree's declarations spell __stdcall /
//     __fastcall / __cdecl (real keywords on the i686 target). glibc hosts
//     have none; empty macros are semantically exact for a syntax check.
//  3. errno_t: MSVC CRT type; glibc has no typedef for it.
#if !defined(_WIN32)

// Runs before d2_platform.h, so the fixed-width types come from the CRT
// directly (the tree's DWORD/BYTE/UINT aliases do not exist yet here).
#include <stdint.h>

#ifndef __FILE_defined
#define __FILE_defined 1
#endif
#ifndef __io_FILE_defined
#define __io_FILE_defined 1
#endif

#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __thiscall
#define __thiscall
#endif

typedef int errno_t;

// ---------------------------------------------------------------------------
// Win32 SDK types the decompiled headers import from windows.h when _WIN32.
// d2_platform.h's non-Windows path covers the common ones but not these.
//
// Fidelity note: a syntax check needs DECLARATIONS, not ABI. Structs the
// tree indexes into field-by-field get their REAL x86 layouts (documented
// winnt.h); opaque pass-through types get minimal stand-ins - the fork TUs
// only pass pointers to those, so nothing checked can depend on the fields.
// ---------------------------------------------------------------------------

// _FLOATING_SAVE_AREA: REAL layout (winnt.h x86). Fog/Debug.h embeds it at
// CONTEXT+0x1C (after ContextFlags + Dr0..Dr7) and annotates that offset, so
// the field sizes must add up to 112 bytes exactly.
typedef struct _FLOATING_SAVE_AREA {
    uint32_t ControlWord;       /* 0x00 */
    uint32_t StatusWord;        /* 0x04 */
    uint32_t TagWord;           /* 0x08 */
    uint32_t ErrorOffset;       /* 0x0C */
    uint32_t ErrorSelector;     /* 0x10 */
    uint32_t DataOffset;        /* 0x14 */
    uint32_t DataSelector;      /* 0x18 */
    uint8_t  RegisterArea[80];  /* 0x1C */
    uint32_t Cr0NpxState;       /* 0x6C */
} FLOATING_SAVE_AREA;           /* 0x70 = 112 */

// Opaque stand-ins (see the fidelity note): D2GFX.h only typedefs and
// pointer-passes these.
struct _devicemodeA { uint32_t dmBitsPerPel; };
struct tagWNDCLASSA { uint32_t style; };

// The decompiled headers import these from windows.h under _WIN32 and carry
// no non-Windows fallback; the fork TUs only meet them as opaque globals or
// pointer pass-throughs (globals.h externs, Fog/Debug.h forward aliases).
// - EXCEPTION_RECORD / CONTEXT / EXCEPTION_POINTERS: Fog/Debug.h defines the
//   full struct bodies itself (its own non-Windows fallback block); the plain
//   aliases are what the SDK would provide. The shadows complete later.
typedef struct _EXCEPTION_RECORD EXCEPTION_RECORD, *PEXCEPTION_RECORD;
typedef struct _CONTEXT CONTEXT, *LPCONTEXT;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;
struct tagCANDIDATELIST { uint32_t dwStyle; };
// RGBQUAD (1792 sync): upstream's regen now defines the struct body itself in
// D2CMP/Draw.h under #ifndef _WIN32 (packed + typed), so the shim's own struct
// definition collided (redefinition error in 3 of 4 TUs). globals.h still
// declares `extern RGBQUAD nColor;` with the SDK's short typedef name, so the
// shim provides ONLY the alias - forward-typedef, the struct completes later
// in D2CMP/Draw.h. On Windows the real SDK typedef applies (same name).
typedef struct tagRGBQUAD RGBQUAD;
struct _SERVICE_STATUS { uint32_t dwServiceType; };
typedef void* SERVICE_STATUS_HANDLE;
struct _SYSTEM_INFO { uint32_t dwOemId; };

#endif // !defined(_WIN32)
