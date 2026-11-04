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

// Opaque stand-ins (see the fidelity note): pointer-passed-only types.
// HGDIOBJ family returns (SelectObject) — never dereferenced.
typedef void* HGDIOBJ;

// [d2linux M2] POINT/RECT: owned here now that the M2 structs embed them by
// value (tagMSG.pt, WINDOWPLACEMENT.rcNormalPosition, MONITORINFO.rcWork) —
// compat.h is force-included BEFORE d2_platform.h, so the shadow's stubs
// would arrive too late and collide; the shadow drops them instead
// (mk-platform-shadow.sh 13/14). Spelling identical to what it replaces:
// long is 4 bytes on the i386 line this ABI models (the x86-64 skeleton,
// which never indexes these by offset — charter R1 — sees the same shapes
// the tree file always gave it).
struct POINT { long x; long y; };
struct RECT { long left; long top; long right; long bottom; };

// [d2linux M2] tagWNDCLASSA: REAL x86 layout (winuser.h). D2GFX_Initialize
// fills lpfnWndProc/hInstance/hIcon/hCursor/hbrBackground/lpszClassName field
// by field before RegisterClassA, so a 4-byte stand-in would corrupt the stack
// object it fills (D2gfx/D2GFX.cpp's class setup is the named call site).
struct tagWNDCLASSA {
    uint32_t style;             /* 0x00 UINT */
    intptr_t (__stdcall *lpfnWndProc)(void*, unsigned int, uintptr_t, intptr_t); /* 0x04 WNDPROC — LRESULT(__stdcall*)(HWND,UINT,WPARAM,LPARAM) spelled raw (compat.h precedes d2_platform.h's typedefs); the void-returning stub in the tree file is patched out (shadow 16). The shim invokes through its own __stdcall typedef — identical type. The stdcall is load-bearing on i386: the game's procs are callee-clean. */
    int32_t  cbClsExtra;        /* 0x08 */
    int32_t  cbWndExtra;        /* 0x0C */
    void*    hInstance;         /* 0x10 HINSTANCE */
    void*    hIcon;             /* 0x14 HICON */
    void*    hCursor;           /* 0x18 HCURSOR */
    void*    hbrBackground;     /* 0x1C HBRUSH */
    const char* lpszMenuName;   /* 0x20 LPCSTR */
    const char* lpszClassName;  /* 0x24 LPCSTR */
};                              /* 0x28 */
typedef struct tagWNDCLASSA WNDCLASSA, *LPWNDCLASSA;

// [d2linux M2] tagMSG: NOT defined here. D2Client/Message.h (auto-generated)
// owns it on the non-Windows line exactly as windows.h owns it on the PE line
// — both pump TUs (D2WinMain.cpp, MainMenus.cpp) include that header, and a
// compat definition would collide inside the D2Client TUs. The shim's pump
// works on a layout-identical PRIVATE struct (D2WinMsg in
// d2_linux_shim_user32.cpp) and crosses the boundary through the void*
// parameters the api.h pump declarations already spell.

// [d2linux M2] TRACKMOUSEEVENT: REAL layout — D2WinMain.cpp fills cbSize/
// dwFlags/hwndTrack per WM_MOUSEMOVE (TME_LEAVE re-arm).
struct tagTRACKMOUSEEVENT {
    uint32_t cbSize;            /* 0x00 DWORD */
    uint32_t dwFlags;           /* 0x04 DWORD */
    void*    hwndTrack;         /* 0x08 HWND */
    uint32_t dwHoverTime;       /* 0x0C DWORD */
};                              /* 0x10 */
typedef struct tagTRACKMOUSEEVENT TRACKMOUSEEVENT, *LPTRACKMOUSEEVENT;

// [d2linux M2] WINDOWPLACEMENT: REAL layout (winuser.h) — D2WinMain/D2GFX read
// showCmd + rcNormalPosition for the fullscreen-restore math.
struct tagWINDOWPLACEMENT {
    uint32_t length;            /* 0x00 UINT */
    uint32_t flags;             /* 0x04 UINT */
    uint32_t showCmd;           /* 0x08 UINT */
    struct POINT ptMinPosition; /* 0x0C */
    struct POINT ptMaxPosition; /* 0x14 */
    struct RECT  rcNormalPosition; /* 0x1C (d2_platform.h: 4 longs) */
};                              /* 0x2C */
typedef struct tagWINDOWPLACEMENT WINDOWPLACEMENT, *LPWINDOWPLACEMENT;

// [d2linux M2] MONITORINFO: REAL layout — D2WinMain sets cbSize=0x28 and reads
// rcWork for the SC_MAXIMIZE clamp (MonitorFromWindow/GetMonitorInfoA).
typedef struct tagMONITORINFO {
    uint32_t cbSize;            /* 0x00 DWORD */
    struct RECT rcMonitor;      /* 0x04 */
    struct RECT rcWork;         /* 0x14 */
    uint32_t dwFlags;           /* 0x24 DWORD */
} MONITORINFO, *LPMONITORINFO;  /* 0x28 */

// [d2linux M2] _devicemodeA: REAL x86 layout (winuser.h DEVMODEA, 148 bytes).
// D2GFX's small-screen fallback fills dmSize/dmFields/dmBitsPerPel/dmPelsWidth/
// dmPelsHeight and calls ChangeDisplaySettingsA; the previous 4-byte stand-in
// would have turned that fill into a stack smash. Offsets in winuser.h order.
struct _devicemodeA {
    uint8_t  dmDeviceName[32];  /* 0x00 CHAR[32] */
    uint16_t dmSpecVersion;     /* 0x20 WORD */
    uint16_t dmDriverVersion;   /* 0x22 WORD */
    uint16_t dmSize;            /* 0x24 WORD */
    uint16_t dmDriverExtra;     /* 0x26 WORD */
    uint32_t dmFields;          /* 0x28 DWORD */
    uint32_t dmPositionX;       /* 0x2C union POINTL.x / dmDisplayOrientation */
    uint32_t dmPositionY;       /* 0x30 union POINTL.y / dmDisplayFixedOutput */
    uint16_t dmColor;           /* 0x34 SHORT */
    uint16_t dmDuplex;          /* 0x36 SHORT */
    uint16_t dmYResolution;     /* 0x38 SHORT */
    uint16_t dmTTOption;        /* 0x3A SHORT */
    uint16_t dmCollate;         /* 0x3C SHORT */
    uint8_t  dmFormName[32];    /* 0x3E BYTE[32] */
    uint16_t dmLogPixels;       /* 0x5E WORD */
    uint32_t dmBitsPerPel;      /* 0x60 DWORD */
    uint32_t dmPelsWidth;       /* 0x64 DWORD */
    uint32_t dmPelsHeight;      /* 0x68 DWORD */
    uint32_t dmDisplayFlags;    /* 0x6C DWORD */
    uint32_t dmDisplayFrequency;/* 0x70 DWORD */
    uint32_t dmICMMethod;       /* 0x74 DWORD */
    uint32_t dmICMIntent;       /* 0x78 DWORD */
    uint32_t dmMediaType;       /* 0x7C DWORD */
    uint32_t dmDitherType;      /* 0x80 DWORD */
    uint32_t dmReserved1;       /* 0x84 DWORD */
    uint32_t dmReserved2;       /* 0x88 DWORD */
    uint32_t dmPanningWidth;    /* 0x8C DWORD */
    uint32_t dmPanningHeight;   /* 0x90 DWORD */
};                              /* 0x94 = 148 */
typedef struct _devicemodeA DEVMODEA, *LPDEVMODEA;

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
