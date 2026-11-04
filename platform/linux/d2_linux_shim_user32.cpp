// platform/linux/d2_linux_shim_user32.cpp — [d2linux M2] the user32/GDI shim:
// the game's window, its message pump and its cursor, over SDL3.
//
// The boot chain creates a real Win32 window and drives everything through
// messages: D2GFX_Initialize registers the "Diablo II" class with D2WinMain's
// WndProc, D2GFX_CreateWindow creates the window, and three pump shapes
// (D2WinMain::MessagePump's Peek-probe + GetMessage, MainMenus'
// GetAndDispatchAllPossible PM_REMOVE drain, D2Client's Bink/cinematic pump)
// dispatch into the data-driven handler table (globals/D2Win.cpp's
// gaUIWindowHandlers). This file makes that surface real on Linux:
//
//   - a window registry mapping the HWND (the SDL_Window*) to its class proc,
//     client size and shown state;
//   - a message queue fed by an SDL event pump that runs inside
//     PeekMessageA/GetMessageA — every pump shape the game uses works because
//     each call first drains SDL into the queue, then serves from it
//     (PM_NOREMOVE peeks, PM_REMOVE pops, GetMessage blocks; WM_QUIT is
//     delivered Win32-style once the queue drains);
//   - input translation to the exact WM_* stream the handler table registers:
//     WM_MOUSEMOVE/NCMOVE/LEAVE, 0x201-0x208 buttons, WM_MOUSEWHEEL, WM_CHAR,
//     WM_KEYDOWN/UP with VK wParams, WM_ACTIVATEAPP, WM_CLOSE;
//   - the load-bearing small stuff: ShowCursor's display-count semantics (the
//     game's HideOSCursor loops on the return value), GetKeyState's 0x8000 bit
//     (edit-box shift-select), FindWindowA as the single-instance mutex (a
//     flock(2) claim — the probe IS the claim), and a MessageBoxA that surfaces
//     as an SDL dialog + the boot-log-style stderr line.
//
// GDI is fail-soft by design: its only slice consumer is D2DDraw's debug-font
// rasterizer, which tolerates a NULL HDC (the mask fills blank = no debug
// text) — verified against D2ddDebugFont_Alloc's null paths.
//
// PE safety: this file compiles only in the linux build (build.cmake).

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

#include "platform/linux/d2_linux_compat.h"
#include "platform/linux/d2_linux_api.h"
#include "platform/linux/d2_linux_video.h"

extern "C" {

// ---------------------------------------------------------------------------
// Constants (winuser.h). d2_linux_api.h spells the _W2 set for the header's
// consumers; the pump needs a few more, defined here where they are used.
// ---------------------------------------------------------------------------
#define WM_DESTROY_U32      0x0002u
#define WM_KEYDOWN_U32      0x0100u
#define WM_KEYUP_U32        0x0101u
#define WM_CHAR_U32         0x0102u
#define WM_MOUSEMOVE_U32    0x0200u
#define WM_LBUTTONDOWN_U32  0x0201u
#define WM_LBUTTONUP_U32    0x0202u
#define WM_RBUTTONDOWN_U32  0x0204u
#define WM_RBUTTONUP_U32    0x0205u
#define WM_XBUTTONDOWN_U32  0x0207u
#define WM_XBUTTONUP_U32    0x0208u
#define WM_MOUSEWHEEL_U32   0x020Au
#define WM_MOUSELEAVE_U32   0x02A3u
#define WM_NCMOUSEMOVE_U32  0x00A0u
#define WM_ACTIVATEAPP_U32  0x001Cu
#define WM_CLOSE_U32        0x0010u
#define WM_QUIT_U32         0x0012u
#define MK_LBUTTON_U32      0x0001u
#define MK_RBUTTON_U32      0x0002u
#define MK_SHIFT_U32        0x0004u
#define MK_CONTROL_U32      0x0008u
#define MK_MBUTTON_U32      0x0010u
#define WS_POPUP_U32        0x80000000u
#define WS_CAPTION_U32      0x00C00000u

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------

struct D2GameWindow {
    SDL_Window* window;
    void*       wndproc;      // the registered class proc (invoked __stdcall)
    char        title[128];
    int         client_w;
    int         client_h;
    bool        shown;
    bool        track_armed;  // TME_LEAVE armed by TrackMouseEvent
};

static std::vector<D2GameWindow*> g_windows;

// [d2linux M2] D2WinMsg — the shim's PRIVATE message record, layout-identical
// to the game's tagMSG (D2Client/Message.h's non-Windows definition, the same
// arrangement windows.h's tagMSG has on the PE line): hwnd 0x00 / message 0x04
// / wParam 0x08 / lParam 0x0C / time 0x10 / pt 0x14. The pump crosses the
// type boundary through PeekMessageA/GetMessageA/DispatchMessageA's void*
// parameters — the game fills ITS tagMSG, the shim fills THIS, one memory
// shape. (The type stays private precisely so the shim never competes with
// the game header over the tagMSG name.)
struct D2WinMsg {
    void*    hwnd;              /* 0x00 HWND */
    uint32_t message;           /* 0x04 UINT */
    uintptr_t wParam;           /* 0x08 WPARAM */
    intptr_t lParam;            /* 0x0C LPARAM */
    uint32_t time;              /* 0x10 DWORD */
    struct POINT pt;            /* 0x14 */
};                              /* 0x1C */

static std::deque<D2WinMsg>       g_queue;
static bool                       g_quitPosted = false;
static int                        g_quitCode   = 0;
static bool                       g_keyDown[256] = {false};
static int                        g_cursorCount  = 0;   // ShowCursor display count
static HGLOBAL                    g_clipHandle   = NULL; // clipboard slot (one)
static char*                      g_clipText     = NULL;

// The SDL_Window* doubles as the HWND. SDL heap pointers are never small, so
// the sentinel pseudo-HWNDs below cannot collide with a real window.
#define D2_HWND_DESKTOP ((HWND)(uintptr_t)0x00000002)

static D2GameWindow* WindowOf(HWND hwnd) {
    if (!hwnd) {
        return g_windows.empty() ? NULL : g_windows.front();
    }
    for (D2GameWindow* w : g_windows) {
        if ((HWND)w->window == hwnd) {
            return w;
        }
    }
    return NULL;
}

// [d2linux M2 round 2] internal cross-shim hook (link-level, NOT game API):
// the DirectDraw shim validates SetCooperativeLevel's hwnd against this
// registry — a cooperative level on a window this process never created is
// refused (FALSE), including the null/desktop sentinels.
bool d2linux_user32_window_registered(HWND hwnd) {
    return hwnd && WindowOf(hwnd) != NULL;
}

static void QueueMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    D2WinMsg m;
    m.hwnd    = hwnd;
    m.message = msg;
    m.wParam  = wp;
    m.lParam  = lp;
    m.time    = SDL_GetTicks();
    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    m.pt.x = (long)mx;
    m.pt.y = (long)my;
    g_queue.push_back(m);
}

