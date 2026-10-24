// [d2boot] Diagnostic boot trace — see support/d2_boot.h.
// Always-on and unbuffered on purpose: the failure being chased is a silent
// exit, so every mark is flushed to disk the moment it is written.
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

#include "support/d2_boot.h"

static HANDLE g_hBootLog = INVALID_HANDLE_VALUE;
static char g_szBootLogPath[MAX_PATH] = "(unopened)";

static void BootLogOpen() {
    if (g_hBootLog != INVALID_HANDLE_VALUE) return;
    char szPath[MAX_PATH];
    DWORD n = GetModuleFileNameA(0, szPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    while (n > 0 && szPath[n - 1] != '\\') n--;
    lstrcpyA(szPath + n, "d2boot.log");
    g_hBootLog = CreateFileA(szPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (g_hBootLog == INVALID_HANDLE_VALUE) {
        // Game dir not writable - fall back to %TEMP% and say so in the log.
        char szTmp[MAX_PATH];
        DWORD m = GetTempPathA(MAX_PATH, szTmp);
        if (m == 0 || m >= MAX_PATH) return;
        lstrcatA(szTmp, "d2boot.log");
        g_hBootLog = CreateFileA(szTmp, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (g_hBootLog == INVALID_HANDLE_VALUE) return;
        lstrcpyA(szPath, szTmp);
    }
    lstrcpyA(g_szBootLogPath, szPath);
}

static void BootLogWrite(const char* pszLine) {
    BootLogOpen();
    if (g_hBootLog == INVALID_HANDLE_VALUE) return;
    char szBuf[640];
    int n = wsprintfA(szBuf, "[%8lu ms] %s\r\n", (unsigned long)GetTickCount(), pszLine);
    DWORD dwWritten = 0;
    WriteFile(g_hBootLog, szBuf, (DWORD)n, &dwWritten, 0);
    FlushFileBuffers(g_hBootLog);
}

extern "C" void D2BOOT_Mark(const char* pszStage) {
    BootLogWrite(pszStage);
}

extern "C" void D2BOOT_MarkFmt(const char* pszFormat, ...) {
    char szBody[512];
    va_list args;
    va_start(args, pszFormat);
    wvsprintfA(szBody, pszFormat, args);
    va_end(args);
    BootLogWrite(szBody);
}

extern "C" void D2BOOT_Fail(const char* pszStage) {
    char szMsg[1024];
    wsprintfA(szMsg, "d2 boot diagnostic\n\nExited early at stage:\n  %s\n\n"
              "Full milestone trail: %s\n\n"
              "(This dialog is from the temporary boot-trace build.)", pszStage, g_szBootLogPath);
    BootLogWrite(szMsg);
    MessageBoxA(0, szMsg, "d2.exe boot diagnostic", MB_ICONERROR | MB_OK);
}

// First mark proves the CRT and static initializers ran. Priority 130 puts it
// ahead of the crash (140) and trace (150) handlers on purpose.
__attribute__((init_priority(130))) static struct D2BootInit {
    D2BootInit() {
        BootLogOpen();
        char szLine[MAX_PATH + 64];
        wsprintfA(szLine, "static-init ok (CRT + globals); log = %s", g_szBootLogPath);
        BootLogWrite(szLine);
    }
} g_d2BootInit;
