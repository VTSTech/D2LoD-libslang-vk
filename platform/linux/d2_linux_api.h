// platform/linux/d2_linux_api.h — [d2linux M1] the linux build's Win32 API
// extension beyond tools/vp_host_api.h.
//
// Force-included AFTER d2_platform.h and vp_host_api.h (its HANDLE/HKEY/
// LPCSTR aliases exist here) by the linux build. vp_host_api.h carries the
// surface the fork's own TUs need (syntax-check provenance); this header
// carries the surface the GAME TUs on the M1 slice reference that neither
// d2_platform.h's non-Windows branch nor vp_host_api.h declares.
//
// ABI note: every function is spelled __stdcall (WINAPI) — the tree's own
// PFN_* typedefs spell the convention (Fog/Src/LogManager.cpp's
// PFN_GetCurrentThread et al.), and on i386 the attribute is real. The shim
// implementations see the same macro, so the build is self-consistent; the
// encodings match the MSVC x86 ABI the decomp was recovered against.
//
// Signatures and values are the canonical Win32 ones, implemented in
// platform/linux/d2_linux_shim_win32.cpp / d2_linux_shim_crt.cpp — every
// family here has named call sites in the slice (inventory: PLAN.md §6 M1).
//
// No-op under _WIN32 (the SDK has everything; the PE build never sees this
// file — Linux-only forced include).
#ifndef D2_LINUX_API_H
#define D2_LINUX_API_H

#ifndef _WIN32

extern "C" {

// The winsock-flavored shapes below stand in for Safesock.h's own fallback
// block (stood down via D2_LINUX_WINSOCK_SUPPLIED — it predates a real
// non-Windows build and clashes with glibc's timeval/fd_set).
#define D2_LINUX_WINSOCK_SUPPLIED 1

// --- registry (winreg.h) — Storm/Source/SReg.cpp, Fog/Engine/Application.cpp,
// --- D2Lang/StrTable/strtable.cpp (video-mode keys are the first readers,
// --- PLAN.md §6 M1: the file-backed store lives in the shim) ---------------
typedef DWORD REGSAM;
// winreg spells LSTATUS `LONG`; the tree's own typedef (Fog/Engine/
// Application.h) is `long` — same spelling kept so the redeclaration is the
// legal same-type form.
typedef long LSTATUS;
typedef HKEY* PHKEY;
#define HKEY_CLASSES_ROOT   ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER   ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE  ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS          ((HKEY)(uintptr_t)0x80000003)
#define HKEY_CURRENT_CONFIG ((HKEY)(uintptr_t)0x80000005)
#define ERROR_SUCCESS 0L
#define REG_SZ        1u
#define REG_EXPAND_SZ 2u
#define REG_BINARY    3u
#define REG_DWORD     4u
#define REG_DWORD_LITTLE_ENDIAN 4u
#define KEY_QUERY_VALUE   0x0001u
#define KEY_SET_VALUE     0x0002u
#define KEY_CREATE_SUB_KEY 0x0004u
#define KEY_ENUMERATE_SUB_KEYS 0x0008u
#define KEY_READ   ((REGSAM)0x20019)
#define KEY_WRITE  ((REGSAM)0x20006)
#define KEY_ALL_ACCESS ((REGSAM)0xF003F)
LSTATUS __stdcall RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
LSTATUS __stdcall RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions,
                        REGSAM samDesired, const SECURITY_ATTRIBUTES* lpSecurityAttributes,
                        PHKEY phkResult, LPDWORD lpdwDisposition);
LSTATUS __stdcall RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                         LPBYTE lpData, LPDWORD lpcbData);
LSTATUS __stdcall RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType,
                       const BYTE* lpData, DWORD cbData);
LSTATUS __stdcall RegDeleteValueA(HKEY hKey, LPCSTR lpValueName);
LSTATUS __stdcall RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey);
LSTATUS __stdcall RegCloseKey(HKEY hKey);
LSTATUS __stdcall RegOpenKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult);      // Application.cpp boot migration
LSTATUS __stdcall RegCreateKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult);
LSTATUS __stdcall RegFlushKey(HKEY hKey);

// --- MSVC secure CRT (_s functions) — game TUs call these relying on the
// --- MSVC CRT; glibc has none. Implemented with MSVC semantics in the shim
// --- (named call sites: Fog/Debug.cpp's symbol-path plumbing so far).
errno_t strncpy_s(char* strDest, size_t numberOfElements, const char* strSource, size_t count);

// --- memory status (winbase.h) — Fog/Memory.cpp's OOM diagnostics; the shim
// --- reports from /proc/meminfo (sysinfo), the real thing the PE build reads.
void __stdcall GlobalMemoryStatus(MEMORYSTATUS* lpBuffer);

// --- service control manager (winsvc.h) — Fog/Engine/Application's Service
// --- (DIABLO2SRV). A desktop game is not a service on Linux: the shim set
// --- fails soft (dispatcher returns FALSE, "not a service"), the same shape
// --- as the game's own not-running-as-service path on Windows. SC_HANDLE is
// --- the tree's own spelling (Fog/Engine/Application/Service.h:
// --- `typedef struct SC_HANDLE__ * SC_HANDLE`) — only the fwd tag here.
struct SC_HANDLE__;
struct _SERVICE_TABLE_ENTRYA {
    char* lpServiceName;
    void* lpServiceProc;
};
BOOL __stdcall StartServiceCtrlDispatcherA(const _SERVICE_TABLE_ENTRYA* lpServiceStartTable);
BOOL __stdcall SetServiceStatus(SERVICE_STATUS_HANDLE hServiceStatus, _SERVICE_STATUS* lpServiceStatus);