// The game's WndProc contract: every registered class proc is a Win32
// WNDPROC — __stdcall on i386 (callee-cleans). The shim owns every call
// through this typedef, so the convention is applied here and nowhere else.
typedef LRESULT (__stdcall *D2_WNDPROC)(HWND, UINT, WPARAM, LPARAM);

static LRESULT DispatchToWindow(const D2WinMsg& m) {
    D2GameWindow* w = WindowOf(m.hwnd);
    if (w && w->wndproc) {
        D2_WNDPROC proc = (D2_WNDPROC)w->wndproc;
        return proc(m.hwnd, m.message, m.wParam, m.lParam);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// SDL scancode/keycode → Win32 VK. The game's handler table keys on
// WM_KEYDOWN wParams (BACK/TAB/RETURN/CTRL/ESCAPE/SPACE/PGUP/PGDN/END/HOME/
// arrows/DELETE/SNAPSHOT/H/M/N/R/F1..) and the edit box on WM_CHAR; specials
// map by scancode (layout-independent), the printable set by SDL's
// layout-mapped keycode — the closest analogue of the Windows VK mapping.
// ---------------------------------------------------------------------------
static UINT SdlKeyToVk(const SDL_Event& ev) {
    SDL_Scancode sc = ev.key.scancode;
    switch (sc) {
    case SDL_SCANCODE_ESCAPE:         return 0x1B;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:       return 0x0D;
    case SDL_SCANCODE_BACKSPACE:      return 0x08;
    case SDL_SCANCODE_TAB:            return 0x09;
    case SDL_SCANCODE_SPACE:          return 0x20;
    case SDL_SCANCODE_PAGEUP:         return 0x21;
    case SDL_SCANCODE_PAGEDOWN:       return 0x22;
    case SDL_SCANCODE_END:            return 0x23;
    case SDL_SCANCODE_HOME:           return 0x24;
    case SDL_SCANCODE_LEFT:           return 0x25;
    case SDL_SCANCODE_UP:             return 0x26;
    case SDL_SCANCODE_RIGHT:          return 0x27;
    case SDL_SCANCODE_DOWN:           return 0x28;
    case SDL_SCANCODE_INSERT:         return 0x2D;
    case SDL_SCANCODE_DELETE:         return 0x2E;
    case SDL_SCANCODE_PRINTSCREEN:    return 0x2C;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:         return 0x10;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:          return 0x11;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:           return 0x12;
    case SDL_SCANCODE_CAPSLOCK:       return 0x14;
    case SDL_SCANCODE_NUMLOCKCLEAR:   return 0x90;
    case SDL_SCANCODE_SCROLLLOCK:     return 0x91;
    case SDL_SCANCODE_KP_MULTIPLY:    return 0x6A;
    case SDL_SCANCODE_KP_PLUS:        return 0x6B;
    case SDL_SCANCODE_KP_MINUS:       return 0x6D;
    case SDL_SCANCODE_KP_DECIMAL:     return 0x6E;
    case SDL_SCANCODE_KP_DIVIDE:      return 0x6F;
    default: break;
    }
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) {
        return 0x70 + (UINT)(sc - SDL_SCANCODE_F1);
    }
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) {
        return 0x61 + (UINT)(sc - SDL_SCANCODE_KP_1);
    }
    if (sc == SDL_SCANCODE_KP_0) {
        return 0x60;
    }
    SDL_Keycode kc = ev.key.key;
    if (kc >= 'a' && kc <= 'z') return (UINT)(kc - 'a' + 'A');  // VK_A..VK_Z
    if (kc >= 'A' && kc <= 'Z') return (UINT)kc;
    if (kc >= '0' && kc <= '9') return (UINT)kc;
    switch (kc) {
    case ';':  return 0xBA;  // VK_OEM_1
    case '=':  return 0xBB;  // VK_OEM_PLUS
    case ',':  return 0xBC;  // VK_OEM_COMMA
    case '-':  return 0xBD;  // VK_OEM_MINUS
    case '.':  return 0xBE;  // VK_OEM_PERIOD
    case '/':  return 0xBF;  // VK_OEM_2
    case '`':  return 0xC0;  // VK_OEM_3
    case '[':  return 0xDB;  // VK_OEM_4
    case '\\': return 0xDC;  // VK_OEM_5
    case ']':  return 0xDD;  // VK_OEM_6
    case '\'': return 0xDE;  // VK_OEM_7
    default:   break;
    }
    return 0;
}

