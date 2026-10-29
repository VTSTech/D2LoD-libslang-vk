// glide3xvk — 0.4.5 present-window presets + DPI awareness.
//
// L4 (SCOPE.md) locks the GAME's render resolution — the game creates the
// window and the back buffer, and D2 renders 800x600, full stop. What L4
// does NOT lock is the window that present scales into: the wrapper receives
// the game's HWND and the swapchain follows caps.currentExtent (the
// "windowed scale — window WxH, game renders 800x600" log line). This module
// owns that third layer: it sizes the OS window to a preset and keeps the
// game from ever noticing. Window presets = output sizing only; making the
// game RENDER at other resolutions is the mode-2 custom-res project (SCOPE
// section 4, P6).
//
// d2slangvk.conf [slangvk] — config-only, no hotkey (F10 is the game's own
// skill-slot bind; the wrapper must not steal it):
//   window=native|240p|480p|576p|720p|768p|1080p|desktop|WxH
//   borderless=0|1        ("desktop" is borderless regardless)
// Applied at every renderer (re)open and re-read from the conf every 32nd
// frame — edit the file and the window follows (same file-is-truth semantics
// as the slang chain's hot reload).
//
// DPI: D2 is a DPI-unaware process, so SetWindowPos from inside it would be
// DPI-VIRTUALIZED (125% scaling turns a requested 1920x1080 into a
// 2400x1350 physical window — the 1536x864 = 1080p/1.25 field receipt). The
// apply runs inside SetThreadDpiAwarenessContext(PER_MONITOR_AWARE_V2),
// loaded dynamically so the DLL still starts on pre-1607 Windows (those get
// the old best-effort virtualized behavior).
//
// Fail-soft: with no window= line this module does nothing (the game's own
// window, exactly as before). Any apply failure logs and leaves the window
// as the game made it.
//
// The pure preset core below (table + parse) is host-tested
// (scripts/wintest.cpp); the header deliberately stays window-free, so the
// core lives here as inline functions. The Win32 apply side is g3x_window.cpp.
#pragma once

#include <stddef.h>

// One conf preset. w/h = 0 marks a SYMBOLIC size resolved at apply time
// (native = the game's own client size; desktop = the current monitor).
// kind: 0 = native, 1 = fixed size, 2 = desktop (monitor-sized, borderless).
struct G3xWindowPreset {
    const char* name;
    unsigned    w, h;
    int         kind;
};

// Table order is documentation only (no cycle anymore). 576p is 720x576
// (PAL 4:3 DAR); 768p is 1366x768 (the common WXGA panel). Content aspect
// inside the window is the chain's business (scale=/aspect=/F11) and the
// legacy path's fit+center.
inline const G3xWindowPreset* g3x_window_preset_table(unsigned* count) {
    static const G3xWindowPreset kTable[] = {
        { "native",     0,    0, 0 },
        { "240p",     320,  240, 1 },
        { "480p",     640,  480, 1 },
        { "576p",     720,  576, 1 },
        { "720p",    1280,  720, 1 },
        { "768p",    1366,  768, 1 },
        { "1080p",   1920, 1080, 1 },
        { "desktop",   0,    0, 2 },
    };
    if (count) *count = (unsigned)(sizeof(kTable) / sizeof(kTable[0]));
    return kTable;
}

inline bool g3x_window_ci_eq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += ('a' - 'A');
        if (cb >= 'A' && cb <= 'Z') cb += ('a' - 'A');
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == *b;
}

// Index of `name` in the table, or -1. Case-insensitive. "" and NULL are not
// table names ("" means native by value — see g3x_window_preset_size).
inline int g3x_window_preset_index(const char* name) {
    if (!name || !name[0]) return -1;
    unsigned count = 0;
    const G3xWindowPreset* t = g3x_window_preset_table(&count);
    for (unsigned i = 0; i < count; ++i)
        if (g3x_window_ci_eq(name, t[i].name)) return (int)i;
    return -1;
}

// Resolve a preset token to a client size + kind. Accepts every table name,
// a decimal "WxH" form (kind 1, both sides 1..8192), and "" (== native).
// NULL or unknown tokens return false with the outputs zeroed. Symbolic
// presets return w/h = 0 (resolved at apply time).
inline bool g3x_window_preset_size(const char* name, unsigned* w, unsigned* h, int* kind) {
    if (w) *w = 0;
    if (h) *h = 0;
    if (kind) *kind = 0;
    if (!name) return false;
    if (!name[0]) return true;                        // "" == native
    const int i = g3x_window_preset_index(name);
    if (i >= 0) {
        const G3xWindowPreset* t = g3x_window_preset_table(nullptr);
        if (w) *w = t[i].w;
        if (h) *h = t[i].h;
        if (kind) *kind = t[i].kind;
        return true;
    }
    // decimal WxH
    const char* x = name;
    while (*x && *x != 'x' && *x != 'X') ++x;
    if (!*x || x == name || !x[1]) return false;
    unsigned lw = 0, lh = 0;
    for (const char* p = name; p < x; ++p) {
        if (*p < '0' || *p > '9') return false;
        lw = lw * 10u + (unsigned)(*p - '0');
    }
    for (const char* p = x + 1; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        lh = lh * 10u + (unsigned)(*p - '0');
    }
    if (!lw || !lh || lw > 8192u || lh > 8192u) return false;
    if (w) *w = lw;
    if (h) *h = lh;
    if (kind) *kind = 1;
    return true;
}

// ---- lifecycle (called from glide_exports) --------------------------------
//
// void* hwnd keeps this header window-free (the host tests include it).

// grSstWinOpen: (re)bind the game window + its render size ("native"), and
// re-apply the active preset if one is set (the game may have re-made or
// re-sized its window on a mode change).
void g3x_window_init(void* hwnd, unsigned game_w, unsigned game_h);

// grBufferSwap, every frame: a conf mtime check every 32nd frame so an edit
// to window=/borderless= follows without a restart. Cheap no-op otherwise.
void g3x_window_frame_tick();
