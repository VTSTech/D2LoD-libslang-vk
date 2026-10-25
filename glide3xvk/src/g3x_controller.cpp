// glide3xvk — 0.4.4 controller support, Win32 side (pure core in
// g3x_controller.h). Polls pad 0 via XInput on a background thread (~125 Hz,
// independent of the game's 25 Hz tick so stick-cursor motion stays smooth)
// and executes the core's events through SendInput — the game just sees a
// normal mouse and keyboard.
//
// Conf: controller=/controller_sens=/controller_deadzone= in [slangvk]
// (parsed by the shared g3x_slang_conf_parse; fields documented in
// g3x_controller.h). The thread probes the conf mtime every 256th poll
// (~2s) — same file-is-truth hot-reload as everywhere else, zero cost on
// the present path (no grBufferSwap hook at all).
//
// Fail-soft house style: opt-in (off by default), XInput loaded dynamically
// (xinput1_4 -> xinput9_1_0 -> xinput1_3; none = logged once and the
// feature stays off), injection gated on the game window being foreground,
// everything held is released on focus loss / disable / shutdown. The
// conf-file helpers mirror g3x_window.cpp's — deliberately duplicated, NOT
// shared (see the note there).
#include "g3x_controller.h"

#include "g3x_log.h"
#include "g3x_slang.h"       // G3xSlangConf + g3x_slang_conf_parse (reused)

#include <cstdio>
#include <cstdlib>           // atof
#include <cstring>
#include <cmath>             // lroundf
#include <stdint.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

// ---- conf-file helpers (twins of g3x_window.cpp's) -------------------------

char s_exe_dir[MAX_PATH] = "";

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

// ---- XInput, loaded dynamically (no import-lib dependency) -----------------

// Minimal replicas of the XInput structs (the real headers are not included;
// layouts match xinput.h's XINPUT_STATE exactly).
struct XINPUT_GAMEPAD_S {
    WORD  wButtons;
    BYTE  bLeftTrigger;
    BYTE  bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
};
struct XINPUT_STATE_S {
    DWORD          dwPacketNumber;
    XINPUT_GAMEPAD_S gamepad;
};
typedef DWORD (WINAPI *XInputGetStateFn)(DWORD, XINPUT_STATE_S*);

HMODULE        s_xinput = nullptr;
XInputGetStateFn s_get_state = nullptr;
const char*    s_xinput_name = "";
bool           s_xinput_tried = false;

bool ensure_xinput() {
    if (s_get_state) return true;
    if (s_xinput_tried) return false;
    s_xinput_tried = true;
    const char* names[] = { "xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll" };
    for (const char* name : names) {
        HMODULE m = LoadLibraryA(name);
        if (!m) continue;
        void* fn = (void*)GetProcAddress(m, "XInputGetState");
        if (fn) {
            s_xinput = m;
            s_get_state = (XInputGetStateFn)fn;
            s_xinput_name = name;
            return true;
        }
        FreeLibrary(m);
    }
    g3x_log_once("ctrl-xi", "controller: no XInput DLL (xinput1_4 / xinput9_1_0 / "
                            "xinput1_3) — feature stays off");
    return false;
}

// ---- module state -----------------------------------------------------------
//
// s_conf / s_core / the accumulators are OWNED BY THE POLL THREAD once it
// runs (init writes them only before the thread starts; hot-reload writes
// happen inside the thread) — no locks needed.

HWND            s_hwnd = nullptr;
HANDLE          s_thread = nullptr;
HANDLE          s_exit_evt = nullptr;
volatile LONG   s_run = 0;
uint64_t        s_conf_mtime = 0;
G3xCtrlConf     s_conf = {};            // zeroed = disabled, defaults
G3xCtrlCore     s_core = {};
float           s_acc_x = 0.0f, s_acc_y = 0.0f;   // stick px remainders

// Cursor speed at full rim deflection before sens= (px/second). The squared
// response curve scales it down near center; at 125 Hz a full-tilt tick
// moves ~10px — smooth at the game's own 25 fps.
const float kMaxPxPerSec = 1200.0f;

enum { kEvCap = 24 };                   // >= worst-case taps+holds of one tick

// ---- injection --------------------------------------------------------------

void inject_key(unsigned char vk, bool up) {
    INPUT inp{};
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = vk;
    inp.ki.wScan = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (up) inp.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
}

