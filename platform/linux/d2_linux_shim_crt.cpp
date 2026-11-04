// platform/linux/d2_linux_shim_crt.cpp — [d2linux M1] the MSVC CRT bits the
// game TUs call that glibc does not supply under these names, plus the
// exception shim's runtime (the VEH chain RaiseException walks) and the house
// boot/trace marks' linux twins.
//
// Named call sites for every family (PLAN.md §6 M1 inventory): _time32 (save
// headers, Bnclient debug), the _itoa/_strlwr family (Storm SSTR, D2Lang
// strtable), strncpy_s (Fog/Debug symbol path), D2BOOT_Mark/D2Trace (boot and
// per-op traces), RaiseException/AddVectoredExceptionHandler/_set_se_translator
// (Fog/Memory OOM raise, Fog crash handlers, Fog/Debug translator).
//
// PE safety: this file compiles only in the linux build (build.cmake).

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "platform/linux/d2_linux_compat.h"
#include "platform/linux/d2_linux_api.h"
// The exception struct bodies (_EXCEPTION_RECORD/_CONTEXT/_EXCEPTION_POINTERS)
// are the tree's own (Fog/Debug.h's non-Windows block) — the shim constructs
// them, so it sees the same definitions the handlers do.
#include "Fog/Debug.h"

extern "C" {

// ---------------------------------------------------------------------------
// _time32: the save-header and log-stamp clock. i386 glibc time_t IS 32-bit
// long, so the cast is exact.
// ---------------------------------------------------------------------------
__attribute__((weak)) __time32_t _time32(__time32_t* pTime) {
    time_t now = time(NULL);
    if (pTime) *pTime = (__time32_t)now;
    return (__time32_t)now;
}

// ---------------------------------------------------------------------------
// MSVC secure string CRT. MSVC semantics, honestly spelled: strncpy_s copies
// up to count chars and ALWAYS NUL-terminates when room allows; count -1
// (_TRUNCATE, the MSVC SIZE_MAX/2 convention) copies whole src and returns
// 80 (STRUNCATE); a too-long source with room only for part is ERANGE with
// the destination emptied — the same behavior the Win11 build gets from UCRT.
// ---------------------------------------------------------------------------
__attribute__((weak)) errno_t strncpy_s(char* strDest, size_t numberOfElements, const char* strSource, size_t count) {
    if (!strDest || !numberOfElements) return EINVAL;
    if (!strSource) { strDest[0] = '\0'; return count ? EINVAL : 0; }
    if (count == (size_t)-1 || count > (size_t)-1 / 2) count = strlen(strSource) ? strlen(strSource) : 0;  // _TRUNCATE
    size_t nSrc = strnlen(strSource, count + 1);
    if (nSrc > count) nSrc = count;
    if (nSrc < numberOfElements) {
        memcpy(strDest, strSource, nSrc);
        strDest[nSrc] = '\0';
        return nSrc == count && strSource[count] != '\0' ? 80 /* _TRUNCATE */ : 0;
    }
    strDest[0] = '\0';
    return ERANGE;
}

// ---------------------------------------------------------------------------
// MSVC number-to-string + case helpers (Storm/SSTR, D2Lang/strtable). The
// radix forms are the classic CRT spellings; no locale, as on the PE build.
// ---------------------------------------------------------------------------
static size_t d2_ulltoa(unsigned long long v, char* buffer, int radix, int bNegative) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (!buffer || (radix < 2 || radix > 36)) return 0;
    char tmp[70];
    size_t n = 0;
    do {
        tmp[n++] = digits[v % (unsigned)radix];
        v /= (unsigned)radix;
    } while (v);
    size_t out = 0;
    if (bNegative) buffer[out++] = '-';
    while (n) buffer[out++] = tmp[--n];
    buffer[out] = '\0';
    return out;
}

__attribute__((weak)) char* _itoa(int value, char* buffer, int radix) {
    d2_ulltoa((unsigned long long)(value < 0 ? -(long long)value : value), buffer, radix, value < 0 && radix == 10);
    return buffer;
}
__attribute__((weak)) char* _ltoa(long value, char* buffer, int radix) {
    d2_ulltoa((unsigned long long)(value < 0 ? -(long long)value : value), buffer, radix, value < 0 && radix == 10);
    return buffer;
}
__attribute__((weak)) char* _ultoa(unsigned long value, char* buffer, int radix) {
    d2_ulltoa(value, buffer, radix, 0);
    return buffer;
}
__attribute__((weak)) char* _i64toa(long long value, char* buffer, int radix) {
    d2_ulltoa((unsigned long long)(value < 0 ? -value : value), buffer, radix, value < 0 && radix == 10);
    return buffer;
}
__attribute__((weak)) char* _ui64toa(unsigned long long value, char* buffer, int radix) {
    d2_ulltoa(value, buffer, radix, 0);
    return buffer;
}

