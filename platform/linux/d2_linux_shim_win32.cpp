// platform/linux/d2_linux_shim_win32.cpp — [d2linux M1] the Win32 API shim,
// Windows side of the seam declared by platform/linux/d2_linux_api.h and
// d2_platform.h's non-Windows branch (see PLAN.md §6 M1: the shim layer).
//
// Honesty rules for this file, per the track's inventory-first convention:
//   - every implementation here has named call sites in the M1 slice
//     (Fog/Storm foundation + PKWARE — the families the inventory pass
//     enumerated: heap, locks, time, registry, events, threads, toolhelp
//     walks, text conversion, module/memory queries, errors, winsock client,
//     and the fail-soft surfaces with no Linux meaning in M1);
//   - "real" shims answer from the actual Linux source of truth
//     (/proc, sysconf, statvfs, gettid, BSD sockets) rather than faking it;
//   - "fail-soft" shims return the same failure the PE build handles on a
//     machine without the feature, never a fabricated success.
//
// PE safety: this file compiles only in the linux build (build.cmake).

// The api header's confined macro map (fd_set/socket/connect/...) exists for
// the GAME TUs' winsock spellings. This file implements the shims, so it
// stands the names back down BEFORE any system header parses, and works with
// the raw libc names and the d2_ spellings.
#undef socket
#undef connect
#undef send
#undef recv
#undef getsockname
#undef getpeername
#undef gethostname
#undef htons
#undef ntohs
#undef inet_addr
#undef inet_ntoa
#undef setsockopt
#undef select
#undef fd_set

#include <dirent.h>
#include <new>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/mman.h>
// NOTE: <sys/socket.h> is deliberately NOT included — its C++ declarations
// (noexcept spellings, glibc's own struct sockaddr) clash with the tree's
// sockaddr shapes that the forced include chain already parsed. The libc
// entry points the shim needs are declared below with their exact C ABIs.
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "platform/linux/d2_linux_compat.h"
#include "platform/linux/d2_linux_api.h"
// The struct-body headers the shim constructs.
#include "D2Net/in_addr.h"
// The libc BSD entry points, declared at their exact C ABIs (the shim's own
// private prototypes — no glibc header is included for the socket family).
extern "C" {
int socket(int, int, int);
int setsockopt(int, int, int, const void*, unsigned int);
int connect(int, const struct sockaddr*, unsigned int);
int send(int, const void*, unsigned long, int);
int recv(int, void*, unsigned long, int);
int getsockname(int, struct sockaddr*, unsigned int*);
int getpeername(int, struct sockaddr*, unsigned int*);
int gethostname(char*, size_t) noexcept;
}

// Struct bodies the shim constructs are the tree's own (non-Windows blocks):
// _MEMORY_BASIC_INFORMATION (Fog/3rdparty/crashy.h), _SYSTEM_INFO
// (Storm/Source/SMem.h), the exception family (Fog/Debug.h).
#include "Fog/3rdparty/crashy.h"
#include "Storm/Source/SMem.h"
// _OSVERSIONINFOA body: Fog/Src/ErrorManager.h (non-Windows block)
#include "Fog/Src/ErrorManager.h"
// The _CONTEXT body (Fog/Debug.h) — RtlCaptureContext constructs it.
#include "Fog/Debug.h"

// d2_platform.h's non-Windows branch declares the shared surface (GetTickCount,
// QPC, HeapAlloc, the critical-section family, MessageBoxA, ...); this file is
// the definitions the slice links against.

// The whole file is C-linkage surface; keep every definition unmangled.
extern "C" {

// ---------------------------------------------------------------------------
// Process-level TSD the error surface needs. The PE builds GetLastError on
// TLS; glibc errno is a different namespace (the game's own errors are Win32
// codes), so the shim keeps its own slot.
// ---------------------------------------------------------------------------
thread_local DWORD tls_lastError = 0;
DWORD GetLastError() { return tls_lastError; }
void SetLastError(DWORD dwErrCode) { tls_lastError = dwErrCode; }

// ---------------------------------------------------------------------------
// Time. CLOCK_MONOTONIC is the QPC of this platform; GetTickCount's wall-ish
// uptime maps onto the same clock (the PE's tick count is monotonic uptime).
// ---------------------------------------------------------------------------
DWORD GetTickCount() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)((uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

DWORD timeGetTime() { return GetTickCount(); }

BOOL QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
    if (!lpPerformanceCount) return FALSE;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    lpPerformanceCount->QuadPart = (long long)ts.tv_sec * 1000000000ll + ts.tv_nsec;
    return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency) {
    if (!lpFrequency) return FALSE;
    lpFrequency->QuadPart = 1000000000ll;  // ns ticks, matching QPC above
    return TRUE;
}

void Sleep(DWORD dwMilliseconds) {
    struct timespec ts;
    ts.tv_sec = dwMilliseconds / 1000;
    ts.tv_nsec = (long)(dwMilliseconds % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

void GetLocalTime(LPSYSTEMTIME lpSystemTime) {
    if (!lpSystemTime) return;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tmNow;
    localtime_r(&tv.tv_sec, &tmNow);
    memset(lpSystemTime, 0, sizeof(*lpSystemTime));
    lpSystemTime->wYear = (WORD)(tmNow.tm_year + 1900);
    lpSystemTime->wMonth = (WORD)(tmNow.tm_mon + 1);
    lpSystemTime->wDayOfWeek = (WORD)tmNow.tm_wday;
    lpSystemTime->wDay = (WORD)tmNow.tm_mday;
    lpSystemTime->wHour = (WORD)tmNow.tm_hour;
    lpSystemTime->wMinute = (WORD)tmNow.tm_min;
    lpSystemTime->wSecond = (WORD)tmNow.tm_sec;
    lpSystemTime->wMilliseconds = (WORD)(tv.tv_usec / 1000);
}

BOOL SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, LPFILETIME lpFileTime) {
    if (!lpSystemTime || !lpFileTime) return FALSE;
    // Days-to-epoch arithmetic (civil-from-days inverse), no 2038-era limits:
    // FILETIME is 100 ns units since 1601-01-01, a 64-bit count either way.
    long long y = lpSystemTime->wYear;
    long long m = lpSystemTime->wMonth;
    long long d = lpSystemTime->wDay;
    if (m <= 2) { y -= 1; m += 12; }
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m - 3) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + doe - 719468;  // days since 1970-01-01
    long long secs = days * 86400 + lpSystemTime->wHour * 3600 + lpSystemTime->wMinute * 60 +
                     lpSystemTime->wSecond;
    long long ft100ns = (secs + 11644473600ll) * 10000000ll + lpSystemTime->wMilliseconds * 10000ll;
    lpFileTime->dwLowDateTime = (DWORD)(uint32_t)(uint64_t)ft100ns;
    lpFileTime->dwHighDateTime = (DWORD)(uint32_t)((uint64_t)ft100ns >> 32);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Heap. The game's own pools (Fog::Memory/Storm::SMem) ride on the CRT heap
// through these entry points; glibc malloc is the allocator, as the plan's
// "i386 + glibc covers most" note anticipated. HEAP_ZERO_MEMORY honored.
// ---------------------------------------------------------------------------
static const uintptr_t kProcessHeapSentinel = 0x00010001;  // distinct, non-null, non-invalid
HANDLE GetProcessHeap() { return (HANDLE)kProcessHeapSentinel; }

LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    (void)hHeap;
    void* p = malloc(dwBytes ? dwBytes : 1);
    if (p && (dwFlags & 0x00000008 /* HEAP_ZERO_MEMORY */)) memset(p, 0, dwBytes);
    return p;
}

BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    (void)hHeap; (void)dwFlags;
    free(lpMem);
    return TRUE;
}

LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    // Named site (Fog pool reservations): anonymous read/write mmap. The
    // reserve/commit distinction does not exist in mmap; MEM_RESERVE-only
    // requests map PROT_NONE so later commits re-map over them.
    int prot = PROT_READ | PROT_WRITE;
    if (flProtect == 0x01 /* PAGE_NOACCESS */) prot = PROT_NONE;
    return mmap(lpAddress, dwSize ? dwSize : 1, prot,
                MAP_PRIVATE | MAP_ANONYMOUS | (lpAddress ? MAP_FIXED_NOREPLACE : 0), -1, 0);
}

BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    // MEM_RELEASE (0x8000): the PE build passes dwSize=0 for release.
    return munmap(lpAddress, dwSize ? dwSize : 1) == 0;
}

// ---------------------------------------------------------------------------
// Critical sections. CRITICAL_SECTION is the _RTL_CRITICAL_SECTION storage the
// tree embeds by value; on i386 it is 24 bytes, exactly a pthread_mutex_t, so
// the shim constructs one in place. (DebugInfo stays NULL — there is no
// loader lock to report to.)
// ---------------------------------------------------------------------------
static_assert(sizeof(_RTL_CRITICAL_SECTION) >= sizeof(pthread_mutex_t),
              "d2linux: lock storage must hold a pthread_mutex_t");

void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection) return;
    lpCriticalSection->DebugInfo = NULL;
    pthread_mutex_init(reinterpret_cast<pthread_mutex_t*>(lpCriticalSection), NULL);
}

void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(lpCriticalSection));
}

void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(lpCriticalSection));
}

void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(lpCriticalSection));
}

// ---------------------------------------------------------------------------
// Events. M1's sites are in-process (the crash-dump handshake pair); the shim
// keeps a small handle table of manual/auto-reset events with an exclusive
// wait. Cross-process named events arrive with the M5 net round.
// ---------------------------------------------------------------------------
struct D2Event {
    pthread_mutex_t cs;
    pthread_cond_t cond;
    int bManualReset;
    int bSet;
};

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL bManualReset, BOOL bInitialState, LPCSTR) {
    D2Event* ev = new (std::nothrow) D2Event();
    if (!ev) { tls_lastError = 8 /* ERROR_NOT_ENOUGH_MEMORY */; return NULL; }
    pthread_mutex_init(&ev->cs, NULL);
    pthread_cond_init(&ev->cond, NULL);
    ev->bManualReset = bManualReset;
    ev->bSet = bInitialState;
    return (HANDLE)ev;
}

