// platform/linux/d2_linux_crash.cpp — [d2linux M1] the crash path's linux
// twin. The PE's lives in Fog/Engine/Application.cpp (DEBUG_* stack walking
// over DbgHelp/PEB/psapi/toolhelp, PATH_GetKnownFolderPath over SHGetFolder)
// and support/d2_crash.cpp (the d2crash-*.txt reporter). Those are PE-shaped
// by construction; this file provides the same named functions with Linux
// bodies so the M1 slice links and the fault path works here:
//
//   - DEBUG_EnumerateModulesViaPsapi/Toolhelp32/PEB -> /proc/self/maps fills
//     the D2FogDebugModuleEntryStrc table (base, size, module/image names;
//     PDB GUID zero — there are no PDBs on this platform, and the report's
//     DBG-MODULE lines say so honestly).
//   - DEBUG_UnwindStackFrames -> the i386 EBP-chain walk (the build is -O0
//     with frame pointers, the same contract the PE's StackWalk64 walk relies
//     on) into Fog/File.h's D2CrashFrameStrc array.
//   - DEBUG_CaptureThreadContext -> getcontext() for the calling thread;
//     other threads report failure (the caller's own suspend path handles it,
//     exactly as on the PE when OpenThread comes up empty).
//   - DEBUG_FormatAddressInfo -> "<module>+<off> <symbol>" via /proc maps and
//     the vendored libbacktrace (thirdparty/libbacktrace, ELF capable).
//   - PATH_GetKnownFolderPath -> XDG documents dir (the My Documents twin).
//   - Signal handlers (SIGSEGV/SIGILL/SIGFPE/SIGABRT/SIGBUS) -> ucontext to
//     CONTEXT (i386 greg mapping), then the same VEH-chain + unhandled-filter
//     dispatch RaiseException uses (d2_linux_shim_crt.cpp's bookkeeping).
//
// Every body has named callers in the M1 slice: Fog/Debug.cpp:995-1000 (the
// module cascade), Fog/File.cpp (CRASH_DumpThreadContext / stack formatting),
// Fog/Source/SFile.cpp:267 (save path), Fog/Debug.cpp's fault filter install.
//
// PE safety: this file compiles only in the linux build (build.cmake).

#include <link.h>       // dl_iterate_phdr: the honest module walk
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <time.h>
#include <unistd.h>

#include "platform/linux/d2_linux_compat.h"

// The tree's own shapes (Fog/Debug.h, Fog/File.h) — included through the
// forced d2_platform + the slice's include path, never redefined here.
#include "Fog/Debug.h"
#include "Fog/File.h"

