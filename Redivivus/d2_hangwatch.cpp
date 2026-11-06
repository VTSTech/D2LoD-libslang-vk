// [vanilla-plus hangwatch] Freeze diagnostic - see support/d2_hangwatch.h.
//
// Why: the port's crash chain (d2_crash) only fires on a fatal exception. A
// wedged load (thread blocked in a wait or a lock) looks like a loading
// screen that never finishes, and until now there was no way to tell WHICH
// wait it was - the SFile async INFINITE wait, the sound critical section, a
// QServer worker, or the D3D device. This module turns any freeze into lines
// in d2boot.log: every thread's EIP as a d2exe.map-resolved symbol, so the
// log alone answers "who is stuck where".
//
// Design rules (matching d2_boot/d2_crash house style):
//   - Always on, no config, no cost while healthy (two volatile stores per
//     loop iteration; the watcher wakes twice a second and does nothing).
//   - Read-only: it never takes a game lock, never patches, never injects.
//     It touches game state the way Task Manager does.
//   - One dump per stall episode, re-dump every 60 s while the stall lasts,
//     and a "heartbeat resumed" line when the loop recovers - so a log also
//     records near-freezes the player never noticed.
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "support/d2_boot.h"
#include "support/d2_hangwatch.h"

#ifndef HANGWATCH_THRESHOLD_MS
#define HANGWATCH_THRESHOLD_MS 12000
#endif
#define HANGWATCH_REDUMP_MS 60000
#define HANGWATCH_MAX_FRAMES 24
#define D2_IMAGE_BASE_DEFAULT 0x00400000u

