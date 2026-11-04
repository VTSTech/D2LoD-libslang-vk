// platform/linux/d2_linux_compat.h — [d2linux M1] the Linux build's forced
// prelude (platform/linux/build.cmake includes it BEFORE d2_platform.h).
//
// The decomp was recovered against the 32-bit MSVC x86 ABI: game TUs spell
// MSVC calling conventions, MSVC CRT shapes and a Win32 SDK surface that
// mingw's windows.h supplies on the PE line. This file (plus tools/
// vp_host_api.h, force-included after d2_platform.h) supplies both. The
// fork's tools/vp_host_shim.h established this exact shape for the host
// syntax check (tools/syntax-check.sh) — the three glibc holes and the Win32
// SDK struct set are absorbed from there; its content is no-op under _WIN32,
// and fidelity is upgraded here from "declaration enough" to "ABI enough for
// a running build" where a struct is indexed, not just pointer-passed.
//
// PE safety: the PE build never sees this file (Linux-only forced include),
// so the _WIN32 path is byte-identical by construction — track rule 1
// (docs/linux-port.md) with zero decomp-TU edits for this surface.
#ifndef D2_LINUX_COMPAT_H
#define D2_LINUX_COMPAT_H

#if !defined(_WIN32)

// Runs before d2_platform.h, so the fixed-width types come from the CRT
// directly (the tree's DWORD/BYTE/UINT aliases do not exist yet here).
// <ctype.h>: game TUs call tolower/toupper and friends relying on the
// windows.h CRT chain to have pulled the C types in (e.g. Fog/SMem.cpp's
// name-hash loop) — on the PE line mingw's headers supply them.
#include <stdint.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// 1. FILE: d2_platform.h claims the MSVC CRT layout (struct _iobuf, guarded
//    by _FILE_DEFINED — the MSVC/mingw guard). glibc uses its own guards
//    (__FILE_defined / __io_FILE_defined) and would typedef FILE to
//    _IO_FILE — a conflicting redeclaration. Pre-defining glibc's guards
//    skips both its struct and its typedef; d2_platform.h's typedef stands
//    and every later FILE* stays pointer-consistent. (vp_host_shim.h hole 1.)
// ---------------------------------------------------------------------------
#ifndef __FILE_defined
#define __FILE_defined 1
#endif
#ifndef __io_FILE_defined
#define __io_FILE_defined 1
#endif

// ---------------------------------------------------------------------------
// 2. MSVC calling conventions. i386: the register-passing forms the decomp
//    names, spelled as GCC attributes — declaration and definition always see
//    the same macro, so the build is self-consistent, and the i386 encodings
//    match the MSVC x86 ABI the decomp was recovered against (ECX/EDX for
//    fastcall, ECX this for thiscall, callee-cleans for stdcall). x86-64:
//    every convention collapses to the default ABI — the skeleton-only
//    configuration (PLAN.md R1: game code ships i386). Variadic stdcall
//    falls back to caller-cleans exactly as it does on mingw.
//    (vp_host_shim.h hole 2, upgraded from empty macros to real attributes.)
// ---------------------------------------------------------------------------
#if defined(__i386__)
#  ifndef __fastcall
#    define __fastcall __attribute__((fastcall))
#  endif
#  ifndef __stdcall
#    define __stdcall __attribute__((stdcall))
#  endif
#  ifndef __thiscall
#    define __thiscall __attribute__((thiscall))
#  endif
#else
#  ifndef __fastcall
#    define __fastcall
#  endif
#  ifndef __stdcall
#    define __stdcall
#  endif
#  ifndef __thiscall
#    define __thiscall
#  endif
#endif
#ifndef __cdecl
#define __cdecl
#endif

// ---------------------------------------------------------------------------
// 3. MSVC CRT types glibc has no typedef for. (vp_host_shim.h hole 3, plus
//    __time32_t: supplied here as `long` — the i386 glibc time_t spelling —
//    and D2_LINUX_TIME32_SUPPLIED tells the tree's one elaborated forward
//    declaration (Bnclient/BnSend.h, inside #ifndef _WIN32) to stand down;
//    Bnclient/net_sid.h's identical typedef is a legal same-type redeclare.
//    _time32 is the MSVC CRT time() the game TUs call — implemented in the
//    shim (named call sites: D2Game/PLAYER/PlayerSave2.cpp, Bnclient/
//    Debug.cpp, Bnclient/BnSend.cpp).
// ---------------------------------------------------------------------------
typedef int errno_t;
typedef long __time32_t;
#define D2_LINUX_TIME32_SUPPLIED 1
extern "C" __time32_t _time32(__time32_t* pTime);

// ---------------------------------------------------------------------------
// 4. Win32 SDK types the decompiled headers import from windows.h when
//    _WIN32 and carry no non-Windows fallback for. Absorbed from
//    tools/vp_host_shim.h (its fidelity note applies): structs the tree
//    indexes into field-by-field get their REAL x86 layouts (documented
//    winnt.h); opaque pass-through types get minimal stand-ins.
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