// --- thread walking (tlhelp32.h) — Fog/File.cpp's crash-report thread walk
// --- (CRASH_EnumerateThreads, THREADENTRY32 dwSize 0x1c = the canonical x86
// --- layout). CreateToolhelp32Snapshot is vp_host_api.h's declaration; the
// --- shim snapshots /proc/<pid>/task, so the report lists real threads.
typedef struct tagTHREADENTRY32 {
    DWORD dwSize;               /* 0x00 */
    DWORD cntUsage;             /* 0x04 */
    DWORD th32ThreadID;         /* 0x08 */
    DWORD th32OwnerProcessID;   /* 0x0C */
    LONG  tpBasePri;            /* 0x10 */
    LONG  tpDeltaPri;           /* 0x14 */
    DWORD dwFlags;              /* 0x18 */
} THREADENTRY32;                /* 0x1C */
typedef THREADENTRY32* LPTHREADENTRY32;
BOOL __stdcall Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte);
BOOL __stdcall Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

// --- error messages (winbase.h) — Fog/File.cpp + Storm/SErr.cpp format
// --- GetLastError-style codes into crash/log lines; the shim maps the
// --- known code families (errno, WSA codes) onto strerror-backed text.
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000u
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200u
DWORD __stdcall FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                     DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list* Arguments);

// --- unhandled-exception filter (winbase.h) — Fog/File.cpp installs the
// --- crash-report filter (the d2crash-*.txt writer); the shim stores it and
// --- the signal/raise path invokes it. The M1 exception shim's spine. The
// --- filter callback is __stdcall exactly as winbase.h spells it (the tree
// --- passes __stdcall-named functions at Fog/File.cpp:397).
typedef LONG (__stdcall *LPTOP_LEVEL_EXCEPTION_FILTER)(EXCEPTION_POINTERS* lpExceptionPointers);
LPTOP_LEVEL_EXCEPTION_FILTER __stdcall SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);

// --- thread primitives (winbase.h) — Fog/Engine/Application.cpp's crash-path
// --- suspend (OpenThread/Suspend/ResumeThread + GetThreadContext),
// --- Fog/Src/LogManager.cpp's thread self-identity (GetCurrentThread
// --- pseudo-handle via PFN_GetCurrentThread, GetThreadPriority SDK form,
// --- SetThreadPriority). i386 ids map 1:1 onto tids; thread priorities
// --- store last-set (advisory; scheduler mapping is a later shim note).
HANDLE __stdcall OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId);
#define THREAD_SUSPEND_RESUME 0x0002u
#define THREAD_GET_CONTEXT    0x0008u
#define THREAD_QUERY_INFORMATION 0x0040u
DWORD __stdcall SuspendThread(HANDLE hThread);
DWORD __stdcall ResumeThread(HANDLE hThread);
BOOL __stdcall GetThreadContext(HANDLE hThread, CONTEXT* lpContext);   // CONTEXT = the tree's _CONTEXT (Fog/Debug.h)
BOOL __stdcall SetThreadContext(HANDLE hThread, const CONTEXT* lpContext);
HANDLE __stdcall GetCurrentThread();     // (-2) pseudo-handle, as on Windows
int __stdcall GetThreadPriority(HANDLE hThread);   // SDK form: priority, THREAD_PRIORITY_ERROR_RETURN on failure
#define THREAD_PRIORITY_ERROR_RETURN 0x7fffffff
BOOL __stdcall SetThreadPriority(HANDLE hThread, int nPriority);
#define THREAD_PRIORITY_NORMAL    0
#define THREAD_PRIORITY_HIGHEST   2
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_BELOW_NORMAL -1
#define THREAD_PRIORITY_LOWEST    -2
#define THREAD_PRIORITY_IDLE      -15

// --- SEH translator + kernel events + COM allocator — the last three named
// --- sites in the slice: Fog/Debug.cpp installs its crash translator
// --- (_set_se_translator; the exception shim invokes it on raise), Fog/
// --- Engine/Application.cpp probes the boot event (OpenEventA — the
// --- single-instance handshake; M1 fail-soft) and frees shell-allocated
// --- memory (CoTaskMemFree → free).
void* __stdcall _set_se_translator(void* pfnTranslator);
HANDLE __stdcall OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
#define EVENT_MODIFY_STATE 0x0002u
void __stdcall CoTaskMemFree(void* pv);

// --- structured exceptions (winbase.h) — Fog/Memory.cpp's OOM raise walks
// --- Storm's fault-handler chain (StormFaultHandlerNode); the shim keeps
// --- the registered handler list (AddVectoredExceptionHandler, declared in
// --- vp_host_api.h) and walks it on raise. PLAN.md §6 M1 exception shim;
// --- the d2crash-*.txt linux twin consumes the same records.
#define EXCEPTION_NONCONTINUABLE 0x1u
void __stdcall RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags,
                    DWORD nNumberOfArguments, const ULONG_PTR* lpArguments);

// --- text conversion (stringapiset.h) — Fog/File.cpp + Fog/Debug.cpp +
// --- Fog/Engine/Application.cpp convert UTF-16 paths/log lines (the M1 call
// --- sites use CP_UTF8/65001 and CP_ACP). The shapes follow d2_platform.h's
// --- own _WIN32 wrappers (WCHAR = uint16_t; the build also compiles
// --- -fshort-wchar so L"..." literals and LPWSTR agree, as on MSVC).
typedef const char* LPCCH;
typedef int* LPBOOL;
int __stdcall MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar);
int __stdcall WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar);
#define CP_ACP   0u
#define CP_UTF8  65001u
#define MB_ERR_INVALID_CHARS 0x00000008u

