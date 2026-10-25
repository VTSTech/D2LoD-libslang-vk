// glide3xvk — 0.4.4 window preset apply (Win32 side; design contract in
// g3x_window.h). The pure preset core (table/parse/cycle) lives in the
// header and is host-tested; everything here is thin Win32 plumbing around
// it: read d2slangvk.conf, poll F10, size the window DPI-aware.
//
// Conf-file helpers below mirror g3x_slang.cpp's (exe dir, mtime probe,
// HeapAlloc text read) — deliberately duplicated, NOT shared: g3x_window
// must keep working when the slang chain is unconfigured or failed, and
// slang's helpers are static to its host-tested TU.
#include "g3x_window.h"

#include "g3x_log.h"
#include "g3x_overlay.h"
#include "g3x_slang.h"       // G3xSlangConf + g3x_slang_conf_parse (reused)
#include "vk_backend.h"      // g3x_vk_request_swapchain_refresh (present-side retarget)

#include <cstdio>
#include <cstring>
#include <stdint.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

HWND     s_hwnd = nullptr;
unsigned s_game_w = 0, s_game_h = 0;      // the game's own client size ("native")
char     s_want[32] = "";                 // desired preset token (conf value or F10 pick)
int      s_borderless_want = 0;           // conf borderless=
uint64_t s_conf_mtime = 0;
bool     s_modified = false;              // we resized/re-styled the window since open
LONG_PTR s_orig_style = 0;                // captured before our first modification
bool     s_prev_f10 = false;
unsigned s_tick = 0;
char     s_exe_dir[MAX_PATH] = "";

// MinGW-w64 headers predate per-monitor DPI on some toolchains, and the
// entry point itself is Win10 1607+: declare by hand and load dynamically.
// Missing on older systems = apply without the context switch (the old
// DPI-virtualized best-effort behavior).
const void* const kDpiCtxPerMonitorV2 = (const void*)-4;
typedef void* (*SetThreadDpiCtxFn)(const void*);

// ---- file helpers (twins of g3x_slang.cpp's) -------------------------------

bool exe_dir_of(char* out, size_t cap) {
    char exe[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return false;
    char* slash = std::strrchr(exe, '\\');
    if (!slash) return false;
    *slash = '\0';
    std::snprintf(out, cap, "%s", exe);
    return true;
}

bool conf_path_of(char* out, size_t cap) {
    if (!s_exe_dir[0] && !exe_dir_of(s_exe_dir, sizeof(s_exe_dir))) return false;
    std::snprintf(out, cap, "%s\\d2slangvk.conf", s_exe_dir);
    return true;
}

uint64_t file_mtime_u64(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) return 0;
    ULARGE_INTEGER u{};
    u.HighPart = fa.ftLastWriteTime.dwHighDateTime;
    u.LowPart  = fa.ftLastWriteTime.dwLowDateTime;
    return u.QuadPart;
}

char* read_text_file(const char* path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (1 << 20)) {
        CloseHandle(h); return nullptr;
    }
    const size_t bytes = (size_t)sz.QuadPart;
    char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, bytes + 1);
    if (!buf) { CloseHandle(h); return nullptr; }
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, (DWORD)bytes, &got, nullptr);
    CloseHandle(h);
    if (!ok) { HeapFree(GetProcessHeap(), 0, buf); return nullptr; }
    buf[got] = '\0';
    return buf;
}

// Record-only conf read (applies are driven by init/tick/F10). An F10 pick
// lives until the next conf file change — the file is the source of truth,
// same semantics as F11/F12.
void reread_conf() {
    char path[MAX_PATH];
    if (!conf_path_of(path, sizeof(path))) return;
    s_conf_mtime = file_mtime_u64(path);          // 0 when absent
    char* text = read_text_file(path);
    G3xSlangConf conf;
    std::memset(&conf, 0, sizeof(conf));          // no file = native defaults
    if (text) {
        g3x_slang_conf_parse(text, &conf);
        HeapFree(GetProcessHeap(), 0, text);
    }
    std::snprintf(s_want, sizeof(s_want), "%s", conf.window);
    s_borderless_want = conf.borderless;
}

