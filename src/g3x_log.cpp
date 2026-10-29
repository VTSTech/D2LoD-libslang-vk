#include "g3x_log.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

FILE* g_log = nullptr;
CRITICAL_SECTION g_cs;
bool g_cs_ok = false;
uint64_t g_start_ms = 0;
char g_path[MAX_PATH] = ""; // full path of the active log (rotation target)
uint64_t g_bytes = 0;       // running size estimate of the active log file

// 0.4.5: hard cap — the file is renamed to glide3x.log.1 and a fresh log is
// started whenever it reaches this size, checked at open AND on every written
// line (tracked bytewise, no per-line stat). Long single sessions no longer
// grow the log unbounded; worst case on disk is ~2 MB (log + .1).
constexpr uint64_t kRotateBytes = 1024ull * 1024;

// log-once registry: tag -> count. Small fixed table, no allocations — this
// runs inside the game's render loop and, at attach, inside the loader lock.
struct OnceEntry {
    char tag[40];  // must hold the longest tag we use: "grTexDownloadTable:palette" (26)
    uint32_t count;
};
constexpr int kMaxOnce = 64;
OnceEntry g_once[kMaxOnce];
int g_once_n = 0;

OnceEntry* once_slot(const char* tag) {
    for (int i = 0; i < g_once_n; ++i) {
        if (strcmp(g_once[i].tag, tag) == 0) return &g_once[i];
    }
    if (g_once_n < kMaxOnce) {
        OnceEntry* e = &g_once[g_once_n++];
        strncpy(e->tag, tag, sizeof(e->tag) - 1);
        e->tag[sizeof(e->tag) - 1] = '\0';
        e->count = 0;
        return e;
    }
    return nullptr; // registry full: silently drop (should never happen)
}

uint64_t now_ms() {
    return GetTickCount64();
}

void rotate_if_big() {
    if (!g_log || !g_path[0] || g_bytes < kRotateBytes) return;
    fclose(g_log);
    g_log = nullptr;
    char old[MAX_PATH];
    _snprintf(old, sizeof(old) - 1, "%s.1", g_path);
    old[sizeof(old) - 1] = '\0';
    remove(old);
    rename(g_path, old);
    g_log = fopen(g_path, "ab");
    g_bytes = 0;
    if (g_log) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        g_bytes = (uint64_t)fprintf(g_log,
                "==== glide3xvk log rotated %02u:%02u:%02u — previous %lu KB moved to %s ===="
                "\n==== glide3xvk session (continued) %04u-%02u-%02u %02u:%02u:%02u ====\n",
                st.wHour, st.wMinute, st.wSecond,
                (unsigned long)(kRotateBytes / 1024), old);
        g_bytes += (uint64_t)fprintf(g_log, "[g3xvk] (rotation marker)\n");
        fflush(g_log);
    }
}

void write_line(const char* line) {
    if (!g_log) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    // "[HH:MM:SS.mmm] " = 14 chars, + line + \n; close enough for a size cap.
    g_bytes += 14 + strlen(line) + 1;
    fprintf(g_log, "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond,
            st.wMilliseconds, line);
    fflush(g_log);
    rotate_if_big(); // callers hold g_cs; mid-session hard cap
}

} // namespace

void g3x_log_open() {
    if (g_log) return;
    if (!g_cs_ok) { InitializeCriticalSection(&g_cs); g_cs_ok = true; }

    // Same placement policy as d2boot.log: beside the game exe first.
    char path[MAX_PATH];
    char chosen[MAX_PATH] = "";
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) > 0) {
        char* slash = strrchr(path, '\\');
        if (slash) {
            *slash = '\0';
            _snprintf(chosen, sizeof(chosen) - 1, "%s\\glide3x.log", path);
            chosen[sizeof(chosen) - 1] = '\0';
        }
    }
    // P2.4: APPEND, don't truncate. The round-6 field report proved a quick
    // relaunch (mode-probe check) wipes the log of the real gameplay session
    // that came before it — sessions are separated by a banner line instead.
    if (chosen[0]) g_log = fopen(chosen, "ab");
    if (!g_log) { // %TEMP% fallback
        if (GetTempPathA(MAX_PATH, path) > 0) {
            _snprintf(chosen, sizeof(chosen) - 1, "%sglide3x.log", path);
            chosen[sizeof(chosen) - 1] = '\0';
            g_log = fopen(chosen, "ab");
        }
    }
    // 0.4.5: 1 MB rotation (was 2 MB, open-time only). Rename to .1 and start
    // fresh so the file can't grow unbounded — across many sessions (this
    // open-time check) or inside one long session (the per-line check in
    // write_line).
    if (g_log) {
        fseek(g_log, 0, SEEK_END);
        const long size = ftell(g_log);
        fclose(g_log); g_log = nullptr;
        if (size > (long)kRotateBytes) {
            char old[MAX_PATH];
            _snprintf(old, sizeof(old) - 1, "%s.1", chosen);
            old[sizeof(old) - 1] = '\0';
            remove(old);
            rename(chosen, old);
        }
        g_log = fopen(chosen, "ab");
    }
    _snprintf(g_path, sizeof(g_path) - 1, "%s", chosen);
    g_path[sizeof(g_path) - 1] = '\0';
    g_bytes = 0;
    if (g_log && g_path[0]) {
        fseek(g_log, 0, SEEK_END);
        g_bytes = (uint64_t)ftell(g_log);
    }
    // Session banner: makes multi-launch logs readable at a glance.
    if (g_log) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(g_log, "==== glide3xvk session %04u-%02u-%02u %02u:%02u:%02u ====\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        fflush(g_log);
    }
    g_start_ms = now_ms();
}

void g3x_log(const char* fmt, ...) {
    if (!g_log) return;
    EnterCriticalSection(&g_cs);
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
    msg[sizeof(msg) - 1] = '\0';
    va_end(ap);
    char line[1100];
    _snprintf(line, sizeof(line) - 1, "[g3xvk] %s", msg);
    line[sizeof(line) - 1] = '\0';
    write_line(line);
    LeaveCriticalSection(&g_cs);
}

void g3x_log_once(const char* tag, const char* fmt, ...) {
    if (!g_log) return;
    EnterCriticalSection(&g_cs);
    OnceEntry* e = once_slot(tag);
    uint32_t n = e ? ++e->count : 0;
    if (n > 8) { // same "budget" idea as the d2boot AllocAndLoad marks
        LeaveCriticalSection(&g_cs);
        return;
    }
    char msg[1000];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
    msg[sizeof(msg) - 1] = '\0';
    va_end(ap);
    char line[1100];
    if (n == 8) {
        _snprintf(line, sizeof(line) - 1, "[g3xvk] %s: budget reached, going quiet", tag);
    } else {
        _snprintf(line, sizeof(line) - 1, "[g3xvk] %s(#%u): %s", tag, n, msg);
    }
    line[sizeof(line) - 1] = '\0';
    write_line(line);
    LeaveCriticalSection(&g_cs);
}

void g3x_log_summary() {
    if (!g_log) return;
    EnterCriticalSection(&g_cs);
    write_line("[g3xvk] ---- activity summary ----");
    for (int i = 0; i < g_once_n; ++i) {
        char line[144];
        _snprintf(line, sizeof(line) - 1, "[g3xvk]   %-40s x %u call(s)",
                  g_once[i].tag, g_once[i].count);
        write_line(line);
    }
    LeaveCriticalSection(&g_cs);
}

void g3x_log_close() {
    if (g_cs_ok) { g3x_log_summary(); }
    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }
    if (g_cs_ok) { DeleteCriticalSection(&g_cs); g_cs_ok = false; }
}