// --- wide/unicode module + file + process surface (winbase/winnt) — the
// --- same TUs' wide paths. The W entry points convert to UTF-8 and ride the
// --- A shims where the semantic exists; process creation fails soft in M1
// --- (the crash-report helper process is a later shim round, named site
// --- Fog/File.cpp:587).
HMODULE __stdcall LoadLibraryW(LPCWSTR lpLibFileName);
BOOL __stdcall CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                    SECURITY_ATTRIBUTES* lpProcessAttributes, SECURITY_ATTRIBUTES* lpThreadAttributes,
                    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                    LPCWSTR lpCurrentDirectory, void* lpStartupInfo, void* lpProcessInformation);
DWORD __stdcall GetProcessId(HANDLE hProcess);

// --- more user/kernel glue with named sites in the slice -------------------
BOOL __stdcall GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize);   // Fog/Src/LogManager.cpp log headers
int __stdcall LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax); // Storm/SErr.cpp
typedef struct _WIN32_FIND_DATAA WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;  // body: Storm/Source/SFile.h (!defined(_WIN32))
HANDLE __stdcall FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);  // Storm/SErr.cpp dir scan
BOOL __stdcall FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
BOOL __stdcall FindClose(HANDLE hFindFile);
BOOL __stdcall IsWindow(HWND hWnd);   // Storm/SErr.cpp's window-owner check (no window system in M1: FALSE)
BOOL __stdcall IsWindowVisible(HWND hWnd);   // Storm/SErr.cpp:740, same answer

// --- shell paths (shlobj.h) — Fog/Src/LogManager.cpp's log directory under
// --- "My Documents" (CSIDL_PERSONAL); the shim maps it to the XDG documents
// --- dir (or $HOME when absent).
#define CSIDL_PERSONAL 0x0005
HRESULT __stdcall SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath);

// --- time conversion (winbase.h) — Fog/Src/LogManager.cpp stamps log lines;
// --- pure SYSTEMTIME→FILETIME math in the shim (the canonical formula).
BOOL __stdcall SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, LPFILETIME lpFileTime);

// CreateFileW: Fog/Debug.cpp's report-file probe (the PE opens debug logs
// through it); the shim fails soft like the PE on a write-protected dir.
HANDLE __stdcall CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   SECURITY_ATTRIBUTES* lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

// --- wide find/copy/file + process/toolhelp W surface — the same crash/log
// --- TUs' wide paths (named sites: Fog/File.cpp's FindFirstFileW/_wfopen/
// --- CopyFileW, Fog/Engine/Application.cpp's MODULEENTRY32W module walk).
// --- The W entry points convert to UTF-8 and ride the A shims.
HANDLE __stdcall FindFirstFileW(LPCWSTR lpFileName, void* lpFindFileData);  // W32_FIND_DATAW is tree-spelled; see shim note
BOOL __stdcall FindNextFileW(HANDLE hFindFile, void* lpFindFileData);
BOOL __stdcall CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists);
BOOL __stdcall CreateDirectoryW(LPCWSTR lpPathName, SECURITY_ATTRIBUTES* lpSecurityAttributes);
DWORD __stdcall GetFileAttributesW(LPCWSTR lpFileName);
#define INVALID_FILE_ATTRIBUTES_W INVALID_FILE_ATTRIBUTES
HMODULE __stdcall GetModuleHandleW(LPCWSTR lpModuleName);
DWORD __stdcall GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer);
BOOL __stdcall IsBadStringPtrA(LPCSTR lpsz, UINT ucchMax);
DWORD __stdcall GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);   // Fog/Debug.cpp cwd probe
DWORD __stdcall GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);  // Fog/Debug.cpp _NT_SYMBOL_PATH
LSTATUS __stdcall RegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName, LPDWORD lpcchValueName,
                      LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);  // Application.cpp value copy
BOOL __stdcall SetCurrentDirectoryA(LPCSTR lpPathName);   // Application.cpp cwd switch
SHORT __stdcall GetAsyncKeyState(int nVirtKey);   // ErrorManager.cpp's dump-key probe; M1: 0 (no key state)
BOOL __stdcall GetUserNameA(LPSTR lpBuffer, LPDWORD pcbBuffer);   // ClientModeInGame.cpp; M1: getpwuid-backed
// The _Wrappers CRT-filter declarations: d2_platform.h spells the namespace
// only under _WIN32; the linux build compiles ErrorManager.cpp's references
// against these (definitions: support/d2_crt_impl.cpp under D2LINUX_GAME).
// C++ linkage, outside the extern "C" block — same as the tree's.
namespace _Wrappers {
void CRT_ExceptionFilter1(EHExceptionRecord* pExceptionRecord, EHRegistrationNode* pRegistrationNode, CONTEXT* pContext, void* pDispatcherContext);
void CRT_ExceptionFilter2(EHExceptionRecord* pExceptionRecord, EHRegistrationNode* pRegistrationNode, CONTEXT* pContext, void* pDispatcherContext);
void CRT_ExceptionFilter3(EHExceptionRecord* pExceptionRecord, EHRegistrationNode* pRegistrationNode, CONTEXT* pContext, void* pDispatcherContext);
}  // namespace _Wrappers
BOOL __stdcall DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
                     HANDLE* lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
#define DUPLICATE_SAME_ACCESS 0x0002u

// --- virtual memory query (winbase.h) — Fog/Debug.cpp's stack walker finds
// --- the module/section an address belongs to; the shim answers from
// --- /proc/self/maps (the same map the PE build's VirtualQuery walks).
#define MEM_COMMIT   0x1000u
#define MEM_RESERVE  0x2000u
#define MEM_FREE     0x10000u
#define PAGE_READWRITE 0x04u
#define PAGE_READONLY  0x02u
#define PAGE_EXECUTE_READ 0x20u
#define PAGE_EXECUTE_READWRITE 0x40u
#define PAGE_NOACCESS 0x01u
// MEMORY_BASIC_INFORMATION body: the tree's own Fog/3rdparty/crashy.h
// (offset-annotated, !defined(_WIN32)) — only the fwd typedef here; this
// header parses before it.
typedef struct _MEMORY_BASIC_INFORMATION MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;
SIZE_T __stdcall VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
SIZE_T __stdcall VirtualQueryEx(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
BOOL __stdcall GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault,
                              LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName);  // Fog/Engine/Application.cpp ini read
