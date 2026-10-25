// glide3xvk — 0.4.4 controller support: gamepad-to-mouse/keys at the
// wrapper layer.
//
// D2 has no gamepad input path (input is Game.exe territory — SCOPE.md), so
// the wrapper cannot add one "into" the game. What it CAN do without
// touching Game.exe: poll an XInput pad on a background thread and INJECT
// ordinary mouse/keyboard input (SendInput). The game sees a normal mouse
// and keyboard — every D2 screen works (menus, chat, inventory), no
// game-side changes, no driver installs, no extra DLL imports (XInput is
// loaded dynamically; absent = the feature logs and stays off).
//
// d2slangvk.conf [slangvk] — config-only, no hotkeys (the F10 lesson):
//   controller=0|1             master switch, OFF by default (opt-in)
//   controller_sens=<float>    cursor speed multiplier (0/missing = 1.0)
//   controller_deadzone=<%>    radial deadzone 1..80 (0/missing = 15%)
// Hot-reloaded from the conf by the poll thread (~2s mtime probe) — same
// file-is-truth semantics as the window presets and the slang chain.
//
// Default map (fixed in v1; documented in README):
//   Left stick   mouse cursor          A    left click (hold)
//   Right stick  (reserved, unused)    B    right click (hold)
//   X            hold Alt (items)      Y    tap I (inventory)
//   D-pad        tap 1/2/3/4 (belt)    LB   tap C (character)
//   RT           hold Shift (attack    RB   tap T (skill tree)
//                in place)
//   LT           tap Ctrl (run/walk)   Start tap Esc (menu)
//                                      Back tap Tab (automap)
//
// Injection is gated on the game window being foreground; losing focus
// releases everything held (re-press after refocus).
//
// The pure core below (conf fixups, bind table, stick response, edge/hold/
// tap step) is host-tested (scripts/ctrltest.cpp) and stays Win32/XInput-
// free: the platform side feeds it a G3xCtrlPadState and executes the
// returned G3xCtrlEvents. Button mask values mirror XINPUT_GAMEPAD so the
// platform side can copy the raw wButtons word straight in.
#pragma once

#include <stddef.h>
#include <math.h>      // sqrtf (stick magnitude)

// XINPUT_GAMEPAD button masks (duplicated so the core compiles on the host
// without any Windows header).
enum {
    G3XCTRL_BTN_DPAD_UP    = 0x0001,
    G3XCTRL_BTN_DPAD_DOWN  = 0x0002,
    G3XCTRL_BTN_DPAD_LEFT  = 0x0004,
    G3XCTRL_BTN_DPAD_RIGHT = 0x0008,
    G3XCTRL_BTN_START      = 0x0010,
    G3XCTRL_BTN_BACK       = 0x0020,
    G3XCTRL_BTN_L3         = 0x0040,
    G3XCTRL_BTN_R3         = 0x0080,
    G3XCTRL_BTN_LB         = 0x0100,
    G3XCTRL_BTN_RB         = 0x0200,
    G3XCTRL_BTN_A          = 0x1000,
    G3XCTRL_BTN_B          = 0x2000,
    G3XCTRL_BTN_X          = 0x4000,
    G3XCTRL_BTN_Y          = 0x8000,
};

// Events the executor understands (one SendInput each).
enum {
    G3XCTRL_EV_NONE = 0,
    G3XCTRL_EV_KEY_DOWN,        // vk
    G3XCTRL_EV_KEY_UP,          // vk
    G3XCTRL_EV_MOUSE_L_DOWN,
    G3XCTRL_EV_MOUSE_L_UP,
    G3XCTRL_EV_MOUSE_R_DOWN,
    G3XCTRL_EV_MOUSE_R_UP,
};

// Hold vs tap:
//   hold = stays down while the pad control is held (mouse buttons, Alt item
//          labels, Shift attack-in-place — all polled per-frame by D2)
//   tap  = down on the press edge, up on the NEXT step (~8ms later) — one
//          clean keystroke, no auto-repeat. D2 menu screens toggle per
//          key-down, so a held 'I' would flip the inventory closed again on
//          the first auto-repeat (~500ms); an 8ms tap never reaches it.
struct G3xCtrlBind {
    unsigned short mask;   // G3XCTRL_BTN_* mask
    unsigned char  kind;   // G3XCTRL_EV_*_DOWN flavor emitted on press
    unsigned char  vk;     // virtual key (mouse kinds ignore)
    unsigned char  tap;    // 1 = tap (release next step), 0 = hold
};

// Virtual keys used by the default map (WIN32 VK_* values, spelled literally
// so the host tests can assert against them without windows.h).
enum {
    G3XCTRL_VK_TAB    = 0x09,
    G3XCTRL_VK_ESCAPE = 0x1B,
    G3XCTRL_VK_CTRL   = 0x11,   // LT action: run/walk toggle (tap)
    G3XCTRL_VK_SHIFT  = 0x10,   // RT action: attack in place (hold)
    G3XCTRL_VK_MENU   = 0x12,   // X action: item labels (hold)
};