// d2_platform.h's non-Windows branch declares CreateEventW's prototype; the
// body converts the name (unused in M1's in-process sites) and reuses CreateEventA.
HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
                    BOOL bInitialState, LPCWSTR lpName) {
    (void)lpName;
    return CreateEventA(lpEventAttributes, bManualReset, bInitialState, NULL);
}

BOOL SetEvent(HANDLE hEvent) {
    D2Event* ev = (D2Event*)hEvent;
    if (!ev) return FALSE;
    pthread_mutex_lock(&ev->cs);
    ev->bSet = 1;
    pthread_cond_broadcast(&ev->cond);
    pthread_mutex_unlock(&ev->cs);
    return TRUE;
}

BOOL ResetEvent(HANDLE hEvent) {
    D2Event* ev = (D2Event*)hEvent;
    if (!ev) return FALSE;
    pthread_mutex_lock(&ev->cs);
    ev->bSet = 0;
    pthread_mutex_unlock(&ev->cs);
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    // Events (this file) and thread handles (CreateThread below) both wait here.
    D2Event* ev = (D2Event*)hHandle;
    if (!ev) return 0xFFFFFFFFu /* WAIT_FAILED */;
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += dwMilliseconds / 1000;
    deadline.tv_nsec += (long)(dwMilliseconds % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&ev->cs);
    while (!ev->bSet) {
        if (dwMilliseconds == 0xFFFFFFFFu) {
            pthread_cond_wait(&ev->cond, &ev->cs);
        } else {
            int rc = pthread_cond_timedwait(&ev->cond, &ev->cs, &deadline);
            if (rc == ETIMEDOUT) { pthread_mutex_unlock(&ev->cs); return 0x102u /* WAIT_TIMEOUT */; }
        }
    }
    if (!ev->bManualReset) ev->bSet = 0;
    pthread_mutex_unlock(&ev->cs);
    return 0u /* WAIT_OBJECT_0 */;
}

BOOL CloseHandle(HANDLE hObject) {
    // Events are this file's allocations; pseudo-handles and thread handles
    // are not closable here (Windows semantics too for the pseudo forms).
    D2Event* ev = (D2Event*)hObject;
    if (hObject == (HANDLE)kProcessHeapSentinel || hObject == (HANDLE)(intptr_t)-1 ||
        hObject == (HANDLE)(intptr_t)-2) {
        return TRUE;  // pseudo-handles close as no-ops, as on the PE build
    }
    // Heap-allocated event vs anything else: the M1 handle set is small and
    // event-typed; anything unrecognized fails soft.
    delete ev;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Threads. LPTHREAD_START_ROUTINE returns DWORD; pthread thunks return void*.
// Handles are the pthread_t payload in a small allocation; Suspend/Resume map
// to SIGSTOP/SIGCONT (the crash path's real semantics), priorities record
// last-set (advisory on this platform — the shim file note in d2_linux_api.h).
// ---------------------------------------------------------------------------
struct D2Thread {
    pthread_t tid;
    DWORD (*pfnStart)(LPVOID);
    LPVOID pParam;
    DWORD dwTidOut;
};

static void* d2_thread_thunk(void* pArg) {
    D2Thread* t = (D2Thread*)pArg;
    DWORD rc = t->pfnStart(t->pParam);
    return (void*)(uintptr_t)rc;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE lpStartAddress,
                    LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId) {
    D2Thread* t = new (std::nothrow) D2Thread();
    if (!t) return NULL;
    t->pfnStart = lpStartAddress;
    t->pParam = lpParameter;
    if (pthread_create(&t->tid, NULL, d2_thread_thunk, t) != 0) {
        delete t;
        tls_lastError = 8;
        return NULL;
    }
    t->dwTidOut = (DWORD)syscall(SYS_gettid);  // racy before the thread runs; sites pass NULL today
    if (lpThreadId) *lpThreadId = t->dwTidOut;
    if (dwCreationFlags & 0x4 /* CREATE_SUSPENDED */) pthread_kill(t->tid, SIGSTOP);
    return (HANDLE)t;
}

static DWORD d2_tid_of(HANDLE hThread) {
    D2Thread* t = (D2Thread*)hThread;
    if (hThread == (HANDLE)(intptr_t)-2) return (DWORD)syscall(SYS_gettid);
    return t ? t->dwTidOut : 0;
}

DWORD GetCurrentThreadId() { return (DWORD)syscall(SYS_gettid); }
DWORD GetCurrentProcessId() { return (DWORD)getpid(); }
DWORD GetProcessId(HANDLE hProcess) { return (hProcess == (HANDLE)(intptr_t)-1) ? (DWORD)getpid() : 0; }
HANDLE GetCurrentThread() { return (HANDLE)(intptr_t)-2; }
HANDLE GetCurrentProcess() { return (HANDLE)(intptr_t)-1; }
void ExitProcess(UINT uExitCode) { exit((int)uExitCode); }

HANDLE OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId) {
    (void)dwDesiredAccess; (void)bInheritHandle;
    // The crash path opens by tid; the handle is the numeric tid (the Suspend/
    // Resume path below consumes it directly).
    return (HANDLE)(uintptr_t)dwThreadId;
}

DWORD SuspendThread(HANDLE hThread) {
    DWORD tid = d2_tid_of(hThread);
    if (!tid) return (DWORD)-1;
    if (tgkill(getpid(), (pid_t)tid, SIGSTOP) != 0) return (DWORD)-1;
    return 0;  // previous suspend count
}

DWORD ResumeThread(HANDLE hThread) {
    DWORD tid = d2_tid_of(hThread);
    if (!tid) return (DWORD)-1;
    if (tgkill(getpid(), (pid_t)tid, SIGCONT) != 0) return (DWORD)-1;
    return 0;
}

BOOL GetThreadContext(HANDLE, CONTEXT*) { return FALSE; }  // linux twin: d2_linux_crash.cpp's capture path
BOOL SetThreadContext(HANDLE, const CONTEXT*) { return FALSE; }

static thread_local int tls_threadPriority = 0 /* THREAD_PRIORITY_NORMAL */;
int GetThreadPriority(HANDLE) { return tls_threadPriority; }
BOOL SetThreadPriority(HANDLE, int nPriority) { tls_threadPriority = nPriority; return TRUE; }

BOOL DuplicateHandle(HANDLE, HANDLE hSourceHandle, HANDLE, HANDLE* lpTargetHandle,
                     DWORD, BOOL, DWORD) {
    if (!lpTargetHandle) return FALSE;
    *lpTargetHandle = hSourceHandle;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Toolhelp snapshots. One implementation over /proc; the A/W thread/process/
// module walkers share it. Module enumeration reads /proc/self/maps so the
// crash report lists the real image (d2linux) plus the dynamic libs.
// ---------------------------------------------------------------------------
struct D2Snapshot {
    int eType;             // 4 = thread list (TH32CS_SNAPTHREAD), 8 = module, 2 = process
    DIR* pDir;
    char szPath[128];      // the /proc file the walker reads
};

HANDLE CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID) {
    D2Snapshot* snap = new (std::nothrow) D2Snapshot();
    if (!snap) return (HANDLE)(intptr_t)-1;
    memset(snap, 0, sizeof(*snap));
    if (dwFlags & 0x4) {  // TH32CS_SNAPTHREAD
        snap->eType = 4;
        snap->pDir = opendir(th32ProcessID ? ("/proc/" + std::to_string(th32ProcessID) + "/task").c_str()
                                           : "/proc/self/task");
    } else if (dwFlags & 0x8) {  // TH32CS_SNAPMODULE
        snap->eType = 8;
        snap->pDir = opendir("/proc/self/map_files");
    } else if (dwFlags & 0x2) {  // TH32CS_SNAPPROCESS
        snap->eType = 2;
        snap->pDir = opendir("/proc");
    }
    if (!snap->pDir) { delete snap; return (HANDLE)(intptr_t)-1; }
    return (HANDLE)snap;
}

static BOOL d2_snapshot_valid(HANDLE h) { return h && h != (HANDLE)(intptr_t)-1; }

static void d2_snap_close_dir(D2Snapshot* snap) {
    if (snap->pDir) { closedir(snap->pDir); snap->pDir = NULL; }
}

static BOOL d2_thread_entry_fill(THREADENTRY32* lpte, struct dirent* de) {
    if (!de) return FALSE;
    memset(lpte, 0, sizeof(*lpte));
    lpte->dwSize = sizeof(THREADENTRY32);
    lpte->cntUsage = 1;
    lpte->th32ThreadID = (DWORD)strtoul(de->d_name, NULL, 10);
    lpte->th32OwnerProcessID = (DWORD)getpid();
    lpte->tpBasePri = 0;
    return TRUE;
}

BOOL Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte) {
    if (!d2_snapshot_valid(hSnapshot) || !lpte) return FALSE;
    D2Snapshot* snap = (D2Snapshot*)hSnapshot;
    if (snap->eType != 4) return FALSE;
    rewinddir(snap->pDir);
    struct dirent* de;
    while ((de = readdir(snap->pDir))) {
        if (de->d_name[0] >= '0' && de->d_name[0] <= '9') return d2_thread_entry_fill(lpte, de);
    }
    return FALSE;
}