static UINT MouseWParam(bool left, bool right, bool middle) {
    UINT flags = 0;
    if (left)   flags |= MK_LBUTTON_U32;
    if (right)  flags |= MK_RBUTTON_U32;
    if (middle) flags |= MK_MBUTTON_U32;
    if (g_keyDown[0x10]) flags |= MK_SHIFT_U32;
    if (g_keyDown[0x11]) flags |= MK_CONTROL_U32;
    return flags;
}

// ---------------------------------------------------------------------------
// The SDL event pump — drains SDL into the shim's message queue. Runs at the
// top of every PeekMessageA/GetMessageA so ANY of the game's pump shapes see
// a coherent queue without a host-side loop of its own.
// ---------------------------------------------------------------------------
static void PumpSDL() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            // Window-manager quit (the X button on decorations). The game's
            // own WndProc turns WM_CLOSE into PostQuitMessage(0).
            QueueMessage(NULL, WM_CLOSE_U32, 0, 0);
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            SDL_Window* win = SDL_GetWindowFromID(ev.window.windowID);
            QueueMessage((HWND)win, WM_CLOSE_U32, 0, 0);
            break;
        }

        case SDL_EVENT_WINDOW_FOCUS_GAINED: {
            SDL_Window* win = SDL_GetWindowFromID(ev.window.windowID);
            QueueMessage((HWND)win, WM_ACTIVATEAPP_U32, 1, 0);
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_LOST: {
            SDL_Window* win = SDL_GetWindowFromID(ev.window.windowID);
            QueueMessage((HWND)win, WM_ACTIVATEAPP_U32, 0, 0);
            break;
        }

        case SDL_EVENT_WINDOW_MOUSE_ENTER: {
            D2GameWindow* w = WindowOf((HWND)SDL_GetWindowFromID(ev.window.windowID));
            if (w) {
                // Re-arm handled at TrackMouseEvent; entering resets the leave
                // state machine.
                w->track_armed = false;
            }
            break;
        }
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            D2GameWindow* w = WindowOf((HWND)SDL_GetWindowFromID(ev.window.windowID));
            if (w && w->track_armed) {
                w->track_armed = false;
                QueueMessage((HWND)w->window, WM_MOUSELEAVE_U32, 0, 0);
            }
            break;
        }

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            UINT vk = SdlKeyToVk(ev);
            if (ev.key.down && vk < 256) {
                g_keyDown[vk] = true;
            } else if (!ev.key.down && vk < 256) {
                g_keyDown[vk] = false;
            }
            if (!vk) {
                break;
            }
            SDL_Window* win = SDL_GetWindowFromID(ev.key.windowID);
            // lParam bits (winuser.h): 0-15 repeat, 16-23 scan, 30 prev state,
            // 31 transition. The game reads wParam only; fill for fidelity.
            LPARAM lp = (LPARAM)(1 | ((int)ev.key.scancode << 16));
            if (!ev.key.down) {
                lp |= (LPARAM)0xC0000000;
            }
            QueueMessage((HWND)win, ev.key.down ? WM_KEYDOWN_U32 : WM_KEYUP_U32,
                         (WPARAM)vk, lp);
            break;
        }

        case SDL_EVENT_TEXT_INPUT: {
            SDL_Window* win = SDL_GetWindowFromID(ev.text.windowID);
            // WM_CHAR per codepoint (the game's edit box consumes ASCII).
            const char* t = ev.text.text;
            while (*t) {
                unsigned int cp = (unsigned char)*t;
                int adv = 1;
                if (cp >= 0xF0)      { cp = ((cp & 0x07) << 18) | ((t[1] & 0x3F) << 12) | ((t[2] & 0x3F) << 6) | (t[3] & 0x3F); adv = 4; }
                else if (cp >= 0xE0) { cp = ((cp & 0x0F) << 12) | ((t[1] & 0x3F) << 6) | (t[2] & 0x3F); adv = 3; }
                else if (cp >= 0xC0) { cp = ((cp & 0x1F) << 6) | (t[1] & 0x3F); adv = 2; }
                t += adv;
                if (cp > 0xFF) cp = '?';   // ANSI WM_CHAR: one byte
                QueueMessage((HWND)win, WM_CHAR_U32, (WPARAM)cp, (LPARAM)1);
            }
            break;
        }

        case SDL_EVENT_MOUSE_MOTION: {
            SDL_Window* win = SDL_GetWindowFromID(ev.motion.windowID);
            D2GameWindow* w = WindowOf((HWND)win);
            if (w && !w->track_armed) {
                // Win32 re-arms TME_LEAVE on each WM_MOUSEMOVE that wants
                // leave notifications; mirror that by arming on entry.
                w->track_armed = true;
            }
            int x = (int)ev.motion.x, y = (int)ev.motion.y;
            UINT flags = MouseWParam(ev.motion.state & SDL_BUTTON_LMASK,
                                     ev.motion.state & SDL_BUTTON_RMASK,
                                     ev.motion.state & SDL_BUTTON_MMASK);
            QueueMessage((HWND)win, WM_MOUSEMOVE_U32, (WPARAM)flags,
                         (LPARAM)((y << 16) | (x & 0xFFFF)));
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            SDL_Window* win = SDL_GetWindowFromID(ev.button.windowID);
            UINT msg;
            UINT xb = 0;
            switch (ev.button.button) {
            case SDL_BUTTON_LEFT:   msg = ev.button.down ? WM_LBUTTONDOWN_U32 : WM_LBUTTONUP_U32; break;
            case SDL_BUTTON_RIGHT:  msg = ev.button.down ? WM_RBUTTONDOWN_U32 : WM_RBUTTONUP_U32; break;
            case SDL_BUTTON_MIDDLE: msg = ev.button.down ? WM_XBUTTONDOWN_U32 : WM_XBUTTONUP_U32; xb = 1; break;
            case SDL_BUTTON_X1:     msg = ev.button.down ? WM_XBUTTONDOWN_U32 : WM_XBUTTONUP_U32; xb = 1; break;
            case SDL_BUTTON_X2:     msg = ev.button.down ? WM_XBUTTONDOWN_U32 : WM_XBUTTONUP_U32; xb = 2; break;
            default:                msg = 0; break;
            }
            if (msg) {
                int x = (int)ev.button.x, y = (int)ev.button.y;
                UINT flags = MouseWParam(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK,
                                         SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_RMASK,
                                         SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MMASK);
                WPARAM wp = flags | ((WPARAM)xb << 16);
                QueueMessage((HWND)win, msg, wp, (LPARAM)((y << 16) | (x & 0xFFFF)));
            }
            break;
        }

        case SDL_EVENT_MOUSE_WHEEL: {
            SDL_Window* win = SDL_GetWindowFromID(ev.wheel.windowID);
            int delta = (int)(ev.wheel.integer_y * 120);   // Win32 wheel delta
            float x = 0, y = 0;
            SDL_GetMouseState(&x, &y);
            QueueMessage((HWND)win, WM_MOUSEWHEEL_U32,
                         (WPARAM)(((WPARAM)delta << 16) & 0xFFFF0000),
                         (LPARAM)(((int)y << 16) | ((int)x & 0xFFFF)));
            break;
        }

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            D2GameWindow* w = WindowOf((HWND)SDL_GetWindowFromID(ev.window.windowID));
            if (w) {
                w->client_w = ev.window.data1;
                w->client_h = ev.window.data2;
            }
            break;
        }

        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Window class + creation (winuser.h).