namespace {

volatile unsigned long g_nBeatSeq = 0;       // bumped by every heartbeat
volatile unsigned long g_dwLastBeatTick = 0; // GetTickCount() of the last beat
DWORD g_dwMainThreadId = 0;
HANDLE g_hWatchThread = 0;
int g_bStallDumped = 0;                      // one dump per stall episode
unsigned long g_dwLastDumpTick = 0;

// ---------------------------------------------------------------------------
// d2exe.map symbol table. Loaded once, at the first dump (a freeze already
// happened; a few MB of parsing is irrelevant then). The ld -Map lists
// absolute addresses at the preferred base 0x00400000; ASLR can relocate the
// runtime image (owner runs: 0x00890000, 0x004c0000). Names resolve by RVA
// (relocation-invariant): largest map symbol <= rva + 0x400000. Offsets are
// printed map-relative (rva + 0x400000 - sym) and a relocated run logs a note
// line, so the dump is readable at a glance. [d2boot-fix rev36]
// ---------------------------------------------------------------------------
struct MapSym {
    unsigned long dwAddress;
    char* pszName;
};
MapSym* g_pSyms = 0;
int g_nSymCount = 0;
int* g_pOrder = 0; // symbol indices sorted by address (binary search)
int g_bMapTried = 0;

bool MapLineSymbol(const char* pszLine, unsigned long& rdwAddr, const char*& rpszNameStart,
                   int& rnNameLen) {
    // A map line is whitespace, then either a section header ("text 0x00... 0x1a0")
    // or an entry ("          0x00401000        SymbolName"). Accept the first
    // token when it parses as 0x-hex, then take the NEXT token as the symbol -
    // rejecting names that carry path/section syntax ('/', '\\', '(', '.').
    while (*pszLine == ' ' || *pszLine == '\t' || *pszLine == '\r') pszLine++;
    if (*pszLine != '0' || pszLine[1] != 'x' && pszLine[1] != 'X') return false;
    char* pszEnd = 0;
    unsigned long dwAddr = strtoul(pszLine, &pszEnd, 16);
    if (!pszEnd || pszEnd == pszLine || dwAddr < D2_IMAGE_BASE_DEFAULT) return false;
    const char* pszName = pszEnd;
    while (*pszName == ' ' || *pszName == '\t') pszName++;
    int nLen = 0;
    while (pszName[nLen] && pszName[nLen] != ' ' && pszName[nLen] != '\t' &&
           pszName[nLen] != '\r' && pszName[nLen] != '\n') {
        char c = pszName[nLen];
        if (c == '/' || c == '\\' || c == '(' || c == '.') return false;
        nLen++;
    }
    if (nLen < 2 || nLen > 511) return false;
    rdwAddr = dwAddr;
    rpszNameStart = pszName;
    rnNameLen = nLen;
    return true;
}

void MapSort() {
    // insertion sort on the index: tens of thousands of entries, once per run
    for (int i = 1; i < g_nSymCount; i++) {
        int nTmp = g_pOrder[i];
        unsigned long dwKey = g_pSyms[nTmp].dwAddress;
        int j = i - 1;
        while (j >= 0 && g_pSyms[g_pOrder[j]].dwAddress > dwKey) {
            g_pOrder[j + 1] = g_pOrder[j];
            j--;
        }
        g_pOrder[j + 1] = nTmp;
    }
}

void MapLoad() {
    if (g_bMapTried) return;
    g_bMapTried = 1;
    char szPath[MAX_PATH];
    DWORD n = GetModuleFileNameA(0, szPath, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    while (n > 0 && szPath[n - 1] != '\\') n--;
    lstrcpyA(szPath + n, "d2exe.map");
    FILE* fp = fopen(szPath, "rb");
    if (!fp) {
        // The exe is often copied to the game dir without the map beside it;
        // fall back to the working directory (the build dir layout).
        fp = fopen("d2exe.map", "rb");
        if (!fp) {
            D2BOOT_Mark("hangwatch: d2exe.map not found beside d2.exe - dumps carry rva only "
                        "(copy build/d2exe.map next to d2.exe for names)");
            return;
        }
    }
    fseek(fp, 0, SEEK_END);
    long nSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (nSize <= 0 || nSize > 96 * 1024 * 1024) { fclose(fp); return; }
    char* pszBuf = (char*)malloc((size_t)nSize + 1);
    if (!pszBuf) { fclose(fp); return; }
    size_t nRead = fread(pszBuf, 1, (size_t)nSize, fp);
    fclose(fp);
    pszBuf[nRead] = '\0';

    int nCap = 4096;
    g_pSyms = (MapSym*)malloc((size_t)nCap * sizeof(MapSym));
    if (!g_pSyms) { free(pszBuf); return; }
    char* pszLine = pszBuf;
    while (pszLine < pszBuf + nRead) {
        char* pszNext = strchr(pszLine, '\n');
        if (pszNext) *pszNext = '\0';
        unsigned long dwAddr;
        const char* pszName;
        int nLen;
        if (MapLineSymbol(pszLine, dwAddr, pszName, nLen)) {
            if (g_nSymCount == nCap) {
                int nNewCap = nCap * 2;
                MapSym* pNew = (MapSym*)realloc(g_pSyms, (size_t)nNewCap * sizeof(MapSym));
                if (!pNew) break;
                g_pSyms = pNew;
                nCap = nNewCap;
            }
            char* pszCopy = (char*)malloc((size_t)nLen + 1);
            if (pszCopy) {
                memcpy(pszCopy, pszName, (size_t)nLen);
                pszCopy[nLen] = '\0';
                g_pSyms[g_nSymCount].dwAddress = dwAddr;
                g_pSyms[g_nSymCount].pszName = pszCopy;
                g_nSymCount++;
            }
        }
        if (!pszNext) break;
        pszLine = pszNext + 1;
    }
    free(pszBuf);
    if (g_nSymCount) {
        g_pOrder = (int*)malloc((size_t)g_nSymCount * sizeof(int));
        if (g_pOrder) {
            for (int i = 0; i < g_nSymCount; i++) g_pOrder[i] = i;
            MapSort();
        }
        D2BOOT_MarkFmt("hangwatch: d2exe.map loaded (%d symbols, %08lx..%08lx)", g_nSymCount,
                       (unsigned long)g_pSyms[g_pOrder[0]].dwAddress,
                       (unsigned long)g_pSyms[g_pOrder[g_nSymCount - 1]].dwAddress); // [d2boot-fix rev36] stale-map check: span must match the running build
    }
}

// Largest symbol at or below dwRva, within a 1 MB window (section gaps).
const char* MapResolve(unsigned long dwRva, unsigned long& rdwSymAddr) {
    if (!g_pOrder || !g_nSymCount) return 0;
    int nLo = 0, nHi = g_nSymCount - 1, nBest = -1;
    while (nLo <= nHi) {
        int nMid = (nLo + nHi) / 2;
        if (g_pSyms[g_pOrder[nMid]].dwAddress <= dwRva + D2_IMAGE_BASE_DEFAULT) {
            nBest = nMid;
            nLo = nMid + 1;
        } else {
            nHi = nMid - 1;
        }
    }
    if (nBest < 0) return 0;
    MapSym& rSym = g_pSyms[g_pOrder[nBest]];
    if (dwRva + D2_IMAGE_BASE_DEFAULT - rSym.dwAddress > 0x100000) return 0;
    rdwSymAddr = rSym.dwAddress;
    return rSym.pszName;
}

// ---------------------------------------------------------------------------
// Dump.
// ---------------------------------------------------------------------------
// Kernel-side readability check, no SEH (gcc i686 has no __try) and no race
// with the owner thread: the same primitive d2_crash uses for remote-style
// reads. One syscall per frame, once per dump - irrelevant at freeze time.
bool SafeReadUInt(uintptr_t nAddr, uintptr_t& rnValue) {
    SIZE_T nGot = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)nAddr, &rnValue,
                           sizeof(rnValue), &nGot) || nGot != sizeof(rnValue)) {
        return false;
    }
    return true;
}