FILE* _wfopen(const uint16_t* lpFileName, const uint16_t* lpMode);          // MSVC CRT wide fopen; the decomp's
// wide strings are ushort in the bodies (Fog/File.cpp FILETOOLS_*_UTF8)

// --- debugging support (winbase/psapi) — Fog/Debug.cpp's symbolizer probes
// --- (IsDebuggerPresent → /proc/self/status TracerPid; GetModuleInformation's
// --- _MODULEINFO image base/size for the elf module walk).
#define MODULEINFO_SIZE_FIELD 0  // (the tree passes _MODULEINFO* opaquely; shim fills LPVOID lpBase + DWORD SizeOfImage)
struct _MODULEINFO {
    LPVOID lpBaseOfDll;     // psapi's canonical member spellings (Fog/Debug.cpp reads them)
    DWORD SizeOfImage;
    DWORD EntryPoint;
};
typedef _MODULEINFO MODULEINFO, *LPMODULEINFO;
BOOL __stdcall GetModuleInformation(HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb);
BOOL __stdcall IsDebuggerPresent();

// --- process + version + disk (winbase.h) — Storm/SErr.cpp's helper-process
// --- plumbing (GetExitCodeProcess/TerminateProcess: fail-soft until the M1
// --- process-shim round), version self-report (GetVersionExA/GetVersion: the
// --- linux build reports a fixed, documented 1.14d-host shape), disk free
// --- (GetDiskFreeSpaceA → statvfs), DeleteFileA (LogManager log rotation).
BOOL __stdcall GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode);
BOOL __stdcall TerminateProcess(HANDLE hProcess, UINT uExitCode);
DWORD __stdcall GetVersion();
// ANSI + wide version structs ( LogManager.cpp uses the A form) — the W body
// lives in vp_host_api.h; the A body mirrors it with char[128].
// _OSVERSIONINFOA body: the tree's own Fog/Src/ErrorManager.h (offset-annotated,
// !defined(_WIN32)) — only the fwd declaration here, this header parses first.
struct _OSVERSIONINFOA;
BOOL __stdcall GetVersionExA(_OSVERSIONINFOA* lpVersionInformation);
BOOL __stdcall DeleteFileA(LPCSTR lpFileName);
BOOL __stdcall GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
                       LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