// ---------------------------------------------------------------------------
struct D2WindowClass {
    char    name[128];
    void*   wndproc;
    void*   hcursor;
};
static std::vector<D2WindowClass> g_classes;

ATOM __stdcall RegisterClassA(const WNDCLASSA* lpWndClass) {
    if (!lpWndClass || !lpWndClass->lpszClassName) {
        return 0;
    }
    for (const D2WindowClass& c : g_classes) {
        if (strcmp(c.name, lpWndClass->lpszClassName) == 0) {
            return 1;  // already registered — Win32 errors; the game registers once
        }
    }
    D2WindowClass c;
    snprintf(c.name, sizeof(c.name), "%s", lpWndClass->lpszClassName);
    c.wndproc = (void*)lpWndClass->lpfnWndProc;
    c.hcursor = lpWndClass->hCursor;
    g_classes.push_back(c);
    return 1;   // nonzero atom
}

BOOL __stdcall UnregisterClassA(LPCSTR lpClassName, HINSTANCE) {
    if (!lpClassName) return FALSE;
    for (size_t i = 0; i < g_classes.size(); ++i) {
        if (strcmp(g_classes[i].name, lpClassName) == 0) {
            g_classes.erase(g_classes.begin() + i);
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Single instance: FindWindowA is the game's mutex (D2GFX_CreateWindow shows
// the "only one copy" box when it finds an existing "Diablo II" window). The
// shim implements the check as a flock(2) claim: the probe that finds no
// other window ACQUIRES the claim for the process lifetime (the fd closes on
// exit), and a probe from a second process sees the held lock.
// ---------------------------------------------------------------------------
static int g_instanceLockFd = -1;

static const char* InstanceLockPath() {
    static char path[512];
    if (!path[0]) {
        const char* runtime = SDL_getenv("XDG_RUNTIME_DIR");
        snprintf(path, sizeof(path), "%s/d2linux-instance.lock",
                 (runtime && *runtime) ? runtime : "/tmp");
    }
    return path;
}

HWND __stdcall FindWindowA(LPCSTR lpClassName, LPCSTR) {
    if (!lpClassName) {
        return NULL;
    }
    if (g_instanceLockFd >= 0) {
        return NULL;   // this process already claimed the class
    }
    extern int flock(int fd, int operation);
    extern int open(const char*, int, ...);
    extern int close(int);
    g_instanceLockFd = open(InstanceLockPath(), 2 /*O_RDWR*/ | 0x40 /*O_CREAT*/, 0666);
    if (g_instanceLockFd < 0) {
        return NULL;   // no lock file possible — allow the boot (single user)
    }
    if (flock(g_instanceLockFd, 2 /*LOCK_EX*/ | 4 /*LOCK_NB*/) != 0) {
        close(g_instanceLockFd);
        g_instanceLockFd = -1;
        return (HWND)(uintptr_t)0x0000DEAD;   // held by another instance
    }
    return NULL;   // claim acquired; no other "Diablo II" window exists
}

HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                     DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                     HWND, HMENU, HINSTANCE, LPVOID) {
    (void)dwExStyle;
    if (!lpClassName) {
        return NULL;
    }
    const D2WindowClass* cls = NULL;
    for (const D2WindowClass& c : g_classes) {
        if (strcmp(c.name, lpClassName) == 0) {
            cls = &c;
            break;
        }
    }
    if (!cls) {
        return NULL;   // the game always registers first
    }

    // SDL3 window sizes are the CLIENT area — exactly the values the game
    // hands in (its AdjustWindowRectEx math computed an outer frame around
    // them; the frame belongs to the WM here).
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
    if (dwStyle & WS_POPUP_U32) {
        flags |= SDL_WINDOW_BORDERLESS;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    int x = (X == -2147483648 /*CW_USEDEFAULT*/) ? SDL_WINDOWPOS_UNDEFINED : X;
    int y = (Y == -2147483648) ? SDL_WINDOWPOS_UNDEFINED : Y;
    SDL_Window* win = SDL_CreateWindow(lpWindowName ? lpWindowName : "Diablo II",
                                       nWidth, nHeight, flags);
    if (!win) {
        return NULL;
    }
    SDL_SetWindowPosition(win, x, y);

    D2GameWindow* w = new D2GameWindow();
    w->window      = win;
    w->wndproc     = cls->wndproc;
    w->client_w    = nWidth;
    w->client_h    = nHeight;
    w->shown       = false;
    w->track_armed = false;
    snprintf(w->title, sizeof(w->title), "%s", lpWindowName ? lpWindowName : "Diablo II");
    g_windows.push_back(w);

    // The presentation host attaches to THIS window (renderer + streaming
    // texture); the DirectDraw shim's Flip drives it.
    d2linux::Video_Attach(win);
    return (HWND)win;
}

BOOL DestroyWindow(HWND hWnd) {
    D2GameWindow* w = WindowOf(hWnd);
    if (!w) {
        return FALSE;
    }
    for (size_t i = 0; i < g_windows.size(); ++i) {
        if (g_windows[i] == w) {
            g_windows.erase(g_windows.begin() + i);
            break;
        }
    }
    QueueMessage(hWnd, WM_DESTROY_U32, 0, 0);
    d2linux::Video_Detach(w->window);   // only if THIS window hosts the plumbing
    SDL_DestroyWindow(w->window);
    delete w;
    return TRUE;
}

BOOL __stdcall UpdateWindow(HWND hWnd) {
    return WindowOf(hWnd) ? TRUE : FALSE;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow) {
    D2GameWindow* w = WindowOf(hWnd);
    if (!w) return FALSE;
    w->shown = (nCmdShow != 0 /*SW_HIDE*/);
    if (w->shown) {
        SDL_ShowWindow(w->window);
        SDL_RaiseWindow(w->window);
    } else {
        SDL_HideWindow(w->window);
    }
    return TRUE;
}

BOOL SetWindowPos(HWND hWnd, HWND, int X, int Y, int cx, int cy, UINT uFlags) {
    D2GameWindow* w = WindowOf(hWnd);
    if (!w) return FALSE;
    if (!(uFlags & 0x0002 /*SWP_NOMOVE*/)) {
        SDL_SetWindowPosition(w->window, X, Y);
    }
    if (!(uFlags & 0x0001 /*SWP_NOSIZE*/)) {
        SDL_SetWindowSize(w->window, cx, cy);
        w->client_w = cx;
        w->client_h = cy;
    }
    return TRUE;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect) {
    if (!lpRect) return FALSE;
    lpRect->left = lpRect->top = 0;
    D2GameWindow* w = WindowOf(hWnd);
    if (w) {
        SDL_GetWindowSize(w->window, &w->client_w, &w->client_h);
        lpRect->right  = w->client_w;
        lpRect->bottom = w->client_h;
    } else if (hWnd == D2_HWND_DESKTOP) {
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(1);
        lpRect->right  = dm ? dm->w : 800;
        lpRect->bottom = dm ? dm->h : 600;
    } else {
        lpRect->right  = 800;
        lpRect->bottom = 600;
    }
    return TRUE;
}

BOOL __stdcall GetWindowRect(HWND hWnd, LPRECT lpRect) {
    if (!lpRect) return FALSE;
    D2GameWindow* w = WindowOf(hWnd);
    if (!w) {
        lpRect->left = lpRect->top = 0;
        lpRect->right = 800;
        lpRect->bottom = 600;
        return FALSE;
    }
    int x = 0, y = 0;
    SDL_GetWindowPosition(w->window, &x, &y);
    int cw = 0, ch = 0;
    SDL_GetWindowSize(w->window, &cw, &ch);
    lpRect->left   = x;
    lpRect->top    = y;
    lpRect->right  = x + cw;
    lpRect->bottom = y + ch;
    return TRUE;
}

BOOL __stdcall GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT* lpwndpl) {
    if (!lpwndpl) return FALSE;
    D2GameWindow* w = WindowOf(hWnd);
    memset(lpwndpl, 0, sizeof(*lpwndpl));
    lpwndpl->length  = sizeof(*lpwndpl);
    lpwndpl->showCmd = (w && w->shown) ? 1 /*SW_SHOWNORMAL*/ : 0;
    if (w) {
        int x = 0, y = 0;
        SDL_GetWindowPosition(w->window, &x, &y);
        int cw = 0, ch = 0;
        SDL_GetWindowSize(w->window, &cw, &ch);
        lpwndpl->rcNormalPosition.left   = x;
        lpwndpl->rcNormalPosition.top    = y;
        lpwndpl->rcNormalPosition.right  = (long)x + cw;
        lpwndpl->rcNormalPosition.bottom = (long)y + ch;
    }
    return TRUE;
}

BOOL __stdcall AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL, DWORD) {
    // Win10-style insets for the caption/frame. SDL owns the real frame; the
    // client area is what the shim creates, so these deltas only need to keep
    // the game's outer-rect math self-consistent.
    if (!lpRect) return FALSE;
    if (dwStyle & WS_CAPTION_U32) {
        lpRect->top -= 31;
    }
    lpRect->bottom += 8;
    lpRect->left   -= 8;
    lpRect->right  += 8;
    return TRUE;
}

HWND __stdcall GetDesktopWindow(void) {
    return D2_HWND_DESKTOP;
}

int GetSystemMetrics(int nIndex) {
    const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(1);
    if (nIndex == 0 /*SM_CXSCREEN*/) return dm ? dm->w : 0;
    if (nIndex == 1 /*SM_CYSCREEN*/) return dm ? dm->h : 0;
    return 0;
}

BOOL __stdcall SystemParametersInfoA(UINT uiAction, UINT, void* pvParam, UINT) {
    if (uiAction == 0x0030 /*SPI_GETWORKAREA*/ && pvParam) {
        RECT* rc = (RECT*)pvParam;
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(1);
        rc->left = rc->top = 0;
        rc->right  = dm ? dm->w : 800;
        rc->bottom = dm ? dm->h : 600;
        return TRUE;
    }
    return FALSE;
}

LONG __stdcall ChangeDisplaySettingsA(DEVMODEA*, DWORD) {
    // Fire-and-forget at both call sites (the small-screen fallback and the
    // restore-on-close). The SDL window already carries the game's resolution
    // through the letterboxed logical presentation.
    return 0;   // DISP_CHANGE_SUCCESSFUL
}

void* __stdcall MonitorFromWindow(HWND, DWORD) {
    return (void*)(uintptr_t)1;   // the single virtual monitor
}

BOOL __stdcall GetMonitorInfoA(void* hMonitor, MONITORINFO* lpmi) {
    if (!lpmi || hMonitor != (void*)(uintptr_t)1) return FALSE;
    const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(1);
    lpmi->rcMonitor.left = lpmi->rcMonitor.top = 0;
    lpmi->rcMonitor.right  = dm ? dm->w : 800;
    lpmi->rcMonitor.bottom = dm ? dm->h : 600;
    lpmi->rcWork = lpmi->rcMonitor;
    lpmi->dwFlags = 1;   // MONITORINFOF_PRIMARY
    return TRUE;
}

HWND __stdcall SetFocus(HWND hWnd) {
    D2GameWindow* w = WindowOf(hWnd);
    if (w) {
        SDL_RaiseWindow(w->window);
        return hWnd;
    }
    return NULL;
}

BOOL __stdcall IsIconic(HWND hWnd) {
    D2GameWindow* w = WindowOf(hWnd);
    return (w && (SDL_GetWindowFlags(w->window) & SDL_WINDOW_MINIMIZED)) ? TRUE : FALSE;
}

BOOL __stdcall SetWindowTextA(HWND hWnd, LPCSTR lpString) {
    D2GameWindow* w = WindowOf(hWnd);
    if (!w || !lpString) return FALSE;
    SDL_SetWindowTitle(w->window, lpString);
    snprintf(w->title, sizeof(w->title), "%s", lpString);
    return TRUE;
}

BOOL __stdcall IsWindow(HWND hWnd) {
    return WindowOf(hWnd) ? TRUE : FALSE;
}

BOOL __stdcall IsWindowVisible(HWND hWnd) {
    D2GameWindow* w = WindowOf(hWnd);
    return (w && w->shown) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Message pump (winuser.h) — the M1 fail-soft stubs become the real queue.
// ---------------------------------------------------------------------------
BOOL __stdcall PeekMessageA(void* lpMsg, HWND, UINT, UINT, UINT wRemoveMsg) {
    if (!lpMsg) return FALSE;
    PumpSDL();
    if (g_queue.empty() && g_quitPosted) {
        D2WinMsg* m = (D2WinMsg*)lpMsg;
        m->hwnd    = NULL;
        m->message = WM_QUIT_U32;
        m->wParam  = (WPARAM)g_quitCode;
        m->lParam  = 0;
        m->time    = SDL_GetTicks();
        m->pt.x = m->pt.y = 0;
        if (wRemoveMsg & 0x0001 /*PM_REMOVE*/) {
            g_quitPosted = false;
        }
        return TRUE;
    }
    if (g_queue.empty()) {
        return FALSE;
    }
    D2WinMsg* m = (D2WinMsg*)lpMsg;
    if (wRemoveMsg & 0x0001) {
        *m = g_queue.front();
        g_queue.pop_front();
    } else {
        *m = g_queue.front();   // PM_NOREMOVE: peek, do not pop
    }
    return TRUE;
}

BOOL __stdcall GetMessageA(void* lpMsg, HWND, UINT, UINT) {
    if (!lpMsg) return -1;
    D2WinMsg* m = (D2WinMsg*)lpMsg;
    for (;;) {
        PumpSDL();
        if (!g_queue.empty()) {
            *m = g_queue.front();
            g_queue.pop_front();
            return 1;
        }
        if (g_quitPosted) {
            g_quitPosted = false;
            m->hwnd    = NULL;
            m->message = WM_QUIT_U32;
            m->wParam  = (WPARAM)g_quitCode;
            m->lParam  = 0;
            m->time    = SDL_GetTicks();
            m->pt.x = m->pt.y = 0;
            return 0;   // the loop-exit verdict the game's pump tests
        }
        SDL_Delay(1);   // Win32 blocks here; the game only reaches this after
                        // a PM_NOREMOVE probe, so the wait is one queue beat
    }
}

BOOL __stdcall TranslateMessage(const void*) {
    // WM_CHAR is synthesized directly from SDL text-input events — the
    // scancode→VK→char chain Win32 does here has no work left.
    return FALSE;
}

LRESULT __stdcall DispatchMessageA(const void* lpMsg) {
    if (!lpMsg) return 0;
    return DispatchToWindow(*(const D2WinMsg*)lpMsg);
}

void PostQuitMessage(int nExitCode) {
    g_quitPosted = true;
    g_quitCode   = nExitCode;
}

BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    QueueMessage(hWnd, Msg, wParam, lParam);
    return TRUE;
}

LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    // Win32 delivers synchronously — a direct proc call, not a queue hop.
    D2WinMsg m;
    m.hwnd = hWnd;
    m.message = Msg;
    m.wParam = wParam;
    m.lParam = lParam;
    m.time = SDL_GetTicks();
    m.pt.x = m.pt.y = 0;
    return DispatchToWindow(m);
}