// The decompiled headers import these from windows.h under _WIN32; Fog/Debug.h
// defines the full _EXCEPTION_RECORD/_CONTEXT struct bodies itself (its own
// non-Windows fallback block); the plain aliases are what the SDK would
// provide.
typedef struct _EXCEPTION_RECORD EXCEPTION_RECORD, *PEXCEPTION_RECORD;
typedef struct _CONTEXT CONTEXT, *LPCONTEXT;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;
struct tagCANDIDATELIST { uint32_t dwStyle; };
// RGBQUAD: upstream's regen defines the struct body itself in D2CMP/Draw.h
// under #ifndef _WIN32 (packed + typed), so this is ONLY the forward alias —
// the struct completes later in D2CMP/Draw.h.
typedef struct tagRGBQUAD RGBQUAD;
// _SERVICE_STATUS: REAL x86 layout (winnt.h) — Fog/Engine/Application.cpp's
// service loop reads/writes dwCurrentState et al. field by field.
struct _SERVICE_STATUS {
    uint32_t dwServiceType;             /* 0x00 */
    uint32_t dwCurrentState;            /* 0x04 */
    uint32_t dwControlsAccepted;        /* 0x08 */
    uint32_t dwWin32ExitCode;           /* 0x0C */
    uint32_t dwServiceSpecificExitCode; /* 0x10 */
    uint32_t dwCheckPoint;              /* 0x14 */
    uint32_t dwWaitHint;                /* 0x18 */
};
typedef void* SERVICE_STATUS_HANDLE;
// in_addr: the SDK-faithful shape d2_platform.h's own _WIN32 block spells
// (the S_un union with conversion operators — Safesock.cpp's regenerated
// bodies reference _union_1226 members and construct from uint32_t directly).
// D2Linux_SDK_IN_ADDR stands the tree's simple regen body down (D2Net/
// in_addr.h, guarded there).
#define D2_LINUX_SDK_IN_ADDR 1
struct _struct_1227 { unsigned char s_b1; unsigned char s_b2; unsigned char s_b3; unsigned char s_b4; operator unsigned long() const { return *reinterpret_cast<const unsigned long*>(this); } };
struct _struct_1228 { unsigned short s_w1; unsigned short s_w2; operator unsigned long() const { return *reinterpret_cast<const unsigned long*>(this); } };
union _union_1226 {
    _struct_1227 S_un_b;
    _struct_1228 S_un_w;
    unsigned long S_addr;
    _union_1226() : S_addr(0) {}
    _union_1226(unsigned long a) : S_addr(a) {}
    _union_1226(_struct_1227 b) : S_un_b(b) {}
    _union_1226(_struct_1228 w) : S_un_w(w) {}
    operator unsigned long() const { return S_addr; }
};
typedef struct in_addr {
    _union_1226 S_un;
    in_addr() {}
    in_addr(_union_1226 u) : S_un(u) {}
    in_addr(unsigned long a) : S_un(a) {}
    in_addr(_struct_1227 b) : S_un(b) {}
    in_addr(_struct_1228 w) : S_un(w) {}
    operator unsigned long() const { return S_un.S_addr; }
} IN_ADDR, *PIN_ADDR, *LPIN_ADDR;

// _CERT_CONTEXT: D2Hell.h only carries PCCERT_CONTEXT (pointer pass-through);
// incomplete is enough.
// _SYSTEM_INFO: forward declaration only — globals.h declares
// `extern _SYSTEM_INFO gSystemInfo;` (legal on an incomplete type) and the
// tree's own Storm/Source/SMem.h completes the offset-annotated body for
// !defined(_WIN32); a full body here would be a second definition.
typedef struct _CERT_CONTEXT CERT_CONTEXT;
struct _SYSTEM_INFO;

// _CRYPT_BIT_BLOB: REAL x86 layout (winnt.h) — D2Hell/D2Hell.h typedefs it for
// !defined(_WIN32); certificate-blob fields are annotated by offset there.
typedef struct _CRYPT_BIT_BLOB {
    uint32_t cbData;        /* 0x00 */
    uint8_t* pbData;        /* 0x04 */
    uint32_t cUnusedBits;   /* 0x08 */
} CRYPT_BIT_BLOB;

// The wincrypt blob family _CERT_INFO's !defined(_WIN32) body (D2Hell.h)
// references — REAL x86 layouts, offset-confirmed against that body's own
// annotations (SignatureAlgorithm 0x0C→0x18 = 12 bytes, SubjectPublicKeyInfo
// 0x38→0x50 = 24 bytes). The SDK spells every plain blob as the one
// _CRYPTOAPI_BLOB tag (D2Hell.h: `typedef _CRYPTOAPI_BLOB CERT_NAME_BLOB;`).
typedef struct _CRYPTOAPI_BLOB {
    uint32_t cbData;        /* 0x00 */
    uint8_t* pbData;        /* 0x04 */
} CRYPTOAPI_BLOB;
typedef CRYPTOAPI_BLOB CRYPT_INTEGER_BLOB;
typedef CRYPTOAPI_BLOB CRYPT_DATA_BLOB;
typedef CRYPTOAPI_BLOB CRYPT_OBJID_BLOB;
typedef CRYPTOAPI_BLOB CERT_NAME_BLOB;
typedef struct _CRYPT_ALGORITHM_IDENTIFIER {
    char* pszObjId;                       /* 0x00 */
    CRYPT_OBJID_BLOB Parameters;          /* 0x04 */
} CRYPT_ALGORITHM_IDENTIFIER;             /* 0x0C */
typedef struct _CERT_PUBLIC_KEY_INFO {
    CRYPT_ALGORITHM_IDENTIFIER Algorithm; /* 0x00 */
    CRYPT_BIT_BLOB PublicKey;             /* 0x0C */
} CERT_PUBLIC_KEY_INFO;                   /* 0x18 */
typedef struct _CERT_EXTENSION {
    char* pszObjId;                       /* 0x00 */
    int32_t fCritical;                    /* 0x04 */
    CRYPT_OBJID_BLOB Value;               /* 0x08 */
} CERT_EXTENSION;                         /* 0x10 */
typedef CERT_EXTENSION* PCERT_EXTENSION;

#endif // !defined(_WIN32)

#endif // D2_LINUX_COMPAT_H