__attribute__((weak)) char* _strlwr(char* string) {
    for (char* p = string; p && *p; ++p) {
        if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
    }
    return string;
}

__attribute__((weak)) char* _strupr(char* string) {
    for (char* p = string; p && *p; ++p) {
        if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
    }
    return string;
}

int strcasecmp(const char*, const char*);  // glibc supplies it; the decl keeps -Wproto quiet in this TU
int strncasecmp(const char*, const char*, size_t);

// ---------------------------------------------------------------------------
// House boot/trace marks, linux twins. d2boot.log keeps its PE meaning — the
// trail of the most recent run, unbuffered, next to the binary — with the
// path resolved from /proc/self/exe. D2Trace/TraceFmt ride stderr (the
// msvc8-oracle path on the PE compiles them to nothing; the linux build runs
// the real bodies so field logs carry the same marks).
// ---------------------------------------------------------------------------
static FILE* d2_boot_log() {
    static FILE* g_pLog = NULL;
    if (g_pLog) return g_pLog;
    char szExe[1024];
    ssize_t n = readlink("/proc/self/exe", szExe, sizeof(szExe) - 1);
    if (n <= 0) return NULL;
    szExe[n] = '\0';
    char* p = strrchr(szExe, '/');
    if (p) *(p + 1) = '\0'; else szExe[0] = '\0';
    strncat(szExe, "d2boot.log", sizeof(szExe) - strlen(szExe) - 1);
    g_pLog = fopen(szExe, "w");  // CREATE_ALWAYS: the most recent run only (PE comment)
    if (g_pLog) setvbuf(g_pLog, NULL, _IONBF, 0);
    return g_pLog;
}

static void d2_boot_vmark(const char* pszFormat, va_list ap) {
    FILE* f = d2_boot_log();
    if (!f) return;
    vfprintf(f, pszFormat, ap);
    fputc('\n', f);
}

void D2BOOT_Mark(const char* pszMark) {
    FILE* f = d2_boot_log();
    if (!f) return;
    fprintf(f, "%s\n", pszMark ? pszMark : "");
}

void D2BOOT_MarkFmt(const char* pszFormat, ...) {
    va_list ap;
    va_start(ap, pszFormat);
    d2_boot_vmark(pszFormat, ap);
    va_end(ap);
}

void D2Trace(const char* pszWhere) {
    fprintf(stderr, "d2trace: %s\n", pszWhere ? pszWhere : "");
}

void D2TraceFmt(const char* pszFormat, ...) {
    va_list ap;
    va_start(ap, pszFormat);
    fprintf(stderr, "d2trace: ");
    vfprintf(stderr, pszFormat ? pszFormat : "", ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

// ---------------------------------------------------------------------------
// The exception shim runtime: the vectored-handler chain RaiseException walks,
// the unhandled filter SetUnhandledExceptionFilter stores, and the
// _set_se_translator slot Fog/Debug installs into. The signal half — ucontext
// capture, the chain dispatch on a real fault, the d2crash-*.txt linux twin —
// lives in d2_linux_crash.cpp; this is the shared bookkeeping.
// ---------------------------------------------------------------------------
struct D2VehNode {
    PVECTORED_EXCEPTION_HANDLER pfnHandler;
    D2VehNode* pNext;
};

static pthread_mutex_t g_csVeh = PTHREAD_MUTEX_INITIALIZER;
static D2VehNode* g_pVehHead = NULL;
static LPTOP_LEVEL_EXCEPTION_FILTER g_pUnhandledFilter = NULL;
static pthread_mutex_t g_csFilter = PTHREAD_MUTEX_INITIALIZER;
static thread_local void* tls_seTranslator = NULL;

ULONG AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER pfnHandler) {
    if (!pfnHandler) return 0;
    D2VehNode* pNode = (D2VehNode*)malloc(sizeof(*pNode));
    if (!pNode) return 0;
    pNode->pfnHandler = pfnHandler;
    pthread_mutex_lock(&g_csVeh);
    if (First) {
        pNode->pNext = g_pVehHead;
        g_pVehHead = pNode;
    } else {
        pNode->pNext = NULL;
        if (!g_pVehHead) {
            g_pVehHead = pNode;
        } else {
            D2VehNode* p = g_pVehHead;
            while (p->pNext) p = p->pNext;
            p->pNext = pNode;
        }
    }
    pthread_mutex_unlock(&g_csVeh);
    return (ULONG)pNode;
}

ULONG RemoveVectoredExceptionHandler(ULONG hHandler) {
    pthread_mutex_lock(&g_csVeh);
    D2VehNode** pp = &g_pVehHead;
    while (*pp && (ULONG)*pp != hHandler) pp = &(*pp)->pNext;
    if (!*pp) { pthread_mutex_unlock(&g_csVeh); return 0; }
    D2VehNode* pDead = *pp;
    *pp = pDead->pNext;
    pthread_mutex_unlock(&g_csVeh);
    free(pDead);
    return 1;
}

LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter) {
    pthread_mutex_lock(&g_csFilter);
    LPTOP_LEVEL_EXCEPTION_FILTER pPrevious = g_pUnhandledFilter;
    g_pUnhandledFilter = lpTopLevelExceptionFilter;
    pthread_mutex_unlock(&g_csFilter);
    return pPrevious;
}