DWORD __stdcall GetModuleFileNameExA(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
typedef struct tagMODULEENTRY32W {
    DWORD dwSize;
    DWORD th32ModuleID;
    DWORD th32ProcessID;
    DWORD GlblcntUsage;
    DWORD ProccntUsage;
    BYTE* modBaseAddr;
    DWORD modBaseSize;
    HMODULE hModule;
    uint16_t szModule[MAX_MODULE_NAME32 + 1];
    uint16_t szExePath[MAX_PATH];
} MODULEENTRY32W, *LPMODULEENTRY32W;
BOOL __stdcall Module32FirstW(HANDLE hSnapshot, LPMODULEENTRY32W lpme);
BOOL __stdcall Module32NextW(HANDLE hSnapshot, LPMODULEENTRY32W lpme);

// --- winsock (winsock2.h) — Fog/Src/Safesock (Safesock.h's fallback types
// --- are stood down via D2_LINUX_WINSOCK_SUPPLIED; these are the canonical
// --- winsock shapes) and Fog/Src/D2Net. Handles are fds: closesocket->close,
// --- WSAGetLastError->errno map, FIONBIO for non-blocking mode. M1 covers
// --- the Safesock client path; the full Winsock surface is M5.
// sockaddr/in_addr: the tree's own non-Windows shapes (D2Net/sockaddr.h,
// D2Net/in_addr.h) — only the forward tags here; those headers complete them.
struct sockaddr;
struct in_addr;
struct sockaddr_in {
    short sin_family;           /* 0x00 */
    u_short sin_port;           /* 0x02, network byte order */
    uint32_t sin_addr;          /* 0x04, IPv4, network byte order (Ghidra's word spelling) */
    uint8_t sin_zero[8];        /* 0x08 */
};
typedef struct sockaddr SOCKADDR, *PSOCKADDR, *LPSOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN, *PSOCKADDR_IN, *LPSOCKADDR_IN;

// The winsock fd_set shape. glibc's own fd_set (bits array, select(2) shape)
// is already in scope by the time game TUs parse, so the winsock spelling is
// delivered by a confined macro map: the game code says fd_set and means the
// winsock shape — the shim's select() below translates it honestly. The FD_*
// macros below REPLACE glibc's on purpose (sys/select.h is in scope by here
// through the C++ headers), so each is #undef'd first — without that, every
// TU warns "redefined" five times and the noise buries real signal (owner
// build, round 3).
#define fd_set d2_winsock_fd_set
struct d2_winsock_fd_set {
    u_int fd_count;             /* 0x00 */
    SOCKET fd_array[64];        /* 0x04, winsock's FD_SETSIZE */
};
#undef FD_SETSIZE
#undef FD_CLR
#undef FD_SET
#undef FD_ISSET
#undef FD_ZERO
#define FD_SETSIZE 64
#define FD_CLR(fd, set) d2_fd_do((set), (fd), -1)
#define FD_SET(fd, set) d2_fd_do((set), (fd), 1)
#define FD_ISSET(fd, set) d2_fd_in((set), (fd))
#define FD_ZERO(set) d2_fd_zero((set))
void d2_fd_zero(d2_winsock_fd_set* set);
void d2_fd_do(d2_winsock_fd_set* set, SOCKET fd, int nAdd);
int d2_fd_in(const d2_winsock_fd_set* set, SOCKET fd);
int __stdcall __WSAFDIsSet(SOCKET s, d2_winsock_fd_set* set);
// glibc's own select(int, fd_set*, ...) is already in scope here (the macro
// map above applies only to later spellings), so the winsock call name maps
// onto the shim's spelling instead of redeclaring.
int __stdcall d2_winsock_select(int nfds, d2_winsock_fd_set* readfds, d2_winsock_fd_set* writefds,
                     d2_winsock_fd_set* exceptfds, struct timeval* timeout);
#define select d2_winsock_select

struct _WSAData {
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char* lpVendorInfo;
};
typedef _WSAData WSAData;      // the SDK's primary spelling (Safesock.h: `typedef WSAData WSADATA;`)
typedef _WSAData WSADATA, *LPWSADATA;
#define MAKEWORD(a, b) ((uint16_t)(((uint8_t)(a)) | ((uint16_t)((uint8_t)(b))) << 8))
#define WSAEWOULDBLOCK 10035
#define WSAENOTSOCK    10038
#define FIONBIO 0x8004667e   // same value Linux and Windows spell for ioctlsocket's non-blocking cmd
// The BSD-callable family carries the winsock call names the game spells.
// glibc declares several of the same names with C++ exception specs that
// clash with re-declaration here, so every name maps onto the d2_ shim
// spelling (the same confined macro map as fd_set above); the shim's bodies
// call libc through its own private C declarations.
#define socket d2_socket
#define connect d2_connect
#define send d2_send
#define recv d2_recv
#define getsockname d2_getsockname
#define getpeername d2_getpeername
#define gethostname d2_gethostname
#define htons d2_htons
#define ntohs d2_ntohs
#define inet_addr d2_inet_addr
#define inet_ntoa d2_inet_ntoa
#define setsockopt d2_setsockopt
// The mapped family is PLAIN C linkage: glibc's own declarations ride the
// same macros wherever a game header pulls <unistd.h> (gethostname), and the
// shim's definitions match — one linkage, no exceptions-specifier fights.
SOCKET d2_socket(int af, int type, int protocol);
int __stdcall closesocket(SOCKET s);
int d2_connect(SOCKET s, const sockaddr* name, int namelen);
int d2_send(SOCKET s, const char* buf, int len, int flags);
int d2_recv(SOCKET s, char* buf, int len, int flags);
int __stdcall ioctlsocket(SOCKET s, long cmd, u_long* argp);
unsigned long d2_inet_addr(const char* cp);
char* d2_inet_ntoa(struct in_addr in);
u_short d2_htons(u_short hostshort);
u_short d2_ntohs(u_short netshort);
int d2_getsockname(SOCKET s, sockaddr* name, int* namelen);
int d2_getpeername(SOCKET s, sockaddr* name, int* namelen);
int d2_gethostname(char* name, size_t namelen) noexcept;   // noexcept: glibc's own unistd.h spelling rides the map too
int d2_setsockopt(SOCKET s, int level, int optname, const void* optval, int optlen);

// --- file mapping + file times + path (winbase.h) — Storm/SFile.cpp's
// --- MPQ-backed reads. MapViewOfFile rides mmap over the CreateFileA fd
// --- (the shim keeps an fd table for the handles it hands out); GetFileTime
// --- reports fstat timestamps; _fullpath resolves to realpath(3).
#define PAGE_READONLY_W 0x02u
#define FILE_MAP_READ 0x0004u
HANDLE __stdcall CreateFileMappingA(HANDLE hFile, SECURITY_ATTRIBUTES* lpAttributes, DWORD flProtect,
                          DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName);
LPVOID __stdcall MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh,
                     DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
BOOL __stdcall UnmapViewOfFile(LPCVOID lpBaseAddress);
BOOL __stdcall GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime,
                 LPFILETIME lpLastWriteTime);
LPSTR __stdcall _fullpath(char* absPath, const char* relPath, size_t maxLength);

// --- resources (winbase.h) — Storm/SFile.cpp's embedded-fallback probe (it
// --- looks for a resource-borne MPQ; the linux ELF has none, and the PE's
// --- FindResource NULL-fallback path is the honest answer).
HRSRC __stdcall FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
HGLOBAL __stdcall LoadResource(HMODULE hModule, HRSRC hResInfo);
LPVOID __stdcall LockResource(HGLOBAL hResData);
DWORD __stdcall SizeofResource(HMODULE hModule, HRSRC hResInfo);
BOOL __stdcall FreeResource(HGLOBAL hResData);
int __stdcall WSAStartup(WORD wVersionRequested, WSADATA* lpWSAData);
// Fog/3rdparty/crashy.cpp's blizzard-corp check — on Linux the resolver call
// simply fails (no such host), which is the honest "not inside Blizzard" answer.
// hostent: the GLIBC shape (int h_addrtype/h_length — the resolver returns
// glibc's own struct, so the callers' offsets must match it, not the winsock
// short/short x86 shape Safesock.h's guarded fallback spelled).
struct hostent {
    char* h_name;           /* 0x00 */
    char** h_aliases;       /* 0x04 */
    int h_addrtype;         /* 0x08 */
    int h_length;           /* 0x0C */
    char** h_addr_list;     /* 0x10 */
};
struct hostent* gethostbyname(const char* name);   // libc serves it; the decl makes it visible
#define AF_INET 2
#define SOCK_STREAM 1

