// glide3xvk — 0.4.5 controller support: gamepad-to-mouse/keys at the
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
//   controller_sens=<float>    left-stick speed multiplier (0/missing = 1.0)
//   controller_aimsens=<float> right-stick fine-aim multiplier (0/missing=0.35)
//   controller_deadzone=<%>    radial deadzone 1..80 (0/missing = 15%)
//   controller_moveclick=      present "0" = left stick cursor-only; absent or
//                              any other value = stick-walk (the default)
//   controller_home=           present "0" = free cursor; absent or any other
//                              value = cursor-home (the default, see below)
// Hot-reloaded from the conf by the poll thread (~2s mtime probe) — same
// file-is-truth semantics as the window presets and the slang chain.
//
// Default map v2 (field spec — "left analog moves me, triggers click, d-pad
// belts, and I want to hit things NEAR me without pixel-perfect cursor
// work"). In D2 movement IS the mouse: walking = cursor + held LMB, so the
// left stick steers the cursor and auto-holds LMB while deflected; the
// near-target problem is answered by D2's own mechanics, injected:
//   Left stick   walk (cursor + held LMB)   A    hold Shift+LMB — attack in
//   Right stick  fine aim (~35% speed)           place toward the cursor
//   LT           left click (hold)          B    hold Alt (item labels)
//   RT           right click (hold)         Y    tap I (inventory)
//   D-pad        tap 1/2/3/4 (belt)         X    tap Space (close screens)
//   LB           tap C (character)          RB   tap T (skill tree)
//   L3           tap R (run/walk)           Start tap Esc (menu)
//                                          Back  tap Tab (automap)
// Attacking/interacting near you: flick the RIGHT stick onto the target and
// tap LT (interact / click), or hold A to swing in place toward the cursor
// (melee/ranged both work — the click never moves you), or just brush the
// walking cursor across a monster (held LMB attacks on contact). Alt-held
// looting clicks the much larger label text instead of the item sprite.
//
// Cursor-home (field round 2 — "when no analog input, put the cursor on the
// player and hide it, then when the analog moves, it's from player position
// and when analog stops, it's back to player position"). The wrapper cannot
// see the player, and it does not need to: D2 walk-to-click always ends with
// the character standing AT the cursor, so the cursor position at every
// stick-walk release IS the player's screen position (self-correcting — a
// walk that ends early on an obstacle just re-syncs on the next one). The
// executor keeps that position as the HOME anchor: after a short quiet dwell
// (sticks centered, nothing injected) the cursor is SetCursorPos'd back onto
// the anchor; stick motion then always emanates from the player. Effects:
// the cursor can never get lost at the screen edge, attacking/interacting
// things NEAR you is always one small stick flick away, and the model reads
// exactly like a console cursor. Honest limits, documented in the README:
// the OS cursor is parked, not hidden (hiding D2's own cursor sprite would
// mean intercepting the game's WM_SETCURSOR — Game.exe territory, out of
// scope), and a click that WALKS the character somewhere (LT on a far
// portal) leaves the anchor there until the next stick-walk re-syncs it.
// Requires stick-walk (moveclick) — without walk-click the anchor has
// nothing to self-correct from, so home is skipped when moveclick is off.
//
// Injection is gated on the game window being foreground; losing focus
// releases everything held (re-press after refocus — no surprise clicks).
//
// The pure core below (conf fixups, bind table, stick response, desired-
// state step) is host-tested (scripts/ctrltest.cpp) and stays Win32/XInput-
// free: the platform side feeds it a G3xCtrlPadState and executes the
// returned G3xCtrlEvents. Button mask values mirror XINPUT_GAMEPAD so the
// platform side can copy the raw wButtons word straight in; the analog
// triggers are thresholded to LT/RT pseudo-bits inside the core.
//
// Event model (v2): every tick the core builds the DESIRED injected state
// (which mouse buttons / keys should be down) and emits the minimal diff
// against what it already injected. Sources OR together, so the stick-walk
// LMB and the A combo's LMB can never double-press or steal each other's
// release — e.g. releasing A while still walking only lifts Shift, not LMB.
#pragma once

#include <stddef.h>
#include <math.h>      // sqrtf (stick magnitude)

// XINPUT_GAMEPAD button masks (duplicated so the core compiles on the host
// without any Windows header) + LT/RT pseudo-bits for the thresholded
// analog triggers (outside the WORD the pad reports).
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
    G3XCTRL_BTN_LT         = 0x00010000,   // pseudo: bLeftTrigger  > threshold
    G3XCTRL_BTN_RT         = 0x00020000,   // pseudo: bRightTrigger > threshold
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