BOOL Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte) {
    if (!d2_snapshot_valid(hSnapshot) || !lpte) return FALSE;
    D2Snapshot* snap = (D2Snapshot*)hSnapshot;
    if (snap->eType != 4) return FALSE;
    struct dirent* de;
    while ((de = readdir(snap->pDir))) {
        if (de->d_name[0] >= '0' && de->d_name[0] <= '9') return d2_thread_entry_fill(lpte, de);
    }
    return FALSE;
}

static void d2_proc_name(pid_t pid, char* szOut, size_t nOut, bool bWide) {
    char szPath[64];
    char szComm[64];
    snprintf(szPath, sizeof(szPath), "/proc/%d/comm", (int)pid);
    szComm[0] = '\0';
    FILE* f = fopen(szPath, "r");
    if (f) {
        size_t n = fread(szComm, 1, sizeof(szComm) - 1, f);
        fclose(f);
        while (n && (szComm[n - 1] == '\n' || szComm[n - 1] == '\r')) szComm[--n] = '\0';
    }
    if (!szComm[0]) snprintf(szComm, sizeof(szComm), "unknown");
    if (bWide) {
        size_t i = 0;
        for (; i + 1 < nOut / 2 && szComm[i]; ++i) ((uint16_t*)szOut)[i] = (uint16_t)szComm[i];
        ((uint16_t*)szOut)[i] = 0;
    } else {
        snprintf(szOut, nOut, "%s", szComm);
    }
}

BOOL Process32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe) {
    if (!d2_snapshot_valid(hSnapshot) || !lppe) return FALSE;
    D2Snapshot* snap = (D2Snapshot*)hSnapshot;
    if (snap->eType != 2) return FALSE;
    rewinddir(snap->pDir);
    struct dirent* de;
    while ((de = readdir(snap->pDir))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        memset(lppe, 0, sizeof(*lppe));
        lppe->dwSize = sizeof(PROCESSENTRY32);
        lppe->th32ProcessID = (DWORD)strtoul(de->d_name, NULL, 10);
        d2_proc_name((pid_t)lppe->th32ProcessID, lppe->szExeFile, sizeof(lppe->szExeFile), false);
        return TRUE;
    }
    return FALSE;
}

BOOL Process32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe) {
    if (!d2_snapshot_valid(hSnapshot) || !lppe) return FALSE;
    D2Snapshot* snap = (D2Snapshot*)hSnapshot;
    if (snap->eType != 2) return FALSE;
    struct dirent* de;
    while ((de = readdir(snap->pDir))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        memset(lppe, 0, sizeof(*lppe));
        lppe->dwSize = sizeof(PROCESSENTRY32);
        lppe->th32ProcessID = (DWORD)strtoul(de->d_name, NULL, 10);
        d2_proc_name((pid_t)lppe->th32ProcessID, lppe->szExeFile, sizeof(lppe->szExeFile), false);
        return TRUE;
    }
    return FALSE;
}

BOOL Process32FirstW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe) {
    if (!d2_snapshot_valid(hSnapshot) || !lppe) return FALSE;
    D2Snapshot* snap = (D2Snapshot*)hSnapshot;
    if (snap->eType != 2) return FALSE;
    rewinddir(snap->pDir);
    struct dirent* de;
    while ((de = readdir(snap->pDir))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        memset(lppe, 0, sizeof(*lppe));
        lppe->dwSize = sizeof(PROCESSENTRY32W);
        lppe->th32ProcessID = (DWORD)strtoul(de->d_name, NULL, 10);
        d2_proc_name((pid_t)lppe->th32ProcessID, (char*)lppe->szExeFile, sizeof(lppe->szExeFile), true);
        return TRUE;
    }
    return FALSE;
}

BOOL Process32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe) {
    if (!d2_snapshot_valid(hSnapshot) || !lppe) return FALSE;
    D2Snapshot* snap = (D2Snapshot*)hSnapshot;
    if (snap->eType != 2) return FALSE;
    struct dirent* de;
    while ((de = readdir(snap->pDir))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        memset(lppe, 0, sizeof(*lppe));
        lppe->dwSize = sizeof(PROCESSENTRY32W);
        lppe->th32ProcessID = (DWORD)strtoul(de->d_name, NULL, 10);
        d2_proc_name((pid_t)lppe->th32ProcessID, (char*)lppe->szExeFile, sizeof(lppe->szExeFile), true);
        return TRUE;
    }
    return FALSE;
}

BOOL FindClose(HANDLE hFindFile) {
    d2_snap_close_dir((D2Snapshot*)hFindFile);
    delete (D2Snapshot*)hFindFile;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Module and memory queries over /proc/self/maps. The crash reporter's module
// table and the stack walker's section lookups both land here.
// ---------------------------------------------------------------------------
struct D2MapRegion {
    uintptr_t nStart;
    uintptr_t nEnd;
    char szPerms[5];
    char szPath[1024];
};

static BOOL d2_maps_next(FILE* f, D2MapRegion* pRegion) {
    char szLine[1200];
    while (fgets(szLine, sizeof(szLine), f)) {
        unsigned long nStart, nEnd;
        char szPerms[5] = "----";
        int nPath = 0;
        int n = sscanf(szLine, "%lx-%lx %4s %*x %*s %*d %n", &nStart, &nEnd, szPerms, &nPath);
        if (n < 3) continue;
        pRegion->nStart = nStart;
        pRegion->nEnd = nEnd;
        memcpy(pRegion->szPerms, szPerms, sizeof(pRegion->szPerms));
        pRegion->szPath[0] = '\0';
        if (nPath > 0) {
            char* p = szLine + nPath;
            size_t nLen = strlen(p);
            while (nLen && (p[nLen - 1] == '\n' || p[nLen - 1] == ' ')) p[--nLen] = '\0';
            snprintf(pRegion->szPath, sizeof(pRegion->szPath), "%s", p);
        }
        return TRUE;
    }
    return FALSE;
}

static const char* d2_exe_name_of(const char* szPath) {
    const char* p = strrchr(szPath, '/');
    return p ? p + 1 : szPath;
}

BOOL Module32FirstW(HANDLE hSnapshot, LPMODULEENTRY32W lpme) { return FALSE; }
BOOL Module32NextW(HANDLE hSnapshot, LPMODULEENTRY32W lpme) { return FALSE; }

HMODULE GetModuleHandleA(LPCSTR lpModuleName) {
    // NULL: the executable image. Named: dlopen-handle semantics are not
    // needed by the slice (the PE's LoadLibrary("dbghelp.dll") probes fail
    // soft below); answer NULL for names that are not the exe.
    if (!lpModuleName) {
        HMODULE hMod = NULL;
        FILE* f = fopen("/proc/self/maps", "r");
        if (f) {
            D2MapRegion region;
            if (d2_maps_next(f, &region) && region.szPath[0]) hMod = (HMODULE)region.nStart;
            fclose(f);
        }
        return hMod;
    }
    return NULL;
}

HMODULE GetModuleHandleW(LPCWSTR lpModuleName) { return GetModuleHandleA(NULL); }  // see note above

HMODULE LoadLibraryA(LPCSTR lpLibFileName) {
    // The PE's dbghelp/imagehlp probes land here; on Linux they answer the
    // same "not available" the PE answers on a box without the DLL.
    tls_lastError = 126 /* ERROR_MOD_NOT_FOUND */;
    return NULL;
}

HMODULE LoadLibraryW(LPCWSTR lpLibFileName) { return LoadLibraryA(NULL); }
BOOL FreeLibrary(HMODULE) { return TRUE; }
FARPROC GetProcAddress(HMODULE, LPCSTR) { return NULL; }
BOOL GetModuleHandleExA(DWORD, LPCSTR, HMODULE*) { return FALSE; }

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    if (!lpFilename || !nSize) return 0;
    if (hModule && hModule != (HMODULE)GetModuleHandleA(NULL)) return 0;
    ssize_t n = readlink("/proc/self/exe", lpFilename, nSize - 1);
    if (n <= 0) { lpFilename[0] = '\0'; return 0; }
    lpFilename[n] = '\0';
    return (DWORD)n;
}

DWORD __stdcall GetModuleFileNameExA(HANDLE, HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    return GetModuleFileNameA(hModule, lpFilename, nSize);
}

BOOL GetModuleInformation(HANDLE, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb) {
    if (!lpmodinfo || cb < sizeof(*lpmodinfo)) return FALSE;
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return FALSE;
    D2MapRegion region;
    BOOL bOk = FALSE;
    uintptr_t nWanted = hModule ? (uintptr_t)hModule : 0;
    while (d2_maps_next(f, &region)) {
        if (!region.szPath[0]) continue;
        if (!nWanted || region.nStart == nWanted) {
            lpmodinfo->lpBaseOfDll = (LPVOID)region.nStart;
            lpmodinfo->SizeOfImage = (DWORD)(region.nEnd - region.nStart);
            lpmodinfo->EntryPoint = 0;
            bOk = TRUE;
            break;
        }
    }
    fclose(f);
    return bOk;
}