// --- kernel objects + misc (winbase.h) — named sites: Fog/Engine/Application
// --- (boot event), Storm/WindowHandle (message pump; M1 pumps nothing),
// --- D2Hell (process spawn, drive list — fail-soft in M1).
#define WAIT_OBJECT_0_W 0
#define WAIT_TIMEOUT 258u
#define INFINITE 0xFFFFFFFFu
// CreateEventW: already declared by d2_platform.h's non-Windows branch (plain).
HANDLE __stdcall OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
BOOL __stdcall PeekMessageA(void* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
#define PM_NOREMOVE 0x0000
#define PM_REMOVE   0x0001
BOOL __stdcall GetMessageA(void* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
BOOL __stdcall TranslateMessage(const void* lpMsg);   // WindowHandle.cpp pump; M1: no-op
LRESULT __stdcall DispatchMessageA(const void* lpMsg);   // WindowHandle.cpp pump; M1: no-op
LRESULT __stdcall DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);  // WindowHandle.cpp default proc
#define WM_QUIT 0x0012
#define DRIVE_UNKNOWN 0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE 2
#define DRIVE_FIXED 3
UINT __stdcall GetDriveTypeA(LPCSTR lpRootPathName);   // D2Hell.cpp install-path probe
BOOL __stdcall GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer, DWORD nVolumeNameSize,
                           LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
                           LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
BOOL __stdcall CreateDirectoryA(LPCSTR lpPathName, SECURITY_ATTRIBUTES* lpSecurityAttributes);  // Fog/SFile.cpp save dirs
// --- wintrust / crypt32 (D2Hell.cpp's file-signature verify). The PE build
// --- asks Windows to verify Authenticode signatures; the linux twin reports
// --- "verification unavailable" (the caller's explicit fallback path) — it
// --- does not pretend to verify. Signatures follow the decompiled call
// --- shapes (HCERTSTORE/HCRYPTMSG/PCCERT_CONTEXT are void* there).
#define WINTRUST_ACTION_GENERIC_VERIFY_V2 0
typedef void* HCERTSTORE;
typedef void* HCRYPTMSG;
typedef struct _CERT_CONTEXT* PCCERT_CONTEXT;   // tag: d2_linux_compat.h
LONG __stdcall WinVerifyTrust(HWND hWnd, void* pgActionID, void* pWinTrustData);
BOOL __stdcall CryptQueryObject(DWORD dwObjectType, const void* pvObject, DWORD dwExpectedContentTypeFlags,
                       DWORD dwExpectedFormatTypeFlags, DWORD dwFlags, DWORD* pdwMsgAndCertEncodingType,
                       DWORD* pdwContentType, DWORD* pdwFormatType, HCERTSTORE* phCertStore,
                       HCRYPTMSG* phMsg, PCCERT_CONTEXT* ppvContext);
BOOL __stdcall CryptMsgGetParam(HCRYPTMSG hCryptMsg, DWORD dwParamType, DWORD dwIndex, void* pvData, DWORD* pcbData);
BOOL __stdcall CryptMsgClose(HCRYPTMSG hCryptMsg);
PCCERT_CONTEXT __stdcall CertFindCertificateInStore(HCERTSTORE hCertStore, DWORD dwCertEncodingType,
                          DWORD dwFindFlags, DWORD dwFindType, const void* pvFindPara,
                          PCCERT_CONTEXT pPrevCertContext);
DWORD __stdcall CertGetNameStringW(PCCERT_CONTEXT pCertContext, DWORD dwType, DWORD dwFlags,
                         void* pvTypePara, LPWSTR pszNameString, DWORD cchNameString);
void __stdcall CertCloseStore(HCERTSTORE hCertStore, DWORD dwFlags);
BOOL __stdcall CertFreeCertificateContext(PCCERT_CONTEXT pCertContext);
// --- local heap (winbase.h) — D2Hell.cpp's msg-info scratch allocations.
typedef void* HLOCAL;   // tree spells it too (D2Hell.h) — same-type redeclare
#define LMEM_FIXED 0x0000u
#define LMEM_MOVEABLE 0x0002u
#define LMEM_ZEROINIT 0x0040u
HLOCAL __stdcall LocalAlloc(UINT uFlags, SIZE_T uBytes);
HLOCAL __stdcall LocalFree(HLOCAL hMem);
// --- process/token (winbase/advapi) — ErrorManager.cpp's reporter context
// --- (open the faulting process, admin check). The admin check fails soft
// --- (not elevated) — the reporter's non-admin path, as on Windows.
HANDLE __stdcall OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
#define PROCESS_DUP_HANDLE 0x0040u
#define PROCESS_QUERY_INFORMATION 0x0400u
#define PROCESS_VM_READ 0x0010u
typedef uint32_t* PSID_;   // (Safesock.h's PSID is void*; ErrorManager's own spelling wins where included)
BOOL __stdcall AllocateAndInitializeSid(void* pIdentifierAuthority, BYTE nSubAuthorityCount,
                              DWORD dwSubAuthority0, DWORD dwSubAuthority1, DWORD dwSubAuthority2,
                              DWORD dwSubAuthority3, DWORD dwSubAuthority4, DWORD dwSubAuthority5,
                              DWORD dwSubAuthority6, DWORD dwSubAuthority7, void** pSid);
BOOL __stdcall CheckTokenMembership(void* TokenHandle, void* SidToCheck, BOOL* IsMember);
void* __stdcall FreeSid(void* pSid);
BOOL __stdcall MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);   // Fog/SFile.cpp save moves
BOOL __stdcall RemoveDirectoryA(LPCSTR lpPathName);                          // Fog/SFile.cpp dir cleanup
// --- wide process walk (crashy.cpp's crash-helper check)
typedef struct tagPROCESSENTRY32W {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD th32ModuleID;
    DWORD cntThreads;
    DWORD th32ParentProcessID;
    LONG  pcPriClassBase;
    DWORD dwFlags;
    uint16_t szExeFile[MAX_PATH];
} PROCESSENTRY32W, *LPPROCESSENTRY32W;
BOOL __stdcall Process32FirstW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
BOOL __stdcall Process32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
// --- MSVC secure CRT (crashy.cpp's report timestamps)
errno_t _localtime64_s(struct tm* tmDest, const long long* sourceTime);
// --- toolhelp process walk (ErrorManager.cpp's crash-helper check)
typedef struct tagPROCESSENTRY32 {
    DWORD dwSize;               /* 0x00 */
    DWORD cntUsage;             /* 0x04 */
    DWORD th32ProcessID;        /* 0x08 */
    ULONG_PTR th32DefaultHeapID; /* 0x0C */
    DWORD th32ModuleID;         /* 0x10 */
    DWORD cntThreads;           /* 0x14 */
    DWORD th32ParentProcessID;  /* 0x18 */
    LONG  pcPriClassBase;       /* 0x1C */
    DWORD dwFlags;              /* 0x20 */
    char  szExeFile[MAX_PATH];  /* 0x24 */
} PROCESSENTRY32, *LPPROCESSENTRY32;
BOOL __stdcall Process32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
BOOL __stdcall Process32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
BOOL __stdcall CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                    SECURITY_ATTRIBUTES* lpProcessAttributes, SECURITY_ATTRIBUTES* lpThreadAttributes,
                    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                    LPCSTR lpCurrentDirectory, void* lpStartupInfo, void* lpProcessInformation);