// One injected input inside a bind: a key press or a mouse button press.
typedef struct G3xCtrlStep_ {
    unsigned char kind;    // G3XCTRL_EV_*_DOWN flavor
    unsigned char vk;      // virtual key (mouse kinds ignore)
} G3xCtrlStep;

enum { G3XCTRL_MAX_STEPS = 3 };

// A bind maps one pad control to 1..3 injected inputs (a combo). Hold
// semantics contribute while the control is down; tap semantics contribute
// ONLY on the press-edge tick (down this tick, up the next) — one clean
// keystroke, no auto-repeat: D2 menu screens toggle per key-down, so a held
// 'I' would flip the inventory closed again on the first auto-repeat
// (~500ms); an 8ms tap never reaches it. Combos press in steps[] order
// (modifier first) and release in reverse (mouse up before its modifier).
struct G3xCtrlBind {
    unsigned     mask;     // G3XCTRL_BTN_* (incl. LT/RT pseudo-bits)
    unsigned char nsteps;  // 1..G3XCTRL_MAX_STEPS
    G3xCtrlStep  steps[G3XCTRL_MAX_STEPS];
    unsigned char tap;     // 1 = edge-tick only, 0 = hold
};

// Virtual keys used by the default map (WIN32 VK_* values, spelled literally
// so the host tests can assert against them without windows.h).
enum {
    G3XCTRL_VK_TAB    = 0x09,
    G3XCTRL_VK_SHIFT  = 0x10,   // A combo modifier: attack in place
    G3XCTRL_VK_MENU   = 0x12,   // B: item labels
    G3XCTRL_VK_ESCAPE = 0x1B,   // Start: menu
    G3XCTRL_VK_SPACE  = 0x20,   // X: close all screens
};

// Analog trigger digital threshold (XINPUT_GAMEPAD_TRIGGER_THRESHOLD).
enum { G3XCTRL_TRIGGER_THRESHOLD = 30 };

// The default map v2. R3 is reserved (unbound). LT/RT are first so the
// plain clicks exist before the combos that also reference them.
inline const G3xCtrlBind* g3x_ctrl_bind_table(unsigned* count) {
    static const G3xCtrlBind kTable[] = {
        { G3XCTRL_BTN_LT,        1, {{ G3XCTRL_EV_MOUSE_L_DOWN, 0 }},           0 },
        { G3XCTRL_BTN_RT,        1, {{ G3XCTRL_EV_MOUSE_R_DOWN, 0 }},           0 },
        { G3XCTRL_BTN_A,         2, {{ G3XCTRL_EV_KEY_DOWN, G3XCTRL_VK_SHIFT },
                                     { G3XCTRL_EV_MOUSE_L_DOWN, 0 }},           0 },
        { G3XCTRL_BTN_B,         1, {{ G3XCTRL_EV_KEY_DOWN, G3XCTRL_VK_MENU }}, 0 },
        { G3XCTRL_BTN_X,         1, {{ G3XCTRL_EV_KEY_DOWN, G3XCTRL_VK_SPACE }},1 },
        { G3XCTRL_BTN_Y,         1, {{ G3XCTRL_EV_KEY_DOWN, 'I' }},             1 },
        { G3XCTRL_BTN_DPAD_UP,   1, {{ G3XCTRL_EV_KEY_DOWN, '1' }},             1 },
        { G3XCTRL_BTN_DPAD_DOWN, 1, {{ G3XCTRL_EV_KEY_DOWN, '2' }},             1 },
        { G3XCTRL_BTN_DPAD_LEFT, 1, {{ G3XCTRL_EV_KEY_DOWN, '3' }},             1 },
        { G3XCTRL_BTN_DPAD_RIGHT,1, {{ G3XCTRL_EV_KEY_DOWN, '4' }},             1 },
        { G3XCTRL_BTN_LB,        1, {{ G3XCTRL_EV_KEY_DOWN, 'C' }},             1 },
        { G3XCTRL_BTN_RB,        1, {{ G3XCTRL_EV_KEY_DOWN, 'T' }},             1 },
        { G3XCTRL_BTN_L3,        1, {{ G3XCTRL_EV_KEY_DOWN, 'R' }},             1 },
        { G3XCTRL_BTN_START,     1, {{ G3XCTRL_EV_KEY_DOWN, G3XCTRL_VK_ESCAPE }},1 },
        { G3XCTRL_BTN_BACK,      1, {{ G3XCTRL_EV_KEY_DOWN, G3XCTRL_VK_TAB }},  1 },
    };
    if (count) *count = (unsigned)(sizeof(kTable) / sizeof(kTable[0]));
    return kTable;
}