namespace Fog { namespace Engine { namespace Application {

// ---------------------------------------------------------------------------
// Module enumeration. dl_iterate_phdr walks the loaded images with their real
// base addresses; /proc/self/exe names the main image. The PE's three
// implementations are a psapi/toolhelp/PEB fallback cascade; the linux twin
// has one source of truth, so the first callee answers and the cascade
// never descends — the same observable behavior as a healthy PE pass.
// ---------------------------------------------------------------------------
static void d2_module_name(const char* szPath, char* szOut, size_t nOut) {
    const char* p = szPath ? strrchr(szPath, '/') : NULL;
    snprintf(szOut, nOut, "%s", p ? p + 1 : (szPath ? szPath : "unknown"));
}

// The truncations inside d2_phdr_cb and DEBUG_FormatAddressInfo are INTENDED:
// the PE-shape module fields are 256 bytes and longer paths truncate exactly
// as they do on Windows (szImagePath/szModuleName), as do symbol lines into
// the 256-byte address buffer. The analyzer's "may be truncated" note is
// noise about the spec here - silenced for these two functions only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static int d2_phdr_cb(struct dl_phdr_info* pInfo, size_t, void* pCtx) {
    D2FogDebugModuleTableStrc* pTable = (D2FogDebugModuleTableStrc*)pCtx;
    if (pTable->aEntries[0].pImageBase && (uintptr_t)pInfo->dlpi_addr == 0) {
        // The argv[0] image also appears with dlpi_addr 0 for the vDSO pass;
        // skip repeats once the real first entry is recorded.
        return 0;
    }
    if (!pInfo->dlpi_name || !pInfo->dlpi_name[0]) {
        // The main image: name it from /proc/self/exe.
        char szExe[512];
        ssize_t n = readlink("/proc/self/exe", szExe, sizeof(szExe) - 1);
        if (n <= 0) return 0;
        szExe[n] = '\0';
        static char szMainPath[512];
        snprintf(szMainPath, sizeof(szMainPath), "%s", szExe);
        static char szMainName[256];
        d2_module_name(szMainPath, szMainName, sizeof(szMainName));
        D2FogDebugModuleEntryStrc* pEntry = pTable->aEntries;
        memset(pEntry, 0, sizeof(*pEntry));
        pEntry->pImageBase = (void*)(uintptr_t)pInfo->dlpi_addr;
        pEntry->nImageSize = 0;  // size lands from the maps pass below
        snprintf(pEntry->szModuleName, sizeof(pEntry->szModuleName), "%s", szMainName);
        snprintf(pEntry->szImagePath, sizeof(pEntry->szImagePath), "%s", szMainPath);
        pEntry->dwTimestamp = 0;
        return 0;
    }
    for (int i = 0; i < pInfo->dlpi_phnum; ++i) {
        const ElfW(Phdr)* ph = &pInfo->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD) continue;
        // Find a free entry (the table's slots are zeroed by the caller's init).
        for (int e = 0; e < 256; ++e) {
            D2FogDebugModuleEntryStrc* pEntry = &pTable->aEntries[e];
            if (pEntry->pImageBase) continue;
            static char szPath[512];
            static char szName[256];
            snprintf(szPath, sizeof(szPath), "%s", pInfo->dlpi_name);
            d2_module_name(szPath, szName, sizeof(szName));
            memset(pEntry, 0, sizeof(*pEntry));
            pEntry->pImageBase = (void*)(uintptr_t)(pInfo->dlpi_addr + ph->p_vaddr);
            pEntry->nImageSize = (uint32_t)ph->p_memsz;
            snprintf(pEntry->szModuleName, sizeof(pEntry->szModuleName), "%s", szName);
            snprintf(pEntry->szImagePath, sizeof(pEntry->szImagePath), "%s", szPath);
            return 0;
        }
        return 1;  // table full
    }
    return 0;
}

static bool d2_enumerate_modules(D2FogDebugModuleTableStrc* pTable) {
    if (!pTable) return false;
    dl_iterate_phdr(d2_phdr_cb, pTable);
    return pTable->aEntries[0].pImageBase != NULL;
}

bool __stdcall DEBUG_EnumerateModulesViaPsapi(int* pModuleList) {
    return d2_enumerate_modules((D2FogDebugModuleTableStrc*)pModuleList);
}

bool DEBUG_EnumerateModulesViaToolhelp32(int* pModuleList) {
    return d2_enumerate_modules((D2FogDebugModuleTableStrc*)pModuleList);
}

bool __fastcall DEBUG_EnumerateModulesViaPEB(int* pModuleList) {
    return d2_enumerate_modules((D2FogDebugModuleTableStrc*)pModuleList);
}

// ---------------------------------------------------------------------------
// Thread-context capture. getcontext() for the calling thread; the i386
// greg mapping follows glibc's x86 register indices.
// ---------------------------------------------------------------------------
// The greg->CONTEXT mapping is the i386 charter's shape (the x86_64 skeleton
// build has no i386 register indices and no game code to capture for — the
// capture entry points answer false there, matching the charter's "native
// builds the skeleton only").
#if defined(__i386__)
static void d2_ucontext_to_context(const ucontext_t* pUc, CONTEXT* pContext) {
    if (!pUc || !pContext) return;
    memset(pContext, 0, sizeof(*pContext));
    pContext->ContextFlags = 0x1003F;  // CONTEXT_FULL, the PE's own flag word
    const greg_t* g = pUc->uc_mcontext.gregs;
    pContext->SegGs = (DWORD)g[REG_GS];
    pContext->SegFs = (DWORD)g[REG_FS];
    pContext->SegEs = (DWORD)g[REG_ES];
    pContext->SegDs = (DWORD)g[REG_DS];
    pContext->Edi = (DWORD)g[REG_EDI];
    pContext->Esi = (DWORD)g[REG_ESI];
    pContext->Ebx = (DWORD)g[REG_EBX];
    pContext->Edx = (DWORD)g[REG_EDX];
    pContext->Ecx = (DWORD)g[REG_ECX];
    pContext->Eax = (DWORD)g[REG_EAX];
    pContext->Ebp = (DWORD)g[REG_EBP];
    pContext->Eip = (DWORD)g[REG_EIP];
    pContext->Esp = (DWORD)g[REG_ESP];
}
#else
static void d2_ucontext_to_context(const ucontext_t*, CONTEXT* pContext) {
    if (pContext) memset(pContext, 0, sizeof(*pContext));
}
#endif

bool DEBUG_CaptureThreadContext(uint32_t dwThreadId, LPCONTEXT pContext) {
    // The PE captures the thread whose id is passed (it suspends it first);
    // the linux twin captures the CALLING thread and refuses others — the
    // crash path's own thread is the one that matters in M1, and the refusal
    // is the same answer the PE gives for a dead thread.
    if (!pContext) return false;
    if (dwThreadId && dwThreadId != (uint32_t)gettid()) return false;
    ucontext_t uc;
    if (getcontext(&uc) != 0) return false;
    d2_ucontext_to_context(&uc, pContext);
    return true;
}

// ---------------------------------------------------------------------------
// Stack unwind: the i386 EBP-chain walk. The -O0 build keeps frame pointers
// (the charter's flags), so [ebp] = saved ebp, [ebp+4] = return address — the
// same chain StackWalk64's AddrFrame follows on the PE. Walks callers'
// D2CrashFrameStrc array (Fog/File.h), stopping at the bottom or when the
// chain stops looking like frames.
// ---------------------------------------------------------------------------
int __thiscall DEBUG_UnwindStackFrames(CONTEXT* pContext, DWORD dwThreadId,
                                       D2CrashFrameStrc* pFrameArray, uint32_t nSkipFrames) {
    (void)dwThreadId;
    if (!pContext || !pFrameArray) return 0;
    uint32_t nEbp = pContext->Ebp;
    uint32_t nEip = pContext->Eip;
    int nFrames = 0;
    uint32_t nSkipped = 0;
    while (nEbp && nFrames < 64) {
        // The chain read happens through safe pointers only: a broken chain
        // (not our stack) ends the walk, as StackWalk64's sanity checks do.
        if (nEbp < 0x1000 || nEbp > 0xBFFFFFFF || (nEbp & 3)) break;
        uint32_t nNext = *(uint32_t*)(uintptr_t)(nEbp);
        uint32_t nRet = *(uint32_t*)(uintptr_t)(nEbp + 4);
        if (nRet < 0x1000 || nRet > 0xBFFFFFFF) break;
        if (nSkipped < nSkipFrames) {
            nSkipped++;
        } else {
            D2CrashFrameStrc* pFrame = &pFrameArray[nFrames];
            memset(pFrame, 0, sizeof(*pFrame));
            pFrame->dwReturnAddr = nRet;
            nFrames++;
        }
        if (nNext <= nEbp) break;  // chain must climb
        nEbp = nNext;
        (void)nEip;
    }
    return nFrames;
}

// ---------------------------------------------------------------------------
// Address formatting for the report lines: "<module>+<off> <symbol>". The
// module half walks /proc/self/maps; the symbol half asks dladdr() against
// the -rdynamic executable's own symbol table. (The vendored libbacktrace is
// the PE's i686-mingw configuration with no ELF backend — see build.cmake's
// note; the ELF-config upgrade is its own documented round.)
// ---------------------------------------------------------------------------
#include <dlfcn.h>

bool __stdcall DEBUG_FormatAddressInfo(uint32_t nAddress, char* pOutput) {
    if (!pOutput) return false;
    // Module half: which /proc/self/maps image holds the address.
    char szModule[256] = "unknown";
    uint32_t nBase = 0;
    FILE* f = fopen("/proc/self/maps", "r");
    if (f) {
        char szLine[1200];
        unsigned long nStart, nEnd;
        char szPerms[5];
        int nPath = 0;
        while (fgets(szLine, sizeof(szLine), f)) {
            int n = sscanf(szLine, "%lx-%lx %4s %*x %*s %*d %n", &nStart, &nEnd, szPerms, &nPath);
            if (n < 3) continue;
            if (nAddress >= nStart && nAddress < nEnd && nPath > 0) {
                char* p = szLine + nPath;
                size_t nLen = strlen(p);
                while (nLen && (p[nLen - 1] == '\n')) p[--nLen] = '\0';
                const char* pName = strrchr(p, '/');
                snprintf(szModule, sizeof(szModule), "%s", pName ? pName + 1 : p);
                nBase = (uint32_t)nStart;
                break;
            }
        }
        fclose(f);
    }
    // Symbol half: dladdr against the -rdynamic image; nearest exported
    // symbol at or below the address, or none.
    Dl_info info;
    memset(&info, 0, sizeof(info));
    const char* szSymbol = NULL;
    if (dladdr((void*)(uintptr_t)nAddress, &info) && info.dli_sname && info.dli_fname &&
        (uintptr_t)info.dli_saddr <= (uintptr_t)nAddress) {
        szSymbol = info.dli_sname;
        nBase = (uint32_t)(uintptr_t)info.dli_saddr;
    }
    if (szSymbol) {
        snprintf(pOutput, 256, "%s!%s+0x%x", szModule, szSymbol, nAddress - nBase);
    } else {
        snprintf(pOutput, 256, "%s+0x%x", szModule, nAddress - nBase);
    }
    return true;
}
#pragma GCC diagnostic pop

// ---------------------------------------------------------------------------
// Known-folder path (PATH_GetKnownFolderPath): the save path root. XDG's
// documents dir is the My Documents analogue; $HOME is the fallback, the
// same "present or the caller fails cleanly" contract the PE function keeps.
// ---------------------------------------------------------------------------
uint32_t __stdcall PATH_GetKnownFolderPath(LPSTR szOutPath, int32_t nBufferSize) {
    if (!szOutPath || nBufferSize <= 0) return 0;
    const char* home = getenv("HOME");
    if (!home || !home[0]) return 0;
    const char* docs = getenv("XDG_DOCUMENTS_DIR");
    char szDocs[512];
    if (docs && docs[0]) {
        snprintf(szDocs, sizeof(szDocs), "%s", docs);
    } else {
        snprintf(szDocs, sizeof(szDocs), "%s/Documents", home);
        struct stat st;
        if (stat(szDocs, &st) != 0 || !S_ISDIR(st.st_mode)) {
            snprintf(szDocs, sizeof(szDocs), "%s", home);
        }
    }
    if ((int32_t)strlen(szDocs) >= nBufferSize) return 0;
    snprintf(szOutPath, (size_t)nBufferSize, "%s", szDocs);
    return 1;
}

}}}  // namespace Fog::Engine::Application