// Analog trigger digital threshold (XINPUT_GAMEPAD_TRIGGER_THRESHOLD).
enum { G3XCTRL_TRIGGER_THRESHOLD = 30 };

// The default map. L3/R3 and the right stick are reserved (unbound).
inline const G3xCtrlBind* g3x_ctrl_bind_table(unsigned* count) {
    static const G3xCtrlBind kTable[] = {
        { G3XCTRL_BTN_A,          G3XCTRL_EV_MOUSE_L_DOWN, 0,                0 },
        { G3XCTRL_BTN_B,          G3XCTRL_EV_MOUSE_R_DOWN, 0,                0 },
        { G3XCTRL_BTN_X,          G3XCTRL_EV_KEY_DOWN,     G3XCTRL_VK_MENU,  0 },
        { G3XCTRL_BTN_Y,          G3XCTRL_EV_KEY_DOWN,     'I',              1 },
        { G3XCTRL_BTN_DPAD_UP,    G3XCTRL_EV_KEY_DOWN,     '1',              1 },
        { G3XCTRL_BTN_DPAD_DOWN,  G3XCTRL_EV_KEY_DOWN,     '2',              1 },
        { G3XCTRL_BTN_DPAD_LEFT,  G3XCTRL_EV_KEY_DOWN,     '3',              1 },
        { G3XCTRL_BTN_DPAD_RIGHT, G3XCTRL_EV_KEY_DOWN,     '4',              1 },
        { G3XCTRL_BTN_LB,         G3XCTRL_EV_KEY_DOWN,     'C',              1 },
        { G3XCTRL_BTN_RB,         G3XCTRL_EV_KEY_DOWN,     'T',              1 },
        { G3XCTRL_BTN_START,      G3XCTRL_EV_KEY_DOWN,     G3XCTRL_VK_ESCAPE,1 },
        { G3XCTRL_BTN_BACK,       G3XCTRL_EV_KEY_DOWN,     G3XCTRL_VK_TAB,   1 },
    };
    if (count) *count = (unsigned)(sizeof(kTable) / sizeof(kTable[0]));
    return kTable;
}

// Parallel-array capacity inside the core state (bind table must fit).
enum { G3XCTRL_MAX_BINDS = 16 };

// Conf fixups: the conf parser zero-fills the struct, so 0 = "module
// default" (the same convention window= uses for native).
struct G3xCtrlConf {
    int   enabled;       // conf controller=      (0/1)
    float sens;          // conf controller_sens= (<=0 -> 1.0, clamp 16)
    int   deadzone_pct;  // conf controller_deadzone= (<=0 -> 15, clamp 1..80)
};

inline void g3x_ctrl_conf_fixup(G3xCtrlConf* c) {
    if (!c) return;
    if (c->sens <= 0.0f) c->sens = 1.0f;
    if (c->sens > 16.0f) c->sens = 16.0f;
    if (c->deadzone_pct <= 0) c->deadzone_pct = 15;
    if (c->deadzone_pct > 80) c->deadzone_pct = 80;
    if (c->enabled != 1) c->enabled = 0;
}

// Stick response: radial deadzone + squared response curve (precision near
// center, full speed at the rim). Returns the response 0..1; nx/ny receive
// the unit direction in PAD convention (y up = +; the executor flips y for
// Windows, where +dy is down).
inline float g3x_ctrl_stick(int sx, int sy, int deadzone_pct,
                            float* nx, float* ny) {
    if (nx) *nx = 0.0f;
    if (ny) *ny = 0.0f;
    if (deadzone_pct < 1) deadzone_pct = 1;
    if (deadzone_pct > 80) deadzone_pct = 80;
    const float dz = (float)deadzone_pct * (32767.0f / 100.0f);
    const float mag = sqrtf((float)sx * (float)sx + (float)sy * (float)sy);
    if (mag <= dz || mag <= 0.0f) return 0.0f;
    float t = (mag - dz) / (32767.0f - dz);
    if (t > 1.0f) t = 1.0f;
    const float resp = t * t;                     // precision curve
    const float inv = 1.0f / mag;
    if (nx) *nx = (float)sx * inv;
    if (ny) *ny = (float)sy * inv;
    return resp;
}

// One pad snapshot in PAD convention (XInput's own axes and trigger ranges).
struct G3xCtrlPadState {
    short          sx, sy;      // left stick, -32768..32767 (y up = +)
    unsigned short buttons;     // G3XCTRL_BTN_* mask
    unsigned char  lt, rt;      // triggers 0..255
};

// One injected event.
struct G3xCtrlEvent {
    unsigned char kind;         // G3XCTRL_EV_*
    unsigned char vk;           // key kinds only
};

// Persistent edge/hold state (value-init to zero = fresh pad).
struct G3xCtrlCore {
    unsigned short prev_buttons;
    unsigned char  prev_lt, prev_rt;     // digital trigger edges
    unsigned char  lt_tap_pending;       // LT is a tap (run/walk toggle)
    unsigned char  rt_hold_down;         // RT is a hold (attack in place)
    unsigned char  tap_pending[G3XCTRL_MAX_BINDS];
    unsigned char  hold_down[G3XCTRL_MAX_BINDS];
};