// Parallel-array capacity inside the core state (bind table must fit).
enum { G3XCTRL_MAX_BINDS = 16 };

// Conf fixups: the conf parser zero-fills the struct, so 0 = "module
// default" (the same convention window= uses for native).
// controller_moveclick: the parser writes 1 for any present non-zero value
// and -1 for a present "0"; the zero-fill sentinel (key absent) means the
// default — fixup maps >=0 -> on, <0 -> off.
struct G3xCtrlConf {
    int   enabled;       // conf controller=      (0/1)
    float sens;          // conf controller_sens= (<=0 -> 1.0, clamp 16)
    float aimsens;       // conf controller_aimsens= (<=0 -> 0.35, clamp 0.05..4)
    int   deadzone_pct;  // conf controller_deadzone= (<=0 -> 15, clamp 1..80)
    int   moveclick;     // normalized by fixup: 1 = stick-walk, 0 = cursor-only
    int   home;          // normalized by fixup: 1 = cursor-home, 0 = free cursor
};

inline void g3x_ctrl_conf_fixup(G3xCtrlConf* c) {
    if (!c) return;
    if (c->sens <= 0.0f) c->sens = 1.0f;
    if (c->sens > 16.0f) c->sens = 16.0f;
    if (c->aimsens <= 0.0f) c->aimsens = 0.35f;
    if (c->aimsens < 0.05f) c->aimsens = 0.05f;
    if (c->aimsens > 4.0f)  c->aimsens = 4.0f;
    if (c->deadzone_pct <= 0) c->deadzone_pct = 15;
    if (c->deadzone_pct > 80) c->deadzone_pct = 80;
    c->moveclick = (c->moveclick < 0) ? 0 : 1;
    c->home      = (c->home      < 0) ? 0 : 1;
    if (c->enabled != 1) c->enabled = 0;
}

// Stick response: radial deadzone + squared response curve (precision near
// center, full speed at the rim). Returns the response 0..1; nx/ny receive
// the unit direction in PAD convention (y up = +; the executor flips y for
// Windows, where +dy is down). Used for BOTH sticks (the right stick is the
// same curve at aimsens speed) and for the stick-walk engagement decision.
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
    short          rx, ry;      // right stick (fine aim; executor-side only)
    unsigned short buttons;     // raw XINPUT_GAMEPAD wButtons
    unsigned char  lt, rt;      // triggers 0..255
};

// One injected event.
struct G3xCtrlEvent {
    unsigned char kind;         // G3XCTRL_EV_*
    unsigned char vk;           // key kinds only
};

// Persistent desired-vs-injected state (value-init to zero = fresh pad).
// cur_* mirror what was actually injected (the diff target); armed[] latch
// press edges so release_all (focus loss) never re-injects a control the
// user is still holding — a fresh physical press re-arms it.
struct G3xCtrlCore {
    unsigned      prev_buttons;   // combined word incl. LT/RT pseudo-bits
    unsigned char prev_move;      // left stick active last tick (arming edge)
    unsigned char move_armed;     // stick-walk needs a fresh deflection edge
    unsigned char cur_lmb, cur_rmb;
    unsigned char cur_vk[256];    // injected key state, indexed by VK
    unsigned char armed[G3XCTRL_MAX_BINDS];
};