void DumpFrameLine(const char* pszPrefix, unsigned long dwAddr, unsigned long dwBase) {
    unsigned long dwRva = dwAddr - dwBase;
    unsigned long dwSymAddr = 0;
    const char* pszName = MapResolve(dwRva, dwSymAddr);
    if (pszName) {
        // Truncate long mangled names: wvsprintfA (inside D2BOOT_MarkFmt) has no
        // output bounds, and D2GFX/D2Game symbols can carry 400+ byte names.
        char szName[300];
        int nLen = lstrlenA(pszName);
        if (nLen > 220) {
            memcpy(szName, pszName, 220);
            lstrcpyA(szName + 220, "...");
            pszName = szName;
        }
        // [d2boot-fix rev36] map-relative offset: the map sits at the preferred base
        // 0x400000 while the runtime base relocates, so dwAddr - dwSymAddr printed every
        // offset inflated by the relocation delta (+0x490000, +0xc0000 in the owner runs).
        D2BOOT_MarkFmt("%s %08lx rva %06lx  %s+0x%lx", pszPrefix, dwAddr, dwRva, pszName,
                       (unsigned long)(dwRva + D2_IMAGE_BASE_DEFAULT - dwSymAddr));
    } else {
        D2BOOT_MarkFmt("%s %08lx rva %06lx", pszPrefix, dwAddr, dwRva);
    }
}

void DumpOneThread(DWORD dwTid, const char* pszTag) {
    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                                THREAD_QUERY_INFORMATION, FALSE, dwTid);
    if (!hThread) {
        D2BOOT_MarkFmt("hangwatch: tid=%lu%s open failed (err %lu)", (unsigned long)dwTid,
                       pszTag, (unsigned long)GetLastError());
        return;
    }
    BOOL bCtx = FALSE;
    CONTEXT ctx;
    ZeroMemory(&ctx, sizeof(ctx));
    if (SuspendThread(hThread) != (DWORD)-1) {
        ctx.ContextFlags = CONTEXT_FULL;
        bCtx = GetThreadContext(hThread, &ctx);
        ResumeThread(hThread);
    } else {
        D2BOOT_MarkFmt("hangwatch: tid=%lu%s suspend failed", (unsigned long)dwTid, pszTag);
    }
    CloseHandle(hThread);
    if (!bCtx) return;

    unsigned long dwBase = (unsigned long)GetModuleHandleA(0);
    D2BOOT_MarkFmt("hangwatch: tid=%lu%s eip=%08lx ebp=%08lx esp=%08lx",
                   (unsigned long)dwTid, pszTag, (unsigned long)ctx.Eip,
                   (unsigned long)ctx.Ebp, (unsigned long)ctx.Esp);
    DumpFrameLine("hangwatch:   #0 eip", (unsigned long)ctx.Eip, dwBase);
    uintptr_t nFp = (uintptr_t)ctx.Ebp;
    for (int d = 0; d < HANGWATCH_MAX_FRAMES && nFp; d++) {
        uintptr_t nRet = 0, nNext = 0;
        if (!SafeReadUInt(nFp + sizeof(uintptr_t), nRet) || !SafeReadUInt(nFp, nNext)) break;
        if (!nRet) break;
        char szPrefix[32];
        wsprintfA(szPrefix, "hangwatch:   #%d ret", d + 1);
        DumpFrameLine(szPrefix, (unsigned long)nRet, dwBase);
        if (nNext <= nFp || nNext - nFp > 8192) break;
        nFp = nNext;
    }
}

