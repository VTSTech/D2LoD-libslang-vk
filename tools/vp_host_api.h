// vanilla-plus: host-side Win32 API fill-in for tools/syntax-check.sh ONLY.
//
// Force-included AFTER d2_platform.h (its HANDLE/LPCSTR/... aliases exist
// here) and BEFORE support/d2_hand.h. The fork TUs call a handful of Win32
// functions and constants that d2_platform.h's non-Windows path does not
// carry (they come from windows.h/winbase.h on the real build); without
// these the host check dies on the fork's own ini/file plumbing instead of
// on real defects. Signatures and values are the canonical Win32 ones, so a
// collision with any tree header surfaces as a duplicate-declaration error
// and gets reconciled, never papered over.
//
// tools/syntax-check.sh force-includes this; no normal build ever sees it.
// No-op under _WIN32 (the SDK has everything).
#ifndef _WIN32

// --- file system (winbase.h) ------------------------------------------------
BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
BOOL FlushFileBuffers(HANDLE hFile);
BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);
BOOL GetFileSizeEx(HANDLE hFile, LARGE_INTEGER* lpFileSize);
BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove,
                      LARGE_INTEGER* lpNewFilePointer, DWORD dwMoveMethod);

// --- profile strings (winbase.h / profileapis) ------------------------------
UINT GetPrivateProfileIntA(LPCSTR lpAppName, LPCSTR lpKeyName, INT nDefault, LPCSTR lpFileName);
BOOL WritePrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString, LPCSTR lpFileName);

// --- lstring (winbase.h) ----------------------------------------------------
LPSTR lstrcatA(LPSTR lpString1, LPCSTR lpString2);
LPSTR lstrcpyA(LPSTR lpString1, LPCSTR lpString2);
int lstrlenA(LPCSTR lpString1);
int wvsprintfA(LPSTR lpDest, LPCSTR lpFormat, va_list arglist);

// --- modules / temp path (libloaderapi / fileapi) - d2_crash.cpp ------------
BOOL GetModuleHandleExA(DWORD dwFlags, LPCSTR lpModuleName, HMODULE* phModule);
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x00000004u
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x00000002u
DWORD GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer);
#define ERROR_FILE_EXISTS 80

// --- version (winnt.h / winbase.h) - d2_crash.cpp's RtlGetVersion probe -----
#define WINAPI __stdcall
#define CALLBACK __stdcall
#define CDECL __cdecl
typedef struct _OSVERSIONINFOW {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    WCHAR szCSDVersion[128];
} OSVERSIONINFOW;

// --- constants (winbase.h) --------------------------------------------------
#define GENERIC_READ              0x80000000u
#define GENERIC_WRITE             0x40000000u
#define CREATE_NEW                1u
#define CREATE_ALWAYS             2u
#define OPEN_EXISTING             3u
#define FILE_SHARE_READ           0x00000001u
#define FILE_ATTRIBUTE_NORMAL     0x00000080u
#define FILE_BEGIN                0u
#define INVALID_HANDLE_VALUE      ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_ATTRIBUTES   0xFFFFFFFFu
#define MOVEFILE_REPLACE_EXISTING 0x1u
#define MAX_PATH                  260
#define TRUE                      1
#define FALSE                     0

// --- gdi/user misc ----------------------------------------------------------
// HACCEL must spell exactly what D2Client/Message.h re-declares
// (typedef struct HACCEL__* HACCEL) - the SDK's DECLARE_HANDLE form.
struct HACCEL__;
typedef struct HACCEL__* HACCEL;
typedef uintptr_t UINT_PTR;

// --- thread environment (winnt.h) - d2_crash.cpp's stack-bound probe --------
// NT_TIB: REAL x86 layout; d2_crash.cpp reads StackBase/StackLimit to bound
// its frame walk. NtCurrentTeb is an intrinsic on the real build.
typedef struct _NT_TIB {
    void* ExceptionList;        /* 0x00 */
    void* StackBase;            /* 0x04 */
    void* StackLimit;           /* 0x08 */
    void* SubSystemTib;         /* 0x0C */
    union { void* FiberData; DWORD Version; }; /* 0x10 */
    void* ArbitraryUserPointer; /* 0x18 */
    struct _NT_TIB* Self;       /* 0x1C */
} NT_TIB;
void* NtCurrentTeb();