// Advance the core by one pad snapshot, emitting the minimal set of
// press/release events that brings the injected state in line with the
// desired state. conf may be NULL (stick-walk skipped). Returns the number
// of events written (<= cap; overflow drops silently — the executor passes
// a cap >= 32, the worst-case mass-press + simultaneous tap-release tick).
inline int g3x_ctrl_step(G3xCtrlCore* st, const G3xCtrlPadState* now,
                         const G3xCtrlConf* conf,
                         G3xCtrlEvent* ev, unsigned cap) {
    if (!st || !now) return 0;
    unsigned nbinds = 0;
    const G3xCtrlBind* binds = g3x_ctrl_bind_table(&nbinds);
    int n = 0;
    const auto emit = [&](unsigned char kind, unsigned char vk) {
        if ((unsigned)n < cap && ev) { ev[n].kind = kind; ev[n].vk = vk; }
        ++n;   // counted even when dropped, so the caller can log overflow
    };

    // combined button word: analog triggers thresholded to pseudo-bits
    unsigned btns = now->buttons;
    if (now->lt > G3XCTRL_TRIGGER_THRESHOLD) btns |= G3XCTRL_BTN_LT;
    if (now->rt > G3XCTRL_TRIGGER_THRESHOLD) btns |= G3XCTRL_BTN_RT;

    // ---- desired state this tick (sources OR together) --------------------
    bool          want_l = false, want_r = false;
    unsigned char want_vk[256] = {};

    // stick-walk: deflection beyond the deadzone holds LMB. Armed on a fresh
    // center->deflected edge so a release_all (focus loss) can't be undone
    // by a stick that never moved — re-flick to resume, same rule as buttons.
    bool move_active = false;
    if (conf && conf->moveclick) {
        float nx, ny;
        move_active = g3x_ctrl_stick(now->sx, now->sy,
                                     conf->deadzone_pct, &nx, &ny) > 0.0f;
        if (move_active && !st->prev_move) st->move_armed = 1;
    }
    if (move_active && st->move_armed) want_l = true;

    for (unsigned i = 0; i < nbinds && i < G3XCTRL_MAX_BINDS; ++i) {
        const bool down = (btns & binds[i].mask) != 0;
        const bool was  = (st->prev_buttons & binds[i].mask) != 0;
        if (down && !was) st->armed[i] = 1;
        const bool active = binds[i].tap ? (down && !was)      // edge tick only
                                         : (down && st->armed[i] != 0);
        if (!active) continue;
        for (int s = 0; s < binds[i].nsteps && s < G3XCTRL_MAX_STEPS; ++s) {
            const G3xCtrlStep& sp = binds[i].steps[s];
            if (sp.kind == G3XCTRL_EV_MOUSE_L_DOWN)      want_l = true;
            else if (sp.kind == G3XCTRL_EV_MOUSE_R_DOWN) want_r = true;
            else if (sp.kind == G3XCTRL_EV_KEY_DOWN)     want_vk[sp.vk] = 1;
        }
    }

    // ---- diff: releases first (mouse before keys = reverse press order) ---
    if (st->cur_rmb && !want_r) { emit(G3XCTRL_EV_MOUSE_R_UP, 0); st->cur_rmb = 0; }
    if (st->cur_lmb && !want_l) { emit(G3XCTRL_EV_MOUSE_L_UP, 0); st->cur_lmb = 0; }
    for (int vk = 255; vk >= 0; --vk)
        if (st->cur_vk[vk] && !want_vk[vk]) {
            emit(G3XCTRL_EV_KEY_UP, (unsigned char)vk);
            st->cur_vk[vk] = 0;
        }

    // ---- then presses (keys ascending, then mouse: Shift lands before LMB)
    for (int vk = 0; vk < 256; ++vk)
        if (want_vk[vk] && !st->cur_vk[vk]) {
            emit(G3XCTRL_EV_KEY_DOWN, (unsigned char)vk);
            st->cur_vk[vk] = 1;
        }
    if (want_r && !st->cur_rmb) { emit(G3XCTRL_EV_MOUSE_R_DOWN, 0); st->cur_rmb = 1; }
    if (want_l && !st->cur_lmb) { emit(G3XCTRL_EV_MOUSE_L_DOWN, 0); st->cur_lmb = 1; }

    st->prev_buttons = btns;
    st->prev_move    = move_active ? 1 : 0;
    return n > (int)cap ? (int)cap : n;
}

// Release everything the core believes is injected (focus loss, disable,
// shutdown) and disarm: a control still physically held stays silent until
// a fresh press edge, so focus never returns with surprise clicks. Safe to
// call repeatedly; zeroes the diff state.
inline int g3x_ctrl_release_all(G3xCtrlCore* st, G3xCtrlEvent* ev, unsigned cap) {
    if (!st) return 0;
    int n = 0;
    const auto emit = [&](unsigned char kind, unsigned char vk) {
        if ((unsigned)n < cap && ev) { ev[n].kind = kind; ev[n].vk = vk; }
        ++n;
    };
    if (st->cur_rmb) { emit(G3XCTRL_EV_MOUSE_R_UP, 0); st->cur_rmb = 0; }
    if (st->cur_lmb) { emit(G3XCTRL_EV_MOUSE_L_UP, 0); st->cur_lmb = 0; }
    for (int vk = 255; vk >= 0; --vk)
        if (st->cur_vk[vk]) {
            emit(G3XCTRL_EV_KEY_UP, (unsigned char)vk);
            st->cur_vk[vk] = 0;
        }
    for (unsigned i = 0; i < G3XCTRL_MAX_BINDS; ++i) st->armed[i] = 0;
    st->move_armed = 0;
    // prev_buttons / prev_move intentionally KEPT: a control still held must
    // not look like a fresh press edge on the next step (no ghost injection).
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