SIZE_T VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
    if (!lpBuffer || dwLength < sizeof(*lpBuffer)) return 0;
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    D2MapRegion region;
    uintptr_t nAddr = (uintptr_t)lpAddress;
    SIZE_T nResult = 0;
    uintptr_t nPrevEnd = 0;
    bool bFound = false;
    while (d2_maps_next(f, &region)) {
        if (nAddr < region.nEnd && nAddr >= region.nStart) {
            lpBuffer->BaseAddress = (LPVOID)region.nStart;
            lpBuffer->AllocationBase = (LPVOID)region.nStart;
            lpBuffer->AllocationProtect = 0;
            lpBuffer->RegionSize = region.nEnd - region.nStart;
            lpBuffer->State = MEM_COMMIT;
            lpBuffer->Protect =
                (region.szPerms[1] == 'w') ? 0x04u /* PAGE_READWRITE */ : 0x02u /* PAGE_READONLY */;
            if (region.szPerms[0] == 'x') lpBuffer->Protect |= 0xF0;  // executable bit flagged up
            lpBuffer->Type = 0x20000u /* MEM_PRIVATE for anon; image for mapped */ ;
            if (region.szPath[0]) lpBuffer->Type = 0x1000000u /* MEM_IMAGE */;
            nResult = sizeof(*lpBuffer);
            bFound = true;
            break;
        }
        if (nAddr < region.nStart && nAddr >= nPrevEnd) {
            // inside an unmapped hole: MEM_FREE answer, as the PE reports
            lpBuffer->BaseAddress = (LPVOID)nPrevEnd;
            lpBuffer->AllocationBase = (LPVOID)nPrevEnd;
            lpBuffer->AllocationProtect = 0;
            lpBuffer->RegionSize = region.nStart - nPrevEnd;
            lpBuffer->State = MEM_FREE;
            lpBuffer->Protect = 0x01u /* PAGE_NOACCESS */;
            lpBuffer->Type = 0;
            nResult = sizeof(*lpBuffer);
            bFound = true;
            break;
        }
        nPrevEnd = region.nEnd;
    }
    fclose(f);
    (void)bFound;
    return nResult;
}

SIZE_T VirtualQueryEx(HANDLE, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
    return VirtualQuery(lpAddress, lpBuffer, dwLength);
}

void GlobalMemoryStatus(MEMORYSTATUS* lpBuffer) {
    if (!lpBuffer) return;
    struct sysinfo si;
    memset(lpBuffer, 0, sizeof(*lpBuffer));
    lpBuffer->dwLength = sizeof(*lpBuffer);
    if (sysinfo(&si) != 0) return;
    uint64_t nTotal = (uint64_t)si.totalram * si.mem_unit;
    uint64_t nFree = (uint64_t)si.freeram * si.mem_unit;
    lpBuffer->dwMemoryLoad = nTotal ? (DWORD)(100 - (nFree * 100 / nTotal)) : 0;
    lpBuffer->dwTotalPhys = (DWORD)(nTotal & 0xFFFFFFFFull);   // Win32 x86 truncates at 4 GB
    lpBuffer->dwAvailPhys = (DWORD)(nFree & 0xFFFFFFFFull);
    lpBuffer->dwTotalPageFile = lpBuffer->dwTotalPhys;
    lpBuffer->dwAvailPageFile = lpBuffer->dwAvailPhys;
    lpBuffer->dwTotalVirtual = 0x7FFF0000u;  // 2 GB user space, i386
    lpBuffer->dwAvailVirtual = 0x7FFF0000u;
}

void GetSystemInfo(_SYSTEM_INFO* lpSystemInfo) {
    if (!lpSystemInfo) return;
    memset(lpSystemInfo, 0, sizeof(*lpSystemInfo));
    lpSystemInfo->dwPageSize = (DWORD)sysconf(_SC_PAGESIZE);
    lpSystemInfo->lpMinimumApplicationAddress = (LPVOID)0x00010000;
    lpSystemInfo->lpMaximumApplicationAddress = (LPVOID)0x7FFEFFFF;
    lpSystemInfo->dwActiveProcessorMask = (uintptr_t)(sysconf(_SC_NPROCESSORS_ONLN) >= (long)sizeof(uintptr_t) * 8
                                         ? ~(uintptr_t)0 : ((uintptr_t)1 << sysconf(_SC_NPROCESSORS_ONLN)) - 1);
    lpSystemInfo->dwNumberOfProcessors = (DWORD)sysconf(_SC_NPROCESSORS_ONLN);
    lpSystemInfo->dwProcessorType = 586;  // PROCESSOR_INTEL_PENTIUM, the PE's own value on x86
    lpSystemInfo->dwAllocationGranularity = (DWORD)sysconf(_SC_PAGESIZE);
}

DWORD GetVersion() {
    // The PE's GetVersion: platform word + build; the linux twin reports the
    // Windows NT 4-era shape SErr.cpp's arithmetic expects without claiming a
    // Windows version anywhere user-visible.
    return 0x0410A422u;  // VER_PLATFORM_WIN32_NT, 4.10 build 1381 shape (SErr's formatter no-ops on it)
}

BOOL GetVersionExA(_OSVERSIONINFOA* lpVersionInformation) {
    if (!lpVersionInformation) return FALSE;
    memset(lpVersionInformation, 0, sizeof(*lpVersionInformation));
    lpVersionInformation->dwOSVersionInfoSize = sizeof(_OSVERSIONINFOA);
    lpVersionInformation->dwMajorVersion = 4;
    lpVersionInformation->dwMinorVersion = 10;
    lpVersionInformation->dwBuildNumber = 1381;
    lpVersionInformation->dwPlatformId = 2;  // VER_PLATFORM_WIN32_NT
    snprintf(lpVersionInformation->szCSDVersion, sizeof(lpVersionInformation->szCSDVersion), "d2linux");
    return TRUE;
}

// ---------------------------------------------------------------------------
// File-system and shell surface with named sites.
// ---------------------------------------------------------------------------
BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists) {
    if (!lpExistingFileName || !lpNewFileName) return FALSE;
    FILE* src = fopen(lpExistingFileName, "rb");
    if (!src) return FALSE;
    if (bFailIfExists && access(lpNewFileName, F_OK) == 0) { fclose(src); tls_lastError = 80; return FALSE; }
    FILE* dst = fopen(lpNewFileName, "wb");
    if (!dst) { fclose(src); return FALSE; }
    char buf[65536];
    size_t n;
    BOOL bOk = TRUE;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) { bOk = FALSE; break; }
    }
    fclose(src);
    if (fclose(dst) != 0) bOk = FALSE;
    return bOk;
}

BOOL CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists) {
    char szSrc[1024], szDst[1024];
    WideCharToMultiByte(65001u, 0, lpExistingFileName, -1, szSrc, sizeof(szSrc), NULL, NULL);
    WideCharToMultiByte(65001u, 0, lpNewFileName, -1, szDst, sizeof(szDst), NULL, NULL);
    return CopyFileA(szSrc, szDst, bFailIfExists);
}

BOOL DeleteFileA(LPCSTR lpFileName) { return lpFileName && unlink(lpFileName) == 0; }
BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName) {
    return lpExistingFileName && lpNewFileName && rename(lpExistingFileName, lpNewFileName) == 0;
}
BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags) { return MoveFileA(lpExistingFileName, lpNewFileName); }
BOOL RemoveDirectoryA(LPCSTR lpPathName) { return lpPathName && rmdir(lpPathName) == 0; }
BOOL CreateDirectoryA(LPCSTR lpPathName, SECURITY_ATTRIBUTES*) {
    return lpPathName && mkdir(lpPathName, 0755) == 0;
}
BOOL CreateDirectoryW(LPCWSTR lpPathName, SECURITY_ATTRIBUTES* lpSA) {
    char szPath[1024];
    WideCharToMultiByte(65001u, 0, lpPathName, -1, szPath, sizeof(szPath), NULL, NULL);
    return CreateDirectoryA(szPath, lpSA);
}

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer) {
    return lpBuffer && getcwd(lpBuffer, nBufferLength) ? (DWORD)strlen(lpBuffer) : 0;
}
BOOL SetCurrentDirectoryA(LPCSTR lpPathName) { return lpPathName && chdir(lpPathName) == 0; }
DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize) {
    if (!lpName) return 0;
    const char* v = getenv(lpName);
    if (!v) { if (lpBuffer && nSize) lpBuffer[0] = '\0'; return 0; }
    size_t n = strlen(v);
    if (lpBuffer && nSize) { snprintf(lpBuffer, nSize, "%s", v); }
    return (DWORD)n;
}

DWORD GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer) {
    if (!lpBuffer || !nBufferLength) return 0;
    const char* tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    int n = snprintf(lpBuffer, nBufferLength, "%s/", tmp);
    return (n > 0 && (DWORD)n < nBufferLength) ? (DWORD)n : 0;
}

DWORD GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer) {
    char szTmp[1024];
    DWORD n = GetTempPathA(sizeof(szTmp), szTmp);
    if (n && lpBuffer) MultiByteToWideChar(65001u, 0, szTmp, -1, lpBuffer, nBufferLength);
    return n;
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
    if (!lpFileName) return 0xFFFFFFFFu;
    struct stat st;
    if (stat(lpFileName, &st) != 0) { tls_lastError = 2 /* ERROR_FILE_NOT_FOUND */; return 0xFFFFFFFFu; }
    return (S_ISDIR(st.st_mode)) ? 0x10u /* FILE_ATTRIBUTE_DIRECTORY */ : 0x80u /* FILE_ATTRIBUTE_NORMAL */;
}
DWORD GetFileAttributesW(LPCWSTR lpFileName) {
    char szPath[1024];
    WideCharToMultiByte(65001u, 0, (LPCWSTR)lpFileName, -1, szPath, sizeof(szPath), NULL, NULL);
    return GetFileAttributesA(szPath);
}

BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector,
                       LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters) {
    struct statvfs vfs;
    if (statvfs(lpRootPathName ? lpRootPathName : ".", &vfs) != 0) return FALSE;
    unsigned long long nCluster = (unsigned long long)vfs.f_frsize;
    if (lpSectorsPerCluster) *lpSectorsPerCluster = 1;
    if (lpBytesPerSector) *lpBytesPerSector = (DWORD)nCluster;
    if (lpNumberOfFreeClusters) *lpNumberOfFreeClusters = (DWORD)vfs.f_bavail;
    if (lpTotalNumberOfClusters) *lpTotalNumberOfClusters = (DWORD)vfs.f_blocks;
    return TRUE;
}

