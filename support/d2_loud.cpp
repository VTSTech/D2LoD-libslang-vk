// The loud build's runtime handlers (REDIVIVUS_LOUD). Compiled into that build only;
// the normal build never sees this file.
//
// The loud build compiles every game TU with
//   -ftrivial-auto-var-init=pattern   uninitialised locals hold 0xFE bytes, not stale stack
//   -fstack-protector-strong          a write past a local array smashes a canary
//   -fsanitize=bounds-strict          an index past a declared array calls a handler
// and this file supplies the handlers those flags call. mingw has no ubsan runtime, so
// the two out-of-bounds entry points are defined here against the ABI GCC emits.
//
// Out of bounds: logged once per check site, then the game continues. Smashed canary:
// logged, the crash report is written, and the process ends; a frame whose canary is
// gone has a return address that cannot be trusted.
//
// Everything goes to d2loud-YYYYMMDD-HHMMSS.txt next to the exe (%TEMP% when that
// folder is not writable). The game thread only records the site and walks the EBP
// chain; a worker thread formats and symbolises it with libbacktrace, so the first
// report does not stall the game while the DWARF is parsed.
//
// This file must be compiled WITHOUT the loud flags: a bounds check in the handler
// would recurse, and the canary check in __stack_chk_fail's own frame is meaningless.

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "thirdparty/libbacktrace/backtrace.h"

#ifndef REDIVIVUS_BUILD_ID
#define REDIVIVUS_BUILD_ID "unknown"
#endif

extern "C" void D2CRASH_ReportHalt(int nLine, unsigned nAddress);

// ---------------------------------------------------------------------------
// The ubsan data GCC emits beside each check (gcc/ubsan.cc; the same layout as
// compiler-rt's ubsan_value.h and ubsan_handlers.h).
// ---------------------------------------------------------------------------
struct UbsanSourceLocation {
    const char* pszFile;
    unsigned int nLine;
    unsigned int nColumn;
};

// nKind 0 is an integer: bit 0 of nInfo is signedness, nInfo >> 1 is log2(bit width).
// szName is the quoted type name, e.g. 'int [10]'.
struct UbsanTypeDescriptor {
    unsigned short nKind;
    unsigned short nInfo;
    char szName[1];
};

struct UbsanOutOfBoundsData {
    UbsanSourceLocation loc;
    const UbsanTypeDescriptor* pArrayType;
    const UbsanTypeDescriptor* pIndexType;
};

// An integer no wider than a pointer is passed by value, a wider one by address.
typedef UINT_PTR UbsanValueHandle;