void inject_event(const G3xCtrlEvent& e) {
    switch (e.kind) {
        case G3XCTRL_EV_KEY_DOWN:      inject_key(e.vk, false); break;
        case G3XCTRL_EV_KEY_UP:        inject_key(e.vk, true);  break;
        case G3XCTRL_EV_MOUSE_L_DOWN: case G3XCTRL_EV_MOUSE_L_UP:
        case G3XCTRL_EV_MOUSE_R_DOWN: case G3XCTRL_EV_MOUSE_R_UP: {
            INPUT inp{};
            inp.type = INPUT_MOUSE;
            switch (e.kind) {
                case G3XCTRL_EV_MOUSE_L_DOWN: inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;  break;
                case G3XCTRL_EV_MOUSE_L_UP:   inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;    break;
                case G3XCTRL_EV_MOUSE_R_DOWN: inp.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
                default:                      inp.mi.dwFlags = MOUSEEVENTF_RIGHTUP;   break;
            }
            SendInput(1, &inp, sizeof(INPUT));
            break;
        }
        default: break;
    }
}

void inject_move(int dx, int dy) {
    INPUT inp{};
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_MOVE;
    inp.mi.dx = dx;
    inp.mi.dy = dy;
    SendInput(1, &inp, sizeof(INPUT));
}

// Release everything the core tracks + flush stick motion (disable, focus
// loss, pad disconnect, shutdown).
void release_now() {
    G3xCtrlEvent ev[kEvCap];
    const int n = g3x_ctrl_release_all(&s_core, ev, kEvCap);
    for (int i = 0; i < n; ++i) inject_event(ev[i]);
    s_acc_x = s_acc_y = 0.0f;
}

bool window_focused() {
    if (!s_hwnd) return false;
    const HWND fg = GetForegroundWindow();
    if (!fg) return false;
    if (fg == s_hwnd) return true;
    return GetAncestor(fg, GA_ROOT) == s_hwnd;   // input focus on a child
}

// ---- conf -------------------------------------------------------------------

void reread_conf(const char* why) {
    char path[MAX_PATH];
    if (!conf_path_of(path, sizeof(path))) return;
    s_conf_mtime = file_mtime_u64(path);          // 0 when absent
    char* text = read_text_file(path);
    G3xSlangConf raw;
    std::memset(&raw, 0, sizeof(raw));            // no file = disabled defaults
    if (text) {
        g3x_slang_conf_parse(text, &raw);
        HeapFree(GetProcessHeap(), 0, text);
    }
    G3xCtrlConf c;
    c.enabled      = raw.controller;
    c.sens         = raw.controller_sens;
    c.deadzone_pct = raw.controller_deadzone;
    g3x_ctrl_conf_fixup(&c);

    const bool was = s_conf.enabled != 0;
    s_conf = c;
    if (c.enabled && !was) {
        if (ensure_xinput()) {
            s_core = G3xCtrlCore{};
            s_acc_x = s_acc_y = 0.0f;
            g3x_log("controller: on (%s, sens=%.2f deadzone=%d%%) — "
                    "stick=cursor A=LMB B=RMB X=Alt Y=I dpad=1-4 LB=C RB=T "
                    "RT=Shift LT=Ctrl Start=Esc Back=Tab",
                    s_xinput_name, c.sens, c.deadzone_pct);
        } else {
            s_conf.enabled = 0;                   // nothing to inject with
        }
    } else if (!c.enabled && was) {
        release_now();
        g3x_log("controller: off (%s)", why);
    }
}

// ---- poll thread ------------------------------------------------------------