DWORD __stdcall GetLogicalDriveStringsA(DWORD nBufferLength, LPSTR lpBuffer);
void __stdcall GetSystemInfo(_SYSTEM_INFO* lpSystemInfo);   // Storm/SMem.cpp fills gSystemInfo

// --- MSVC CRT conveniences the tree calls directly (windows.h's CRT chain
// --- supplied them on the PE line) — Storm/SSTR.cpp, D2Lang/strtable.cpp.
char* _strlwr(char* string);
char* _strupr(char* string);
char* _itoa(int value, char* buffer, int radix);
char* _ultoa(unsigned long value, char* buffer, int radix);
char* _i64toa(long long value, char* buffer, int radix);
char* _ui64toa(unsigned long long value, char* buffer, int radix);

// ===========================================================================
// [d2linux M2] the user32/GDI surface the boot chain (D2Win window manager,
// D2gfx window creation, D2DDraw renderer) actually calls. Every family below
// has named call sites in the step-2 slice (the M2 inventory pass — the same
// empirical method as M1). Implemented in d2_linux_shim_user32.cpp over SDL3;
// the COM surfaces (DirectDraw/DirectSound/Smack/Bink/IJL) live in
// d2_linux_shim_ddraw.cpp. Types ride d2_linux_compat.h (WNDCLASSA/MSG/
// WINDOWPLACEMENT/MONITORINFO/DEVMODEA/TRACKMOUSEEVENT) and d2_platform.h
// (HWND/HDC/RECT/POINT/ATUM-family aliases).
// ===========================================================================

// --- window class + creation (winuser.h) — D2gfx/D2GFX.cpp registers the
// --- "Diablo II" class with the game's WndProc and creates the window;
// --- FindWindowA is the game's single-instance check (the mutex).
// (CreateWindowExA/DestroyWindow/SetWindowPos/ShowWindow/GetClientRect/
// GetSystemMetrics/GetDC/ReleaseDC/PostMessageA/SendMessageA/PostQuitMessage/
// GetKeyState/SetRect are already declared by d2_platform.h's non-Windows
// branch — the shim defines those at their EXISTING plain spellings; the M1
// fail-soft stubs for the pump family (PeekMessageA/GetMessageA/... in
// d2_linux_shim_win32.cpp) MOVE to d2_linux_shim_user32.cpp this round.)
ATOM __stdcall RegisterClassA(const WNDCLASSA* lpWndClass);
BOOL __stdcall UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance);
BOOL __stdcall UpdateWindow(HWND hWnd);
BOOL __stdcall GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL __stdcall GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT* lpwndpl);
BOOL __stdcall AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);
HWND __stdcall GetDesktopWindow(void);
BOOL __stdcall SystemParametersInfoA(UINT uiAction, UINT uiParam, void* pvParam, UINT fWinIni);
LONG __stdcall ChangeDisplaySettingsA(DEVMODEA* lpDevMode, DWORD dwFlags);
void* __stdcall MonitorFromWindow(HWND hWnd, DWORD dwFlags);   // HMONITOR (opaque)
BOOL __stdcall GetMonitorInfoA(void* hMonitor, MONITORINFO* lpmi);
HWND __stdcall FindWindowA(LPCSTR lpClassName, LPCSTR lpWindowName);
HWND __stdcall SetFocus(HWND hWnd);
BOOL __stdcall IsIconic(HWND hWnd);
BOOL __stdcall SetWindowTextA(HWND hWnd, LPCSTR lpString);

// --- message pump (winuser.h) — D2WinMain's MessagePump, MainMenus' cutscene
// --- drain, D2Client's Bink pump, Storm/WindowHandle's generic pump. The
// --- Peek/Get/Translate/Dispatch/PostQuit/DefWindowProc family keeps its M1
// --- declarations (above); M2 is where they become the real SDL-backed pump.
BOOL __stdcall TrackMouseEvent(const TRACKMOUSEEVENT* lpEventTrack);

// --- cursor (winuser.h) — the game hides the OS cursor and draws its own
// --- (ShowCursor's display-count semantics are load-bearing: HideOSCursor
// --- loops on the RETURN value until the sign flips).
int __stdcall ShowCursor(BOOL bShow);
void* __stdcall LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);  // HCURSOR
void* __stdcall SetCursor(void* hCursor);                               // HCURSOR
BOOL __stdcall SetCursorPos(int X, int Y);
BOOL __stdcall GetCursorPos(struct POINT* lpPoint);
BOOL __stdcall ClipCursor(const RECT* lpRect);