// --- PE image headers (winnt.h) - d2_crash.cpp's section walker -------------
// REAL x86 layouts: d2_crash.cpp reads e_lfanew, FileHeader.NumberOfSections,
// section Characteristics/VirtualAddress/Misc.VirtualSize and arithmetic on
// OptionalHeader, so field sizes must add up exactly. (The one-line IMAGE_*
// stubs d2_platform.h would provide under !defined(_WIN32) are shadow-patched
// out by syntax-check.sh - they have none of these members.)
#define FIELD_OFFSET(type, field) ((long)(intptr_t)&(((type*)0)->field))
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16
#define IMAGE_SCN_MEM_EXECUTE 0x20000000u

typedef struct _IMAGE_DOS_HEADER {   /* 0x40 */
    WORD  e_magic;    /* 0x00 */
    WORD  e_cblp, e_cp, e_crlc, e_cparhdr, e_minalloc, e_maxalloc;
    WORD  e_ss, e_sp, e_csum, e_ip, e_cs, e_lfarlc, e_ovno;
    WORD  e_res[4];
    WORD  e_oemid, e_oeminfo;
    WORD  e_res2[10];
    LONG  e_lfanew;   /* 0x3C */
} IMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {  /* 0x14 */
    WORD  Machine;              /* 0x00 */
    WORD  NumberOfSections;     /* 0x02 */
    DWORD TimeDateStamp;        /* 0x04 */
    DWORD PointerToSymbolTable; /* 0x08 */
    DWORD NumberOfSymbols;      /* 0x0C */
    WORD  SizeOfOptionalHeader; /* 0x10 */
    WORD  Characteristics;      /* 0x12 */
} IMAGE_FILE_HEADER;

// 1806 merge: the SDK-faithful IMAGE_DATA_DIRECTORY / IMAGE_OPTIONAL_HEADER32
// now live in Fog/Engine/Application.h (globals.h includes it; the real
// winnt.h wins under _WIN32). The shim keeps its layout-identical copies
// under _SHIM aliases - d2_crash.cpp's PE walk only reads members, never
// those type names - so no typedef name collides with the tree's.
typedef struct _IMAGE_DATA_DIRECTORY { /* 0x08 */
    DWORD VirtualAddress;
    DWORD Size;
} IMAGE_DATA_DIRECTORY_SHIM;

typedef struct _IMAGE_OPTIONAL_HEADER { /* 0xE0 = 224 */
    WORD  Magic;                 /* 0x00 */
    BYTE  MajorLinkerVersion, MinorLinkerVersion;
    DWORD SizeOfCode;            /* 0x04 */
    DWORD SizeOfInitializedData, SizeOfUninitializedData;
    DWORD AddressOfEntryPoint;   /* 0x10 */
    DWORD BaseOfCode, BaseOfData;
    DWORD ImageBase;             /* 0x1C */
    DWORD SectionAlignment, FileAlignment;
    WORD  MajorOperatingSystemVersion, MinorOperatingSystemVersion;
    WORD  MajorImageVersion, MinorImageVersion;
    WORD  MajorSubsystemVersion, MinorSubsystemVersion;
    DWORD Win32VersionValue;     /* 0x34 */
    DWORD SizeOfImage;           /* 0x38 */
    DWORD SizeOfHeaders;         /* 0x3C */
    DWORD CheckSum;              /* 0x40 */
    WORD  Subsystem, DllCharacteristics;
    DWORD SizeOfStackReserve, SizeOfStackCommit, SizeOfHeapReserve, SizeOfHeapCommit;
    DWORD LoaderFlags;           /* 0x58 */
    DWORD NumberOfRvaAndSizes;   /* 0x5C */
    IMAGE_DATA_DIRECTORY_SHIM DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; /* 0x60 */
} IMAGE_OPTIONAL_HEADER_SHIM;