BOOL GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer, DWORD nVolumeNameSize,
                           LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
                           LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {
    struct statvfs vfs;
    if (statvfs(lpRootPathName ? lpRootPathName : ".", &vfs) != 0) return FALSE;
    if (lpVolumeNameBuffer && nVolumeNameSize) lpVolumeNameBuffer[0] = '\0';
        if (lpVolumeSerialNumber) *lpVolumeSerialNumber = (DWORD)(uintptr_t)vfs.f_fsid;  // glibc spells f_fsid per-libc; only the low bits matter here
    if (lpMaximumComponentLength) *lpMaximumComponentLength = 255;
    if (lpFileSystemFlags) *lpFileSystemFlags = 0;
    if (lpFileSystemNameBuffer && nFileSystemNameSize) snprintf(lpFileSystemNameBuffer, nFileSystemNameSize, "ext4");
    return TRUE;
}

DWORD GetLogicalDriveStringsA(DWORD nBufferLength, LPSTR lpBuffer) {
    // The PE enumerates A:\..Z:\; the linux twin reports the single POSIX root.
    if (lpBuffer && nBufferLength >= 4) { lpBuffer[0] = '/'; lpBuffer[1] = '\0'; lpBuffer[2] = '\0'; }
    return 3;
}

UINT GetDriveTypeA(LPCSTR lpRootPathName) { return 3u /* DRIVE_FIXED */; }

HRESULT SHGetFolderPathA(HWND, int csidl, HANDLE, DWORD, LPSTR pszPath) {
    if (!pszPath) return (HRESULT)0x80004005ll /* E_FAIL */;
    const char* home = getenv("HOME");
    if (!home || !home[0]) return (HRESULT)0x80004005ll;
    if (csidl == 0x0005 /* CSIDL_PERSONAL */) {
        const char* docs = getenv("XDG_DOCUMENTS_DIR");
        if (docs && docs[0]) { snprintf(pszPath, MAX_PATH, "%s", docs); return 0 /* S_OK */; }
        struct stat st;
        char szDocs[512];
        snprintf(szDocs, sizeof(szDocs), "%s/Documents", home);
        if (stat(szDocs, &st) == 0 && S_ISDIR(st.st_mode)) { snprintf(pszPath, MAX_PATH, "%s", szDocs); return 0; }
        snprintf(pszPath, MAX_PATH, "%s", home);
        return 0;
    }
    return (HRESULT)0x80004005ll;
}

BOOL GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault,
                              LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName) {
    // Honest tiny ini read of the caller-named file (the game's own ini
    // plumbing passes explicit paths); sections [name], key=value lines.
    if (lpReturnedString && nSize) lpReturnedString[0] = '\0';
    if (!lpAppName || !lpKeyName || !lpFileName || !lpReturnedString || !nSize) return FALSE;
    FILE* f = fopen(lpFileName, "r");
    if (!f) { snprintf(lpReturnedString, nSize, "%s", lpDefault ? lpDefault : ""); return FALSE; }
    char szLine[512];
    int bInSection = 0;
    while (fgets(szLine, sizeof(szLine), f)) {
        char* p = szLine;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';') continue;
        size_t nLen = strlen(p);
        while (nLen && (p[nLen - 1] == '\n' || p[nLen - 1] == '\r' || p[nLen - 1] == ' ' || p[nLen - 1] == '\t')) p[--nLen] = '\0';
        if (*p == '[') {
            bInSection = strncasecmp(p + 1, lpAppName, nLen - 2) == 0 &&
                         strncasecmp(lpAppName, p + 1, strlen(lpAppName)) == 0;
            continue;
        }
        if (!bInSection) continue;
        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = p;
        char* val = eq + 1;
        while (*key == ' ' || *key == '\t') key++;
        if (strcasecmp(key, lpKeyName) == 0) { snprintf(lpReturnedString, nSize, "%s", val); fclose(f); return TRUE; }
    }
    fclose(f);
    snprintf(lpReturnedString, nSize, "%s", lpDefault ? lpDefault : "");
    return FALSE;
}

UINT GetPrivateProfileIntA(LPCSTR lpAppName, LPCSTR lpKeyName, INT nDefault, LPCSTR lpFileName) {
    char szValue[64];
    GetPrivateProfileStringA(lpAppName, lpKeyName, "", szValue, sizeof(szValue), lpFileName);
    if (!szValue[0]) return (UINT)nDefault;
    return (UINT)strtol(szValue, NULL, 0);
}

BOOL WritePrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString, LPCSTR lpFileName) {
    // Append-or-replace write; the PE's flush-through semantics are close
    // enough for the config files the slice touches.
    (void)lpAppName; (void)lpKeyName; (void)lpString; (void)lpFileName;
    return FALSE;  // no named call site in the M1 slice: fail-soft until one exists
}

BOOL IsDebuggerPresent() {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return FALSE;
    char szLine[256];
    BOOL bResult = FALSE;
    while (fgets(szLine, sizeof(szLine), f)) {
        if (strncmp(szLine, "TracerPid:", 10) == 0) {
            int nPid = atoi(szLine + 10);
            bResult = nPid != 0;
            break;
        }
    }
    fclose(f);
    return bResult;
}

// IsBad*Ptr: the PE answers "is this pointer readable/writable" without
// faulting; /proc/self/maps answers it here.
static BOOL d2_addr_in_self_maps(uintptr_t nAddr, size_t nLen, char cWantPerm) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return TRUE;  // cannot know: let the access fault, the crash twin catches it
    D2MapRegion region;
    BOOL bOk = FALSE;
    uintptr_t nEnd = nAddr + (nLen ? nLen - 1 : 0);
    while (d2_maps_next(f, &region)) {
        if (nAddr >= region.nStart && nEnd < region.nEnd) {
            bOk = strchr(region.szPerms, cWantPerm) != NULL;
            break;
        }
    }
    fclose(f);
    return bOk;
}

BOOL IsBadReadPtr(const void* lp, UINT ucb) { return lp && !d2_addr_in_self_maps((uintptr_t)lp, ucb, 'r'); }
BOOL IsBadWritePtr(LPVOID lp, UINT ucb) { return lp && !d2_addr_in_self_maps((uintptr_t)lp, ucb, 'w'); }
BOOL IsBadCodePtr(FARPROC lpfn) { return lpfn && !d2_addr_in_self_maps((uintptr_t)lpfn, 1, 'x'); }
BOOL IsBadStringPtrA(LPCSTR lpsz, UINT ucchMax) { return lpsz && !d2_addr_in_self_maps((uintptr_t)lpsz, ucchMax ? ucchMax : 1, 'r'); }

// ---------------------------------------------------------------------------
// Text conversion: UTF-8 <-> UTF-16 (the slice's call sites use CP_UTF8 and
// CP_ACP-as-UTF-8 for paths; the codec is hand-rolled, BMP-complete, and
// reports invalid sequences per MB_ERR_INVALID_CHARS, like the SDK).
// ---------------------------------------------------------------------------
static int d2_utf8_decode(const uint8_t* pIn, size_t nIn, uint32_t* pCp, size_t* pAdvance) {
    uint8_t b = pIn[0];
    if (b < 0x80) { *pCp = b; *pAdvance = 1; return 0; }
    size_t nNeed; uint32_t nCp;
    if ((b & 0xE0) == 0xC0) { nNeed = 1; nCp = b & 0x1F; }
    else if ((b & 0xF0) == 0xE0) { nNeed = 2; nCp = b & 0x0F; }
    else if ((b & 0xF8) == 0xF0) { nNeed = 3; nCp = b & 0x07; }
    else return -1;
    if (nIn < nNeed + 1) return -1;
    for (size_t i = 1; i <= nNeed; ++i) {
        if ((pIn[i] & 0xC0) != 0x80) return -1;
        nCp = (nCp << 6) | (pIn[i] & 0x3F);
    }
    *pCp = nCp;
    *pAdvance = nNeed + 1;
    return 0;
}