// --- GDI (wingdi.h) — D2DDraw's debug-font rasterizer ONLY (the game's real
// --- fonts are DC6-based). A NULL HDC makes the mask fill blank, which is the
// --- honest "no debug text" answer; nothing else in the slice touches GDI.
typedef uint32_t COLORREF;   // 0x00bbggrr
HDC __stdcall CreateDCA(LPCSTR lpszDriver, LPCSTR lpszDevice, LPCSTR lpszOutput, const void* lpInitData);
HFONT __stdcall CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
                  DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
                  DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality, DWORD iPitchAndFamily,
                  LPCSTR pszFaceName);
HBITMAP __stdcall CreateBitmap(int width, int height, UINT planes, UINT bitsPerPixel, const void* lpBits);
HGDIOBJ __stdcall SelectObject(HDC hdc, HGDIOBJ h);
COLORREF __stdcall SetTextColor(HDC hdc, COLORREF color);
COLORREF __stdcall SetBkColor(HDC hdc, COLORREF color);
int __stdcall SetBkMode(HDC hdc, int mode);
int __stdcall DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format);
COLORREF __stdcall GetPixel(HDC hdc, int x, int y);
BOOL __stdcall GetCharWidthA(HDC hdc, UINT iFirst, UINT iLast, int* lpBuffer);
BOOL __stdcall DeleteObject(HGDIOBJ hObject);
BOOL __stdcall DeleteDC(HDC hdc);
HGDIOBJ __stdcall GetStockObject(int i);
int __stdcall GdiSetBatchLimit(DWORD dwLimit);
HICON __stdcall LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad);

// --- clipboard (winuser.h) — D2Win/Src/D2WinEditBox.cpp's copy/paste. The
// --- moveable-memory family backs SetClipboardData's HGLOBAL hand-off.
#define GMEM_MOVEABLE_W 0x0002u
#define GMEM_DDESHARE_W 0x2000u
#define CF_TEXT_W 1
HGLOBAL __stdcall GlobalAlloc(UINT uFlags, SIZE_T dwBytes);
LPVOID __stdcall GlobalLock(HGLOBAL hMem);
BOOL __stdcall GlobalUnlock(HGLOBAL hMem);
HGLOBAL __stdcall GlobalFree(HGLOBAL hMem);
BOOL __stdcall OpenClipboard(HWND hWndNewOwner);
BOOL __stdcall CloseClipboard(void);
BOOL __stdcall EmptyClipboard(void);
HANDLE __stdcall SetClipboardData(UINT uFormat, HANDLE hMem);
HANDLE __stdcall GetClipboardData(UINT uFormat);

// --- RECT helpers (winuser.h) — the control/layout math. SetRect is already
// --- declared by d2_platform.h's block B.
BOOL __stdcall PtInRect(const RECT* lprc, struct POINT pt);
BOOL __stdcall CopyRect(LPRECT lprcDst, const RECT* lprcSrc);
BOOL __stdcall OffsetRect(LPRECT lprc, int dx, int dy);
BOOL __stdcall InflateRect(LPRECT lprc, int dx, int dy);
BOOL __stdcall IntersectRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);
BOOL __stdcall UnionRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);

// --- misc (winuser.h / kernel32) — named sites in the slice:
// --- D2Sound/D2SoundUntil.cpp (legacy VxD driver probe — fail-soft), the
// --- GetKeyState(VK_SHIFT) edit-box path (GetKeyState: d2_platform.h decl,
// --- M2 gives it real modifier state), D2GFX's version probe.
BOOL __stdcall GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize);

// --- constants (winuser.h) the slice's call sites pass — spell the canonical
// --- values so the shim's switch arms and the game's arguments agree.
#ifndef WM_CLOSE
#define WM_CLOSE 0x0010
#endif
#define WM_CHAR_W2 0x0102          // (WM_CHAR proper; the _W2 suffix avoids any tree redefinition)
#define WM_KEYDOWN_W2 0x0100
#define WM_KEYUP_W2 0x0101
#define WM_MOUSEMOVE_W2 0x0200
#define WM_LBUTTONDOWN_W2 0x0201
#define WM_LBUTTONUP_W2 0x0202
#define WM_RBUTTONDOWN_W2 0x0204
#define WM_RBUTTONUP_W2 0x0205
#define WM_XBUTTONDOWN_W2 0x0207
#define WM_XBUTTONUP_W2 0x0208
#define WM_MOUSEWHEEL_W2 0x020A
#define WM_NCMOUSEMOVE_W2 0x00A0
#define WM_MOUSELEAVE_W2 0x02A3
#define WM_ACTIVATEAPP_W2 0x001C
#define WM_SYSCOMMAND_W2 0x0112
#define WM_COMMAND_W2 0x0111
#define VK_SHIFT_W2 0x10
#define VK_CONTROL_W2 0x11
#define VK_MENU_W2 0x12
#define VK_SNAPSHOT_W2 0x2C
#define SM_CXSCREEN_W2 0
#define SM_CYSCREEN_W2 1
#define SPI_GETWORKAREA_W2 0x0030
#define TME_LEAVE_W2 0x0002
#define MONITOR_DEFAULTTONEAREST_W2 2
#define IMAGE_ICON_W2 1
#define IDC_ARROW_W2 ((LPCSTR)0x7F00)   // (LPCSTR)MAKEINTRESOURCE — the D2GFX call site's exact spelling
#define NULL_BRUSH_W2 5
#define SW_HIDE_W2 0
#define SW_SHOWNORMAL_W2 1

}  // extern "C"

#endif // _WIN32

#endif // D2_LINUX_API_H