LRESULT __stdcall DefWindowProcA(HWND, UINT, WPARAM, LPARAM) {
    return 0;
}

BOOL __stdcall TrackMouseEvent(const TRACKMOUSEEVENT* lpEventTrack) {
    if (!lpEventTrack || !(lpEventTrack->dwFlags & 0x0002 /*TME_LEAVE*/)) {
        return TRUE;
    }
    D2GameWindow* w = WindowOf(lpEventTrack->hwndTrack);
    if (w) {
        w->track_armed = true;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Cursor (winuser.h). ShowCursor's DISPLAY COUNT is load-bearing: the game's
// HideOSCursor does `while (ShowCursor(FALSE) > -1);` — the return value must
// be the post-change count exactly as Win32 spells it.
// ---------------------------------------------------------------------------
int __stdcall ShowCursor(BOOL bShow) {
    g_cursorCount += bShow ? 1 : -1;
    if (g_cursorCount >= 0) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
    return g_cursorCount;
}

void* __stdcall LoadCursorA(HINSTANCE, LPCSTR) {
    return (void*)(uintptr_t)0x0000C0DE;   // opaque non-NULL HCURSOR
}

void* __stdcall SetCursor(void*) {
    return NULL;   // the game's cursor lives in the frame, not the OS
}

BOOL __stdcall SetCursorPos(int X, int Y) {
    D2GameWindow* w = g_windows.empty() ? NULL : g_windows.front();
    if (w) {
        SDL_WarpMouseInWindow(w->window, (float)X, (float)Y);
    }
    return TRUE;
}

BOOL __stdcall GetCursorPos(struct POINT* lpPoint) {
    if (!lpPoint) return FALSE;
    float x = 0, y = 0;
    SDL_GetGlobalMouseState(&x, &y);
    lpPoint->x = (long)x;
    lpPoint->y = (long)y;
    return TRUE;
}

BOOL __stdcall ClipCursor(const RECT*) {
    // No named M2 call site; the grab toggle is the honest analogue.
    D2GameWindow* w = g_windows.empty() ? NULL : g_windows.front();
    if (w) {
        bool grabbed = (SDL_GetWindowFlags(w->window) & SDL_WINDOW_MOUSE_GRABBED) != 0;
        SDL_SetWindowMouseGrab(w->window, !grabbed);
    }
    return TRUE;
}

SHORT GetKeyState(int nVirtKey) {
    // 0x8000 = held. Toggle bits (CapsLock) unused by the slice.
    if (nVirtKey < 0 || nVirtKey >= 256) return 0;
    return g_keyDown[nVirtKey] ? (SHORT)0x8000 : (SHORT)0;
}

SHORT __stdcall GetAsyncKeyState(int nVirtKey) {   // api.h's spelling (the dump-key probe)
    return GetKeyState(nVirtKey);
}

int MessageBoxA(HWND, LPCSTR lpText, LPCSTR lpCaption, UINT) {
    // The PE is modal; the linux twin surfaces an SDL dialog when a window
    // exists and always logs the boot-log-style stderr line.
    fprintf(stderr, "d2linux: %s: %s\n", lpCaption ? lpCaption : "message",
            lpText ? lpText : "");
    SDL_Window* w = g_windows.empty() ? NULL : g_windows.front()->window;
    if (w) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                 lpCaption ? lpCaption : "Diablo II",
                                 lpText ? lpText : "", w);
    }
    return 1;   // IDOK — the Insert-Disc retry callbacks depend on it
}