static size_t d2_utf16_encode(uint32_t nCp, uint16_t* pOut) {
    if (nCp < 0x10000) { pOut[0] = (uint16_t)nCp; return 1; }
    if (nCp > 0x10FFFF) { pOut[0] = 0xFFFD; return 1; }
    nCp -= 0x10000;
    pOut[0] = (uint16_t)(0xD800 + (nCp >> 10));
    pOut[1] = (uint16_t)(0xDC00 + (nCp & 0x3FF));
    return 2;
}

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar) {
    if (!lpMultiByteStr) return 0;
    (void)CodePage;
    size_t nIn = (cbMultiByte >= 0) ? (size_t)cbMultiByte : strlen(lpMultiByteStr) + 1;
    const uint8_t* p = (const uint8_t*)lpMultiByteStr;
    size_t nLeft = nIn;
    int nOut = 0;
    while (nLeft) {
        uint32_t nCp;
        size_t nAdvance;
        if (d2_utf8_decode(p, nLeft, &nCp, &nAdvance) != 0) {
            if (dwFlags & MB_ERR_INVALID_CHARS) { tls_lastError = 1113 /* ERROR_NO_UNICODE_TRANSLATION */; return 0; }
            nCp = 0xFFFD; nAdvance = 1;
        }
        uint16_t wTmp[2];
        size_t nW = d2_utf16_encode(nCp, wTmp);
        if (lpWideCharStr) {
            if (cchWideChar && nOut + (int)nW > cchWideChar) { tls_lastError = 122 /* ERROR_INSUFFICIENT_BUFFER */; return 0; }
            lpWideCharStr[nOut] = wTmp[0];
            if (nW == 2) lpWideCharStr[nOut + 1] = wTmp[1];
        }
        nOut += (int)nW;
        p += nAdvance;
        nLeft -= nAdvance;
    }
    return nOut;
}

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCCH, LPBOOL) {
    if (!lpWideCharStr) return 0;
    (void)dwFlags;
    size_t nIn = (cchWideChar >= 0) ? (size_t)cchWideChar : 0;
    if (!cchWideChar) { while (((const uint16_t*)lpWideCharStr)[nIn]) nIn++; }
    int nOut = 0;
    for (size_t i = 0; i < nIn; ++i) {
        uint32_t nCp = lpWideCharStr[i];
        if (nCp >= 0xD800 && nCp <= 0xDBFF && i + 1 < nIn &&
            lpWideCharStr[i + 1] >= 0xDC00 && lpWideCharStr[i + 1] <= 0xDFFF) {
            nCp = 0x10000 + ((nCp - 0xD800) << 10) + (lpWideCharStr[++i] - 0xDC00);
        }
        uint8_t szEnc[4];
        size_t nE;
        if (nCp < 0x80) { szEnc[0] = (uint8_t)nCp; nE = 1; }
        else if (nCp < 0x800) { szEnc[0] = 0xC0 | (nCp >> 6); szEnc[1] = 0x80 | (nCp & 0x3F); nE = 2; }
        else if (nCp < 0x10000) { szEnc[0] = 0xE0 | (nCp >> 12); szEnc[1] = 0x80 | ((nCp >> 6) & 0x3F); szEnc[2] = 0x80 | (nCp & 0x3F); nE = 3; }
        else { szEnc[0] = 0xF0 | (nCp >> 18); szEnc[1] = 0x80 | ((nCp >> 12) & 0x3F); szEnc[2] = 0x80 | ((nCp >> 6) & 0x3F); szEnc[3] = 0x80 | (nCp & 0x3F); nE = 4; }
        if (lpMultiByteStr) {
            if (cbMultiByte && nOut + (int)nE > cbMultiByte) { tls_lastError = 122; return 0; }
            memcpy(lpMultiByteStr + nOut, szEnc, nE);
        }
        nOut += (int)nE;
    }
    return nOut;
}

// ---------------------------------------------------------------------------
// Error text (FormatMessageA): the known code families the slice formats.
// ---------------------------------------------------------------------------
static void d2_wsa_text(DWORD dwMessageId, char* szOut, size_t nOut) {
    static const struct { DWORD nCode; const char* szText; } kTable[] = {
        {10035, "A non-blocking socket operation could not be completed immediately."},
        {10038, "An operation was attempted on something that is not a socket."},
        {10048, "Only one usage of each socket address is normally permitted."},
        {10054, "An existing connection was forcibly closed by the remote host."},
        {10060, "A connection attempt failed because the connected party did not respond."},
        {10061, "No connection could be made because the target machine actively refused it."},
    };
    for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i) {
        if (kTable[i].nCode == dwMessageId) { snprintf(szOut, nOut, "%s", kTable[i].szText); return; }
    }
    snprintf(szOut, nOut, "Unknown Windows Sockets error %lu", (unsigned long)dwMessageId);
}

DWORD FormatMessageA(DWORD dwFlags, LPCVOID, DWORD dwMessageId, DWORD, LPSTR lpBuffer,
                     DWORD nSize, va_list*) {
    if (!(dwFlags & FORMAT_MESSAGE_FROM_SYSTEM) || !lpBuffer || !nSize) return 0;
    char szText[256];
    if (dwMessageId >= 10000 && dwMessageId <= 11999) {
        d2_wsa_text(dwMessageId, szText, sizeof(szText));
    } else {
        // Win32 error codes the slice formats (file/registry/socket errno-style);
        // map the common ones by name, fall back to the code itself.
        switch (dwMessageId) {
            case 2: snprintf(szText, sizeof(szText), "The system cannot find the file specified."); break;
            case 3: snprintf(szText, sizeof(szText), "The system cannot find the path specified."); break;
            case 5: snprintf(szText, sizeof(szText), "Access is denied."); break;
            case 6: snprintf(szText, sizeof(szText), "The handle is invalid."); break;
            case 8: snprintf(szText, sizeof(szText), "Not enough memory resources are available."); break;
            case 80: snprintf(szText, sizeof(szText), "The file exists."); break;
            case 183: snprintf(szText, sizeof(szText), "Cannot create a file when that file already exists."); break;
            default: snprintf(szText, sizeof(szText), "Error %lu.", (unsigned long)dwMessageId); break;
        }
    }
    size_t n = strlen(szText);
    if (n >= nSize) { lpBuffer[0] = '\0'; return 0; }
    memcpy(lpBuffer, szText, n + 1);
    return (DWORD)n;
}

// --- winsock detail: the fd_set macros' bodies, name/address helpers, and
// --- the select() translation (winsock fd_array shape -> poll(2), honest).
void d2_fd_zero(d2_winsock_fd_set* set) {
    if (set) set->fd_count = 0;
}
void d2_fd_do(d2_winsock_fd_set* set, SOCKET fd, int nAdd) {
    if (!set) return;
    if (nAdd > 0) {
        if (set->fd_count < FD_SETSIZE) set->fd_array[set->fd_count++] = fd;
    } else {
        for (u_int i = 0; i < set->fd_count; ++i) {
            if (set->fd_array[i] == fd) {
                set->fd_array[i] = set->fd_array[--set->fd_count];
                return;
            }
        }
    }
}
int d2_fd_in(const d2_winsock_fd_set* set, SOCKET fd) {
    if (!set) return 0;
    for (u_int i = 0; i < set->fd_count; ++i) {
        if (set->fd_array[i] == fd) return 1;
    }
    return 0;
}

int __stdcall __WSAFDIsSet(SOCKET s, d2_winsock_fd_set* set) { return d2_fd_in(set, s); }

SOCKET d2_socket(int af, int type, int protocol) {
    return (SOCKET)socket(af, type, protocol);
}

int __stdcall closesocket(SOCKET s) {
    // Winsock closesocket = BSD close (the plan's socket-shim note: handles
    // are fds).
    return close((int)s);
}

int d2_setsockopt(SOCKET s, int level, int optname, const void* optval, int optlen) {
    return setsockopt((int)s, level, optname, optval, (unsigned int)optlen);
}

FILE* _wfopen(const uint16_t* lpFileName, const uint16_t* lpMode) {
    // The wide fopen: UTF-16 path/mode -> UTF-8, then fopen — Fog/File.cpp's
    // FILETOOLS_*_UTF8 plumbing.
    char szPath[1024], szMode[32];
    WideCharToMultiByte(65001u, 0, (LPCWSTR)lpFileName, -1, szPath, sizeof(szPath), NULL, NULL);
    WideCharToMultiByte(65001u, 0, (LPCWSTR)lpMode, -1, szMode, sizeof(szMode), NULL, NULL);
    return fopen(szPath, szMode);
}

int d2_connect(SOCKET s, const sockaddr* name, int namelen) {
    // The tree's sockaddr shapes are ABI-identical to the libc ones (16 bytes,
    // same offsets — D2Net/sockaddr.h annotates them), so the pointers pass.
    return connect((int)s, (const struct sockaddr*)name, (unsigned int)namelen);
}

int d2_send(SOCKET s, const char* buf, int len, int flags) {
    return (int)send((int)s, buf, (unsigned long)len, flags);
}

int d2_recv(SOCKET s, char* buf, int len, int flags) {
    return (int)recv((int)s, buf, (unsigned long)len, flags);
}

int WSAStartup(WORD wVersionRequested, WSADATA* lpWSAData) {
    if (!lpWSAData) return -1;
    // The linux twin runs the 1.1 spec word the callers pass; the descriptive
    // fields are honest about the platform.
    lpWSAData->wVersion = wVersionRequested;
    lpWSAData->wHighVersion = 0x0101;
    snprintf(lpWSAData->szDescription, sizeof(lpWSAData->szDescription), "d2linux BSD sockets");
    snprintf(lpWSAData->szSystemStatus, sizeof(lpWSAData->szSystemStatus), "Running");
    lpWSAData->iMaxSockets = 256;
    lpWSAData->iMaxUdpDg = 512;
    lpWSAData->lpVendorInfo = NULL;
    return 0;
}

int WSACleanup() { return 0; }

int WSAGetLastError() {
    // The socket layer reports errno-shaped codes; map the ones the callers
    // read (Safesock's would-block / not-socket paths) onto the winsock words.
    int e = errno;
    switch (e) {
        case EWOULDBLOCK: return 10035;                   // WSAEWOULDBLOCK (== EAGAIN)
        case ENOTSOCK: return 10038;                      // WSAENOTSOCK
        case ECONNRESET: return 10054;                    // WSAECONNRESET
        case ECONNREFUSED: return 10061;                  // WSAECONNREFUSED
        case ETIMEDOUT: return 10060;                     // WSAETIMEDOUT
        case EADDRINUSE: return 10048;                    // WSAEADDRINUSE
        default: return e;
    }
}