namespace {

// ---------------------------------------------------------------------------
// The exe's own extent and the report file.
// ---------------------------------------------------------------------------
DWORD g_dwExeLo = 0, g_dwExeHi = 0;
char g_szExePath[MAX_PATH];

void InitExe() {
    if (g_dwExeLo) return;
    DWORD n = GetModuleFileNameA(0, g_szExePath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) g_szExePath[0] = 0;
    HMODULE hExe = GetModuleHandleA(0);
    const IMAGE_NT_HEADERS* pNt =
        (const IMAGE_NT_HEADERS*)((const char*)hExe + ((const IMAGE_DOS_HEADER*)hExe)->e_lfanew);
    g_dwExeHi = (DWORD)(UINT_PTR)hExe + pNt->OptionalHeader.SizeOfImage;
    g_dwExeLo = (DWORD)(UINT_PTR)hExe;
}

HANDLE g_hFile = INVALID_HANDLE_VALUE;
bool g_bOpenTried = false;

// Same folder rule and naming as the crash report (support/d2_crash.cpp).
bool OpenLog() {
    if (g_bOpenTried) return g_hFile != INVALID_HANDLE_VALUE;
    g_bOpenTried = true;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char szDir[MAX_PATH];
    lstrcpyA(szDir, g_szExePath);
    DWORD n = lstrlenA(szDir);
    while (n > 0 && szDir[n - 1] != '\\' && szDir[n - 1] != '/') --n;
    szDir[n] = 0;
    for (int nTry = 0; nTry < 2; ++nTry) {
        if (nTry == 1) {
            DWORD m = GetTempPathA(MAX_PATH, szDir);
            if (m == 0 || m >= MAX_PATH - 40) return false;
        }
        for (int nSuffix = 1; nSuffix < 10; ++nSuffix) {
            char szPath[MAX_PATH + 40];
            if (nSuffix == 1)
                wsprintfA(szPath, "%sd2loud-%04u%02u%02u-%02u%02u%02u.txt", szDir, st.wYear,
                          st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            else
                wsprintfA(szPath, "%sd2loud-%04u%02u%02u-%02u%02u%02u-%d.txt", szDir, st.wYear,
                          st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, nSuffix);
            g_hFile = CreateFileA(szPath, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, 0);
            if (g_hFile != INVALID_HANDLE_VALUE) {
                OutputDebugStringA("Redivivus loud report: ");
                OutputDebugStringA(szPath);
                OutputDebugStringA("\n");
                return true;
            }
            if (GetLastError() != ERROR_FILE_EXISTS) break;
        }
    }
    return false;
}

void Out(const char* pszFormat, ...) {
    char szLine[2048];
    va_list args;
    va_start(args, pszFormat);
    int n = vsnprintf(szLine, sizeof(szLine), pszFormat, args);
    va_end(args);
    if (n <= 0 || g_hFile == INVALID_HANDLE_VALUE) return;
    if (n >= (int)sizeof(szLine)) n = sizeof(szLine) - 1;
    DWORD dwWritten;
    WriteFile(g_hFile, szLine, (DWORD)n, &dwWritten, 0);
}

const char* BaseName(const char* pszPath) {
    const char* p = pszPath;
    for (const char* q = pszPath; *q; ++q)
        if (*q == '\\' || *q == '/') p = q + 1;
    return p;
}

void ModuleOffset(DWORD dwAddr, char* pszOut) {
    HMODULE hMod = 0;
    char szPath[MAX_PATH];
    if (dwAddr && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                     GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                     (LPCSTR)(UINT_PTR)dwAddr, &hMod) &&
        GetModuleFileNameA(hMod, szPath, MAX_PATH)) {
        wsprintfA(pszOut, "%s+0x%06lX", BaseName(szPath),
                  (unsigned long)(dwAddr - (DWORD)(UINT_PTR)hMod));
    } else {
        wsprintfA(pszOut, "0x%08lX", (unsigned long)dwAddr);
    }
}

// ---------------------------------------------------------------------------
// Symbolisation (worker thread only; libbacktrace state is single-threaded).
// The demangling is the same as the crash report's: strip the i386 call
// decoration, then libstdc++'s callback demangler, which does not allocate.
// ---------------------------------------------------------------------------
backtrace_state* g_pBtState = 0;
bool g_bBtTried = false;

void OnBtError(void*, const char* pszMsg, int nErr) {
    if (nErr == -1) Out("  (no debug info: %s)\r\n", pszMsg);
}

extern "C" int __gcclibcxx_demangle_callback(const char* pszMangled,
                                             void (*pfnSink)(const char*, size_t, void*),
                                             void* pOpaque);

struct DemangleSink {
    char* pszOut;
    size_t nCap;
    size_t nLen;
};

void DemangleAppend(const char* p, size_t n, void* pOpaque) {
    DemangleSink* pSink = (DemangleSink*)pOpaque;
    if (pSink->nLen + n >= pSink->nCap) n = pSink->nCap - 1 - pSink->nLen;
    memcpy(pSink->pszOut + pSink->nLen, p, n);
    pSink->nLen += n;
    pSink->pszOut[pSink->nLen] = 0;
}

const char* Demangle(const char* pszName) {
    static char s_szMangled[2048];
    static char s_szDemangled[4096];
    if (!pszName) return "?";
    const char* p = pszName;
    if (*p == '@') ++p;
    if (p[0] == '_' && p[1] == '_' && p[2] == 'Z') ++p;
    if (p[0] != '_' || p[1] != 'Z') return pszName;
    size_t n = strlen(p);
    if (n >= sizeof(s_szMangled)) return pszName;
    size_t nEnd = n;
    while (nEnd > 0 && p[nEnd - 1] >= '0' && p[nEnd - 1] <= '9') --nEnd;
    if (nEnd < n && nEnd > 0 && p[nEnd - 1] == '@') n = nEnd - 1;
    memcpy(s_szMangled, p, n);
    s_szMangled[n] = 0;
    DemangleSink sink = { s_szDemangled, sizeof(s_szDemangled), 0 };
    s_szDemangled[0] = 0;
    if (__gcclibcxx_demangle_callback(s_szMangled, DemangleAppend, &sink) != 0 || !sink.nLen)
        return pszName;
    return s_szDemangled;
}

struct PcInfo {
    int nHits;
};

int OnPcInfo(void* pData, uintptr_t, const char* pszFile, int nLine, const char* pszFunc) {
    PcInfo* pInfo = (PcInfo*)pData;
    if (!pszFunc && !pszFile) return 0;
    Out(pInfo->nHits ? "\r\n          inlined in %s" : "  %s", Demangle(pszFunc));
    if (pszFile) Out("  %s:%d", pszFile, nLine);
    ++pInfo->nHits;
    return pInfo->nHits >= 3;
}

void OnSymInfo(void* pData, uintptr_t uPc, const char* pszName, uintptr_t uVal, uintptr_t) {
    if (!pszName) return;
    Out("  %s+0x%lX", Demangle(pszName), (unsigned long)(uPc - uVal));
    ++((PcInfo*)pData)->nHits;
}

void OnBtErrorQuiet(void*, const char*, int) {}

// One frame: module+offset, then function and file:line when this exe holds it.
// Frame 0 is the check site itself (the instruction after the handler call);
// the rest are return addresses, stepped back one byte into their CALL.
void WriteFrame(int i, DWORD dwAddr) {
    char szMod[MAX_PATH + 40];
    ModuleOffset(dwAddr, szMod);
    Out("    #%-2d %s", i, szMod);
    if (!g_bBtTried) {
        g_bBtTried = true;
        if (g_szExePath[0]) g_pBtState = backtrace_create_state(g_szExePath, 0, OnBtError, 0);
    }
    if (g_pBtState && dwAddr >= g_dwExeLo && dwAddr < g_dwExeHi) {
        PcInfo info = { 0 };
        backtrace_pcinfo(g_pBtState, dwAddr - 1, OnPcInfo, OnBtErrorQuiet, &info);
        if (!info.nHits) backtrace_syminfo(g_pBtState, dwAddr - 1, OnSymInfo, OnBtErrorQuiet, &info);
    }
    Out("\r\n");
}

// ---------------------------------------------------------------------------
// The stack, walked on the reporting thread along the EBP chain. The game is
// built without optimisation, so every game frame has one. Each link is checked
// against the thread's stack bounds, and a frame is kept only when its return
// address lies inside this exe, so a smashed frame ends the walk rather than
// faulting it.
// ---------------------------------------------------------------------------
const int MAX_FRAMES = 12;

int WalkStack(DWORD dwFirst, DWORD dwEbp, DWORD* pFrames) {
    int n = 0;
    pFrames[n++] = dwFirst;
    NT_TIB* pTib = (NT_TIB*)NtCurrentTeb();
    DWORD dwBase = (DWORD)(UINT_PTR)pTib->StackBase;
    DWORD dwLimit = (DWORD)(UINT_PTR)pTib->StackLimit;
    DWORD dwFrame = dwEbp;
    while (n < MAX_FRAMES) {
        if (dwFrame < dwLimit || dwFrame + 8 > dwBase || (dwFrame & 3)) break;
        const DWORD* p = (const DWORD*)(UINT_PTR)dwFrame;
        DWORD dwRet = p[1];
        if (dwRet < g_dwExeLo || dwRet >= g_dwExeHi) break;
        pFrames[n++] = dwRet;
        if (p[0] <= dwFrame) break;
        dwFrame = p[0];
    }
    return n;
}

// ---------------------------------------------------------------------------
// The queue between the reporting threads and the worker. A spin lock, because
// it needs no initialisation: a check can fire from any constructor.
// ---------------------------------------------------------------------------
enum ReportKind { REPORT_BOUNDS, REPORT_BOUNDS_ABORT, REPORT_STACK_SMASH };

struct Report {
    ReportKind eKind;
    const UbsanOutOfBoundsData* pData;
    long long nIndex;
    bool bIndexSigned;
    bool bIndexKnown;
    unsigned nOrdinal;
    DWORD dwThread;
    SYSTEMTIME st;
    int nFrames;
    DWORD aFrames[MAX_FRAMES];
};

const int QUEUE_SIZE = 256;
Report g_aQueue[QUEUE_SIZE];
volatile LONG g_nQueueHead = 0;   // next to write; only under the lock
volatile LONG g_nQueueTail = 0;   // next to read; only under the lock
volatile LONG g_nQueueLock = 0;
volatile LONG g_nDropped = 0;
volatile LONG g_nWritten = 0;     // reports the worker has finished writing
volatile LONG g_nQueued = 0;      // reports accepted into the queue
HANDLE g_hWake = 0;
HANDLE g_hWorker = 0;
volatile LONG g_nWorkerState = 0; // 0 none, 1 starting, 2 running

void Lock() {
    while (InterlockedCompareExchange(&g_nQueueLock, 1, 0) != 0) Sleep(0);
}
void Unlock() { InterlockedExchange(&g_nQueueLock, 0); }

// ---------------------------------------------------------------------------
// The once-per-site table: open addressing on the check's own data pointer,
// which GCC emits once per check site. Insert is one compare-exchange, so two
// threads hitting a new site at once still report it once.
// ---------------------------------------------------------------------------
const int SITE_SLOTS = 1 << 15;
const void* volatile g_apSite[SITE_SLOTS];
volatile LONG g_anSiteHits[SITE_SLOTS];
volatile LONG g_nSites = 0;
volatile LONG g_nSitesLost = 0;

// True the first time this site is seen.
bool FirstHit(const void* pKey) {
    UINT_PTR h = (UINT_PTR)pKey;
    h ^= h >> 15;
    h *= 0x2C1B3C6DU;
    h ^= h >> 12;
    for (int nProbe = 0; nProbe < SITE_SLOTS; ++nProbe) {
        int i = (int)((h + nProbe) & (SITE_SLOTS - 1));
        const void* pSeen = g_apSite[i];
        if (pSeen == pKey) {
            InterlockedIncrement(&g_anSiteHits[i]);
            return false;
        }
        if (!pSeen) {
            PVOID pOld = InterlockedCompareExchangePointer((PVOID volatile*)&g_apSite[i],
                                                           (PVOID)pKey, 0);
            if (!pOld || pOld == pKey) {
                InterlockedIncrement(&g_anSiteHits[i]);
                if (!pOld) {
                    InterlockedIncrement(&g_nSites);
                    return true;
                }
                return false;
            }
        }
    }
    // Table full: report nothing more rather than everything again.
    InterlockedIncrement(&g_nSitesLost);
    return false;
}

void WriteHeader() {
    Out("Redivivus loud build report\r\n");
    Out("Every entry below is a defect at the location it names. Send this whole file.\r\n\r\n");
    Out("Build:     %s (loud), gcc %s\r\n", REDIVIVUS_BUILD_ID, __VERSION__);
    Out("Exe:       %s\r\n", g_szExePath);
#ifdef REDIVIVUS_LOUD_NOSP
    Out("Checks:    -fsanitize=bounds-strict (logged once per site, game continues), "
        "-ftrivial-auto-var-init=pattern (0xFE); no stack protector\r\n\r\n");
#else
    Out("Checks:    -fsanitize=bounds-strict (logged once per site, game continues), "
        "-fstack-protector-strong (fatal), -ftrivial-auto-var-init=pattern (0xFE)\r\n\r\n");
#endif
    FlushFileBuffers(g_hFile);
}

void WriteIndex(const Report& r) {
    if (!r.bIndexKnown)
        Out("?");
    else if (r.bIndexSigned)
        Out("%I64d", r.nIndex);
    else
        Out("%I64u", (unsigned long long)r.nIndex);
}

void WriteReport(const Report& r) {
    Out("[%u] %04u-%02u-%02u %02u:%02u:%02u  thread %lu\r\n", r.nOrdinal, r.st.wYear,
        r.st.wMonth, r.st.wDay, r.st.wHour, r.st.wMinute, r.st.wSecond,
        (unsigned long)r.dwThread);
    if (r.eKind == REPORT_STACK_SMASH) {
        Out("  STACK SMASHED: a write ran past a local array and over the frame's canary.\r\n");
        Out("  The function that returns through frame #0 below owns the smashed frame.\r\n");
        Out("  The crash report (d2crash-*.txt, HALT line=0) follows; the game ends here.\r\n");
    } else {
        const UbsanOutOfBoundsData* d = r.pData;
        Out("  OUT OF BOUNDS%s: index ", r.eKind == REPORT_BOUNDS_ABORT ? " (abort)" : "");
        WriteIndex(r);
        Out(" into %s (index type %s)\r\n", d->pArrayType->szName, d->pIndexType->szName);
        Out("  at %s:%u:%u\r\n", d->loc.pszFile ? d->loc.pszFile : "?", d->loc.nLine,
            d->loc.nColumn);
    }
    for (int i = 0; i < r.nFrames; ++i) WriteFrame(i, r.aFrames[i]);
    Out("\r\n");
}

DWORD WINAPI WorkerThread(LPVOID) {
    for (;;) {
        WaitForSingleObject(g_hWake, 1000);
        for (;;) {
            Report r;
            Lock();
            bool bAny = g_nQueueTail != g_nQueueHead;
            if (bAny) {
                r = g_aQueue[g_nQueueTail % QUEUE_SIZE];
                g_nQueueTail = g_nQueueTail + 1;
            }
            Unlock();
            if (!bAny) break;
            if (OpenLog()) {
                if (r.nOrdinal == 1) WriteHeader();
                WriteReport(r);
                FlushFileBuffers(g_hFile);
            }
            InterlockedIncrement(&g_nWritten);
        }
    }
    return 0;
}

void EnsureWorker() {
    if (g_nWorkerState == 2) return;
    if (InterlockedCompareExchange(&g_nWorkerState, 1, 0) == 0) {
        g_hWake = CreateEventA(0, FALSE, FALSE, 0);
        g_hWorker = CreateThread(0, 4 << 20, WorkerThread, 0, 0, 0);
        InterlockedExchange(&g_nWorkerState, 2);
    }
    while (g_nWorkerState != 2) Sleep(0);
}

volatile LONG g_nOrdinal = 0;

// Queue one report. Returns its ordinal, or 0 when the queue was full.
unsigned Enqueue(ReportKind eKind, const UbsanOutOfBoundsData* pData, UbsanValueHandle hIndex,
                 DWORD dwSite, DWORD dwEbp) {
    InitExe();
    Report r;
    memset(&r, 0, sizeof(r));
    r.eKind = eKind;
    r.pData = pData;
    if (pData && pData->pIndexType && pData->pIndexType->nKind == 0) {
        unsigned nBits = 1u << (pData->pIndexType->nInfo >> 1);
        r.bIndexSigned = pData->pIndexType->nInfo & 1;
        r.bIndexKnown = true;
        if (nBits > sizeof(UbsanValueHandle) * 8) {
            r.nIndex = *(const long long*)(UINT_PTR)hIndex;
        } else if (r.bIndexSigned) {
            int nShift = 64 - (int)nBits;
            r.nIndex = ((long long)(unsigned long long)hIndex << nShift) >> nShift;
        } else {
            r.nIndex = (long long)(unsigned long long)hIndex;
        }
    }
    r.dwThread = GetCurrentThreadId();
    GetLocalTime(&r.st);
    r.nFrames = WalkStack(dwSite, dwEbp, r.aFrames);

    EnsureWorker();
    unsigned nOrdinal = 0;
    Lock();
    if (g_nQueueHead - g_nQueueTail < QUEUE_SIZE) {
        nOrdinal = (unsigned)InterlockedIncrement(&g_nOrdinal);
        r.nOrdinal = nOrdinal;
        g_aQueue[g_nQueueHead % QUEUE_SIZE] = r;
        g_nQueueHead = g_nQueueHead + 1;
        InterlockedIncrement(&g_nQueued);
    } else {
        InterlockedIncrement(&g_nDropped);
    }
    Unlock();
    if (g_hWake) SetEvent(g_hWake);
    return nOrdinal;
}

// Wait, bounded, until the worker has written everything queued so far.
void Drain(DWORD dwTimeoutMs) {
    DWORD dwStart = GetTickCount();
    while (g_nWritten < g_nQueued && GetTickCount() - dwStart < dwTimeoutMs) {
        if (g_hWake) SetEvent(g_hWake);
        Sleep(10);
    }
}

__attribute__((noreturn)) void FatalHandOff(DWORD dwSite) {
    Drain(60000);
    if (g_hFile != INVALID_HANDLE_VALUE) FlushFileBuffers(g_hFile);
    D2CRASH_ReportHalt(0, (unsigned)dwSite);
    TerminateProcess(GetCurrentProcess(), 0xC0000409);   // STATUS_STACK_BUFFER_OVERRUN
    for (;;) Sleep(INFINITE);
}

// At a normal exit: how often each reported site fired in all.
// Best effort - a process ended by ExitProcess never gets here.
struct LoudSummary {
    ~LoudSummary() {
        if (!g_nQueued) return;
        Drain(5000);
        if (g_nQueueLock || g_hFile == INVALID_HANDLE_VALUE) return;
        Out("Summary: %ld site(s) reported, %ld report(s) dropped (queue full), "
            "%ld site(s) not tracked (table full)\r\n",
            (long)g_nSites, (long)g_nDropped, (long)g_nSitesLost);
        for (int i = 0; i < SITE_SLOTS; ++i) {
            const UbsanOutOfBoundsData* d = (const UbsanOutOfBoundsData*)g_apSite[i];
            if (!d) continue;
            Out("  %8ld hits  %s:%u:%u\r\n", (long)g_anSiteHits[i],
                d->loc.pszFile ? d->loc.pszFile : "?", d->loc.nLine, d->loc.nColumn);
        }
        FlushFileBuffers(g_hFile);
    }
};
LoudSummary g_loudSummary __attribute__((init_priority(101)));

}  // namespace