// Advance the core by one pad snapshot, emitting press/release events.
// Returns the number of events written (<= cap; overflow drops silently —
// the executor passes a cap >= the worst case, taps+holds of one tick).
// Triggers: LT = tap CTRL, RT = hold SHIFT (both at the digital threshold).
inline int g3x_ctrl_step(G3xCtrlCore* st, const G3xCtrlPadState* now,
                         G3xCtrlEvent* ev, unsigned cap) {
    if (!st || !now) return 0;
    unsigned nbinds = 0;
    const G3xCtrlBind* binds = g3x_ctrl_bind_table(&nbinds);
    int n = 0;
    const auto emit = [&](unsigned char kind, unsigned char vk) {
        if ((unsigned)n < cap && ev) { ev[n].kind = kind; ev[n].vk = vk; }
        ++n;   // counted even when dropped, so the caller can log overflow
    };

    // tap releases from the PREVIOUS step first (one clean keystroke; an
    // 8ms down-window cannot reach D2's ~500ms auto-repeat)
    if (st->lt_tap_pending) { emit(G3XCTRL_EV_KEY_UP, G3XCTRL_VK_CTRL); st->lt_tap_pending = 0; }
    for (unsigned i = 0; i < nbinds && i < G3XCTRL_MAX_BINDS; ++i) {
        if (st->tap_pending[i]) {
            emit(G3XCTRL_EV_KEY_UP, binds[i].vk);
            st->tap_pending[i] = 0;
        }
    }

    // trigger edges
    const bool lt = now->lt > G3XCTRL_TRIGGER_THRESHOLD;
    if (lt && !st->prev_lt) { emit(G3XCTRL_EV_KEY_DOWN, G3XCTRL_VK_CTRL); st->lt_tap_pending = 1; }
    st->prev_lt = lt ? 1 : 0;

    const bool rt = now->rt > G3XCTRL_TRIGGER_THRESHOLD;
    if (rt && !st->prev_rt) { emit(G3XCTRL_EV_KEY_DOWN, G3XCTRL_VK_SHIFT); st->rt_hold_down = 1; }
    else if (!rt && st->prev_rt && st->rt_hold_down) {
        emit(G3XCTRL_EV_KEY_UP, G3XCTRL_VK_SHIFT); st->rt_hold_down = 0;
    }
    st->prev_rt = rt ? 1 : 0;

    // button edges
    for (unsigned i = 0; i < nbinds && i < G3XCTRL_MAX_BINDS; ++i) {
        const bool down = (now->buttons & binds[i].mask) != 0;
        const bool was  = (st->prev_buttons & binds[i].mask) != 0;
        if (down && !was) {
            emit(binds[i].kind, binds[i].vk);
            if (binds[i].tap) st->tap_pending[i] = 1;
            else              st->hold_down[i]  = 1;
        } else if (!down && was && st->hold_down[i]) {
            emit((unsigned char)(binds[i].kind + 1), binds[i].vk);   // *_DOWN + 1 = *_UP
            st->hold_down[i] = 0;
        }
    }
    st->prev_buttons = now->buttons;
    return n > (int)cap ? (int)cap : n;
}

// Release everything the core believes is down/pending (focus loss,
// disable, shutdown). Zeroes the tracking flags; the next physical
// re-press produces a fresh press edge.
inline int g3x_ctrl_release_all(G3xCtrlCore* st, G3xCtrlEvent* ev, unsigned cap) {
    if (!st) return 0;
    unsigned nbinds = 0;
    const G3xCtrlBind* binds = g3x_ctrl_bind_table(&nbinds);
    int n = 0;
    const auto emit = [&](unsigned char kind, unsigned char vk) {
        if ((unsigned)n < cap && ev) { ev[n].kind = kind; ev[n].vk = vk; }
        ++n;
    };
    if (st->lt_tap_pending) { emit(G3XCTRL_EV_KEY_UP, G3XCTRL_VK_CTRL); st->lt_tap_pending = 0; }
    if (st->rt_hold_down)   { emit(G3XCTRL_EV_KEY_UP, G3XCTRL_VK_SHIFT); st->rt_hold_down = 0; }
    for (unsigned i = 0; i < nbinds && i < G3XCTRL_MAX_BINDS; ++i) {
        if (st->tap_pending[i]) { emit(G3XCTRL_EV_KEY_UP, binds[i].vk); st->tap_pending[i] = 0; }
        if (st->hold_down[i])   { emit((unsigned char)(binds[i].kind + 1), binds[i].vk); st->hold_down[i] = 0; }
    }
    return n > (int)cap ? (int)cap : n;
}

// ---- lifecycle (called from glide_exports) --------------------------------

// grSstWinOpen: (re)bind the game window and (re)read the conf; starts the
// poll thread on first use. Idempotent across mode changes. Never fails the
// caller — a missing pad or XInput DLL just means no injection.
void g3x_controller_init(void* hwnd);

// grSstWinClose + DLL_PROCESS_DETACH: release anything held, stop the
// poll thread (bounded wait). Safe to call twice.
void g3x_controller_shutdown();