typedef struct _IMAGE_NT_HEADERS {
    DWORD Signature;                    /* 0x00 */
    IMAGE_FILE_HEADER FileHeader;       /* 0x04 */
    IMAGE_OPTIONAL_HEADER_SHIM OptionalHeader; /* 0x18 */
} IMAGE_NT_HEADERS;
typedef IMAGE_NT_HEADERS* PIMAGE_NT_HEADERS;

typedef struct _IMAGE_SECTION_HEADER { /* 0x28 = 40 */
    BYTE  Name[8];              /* 0x00 */
    union {
        DWORD PhysicalAddress;
        DWORD VirtualSize;
    } Misc;                     /* 0x08 */
    DWORD VirtualAddress;       /* 0x0C */
    DWORD SizeOfRawData;        /* 0x10 */
    DWORD PointerToRawData;     /* 0x14 */
    DWORD PointerToRelocations, PointerToLinenumbers;
    WORD  NumberOfRelocations, NumberOfLinenumbers;
    DWORD Characteristics;      /* 0x24 */
} IMAGE_SECTION_HEADER;
typedef IMAGE_SECTION_HEADER* PIMAGE_SECTION_HEADER;

#define IMAGE_FIRST_SECTION(ntheader) \
    ((PIMAGE_SECTION_HEADER)((uintptr_t)(ntheader) \
        + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) \
        + ((PIMAGE_NT_HEADERS)(ntheader))->FileHeader.SizeOfOptionalHeader))

// --- tool help (tlhelp32.h) - d2_crash.cpp's module walker ------------------
// MODULEENTRY32: REAL x86 layout; d2_crash.cpp reads modBaseAddr/modBaseSize/
// szExePath for the module+offset stack annotations.
#define MAX_MODULE_NAME32 255
#define TH32CS_SNAPMODULE 0x00000008u
#define TH32CS_SNAPMODULE32 0x00000010u
typedef struct tagMODULEENTRY32 {
    DWORD   dwSize;         /* 0x00 */
    DWORD   th32ModuleID;   /* 0x04 */
    DWORD   th32ProcessID;  /* 0x08 */
    DWORD   GlblcntUsage;   /* 0x0C */
    DWORD   ProccntUsage;   /* 0x10 */
    BYTE*   modBaseAddr;    /* 0x14 */
    DWORD   modBaseSize;    /* 0x18 */
    HMODULE hModule;        /* 0x1C */
    char    szModule[MAX_MODULE_NAME32 + 1]; /* 0x20 */
    char    szExePath[MAX_PATH];             /* 0x120 */
} MODULEENTRY32;            /* 0x224 */
typedef MODULEENTRY32* PMODULEENTRY32;
typedef MODULEENTRY32* LPMODULEENTRY32;
HANDLE CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID);
BOOL Module32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
BOOL Module32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

// --- SEH / vectored handlers (excpt.h / winnt.h) - d2_crash.cpp -------------
void RtlCaptureContext(CONTEXT* ContextRecord);
typedef LONG (*PVECTORED_EXCEPTION_HANDLER)(EXCEPTION_POINTERS* ExceptionInfo);
ULONG AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler);
BOOL SetThreadStackGuarantee(ULONG* SizeInBytes);
#define EXCEPTION_CONTINUE_SEARCH 0
#define WAIT_OBJECT_0 0u

// --- critical sections (winnt.h struct tag) - globals.h 1801 ----------------
// The Ghidra 1801 globals declare their lpCriticalSection_* globals with the
// SDK's struct-tag form `_RTL_CRITICAL_SECTION` (winnt.h provides the tag on
// the real build; d2_platform.h's non-Windows path only carries the
// CRITICAL_SECTION typedef). Definition matches d2_platform.h's
// CRITICAL_SECTION layout; under _WIN32 the real winnt.h wins.
struct _RTL_CRITICAL_SECTION { void* DebugInfo; long LockCount; long RecursionCount; void* OwningThread; void* LockSemaphore; unsigned long SpinCount; };
typedef struct _RTL_CRITICAL_SECTION RTL_CRITICAL_SECTION;
typedef struct _RTL_CRITICAL_SECTION* PRTL_CRITICAL_SECTION;

#endif // !defined(_WIN32)