// ---------------------------------------------------------------------------
// The entry points the instrumented code calls.
// ---------------------------------------------------------------------------
extern "C" __attribute__((noinline)) void __ubsan_handle_out_of_bounds(UbsanOutOfBoundsData* pData,
                                                                       UbsanValueHandle hIndex) {
    if (!FirstHit(pData)) return;
    Enqueue(REPORT_BOUNDS, pData, hIndex, (DWORD)(UINT_PTR)__builtin_return_address(0),
            (DWORD)(UINT_PTR)__builtin_frame_address(1));
}

// -fno-sanitize-recover: the same report, then the crash report and the end.
extern "C" __attribute__((noinline, noreturn)) void __ubsan_handle_out_of_bounds_abort(
    UbsanOutOfBoundsData* pData, UbsanValueHandle hIndex) {
    DWORD dwSite = (DWORD)(UINT_PTR)__builtin_return_address(0);
    FirstHit(pData);
    Enqueue(REPORT_BOUNDS_ABORT, pData, hIndex, dwSite,
            (DWORD)(UINT_PTR)__builtin_frame_address(1));
    FatalHandOff(dwSite);
}

// Called from the epilogue of a function whose canary no longer matches. The
// return address is inside that function; its EBP is still the live one, but the
// saved EBP and return address above it may be the overwritten bytes, which the
// walk's bounds checks stop on.
extern "C" __attribute__((noinline, noreturn)) void __stack_chk_fail(void) {
    DWORD dwSite = (DWORD)(UINT_PTR)__builtin_return_address(0);
    Enqueue(REPORT_STACK_SMASH, 0, 0, dwSite, (DWORD)(UINT_PTR)__builtin_frame_address(1));
    FatalHandOff(dwSite);
}