void* _set_se_translator(void* pfnTranslator) {
    void* pPrevious = tls_seTranslator;
    tls_seTranslator = pfnTranslator;
    return pPrevious;
}

void RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags,
                    DWORD nNumberOfArguments, const ULONG_PTR* lpArguments) {
    // Build the record the handlers read, capture a live context, walk the
    // chain in registration order, then honor the unhandled filter. The
    // signal path in d2_linux_crash.cpp funnels through the same walk.
    EXCEPTION_RECORD record;
    memset(&record, 0, sizeof(record));
    record.ExceptionCode = dwExceptionCode;
    record.ExceptionFlags = dwExceptionFlags;
    record.NumberParameters = nNumberOfArguments > 15 ? 15 : nNumberOfArguments;
    for (DWORD i = 0; i < record.NumberParameters; ++i) record.ExceptionInformation[i] = lpArguments[i];

    pthread_mutex_lock(&g_csVeh);
    D2VehNode* pChain = g_pVehHead;
    pthread_mutex_unlock(&g_csVeh);
    for (D2VehNode* p = pChain; p; p = p->pNext) {
        // The PE passes the raising thread's context; M1 handlers (Fog's
        // fault logger) read the record, not the context.
        EXCEPTION_POINTERS pointers;
        CONTEXT ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.ContextFlags = 0x1003F;
        pointers.ExceptionRecord = &record;
        pointers.ContextRecord = &ctx;
        LONG disposition = p->pfnHandler(&pointers);
        if (disposition == 0xFFFFFFFFu /* EXCEPTION_CONTINUE_EXECUTION */) return;
        // EXCEPTION_CONTINUE_SEARCH: keep walking, as the PE does.
    }

    pthread_mutex_lock(&g_csFilter);
    LPTOP_LEVEL_EXCEPTION_FILTER pFilter = g_pUnhandledFilter;
    pthread_mutex_unlock(&g_csFilter);
    if (pFilter) {
        EXCEPTION_POINTERS pointers;
        CONTEXT ctx;
        memset(&ctx, 0, sizeof(ctx));
        pointers.ExceptionRecord = &record;
        pointers.ContextRecord = &ctx;
        pFilter(&pointers);
    }
    // Unhandled and non-continuable: the PE would die right here.
    if (dwExceptionFlags & EXCEPTION_NONCONTINUABLE) {
        fprintf(stderr, "d2linux: unhandled exception 0x%08lx\n", (unsigned long)dwExceptionCode);
        abort();
    }
}

// The signal half (d2_linux_crash.cpp) funnels here: dispatch a REAL fault's
// record + context through the same chain, then restore the previous handler
// and re-raise so a core can still be produced when nothing handles it.
void d2_linux_dispatch_exception(EXCEPTION_RECORD* pRecord, CONTEXT* pContext);

void d2_linux_dispatch_exception(EXCEPTION_RECORD* pRecord, CONTEXT* pContext) {
    pthread_mutex_lock(&g_csVeh);
    D2VehNode* pChain = g_pVehHead;
    pthread_mutex_unlock(&g_csVeh);
    for (D2VehNode* p = pChain; p; p = p->pNext) {
        EXCEPTION_POINTERS pointers;
        pointers.ExceptionRecord = pRecord;
        pointers.ContextRecord = pContext;
        LONG disposition = p->pfnHandler(&pointers);
        if (disposition == 0xFFFFFFFFu) return;  // handled
    }

    pthread_mutex_lock(&g_csFilter);
    LPTOP_LEVEL_EXCEPTION_FILTER pFilter = g_pUnhandledFilter;
    pthread_mutex_unlock(&g_csFilter);
    if (pFilter) {
        EXCEPTION_POINTERS pointers;
        pointers.ExceptionRecord = pRecord;
        pointers.ContextRecord = pContext;
        LONG disposition = pFilter(&pointers);
        if (disposition == 0xFFFFFFFFu) return;
    }
    // Nothing handled it: abort produces the core the platform debugger reads.
    fprintf(stderr, "d2linux: unhandled fault 0x%08lx, aborting\n", (unsigned long)pRecord->ExceptionCode);
    abort();
}

}  // extern "C"