HDC GetDC(HWND) {
    return NULL;   // no GDI DCs; the DDraw surface IS the drawable
}

int ReleaseDC(HWND, HDC) {
    return 1;
}

// ---------------------------------------------------------------------------
// GDI (wingdi.h) — D2DDraw's debug-font rasterizer only. A NULL HDC makes the
// glyph mask fill blank (GetPixel never returns the text color), which is the
// honest "no debug overlay text" answer — verified null-safe against
// D2ddDebugFont_Alloc.
// ---------------------------------------------------------------------------
HDC __stdcall CreateDCA(LPCSTR, LPCSTR, LPCSTR, const void*) { return NULL; }
HFONT __stdcall CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD,
                  DWORD, DWORD, DWORD, DWORD, LPCSTR) { return NULL; }
HBITMAP __stdcall CreateBitmap(int, int, UINT, UINT, const void*) { return NULL; }
HGDIOBJ __stdcall SelectObject(HDC, HGDIOBJ h) { return h; }
COLORREF __stdcall SetTextColor(HDC, COLORREF c) { return c; }
COLORREF __stdcall SetBkColor(HDC, COLORREF c) { return c; }
int __stdcall SetBkMode(HDC, int m) { return m; }
int __stdcall DrawTextA(HDC, LPCSTR, int, LPRECT, UINT) { return 0; }
COLORREF __stdcall GetPixel(HDC, int, int) { return 0; }   // never the text color
BOOL __stdcall GetCharWidthA(HDC, UINT iFirst, UINT iLast, int* lpBuffer) {
    if (!lpBuffer) return FALSE;
    for (UINT i = iFirst; i <= iLast; ++i) lpBuffer[i - iFirst] = 6;
    return TRUE;
}
BOOL __stdcall DeleteObject(HGDIOBJ) { return TRUE; }
BOOL __stdcall DeleteDC(HDC) { return TRUE; }
HGDIOBJ __stdcall GetStockObject(int i) {
    // NULL_BRUSH (5) is the hbrBackground the game's class registration wants.
    return (HGDIOBJ)(uintptr_t)(i == 5 ? 0x0000000A : 0);
}
int __stdcall GdiSetBatchLimit(DWORD) { return 1; }
HICON __stdcall LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT) {
    return NULL;   // no Win32 resources in an ELF; the class icon is optional
}