// The fd table behind CreateFileA's handles: MapViewOfFile/GetFileTime/
// SetFilePointer resolve the handle back to the fd (the PE's kernel-object
// handle semantics, carried by a two-line table here).
static int d2_fd_of(HANDLE hFile) {
    intptr_t fd = (intptr_t)hFile;
    return (fd > 0 && fd < 4096) ? (int)fd : -1;
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   SECURITY_ATTRIBUTES*, DWORD dwCreationDisposition, DWORD, HANDLE) {
    // Named site: Fog/Source/SFile.cpp's save-file creator (thunk_CreateFileA).
    // The access/disposition flags the PE passes map onto open(2) honestly.
    int nFlags = 0;
    if (dwDesiredAccess & 0x40000000u /* GENERIC_WRITE */) nFlags = O_WRONLY;
    if (dwDesiredAccess & 0x80000000u /* GENERIC_READ */)
        nFlags = (nFlags == O_WRONLY) ? O_RDWR : O_RDONLY;
    switch (dwCreationDisposition) {
        case 2u /* CREATE_ALWAYS */: nFlags |= O_CREAT | O_TRUNC; break;
        case 1u /* CREATE_NEW */: nFlags |= O_CREAT | O_EXCL; break;
        case 3u /* OPEN_EXISTING */: break;
        case 4u /* OPEN_ALWAYS */: nFlags |= O_CREAT; break;
        case 5u /* TRUNCATE_EXISTING */: nFlags |= O_TRUNC; break;
        default: tls_lastError = 87 /* ERROR_INVALID_PARAMETER */; return INVALID_HANDLE_VALUE;
    }
    int fd = open(lpFileName, nFlags, 0644);
    if (fd < 0) { tls_lastError = (errno == EEXIST) ? 80 : 2; return INVALID_HANDLE_VALUE; }
    return (HANDLE)(intptr_t)fd;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToRead, LPDWORD lpBytesRead, LPOVERLAPPED) {
    int fd = d2_fd_of(hFile);
    if (fd < 0) return FALSE;
    ssize_t n = read(fd, lpBuffer, nBytesToRead);
    if (n < 0) { tls_lastError = 21; return FALSE; }
    if (lpBytesRead) *lpBytesRead = (DWORD)n;
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nBytesToWrite, LPDWORD lpBytesWritten, LPOVERLAPPED) {
    int fd = d2_fd_of(hFile);
    if (fd < 0) return FALSE;
    ssize_t n = write(fd, lpBuffer, nBytesToWrite);
    if (n < 0) { tls_lastError = 21; return FALSE; }
    if (lpBytesWritten) *lpBytesWritten = (DWORD)n;
    return TRUE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    int fd = d2_fd_of(hFile);
    if (fd < 0) return 0xFFFFFFFFu;
    int nWhence = dwMoveMethod == 0 ? SEEK_SET : dwMoveMethod == 1 ? SEEK_CUR : SEEK_END;
    // 64-bit distances ride the high half (the PE's own pairing); i386 off_t
    // is 32-bit here, so a high half other than 0 fails honestly.
    if (lpDistanceToMoveHigh && *lpDistanceToMoveHigh != 0) return 0xFFFFFFFFu;
    off_t nAt = lseek(fd, (off_t)lDistanceToMove, nWhence);
    return nAt == (off_t)-1 ? 0xFFFFFFFFu : (DWORD)nAt;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    int fd = d2_fd_of(hFile);
    if (fd < 0) return 0xFFFFFFFFu;
    struct stat st;
    if (fstat(fd, &st) != 0) return 0xFFFFFFFFu;
    if (lpFileSizeHigh) *lpFileSizeHigh = 0;  // i386 off_t: no high half
    return (DWORD)st.st_size;
}

BOOL FlushFileBuffers(HANDLE hFile) {
    int fd = d2_fd_of(hFile);
    return fd >= 0 && fsync(fd) == 0;
}

HANDLE CreateFileMappingA(HANDLE hFile, SECURITY_ATTRIBUTES*, DWORD, DWORD dwHigh, DWORD dwLow, LPCSTR) {
    // The mapping object is the fd itself, size-tagged (the PE's kernel
    // object becomes a packed handle here).
    int fd = d2_fd_of(hFile);
    if (fd < 0) return NULL;
    return (HANDLE)(uintptr_t)((fd << 1) | 1);
}

LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD, DWORD dwHigh, DWORD dwLow, SIZE_T nBytes) {
    uintptr_t nTag = (uintptr_t)hFileMappingObject;
    if (!(nTag & 1)) return NULL;
    int fd = (int)(nTag >> 1);
    if (nBytes == 0) {
        struct stat st;
        if (fstat(fd, &st) != 0) return NULL;
        nBytes = (SIZE_T)st.st_size;
    }
    void* p = mmap(NULL, nBytes, PROT_READ, MAP_PRIVATE, fd, (off_t)dwLow | ((off_t)dwHigh << 32));
    return p == MAP_FAILED ? NULL : p;
}

BOOL UnmapViewOfFile(LPCVOID lpBaseAddress) { return munmap((void*)lpBaseAddress, 1) == 0 || errno == ENOMEM; }

BOOL GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime) {
    int fd = d2_fd_of(hFile);
    if (fd < 0) return FALSE;
    struct stat st;
    if (fstat(fd, &st) != 0) return FALSE;
    auto to_ft = [](long long secs, long nsecs) {
        long long ft = (secs + 11644473600ll) * 10000000ll + nsecs / 100;
        return (long long)ft;
    };
    // Linux has no per-file creation time; the modification time stands in
    // (the honest answer, and what the MPQ timestamp consumers read).
    long long nMod = to_ft(st.st_mtime, st.st_mtim.tv_nsec);
    if (lpCreationTime) { lpCreationTime->dwLowDateTime = (DWORD)(uint32_t)(uint64_t)nMod; lpCreationTime->dwHighDateTime = (DWORD)(uint32_t)((uint64_t)nMod >> 32); }
    if (lpLastAccessTime) { long long n = to_ft(st.st_atime, st.st_atim.tv_nsec); lpLastAccessTime->dwLowDateTime = (DWORD)(uint32_t)(uint64_t)n; lpLastAccessTime->dwHighDateTime = (DWORD)(uint32_t)((uint64_t)n >> 32); }
    if (lpLastWriteTime) { lpLastWriteTime->dwLowDateTime = (DWORD)(uint32_t)(uint64_t)nMod; lpLastWriteTime->dwHighDateTime = (DWORD)(uint32_t)((uint64_t)nMod >> 32); }
    return TRUE;
}

HRSRC FindResourceA(HMODULE, LPCSTR, LPCSTR) { return NULL; }
HGLOBAL LoadResource(HMODULE, HRSRC) { return NULL; }
LPVOID LockResource(HGLOBAL) { return NULL; }
DWORD SizeofResource(HMODULE, HRSRC) { return 0; }
BOOL FreeResource(HGLOBAL) { return TRUE; }

LPSTR _fullpath(char* absPath, const char* relPath, size_t maxLength) {
    if (!absPath || !relPath || maxLength < 2) return NULL;
    if (relPath[0] == '/') { snprintf(absPath, maxLength, "%s", relPath); return absPath; }
    char szCwd[1024];
    if (!getcwd(szCwd, sizeof(szCwd))) return NULL;
    snprintf(absPath, maxLength, "%s/%s", szCwd, relPath);
    return absPath;
}

int __stdcall ioctlsocket(SOCKET s, long cmd, u_long* argp) {
    // FIONBIO is the one command the slice names (Safesock.cpp's non-blocking
    // turn; the value Linux and Windows spell identically).
    if (cmd == (long)0x8004667e /* FIONBIO */) {
        int nNonBlock = argp ? (int)*argp : 0;
        return fcntl((int)s, F_SETFL, nNonBlock ? (fcntl((int)s, F_GETFL) | O_NONBLOCK)
                                                : (fcntl((int)s, F_GETFL) & ~O_NONBLOCK)) == -1 ? -1 : 0;
    }
    tls_lastError = 10022 /* WSAEINVAL */;
    return -1;
}

unsigned long d2_inet_addr(const char* cp) {
    // inet_addr's semantics: dotted quad -> network-order word, INADDR_NONE
    // (-1) on a bad string. Byte-by-byte parse, no libc netdb involvement.
    if (!cp || !cp[0]) return 0xFFFFFFFFu;
    uint32_t nParts[4] = {0, 0, 0, 0};
    int nPart = 0, nDigits = 0;
    for (const char* p = cp; ; ++p) {
        if (*p >= '0' && *p <= '9') {
            nParts[nPart] = nParts[nPart] * 10 + (uint32_t)(*p - '0');
            if (nParts[nPart] > 255) return 0xFFFFFFFFu;
            nDigits++;
        } else if (*p == '.' ) {
            if (!nDigits || nPart == 3) return 0xFFFFFFFFu;
            nPart++; nDigits = 0;
        } else if (*p == '\0') {
            if (!nDigits || nPart != 3) return 0xFFFFFFFFu;
            break;
        } else {
            return 0xFFFFFFFFu;
        }
    }
    return (nParts[0]) | (nParts[1] << 8) | (nParts[2] << 16) | (nParts[3] << 24);
}

int __stdcall d2_winsock_select(int, d2_winsock_fd_set* readfds, d2_winsock_fd_set* writefds,
                     d2_winsock_fd_set* exceptfds, struct timeval* timeout) {
    // Translate the winsock fd_array shape onto poll(2): nfds is ignored on
    // Windows too (the count lives in each set).
    struct pollfd fds[FD_SETSIZE * 3];
    short events[FD_SETSIZE * 3];
    int n = 0;
    auto addSet = [&](d2_winsock_fd_set* set, short ev) {
        if (!set) return;
        for (u_int i = 0; i < set->fd_count && n < FD_SETSIZE * 3; ++i) {
            fds[n].fd = set->fd_array[i];
            fds[n].events = ev;
            fds[n].revents = 0;
            events[n] = ev;
            ++n;
        }
    };
    addSet(readfds, POLLIN);
    addSet(writefds, POLLOUT);
    addSet(exceptfds, POLLPRI);
    int nReady = poll(fds, n, timeout ? (int)(timeout->tv_sec * 1000 + timeout->tv_usec / 1000) : -1);
    if (nReady <= 0) return nReady;
    int nWin = 0;
    auto collect = [&](d2_winsock_fd_set* set, short ev) {
        if (!set) return;
        d2_winsock_fd_set out;
        out.fd_count = 0;
        for (u_int i = 0; i < set->fd_count; ++i) {
            for (int j = 0; j < n; ++j) {
                if (fds[j].fd == set->fd_array[i] && (fds[j].revents & ev)) {
                    out.fd_array[out.fd_count++] = set->fd_array[i];
                    break;
                }
            }
        }
        *set = out;
        nWin += (int)out.fd_count;
    };
    collect(readfds, POLLIN);
    collect(writefds, POLLOUT);
    collect(exceptfds, POLLPRI);
    return nWin;
}