DWORD WINAPI s_poll_proc(LPVOID) {
    LARGE_INTEGER freq{}, prev{};
    const bool have_freq = QueryPerformanceFrequency(&freq) && freq.QuadPart > 0;
    QueryPerformanceCounter(&prev);
    unsigned iter = 0;
    for (;;) {
        const DWORD w = WaitForSingleObject(s_exit_evt, 8);
        if (w == WAIT_OBJECT_0 || !s_run) break;
        ++iter;

        // conf hot-reload (~2s cadence; the file is the source of truth)
        if ((iter % 256) == 0) {
            char path[MAX_PATH];
            if (conf_path_of(path, sizeof(path))) {
                const uint64_t m = file_mtime_u64(path);
                if (m != s_conf_mtime) {
                    if (m && s_conf_mtime)
                        g3x_log("controller: d2slangvk.conf changed on disk — rereading");
                    reread_conf("conf");
                }
            }
        }
        if (!s_conf.enabled || !s_hwnd) continue;
        if (!window_focused()) {
            // never inject into another app; drop stick motion too
            release_now();
            continue;
        }
        if (!ensure_xinput() || !s_get_state) { s_conf.enabled = 0; continue; }

        XINPUT_STATE_S xs{};
        if (s_get_state(0, &xs) != 0 /* ERROR_SUCCESS */) {
            release_now();                        // pad disconnected
            continue;
        }

        // dt (Sleep(8) drifts; measure so cursor speed is wall-clock true)
        LARGE_INTEGER now{};
        float dt = 0.008f;
        if (have_freq && QueryPerformanceCounter(&now) && now.QuadPart > prev.QuadPart) {
            dt = (float)((double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart);
            if (dt < 0.001f) dt = 0.001f;
            if (dt > 0.05f)  dt = 0.05f;
            prev = now;
        }

        G3xCtrlPadState ps;
        ps.sx      = xs.gamepad.sThumbLX;
        ps.sy      = xs.gamepad.sThumbLY;
        ps.buttons = xs.gamepad.wButtons;
        ps.lt      = xs.gamepad.bLeftTrigger;
        ps.rt      = xs.gamepad.bRightTrigger;

        G3xCtrlEvent ev[kEvCap];
        const int n = g3x_ctrl_step(&s_core, &ps, ev, kEvCap);
        for (int i = 0; i < n; ++i) inject_event(ev[i]);

        // left stick -> cursor (pad y up = +; Windows +dy is down)
        float nx = 0.0f, ny = 0.0f;
        const float resp = g3x_ctrl_stick(ps.sx, ps.sy, s_conf.deadzone_pct, &nx, &ny);
        if (resp > 0.0f) {
            const float speed = kMaxPxPerSec * s_conf.sens * resp * dt;
            s_acc_x += nx * speed;
            s_acc_y -= ny * speed;
            const int dx = (int)lroundf(s_acc_x);
            const int dy = (int)lroundf(s_acc_y);
            if (dx || dy) {
                inject_move(dx, dy);
                s_acc_x -= (float)dx;
                s_acc_y -= (float)dy;
            }
        } else {
            s_acc_x = s_acc_y = 0.0f;
        }
    }
    release_now();                                // exit path: nothing held
    return 0;
}

} // namespace

// ---- lifecycle --------------------------------------------------------------

void g3x_controller_init(void* hwnd) {
    s_hwnd = (HWND)hwnd;
    if (!s_hwnd) return;
    if (s_thread) return;                         // mode change: rebind only
    reread_conf("renderer open");
    if (!s_conf.enabled)
        g3x_log_once("ctrl-off", "controller: off (set controller=1 in "
                                 "d2slangvk.conf to enable)");
    s_exit_evt = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!s_exit_evt) { g3x_log("controller: event creation failed — feature off"); return; }
    s_run = 1;
    s_thread = CreateThread(nullptr, 0, s_poll_proc, nullptr, 0, nullptr);
    if (!s_thread) {
        g3x_log("controller: poll thread failed to start — feature off");
        s_run = 0;
        CloseHandle(s_exit_evt);
        s_exit_evt = nullptr;
    }
}

void g3x_controller_shutdown() {
    if (!s_thread) return;
    s_run = 0;
    if (s_exit_evt) SetEvent(s_exit_evt);
    const DWORD w = WaitForSingleObject(s_thread, 1000);
    if (w != WAIT_OBJECT_0) {
        // The thread parks on an 8ms wait; this never fires in practice. If
        // it does, keep the handles so a later shutdown retries and a later
        // init does NOT start a second thread (double input).
        g3x_log("controller: poll thread did not stop in 1s — abandoned "
                "(retrying on the next close)");
        return;
    }
    if (s_exit_evt) CloseHandle(s_exit_evt);
    CloseHandle(s_thread);
    s_exit_evt = nullptr;
    s_thread = nullptr;
    s_hwnd = nullptr;
    // s_xinput stays loaded for the process lifetime (harmless).
}