// ---------------------------------------------------------------------------
// Clipboard (winuser.h) — D2Win/Src/D2WinEditBox.cpp's copy/paste. One slot;
// GetClipboardData hands out a fresh GlobalAlloc copy per call (the Win32
// contract: the caller must not free it), the previous copy is retired on the
// next clipboard operation.
// ---------------------------------------------------------------------------
struct D2Moveable {
    size_t size;
    void*  data;    // the GlobalLock view (fixed storage: moveable in name)
};

HGLOBAL __stdcall GlobalAlloc(UINT, SIZE_T dwBytes) {
    if (!dwBytes) return NULL;
    D2Moveable* m = new D2Moveable();
    m->size = dwBytes;
    m->data = malloc(dwBytes);
    if (!m->data) {
        delete m;
        return NULL;
    }
    memset(m->data, 0, dwBytes);
    return (HGLOBAL)m;
}

LPVOID __stdcall GlobalLock(HGLOBAL hMem) {
    if (!hMem) return NULL;
    return ((D2Moveable*)hMem)->data;
}

BOOL __stdcall GlobalUnlock(HGLOBAL) {
    return FALSE;   // fixed storage: never still-locked
}

HGLOBAL __stdcall GlobalFree(HGLOBAL hMem) {
    if (!hMem) return NULL;
    D2Moveable* m = (D2Moveable*)hMem;
    free(m->data);
    delete m;
    return NULL;
}