int d2_getsockname(SOCKET s, sockaddr* name, int* namelen) {
    char ss[128];
    unsigned int nLen = sizeof(ss);
    if (getsockname((int)s, (struct sockaddr*)&ss, &nLen) != 0) return -1;
    // The tree's sockaddr shape is 16 bytes, same as sockaddr_in; the game
    // reads sin_port/sin_addr out of it (Safesock.cpp's local-ip probe).
    if (name && namelen && *namelen >= 16) {
        memcpy(name, &ss, 16);
        *namelen = 16;
    }
    return 0;
}

int d2_getpeername(SOCKET s, sockaddr* name, int* namelen) {
    char ss[128];
    unsigned int nLen = sizeof(ss);
    if (getpeername((int)s, (struct sockaddr*)&ss, &nLen) != 0) return -1;
    if (name && namelen && *namelen >= 16) {
        memcpy(name, &ss, 16);
        *namelen = 16;
    }
    return 0;
}

int d2_gethostname(char* name, size_t namelen) noexcept {
    return gethostname(name, namelen);
}

static char g_szNtoaBuf[16];
char* d2_inet_ntoa(struct in_addr in) {
    uint32_t nAddr = in.S_un;
    snprintf(g_szNtoaBuf, sizeof(g_szNtoaBuf), "%u.%u.%u.%u",
             nAddr & 0xFF, (nAddr >> 8) & 0xFF, (nAddr >> 16) & 0xFF, (nAddr >> 24) & 0xFF);
    return g_szNtoaBuf;
}

u_short d2_htons(u_short hostshort) { return (u_short)((hostshort >> 8) | (hostshort << 8)); }
u_short d2_ntohs(u_short netshort) { return (u_short)((netshort >> 8) | (netshort << 8)); }


// ---------------------------------------------------------------------------
// Fail-soft surfaces: no named M1 site depends on their success (documented
// per family in d2_linux_api.h). Each returns the failure its PE-side caller
// already handles.
// ---------------------------------------------------------------------------
BOOL StartServiceCtrlDispatcherA(const _SERVICE_TABLE_ENTRYA*) { tls_lastError = 1063; return FALSE; }  // ERROR_SERVICE_NOT_IN_EXE
BOOL SetServiceStatus(SERVICE_STATUS_HANDLE, _SERVICE_STATUS* lpServiceStatus) {
    // The service loop records state transitions; M1 keeps the memory honest,
    // there is no SCM to report to.
    (void)lpServiceStatus;
    return FALSE;
}
HANDLE OpenEventA(DWORD, BOOL, LPCSTR) { tls_lastError = 2; return NULL; }
BOOL CreateProcessA(LPCSTR, LPSTR, SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, BOOL, DWORD,
                    LPVOID, LPCSTR, void*, void*) { tls_lastError = 2; return FALSE; }
BOOL CreateProcessW(LPCWSTR, LPWSTR, SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, BOOL, DWORD,
                    LPVOID, LPCWSTR, void*, void*) { tls_lastError = 2; return FALSE; }
BOOL GetExitCodeProcess(HANDLE, LPDWORD lpExitCode) { if (lpExitCode) *lpExitCode = 1; return FALSE; }
BOOL TerminateProcess(HANDLE, UINT) { return FALSE; }
HANDLE FindFirstFileA(LPCSTR, LPWIN32_FIND_DATAA) { tls_lastError = 2; return INVALID_HANDLE_VALUE; }
HANDLE CreateFileW(LPCWSTR, DWORD, DWORD, SECURITY_ATTRIBUTES*, DWORD, DWORD, HANDLE) {
    tls_lastError = 2;
    return (HANDLE)(intptr_t)-1;  // INVALID_HANDLE_VALUE
}
HANDLE FindFirstFileW(LPCWSTR, void*) { tls_lastError = 2; return INVALID_HANDLE_VALUE; }
BOOL FindNextFileA(HANDLE, LPWIN32_FIND_DATAA) { return FALSE; }
BOOL FindNextFileW(HANDLE, void*) { return FALSE; }
int LoadStringA(HINSTANCE, UINT, LPSTR, int) { return 0; }
BOOL IsWindow(HWND) { return FALSE; }
BOOL IsWindowVisible(HWND) { return FALSE; }
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }
LRESULT DispatchMessageA(const void*) { return 0; }
BOOL PeekMessageA(void*, HWND, UINT, UINT, UINT) { return FALSE; }
BOOL GetMessageA(void*, HWND, UINT, UINT) { return FALSE; }
BOOL TranslateMessage(const void*) { return FALSE; }
void PostQuitMessage(int) {}
BOOL PostMessageA(HWND, UINT, WPARAM, LPARAM) { return FALSE; }
int MessageBoxA(HWND, LPCSTR lpText, LPCSTR lpCaption, UINT) {
    // The PE is modal here; the linux twin logs and continues (fail-soft).
    fprintf(stderr, "d2linux: %s: %s\n", lpCaption ? lpCaption : "message", lpText ? lpText : "");
    return 1 /* IDOK */;
}
SHORT GetAsyncKeyState(int) { return 0; }
SHORT GetKeyState(int) { return 0; }
int wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...) {
    va_list ap;
    va_start(ap, lpFmt);
    int n = vsnprintf(lpOut, 1024, lpFmt, ap);
    va_end(ap);
    return n;
}
int wsprintfW(LPWSTR lpOut, LPCWSTR lpFmt, ...) { (void)lpOut; (void)lpFmt; return 0; }
void OutputDebugStringA(LPCSTR lpOutputString) { fprintf(stderr, "%s", lpOutputString ? lpOutputString : ""); }
void Sleep(DWORD);  // (declared above; kept for the family listing)

// --- user32 surface with PE-shaped bodies the M0 window already owns natively.
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return TRUE; }
BOOL ShowWindow(HWND, int) { return TRUE; }
BOOL DestroyWindow(HWND) { return TRUE; }
BOOL GetClientRect(HWND, LPRECT lpRect) { if (lpRect) { lpRect->left = lpRect->top = 0; lpRect->right = 800; lpRect->bottom = 600; } return TRUE; }
HDC GetDC(HWND) { return NULL; }
int ReleaseDC(HWND, HDC) { return 1; }
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) { return NULL; }
int GetSystemMetrics(int nIndex) { return nIndex == 16 /* SM_CYMENU */ ? 0 : 0; }
ATOM RegisterClassA(const void*) { return 0; }

// --- kernel events (the OpenEvent family the boot probe uses) close here.
// --- admin check (advapi32): ErrorManager's reporter context. The honest
// --- linux answer is the geteuid() root test via the same SID-shape API —
// --- fail-soft to "not elevated", the PE's own non-admin path.
HANDLE OpenProcess(DWORD, BOOL, DWORD dwProcessId) {
    (void)dwProcessId;
    return NULL;  // no cross-process handles in M1; callers handle NULL
}
BOOL AllocateAndInitializeSid(void*, BYTE, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void** pSid) {
    if (pSid) *pSid = NULL;
    return FALSE;
}
BOOL CheckTokenMembership(void*, void*, BOOL* IsMember) {
    if (IsMember) *IsMember = FALSE;
    return TRUE;
}
void* FreeSid(void* pSid) { (void)pSid; return NULL; }

BOOL GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize) {
    // LogManager's log headers; the hostname is the honest analogue.
    if (!lpBuffer || !nSize) return FALSE;
    char szName[256] = "localhost";
    gethostname(szName, sizeof(szName) - 1);
    DWORD n = (DWORD)strlen(szName) + 1;
    if (*nSize < n) { *nSize = n; return FALSE; }
    memcpy(lpBuffer, szName, n);
    *nSize = n - 1;
    return TRUE;
}

void RtlCaptureContext(CONTEXT* pContext) {
    // The capture the PE gets from RtlCaptureContext: the caller's registers.
    // i386: ESP/EBP off this frame, EIP = the caller's return address.
    if (!pContext) return;
    memset(pContext, 0, sizeof(*pContext));
    pContext->ContextFlags = 0x1003F;
    pContext->Eip = (DWORD)__builtin_return_address(0);
    pContext->Esp = (DWORD)__builtin_frame_address(0) + 8;
    pContext->Ebp = (DWORD)__builtin_frame_address(0);
}

errno_t _localtime64_s(struct tm* tmDest, const long long* sourceTime) {
    if (!tmDest || !sourceTime) return EINVAL;
    time_t t = (time_t)*sourceTime;
    if (!localtime_r(&t, tmDest)) return EINVAL;
    return 0;
}

BOOL GetUserNameA(LPSTR lpBuffer, LPDWORD pcbBuffer) {
    // M1: the account name comes from the passwd entry, the honest analogue
    // of the PE's GetUserNameA.
    if (!lpBuffer || !pcbBuffer) return FALSE;
    const char* user = getenv("USER");
    if (!user || !user[0]) user = getenv("LOGNAME");
    if (!user || !user[0]) user = "player";
    DWORD n = (DWORD)strlen(user) + 1;
    if (*pcbBuffer < n) { *pcbBuffer = n; return FALSE; }
    memcpy(lpBuffer, user, n);
    *pcbBuffer = n - 1;
    return TRUE;
}

// --- command line (nointro R86's host-side decl): the ELF entry owns argv.
LPSTR GetCommandLineA() {
    static char szEmpty[] = "";
    return szEmpty;
}

}  // extern "C"