void DumpAllThreads(const char* pszReason) {
    MapLoad();
    unsigned long dwBase = (unsigned long)GetModuleHandleA(0);
    unsigned long dwAge = (unsigned long)GetTickCount() - g_dwLastBeatTick;
    D2BOOT_MarkFmt("hangwatch: === %s: no main-loop beat for %lu ms (seq %lu, base %08lx) ===",
                   pszReason, dwAge, (unsigned long)g_nBeatSeq, dwBase);
    if (dwBase != D2_IMAGE_BASE_DEFAULT) { // [d2boot-fix rev36]
        D2BOOT_MarkFmt("hangwatch: note base %08lx = map %08lx + reloc %08lx; names reliable, offsets map-relative",
                       dwBase, (unsigned long)D2_IMAGE_BASE_DEFAULT, dwBase - D2_IMAGE_BASE_DEFAULT);
    }
    int nDumped = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        DWORD dwPid = GetCurrentProcessId();
        DWORD dwSelf = GetCurrentThreadId();
        if (Thread32First(hSnap, &te)) {
            do {
                if (te.th32OwnerProcessID != dwPid) continue;
                if (te.th32ThreadID == dwSelf) continue;
                DumpOneThread(te.th32ThreadID,
                              te.th32ThreadID == g_dwMainThreadId ? " MAIN" : "");
                nDumped++;
            } while (Thread32Next(hSnap, &te));
        }
        CloseHandle(hSnap);
    }
    if (!nDumped) DumpOneThread(g_dwMainThreadId, " MAIN");
    D2BOOT_MarkFmt("hangwatch: === dump end (%d threads) ===", nDumped);
}

DWORD WINAPI HangWatchThreadProc(LPVOID) {
    for (;;) {
        Sleep(2000);
        if (!g_nBeatSeq) continue; // armed, but the main loop has not started yet
        unsigned long dwNow = (unsigned long)GetTickCount();
        unsigned long dwAge = dwNow - g_dwLastBeatTick;
        if (dwAge < HANGWATCH_THRESHOLD_MS) {
            if (g_bStallDumped) {
                g_bStallDumped = 0;
                D2BOOT_MarkFmt("hangwatch: heartbeat resumed (stall lasted >%lu ms, seq %lu)",
                               dwAge, (unsigned long)g_nBeatSeq);
            }
            continue;
        }
        if (g_bStallDumped && dwNow - g_dwLastDumpTick < HANGWATCH_REDUMP_MS) continue;
        DumpAllThreads(g_bStallDumped ? "stall ongoing, re-dump" : "stall detected");
        g_bStallDumped = 1;
        g_dwLastDumpTick = dwNow;
    }
}

} // namespace

extern "C" void D2HANGWATCH_Start(void) {
    if (g_hWatchThread) return;
    g_dwMainThreadId = GetCurrentThreadId();
    g_dwLastBeatTick = (unsigned long)GetTickCount();
    g_hWatchThread = CreateThread(0, 0, HangWatchThreadProc, 0, 0, 0);
    D2BOOT_MarkFmt("hangwatch armed (main tid=%lu, stall threshold %lu ms; dumps land in this "
                   "file, named via d2exe.map when present)",
                   (unsigned long)g_dwMainThreadId, (unsigned long)HANGWATCH_THRESHOLD_MS);
}

extern "C" void D2HANGWATCH_Heartbeat(void) {
    g_nBeatSeq++;
    g_dwLastBeatTick = (unsigned long)GetTickCount();
}