BOOL __stdcall OpenClipboard(HWND) {
    free(g_clipText);
    g_clipText = NULL;
    if (g_clipHandle) {
        GlobalFree(g_clipHandle);
        g_clipHandle = NULL;
    }
    return TRUE;
}

BOOL __stdcall CloseClipboard(void) {
    return TRUE;
}

BOOL __stdcall EmptyClipboard(void) {
    SDL_SetClipboardText("");
    free(g_clipText);
    g_clipText = NULL;
    if (g_clipHandle) {
        GlobalFree(g_clipHandle);
        g_clipHandle = NULL;
    }
    return TRUE;
}

HANDLE __stdcall SetClipboardData(UINT uFormat, HANDLE hMem) {
    if (uFormat != 1 /*CF_TEXT*/ || !hMem) return NULL;
    D2Moveable* m = (D2Moveable*)hMem;
    SDL_SetClipboardText(m->data ? (const char*)m->data : "");
    g_clipHandle = hMem;   // the clipboard owns the handle now (Win32 rules)
    return hMem;
}

HANDLE __stdcall GetClipboardData(UINT uFormat) {
    if (uFormat != 1 /*CF_TEXT*/) return NULL;
    if (g_clipHandle) {
        GlobalFree(g_clipHandle);
        g_clipHandle = NULL;
    }
    free(g_clipText);
    g_clipText = NULL;
    const char* text = SDL_GetClipboardText();
    if (!text || !*text) {
        SDL_free((void*)text);
        return NULL;
    }
    size_t n = strlen(text) + 1;
    g_clipHandle = GlobalAlloc(0, n);
    if (g_clipHandle) {
        memcpy(((D2Moveable*)g_clipHandle)->data, text, n);
        g_clipText = strdup(text);   // SDL text must outlive the SDL_free below
    }
    SDL_free((void*)text);
    return g_clipHandle;
}

// ---------------------------------------------------------------------------
// RECT helpers (winuser.h) — the control/layout math.
// ---------------------------------------------------------------------------
BOOL __stdcall PtInRect(const RECT* lprc, struct POINT pt) {
    if (!lprc) return FALSE;
    return (pt.x >= lprc->left && pt.x < lprc->right &&
            pt.y >= lprc->top && pt.y < lprc->bottom) ? TRUE : FALSE;
}

BOOL __stdcall CopyRect(LPRECT dst, const RECT* src) {
    if (!dst || !src) return FALSE;
    *dst = *src;
    return TRUE;
}

BOOL __stdcall OffsetRect(LPRECT lprc, int dx, int dy) {
    if (!lprc) return FALSE;
    lprc->left += dx; lprc->right += dx;
    lprc->top += dy;  lprc->bottom += dy;
    return TRUE;
}

BOOL __stdcall InflateRect(LPRECT lprc, int dx, int dy) {
    if (!lprc) return FALSE;
    lprc->left -= dx; lprc->right += dx;
    lprc->top -= dy;  lprc->bottom += dy;
    return TRUE;
}

BOOL __stdcall IntersectRect(LPRECT dst, const RECT* a, const RECT* b) {
    if (!dst || !a || !b) return FALSE;
    dst->left   = a->left > b->left ? a->left : b->left;
    dst->top    = a->top > b->top ? a->top : b->top;
    dst->right  = a->right < b->right ? a->right : b->right;
    dst->bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (dst->left >= dst->right || dst->top >= dst->bottom) {
        memset(dst, 0, sizeof(*dst));
        return FALSE;
    }
    return TRUE;
}

BOOL __stdcall UnionRect(LPRECT dst, const RECT* a, const RECT* b) {
    if (!dst || !a || !b) return FALSE;
    dst->left   = a->left < b->left ? a->left : b->left;
    dst->top    = a->top < b->top ? a->top : b->top;
    dst->right  = a->right > b->right ? a->right : b->right;
    dst->bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
    return TRUE;
}

BOOL __stdcall GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize) {
    // D2SoundUntil's legacy-VxD probe appends "\SYSTEM\<driver>.VXD" and
    // version-checks it — a nonexistent file is the expected answer here, so
    // the classic directory shape is enough.
    const char winDir[] = "C:\\WINDOWS";
    if (!lpBuffer || uSize < sizeof(winDir)) return 0;
    strcpy(lpBuffer, winDir);
    return (int)(sizeof(winDir) - 1);
}

// [d2linux M2] the shim self-test lives in d2_linux_main.cpp's --smoke path
// (RunUser32SelfTest) — one harness, the full contract, CI-visible.

}  // extern "C"