// ---------------------------------------------------------------------------
// D2CRASH_ReportHalt (support/d2_trace.h's contract, support/d2_crash.cpp's
// PE body): report a fatal inconsistency and stop. The linux twin writes the
// note into the boot log and aborts, which the fault handlers above turn into
// a normal crash report.
extern "C" void D2CRASH_ReportHalt(int nLine, unsigned nAddress) {
    FILE* f = fopen("/dev/stderr", "a");
    if (f) {
        fprintf(f, "d2linux: D2CRASH_ReportHalt line=%d addr=0x%x - halting\n", nLine, nAddress);
        fclose(f);
    }
    abort();
}

// ---------------------------------------------------------------------------
// The fault half: hardware faults funnel into the same VEH-chain + unhandled
// filter dispatch the raise path uses (d2_linux_shim_crt.cpp). Installed at
// process start (the shim ctor below), before the game's own handlers.
// ---------------------------------------------------------------------------
extern "C" void d2_linux_dispatch_fault(int nSig, void* pSiginfo, void* pUctx);
extern "C" void d2_linux_dispatch_exception(EXCEPTION_RECORD* pRecord, CONTEXT* pContext);

void d2_linux_dispatch_fault(int nSig, void*, void* pUctx) {
    // ucontext -> the tree's CONTEXT (i386 greg mapping, d2_ucontext_to_context's
    // register order), then the shared VEH + unhandled-filter dispatch.
    EXCEPTION_RECORD record;
    memset(&record, 0, sizeof(record));
    record.ExceptionCode =
        nSig == SIGSEGV ? 0xC0000005u :   // EXCEPTION_ACCESS_VIOLATION
        nSig == SIGILL  ? 0xC000001Du :   // EXCEPTION_ILLEGAL_INSTRUCTION
        nSig == SIGFPE  ? 0xC0000090u :   // EXCEPTION_FLT_INVALID_OPERATION
        nSig == SIGBUS  ? 0xC0000006u :   // EXCEPTION_IN_PAGE_ERROR
        nSig == SIGABRT ? 0x80000003u :   // EXCEPTION_BREAKPOINT (abort's raise)
                          0xE0000001u;
    CONTEXT ctx;
    Fog::Engine::Application::d2_ucontext_to_context((const ucontext_t*)pUctx, &ctx);
    d2_linux_dispatch_exception(&record, &ctx);
}

namespace {

struct sigaction g_saPrev[32];

void d2_fault_handler(int nSig, siginfo_t* pSiginfo, void* pUctx) {
    d2_linux_dispatch_fault(nSig, pSiginfo, pUctx);
}

struct D2FaultInstaller {
    D2FaultInstaller() {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = d2_fault_handler;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        const int nSigs[] = {SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGABRT};
        for (int nSig : nSigs) {
            if (sigaction(nSig, &sa, &g_saPrev[nSig]) != 0) {
                // A signal this platform lacks; the twin covers the rest.
            }
        }
    }
};
// Install before main() runs, so even the earliest game fault lands in the
// report path (the M0 skeleton installs its own crash paths after SDL; this
// is the same layered-fault order the PE uses).
D2FaultInstaller g_faultInstaller;

}  // namespace