// Back to the game's own window (style + client size). Runs in the thread's
// NORMAL (DPI-unaware) context on purpose: s_game_w/h and s_orig_style are
// the game's own coordinate space, so the undo must speak the game's units.
void revert_native(const char* why) {
    if (!s_hwnd) return;
    if (s_orig_style) SetWindowLongPtrA(s_hwnd, GWL_STYLE, s_orig_style);
    RECT rc = {0, 0, (LONG)s_game_w, (LONG)s_game_h};
    AdjustWindowRect(&rc, (DWORD)s_orig_style, FALSE);
    SetWindowPos(s_hwnd, HWND_TOP, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    // Present-side retarget: the swapchain (and the preset chain) re-queries
    // caps on the next acquire — the window is back to the game's own size.
    g3x_vk_request_swapchain_refresh();
    s_modified = false;
    g3x_log("window: native (%s) — back to the game's own %ux%u",
            why, s_game_w, s_game_h);
    g3x_overlay_toast("window native");
}

void apply_now(const char* why) {
    if (!s_hwnd) return;
    unsigned w = 0, h = 0;
    int kind = 0;                                 // int: matches g3x_window_preset_size
    if (!g3x_window_preset_size(s_want, &w, &h, &kind)) {
        g3x_log_once("win-tok", "window: unknown preset '%s' — treating as native", s_want);
        kind = 0;
    }
    if (kind == 0) {                              // native ("" or the literal token)
        if (s_modified) revert_native(why);
        return;
    }

    // per-monitor-v2 thread context: every coordinate below is PHYSICAL
    // pixels, immune to the DPI virtualization that reports a maximized
    // 1080p window as 1536x864 inside a 125%-scaled unaware process.
    const HMODULE u32 = GetModuleHandleA("user32.dll");
    SetThreadDpiCtxFn set_ctx = (SetThreadDpiCtxFn)(void*)GetProcAddress(
        u32, "SetThreadDpiAwarenessContext");
    void* old_ctx = nullptr;
    if (set_ctx) old_ctx = set_ctx(kDpiCtxPerMonitorV2);

    // style: borderless (or desktop) strips the frame; presets keep the game's
    LONG_PTR style = s_orig_style
        ? s_orig_style
        : (LONG_PTR)GetWindowLongPtrA(s_hwnd, GWL_STYLE);
    const bool strip = (kind == 2) || s_borderless_want != 0;
    if (strip)
        style = (style | WS_POPUP)
              & ~(LONG_PTR)(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU
                            | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

    RECT rc = {0, 0, (LONG)w, (LONG)h};
    HMONITOR mon = MonitorFromWindow(s_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    const bool have_mon = GetMonitorInfoA(mon, &mi) != 0;
    if (kind == 2 && have_mon) {                  // desktop = the monitor itself
        rc.right = mi.rcMonitor.right - mi.rcMonitor.left;
        rc.bottom = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }
    AdjustWindowRect(&rc, (DWORD)style, FALSE);
    const long ow = rc.right - rc.left, oh = rc.bottom - rc.top;

    long x = 0, y = 0;
    if (have_mon) {
        if (kind == 2) {                          // top-left of the monitor
            x = mi.rcMonitor.left;
            y = mi.rcMonitor.top;
        } else {                                  // centered on the work area
            const long mw = mi.rcWork.right - mi.rcWork.left;
            const long mh = mi.rcWork.bottom - mi.rcWork.top;
            x = mi.rcWork.left + (mw - ow) / 2;
            y = mi.rcWork.top + (mh - oh) / 2;
        }
    }
    SetWindowPos(s_hwnd, HWND_TOP, x, y, ow, oh,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    // 0.4.4 fix: don't wait for the driver to notice the resize (OUT_OF_DATE
    // is unreliable on windows we resize from a per-monitor-v2 context while
    // the process is DPI-unaware — the field receipt was the picture keeping
    // the old extent, a top-left crop of the old scale). Recreate the
    // swapchain from fresh caps on this same frame's acquire; it runs before
    // any present because g3x_window_frame_tick sits at the top of
    // grBufferSwap.
    g3x_vk_request_swapchain_refresh();

    RECT crc{};
    GetClientRect(s_hwnd, &crc);                  // aware ctx -> physical
    const long cw = crc.right - crc.left, ch = crc.bottom - crc.top;
    g3x_log("window: %s%s -> client %ldx%ld (%s)",
            s_want, strip ? " borderless" : "", cw, ch, why);

    char toast[32];
    std::snprintf(toast, sizeof(toast), "window %s %ux%u", s_want, (unsigned)cw,
                  (unsigned)ch);
    g3x_overlay_toast(toast);

    if (set_ctx && old_ctx) set_ctx(old_ctx);
    s_modified = true;
}

void apply_current(const char* why) {
    if (!s_hwnd) return;
    if (!s_want[0]) {
        if (s_modified) revert_native(why);       // window= removed mid-session
        return;
    }
    apply_now(why);
}

} // namespace

void g3x_window_init(void* hwnd, unsigned game_w, unsigned game_h) {
    s_hwnd = (HWND)hwnd;
    s_game_w = game_w;
    s_game_h = game_h;
    if (s_hwnd && !s_orig_style)
        s_orig_style = (LONG_PTR)GetWindowLongPtrA(s_hwnd, GWL_STYLE);
    reread_conf();
    apply_current("window open");                 // re-asserts across mode changes
}

void g3x_window_frame_tick() {
    if (!s_hwnd) return;
    ++s_tick;
    if ((s_tick % 32) == 0) {                     // conf hot-reload, like slang's
        char path[MAX_PATH];
        if (conf_path_of(path, sizeof(path))) {
            const uint64_t m = file_mtime_u64(path);
            if (m != s_conf_mtime) {
                if (m && s_conf_mtime) g3x_log("window: d2slangvk.conf changed on disk — rereading");
                reread_conf();
                apply_current("conf");
            }
        }
    }
    if (GetForegroundWindow() != s_hwnd) {        // don't queue presses made elsewhere
        s_prev_f10 = false;
        return;
    }
    const bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    if (f10 && !s_prev_f10) {
        std::snprintf(s_want, sizeof(s_want), "%s", g3x_window_preset_next_name(s_want));
        apply_current("F10");
    }
    s_prev_f10 = f10;
}
