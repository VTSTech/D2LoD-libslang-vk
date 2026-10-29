// glide3xvk — P2.10/P2.11: on-screen version tag + active-shader line.
//
// Line 1 — "glide3xvk 0.4.3 (181780c)" — drawn into the present buffer every
// frame, so a field screenshot always proves which build produced it (rounds
// 6-11 repeatedly opened with "which dll was that?" ambiguity). Since the
// 0.4.5 addendum the BOX draws this line with a live " FPS nnn" field appended
// ("glide3xvk 0.4.5 (181780c) FPS 025") — a per-second presented-frame count
// fed from the present seam, so the in-game /fps readout is no longer needed
// for a quick sanity check. The FPS field lives ONLY in the box: the string
// returned by g3x_overlay_tagline() (the grGetString(GR_RENDERER) text and
// the identity other modules copy) stays the plain static tag, so external
// queries keep seeing a stable, comparable renderer name.
//
// Line 2 — the active slang preset's file name, since 0.4.2 — so a field
// screenshot also proves WHICH shader produced the look. g3x_slang pushes the
// name when a chain goes ready (including hot reload) and clears it when the
// chain dies; with no preset the line is absent and the box shrinks to the
// tag alone (the clean P2 baseline look).
//
// Host-version line — since the 0.4.5 addendum the box also names the exe
// it is running inside ("Game.exe Retail 1.14d", "d2.exe 1.14d",
// "Game.exe v1.0.13.55", "d2.exe [CRC 3A5D2B1F]"), detected once per process
// by g3x_hostver at the first grSstWinOpen. Absent until detection reports
// something.
//
// Bottom line — since 0.4.5 the box always ends with the repo credit,
// "GitHub: VTSTech/D2LoD-libslang-vk" — the one overlay line that never
// clears and never expires, so every screenshot the box stamps also carries
// the project's home. The hotkey toast (below) now shows without the shader
// line too — a conf-panel toast with no preset loaded used to be dropped —
// and the Shift+V conf panel draws before this box, so the stamp wins any
// overlap on tiny present targets.
//
// Placement: the box is the PRIMARY version stamp. An earlier approach
// (0.4.1-0.4.3) repointed the game's own "Video:" debug line at our tag via a
// PE patch (g3x_drivertag) — removed in 0.4.5: field builds showed the stock
// "Video: Glide" line either way, and the box draws the same info at the
// swapchain resolution with none of the patching. [glide3xvk] overlay=1 in
// the ini forces the box on, overlay=0 hides it.
//
// Render seam: g3x_frame_present() calls this AFTER the game frame is scaled +
// gamma-mapped into the swapchain-sized compose buffer and BEFORE the queue
// submit. Consequences (all deliberate):
//   - the tag is post-gamma: pure white/black regardless of the game's
//     guGammaCorrectionRGB setting (menu 1.55 / game 1.0),
//   - it sits at swapchain resolution — visible identically in the 640x480
//     probe phases, the 800x600 menu and the windowed game,
//   - it also covers the LFB/Bink video path (same present seam),
//   - it can never leak into the game's own framebuffer: grLfbLock READ
//     (GetBackBuffer) reads g3x_frame_pixels(), not this buffer, so game-side
//     fade/readback logic never sees the tag (no double-draw, no fading).
//
// Rendering is deliberately dependency-free (this TU also builds in the fxtest
// host harness): an embedded 8x8 1-bit bitmap font (g3x_font8x8.h, generated
// from DejaVu Sans Mono Bold), a 65%-black backdrop box and a 1px drop shadow.
// No windows.h — the ini on/off switch is read by glide_exports (grSstWinOpen)
// and pushed in via g3x_overlay_set_enabled, and the Scroll Lock toggle (0.4.6)
// is polled on the present seam (g3x_frame) and pushed in via
// g3x_overlay_toggle.
#pragma once

#include <cstdint>

// Release version of this glide3xvk build. Shown by the overlay, printed by
// the session banner and reported through grGetString(GR_RENDERER). Bump on
// every field-delivered change set.
#define G3XVK_VERSION "0.4.6"

// Render the tag into a BGRA8 buffer (w*h*4 bytes, stride = w*4). Safe to call
// with any buffer size; the box anchors 4px from the right/top edge and clamps
// to the buffer edges. No-op when disabled.
void g3x_overlay_render(uint8_t* bgra, unsigned w, unsigned h);

// The composed tag line — "glide3xvk <ver> (<build id>)". Static storage,
// valid for the process lifetime. The overlay box draws it; it is also the
// grGetString(GR_RENDERER) text, so the two stamps can never disagree.
// (The BOX's copy of this line carries the " FPS nnn" suffix — see below —
// but THIS string deliberately does not: renderer queries get the stable
// identity, not a number that changes every second.)
const char* g3x_overlay_tagline();

// ---- 0.4.5 addendum: the tag line's live FPS field ---------------------------
//
// One call per PRESENTED frame, from g3x_frame_present() — both present
// paths funnel through it (grBufferSwap for gameplay, grLfbUnlock for Bink
// video, and the video loop never calls grBufferSwap), so the count matches
// the game's own frame pace: what /fps would have shown, without /fps.
//
// now_ms is a MONOTONIC millisecond clock owned by the caller (GetTickCount64
// on Windows). The meter is deliberately pure — no clock calls inside — so
// the window math is host-testable with synthetic timestamps. It keeps a
// 1-second window: frames are counted until >= 1000 ms have elapsed since
// the window opened, then the displayed value becomes the rounded rate
// (frames * 1000 / elapsed, half-rounded) and a new window starts. The box
// shows " FPS 000" until the first window completes; the value clamps at
// 999 for display. The tag line is recomposed ONLY when the value actually
// changes — one snprintf per second of play, not per frame. A non-monotonic
// now_ms (caller's clock glitch) restarts the window harmlessly.
void g3x_overlay_fps_tick(uint64_t now_ms);

// The value currently displayed in the tag line's FPS field (0 until the
// first 1-second window completes). Test seam — the box composes the text
// from the same state, so asserting on this and on the rendered pixels
// together pins the whole path.
unsigned g3x_overlay_fps();

// The overlay's second line: the active slang preset's file name (basename
// only, no path). NULL or empty clears the line and shrinks the box back to
// the tag alone. Called by g3x_slang when a chain goes ready / dies; copies
// internally, caller's storage may be transient.
void g3x_overlay_set_shader(const char* name);

// 0.4.3: transient third line for hotkey feedback — "aspect 4:3",
// "shader base 2x" — drawn inside the box below the shader line for ~120
// presented frames (~2s at 60fps, ~4s at D2's typical 30), then the box
// shrinks back. NULL/empty clears immediately. Copies internally.
void g3x_overlay_toast(const char* text);

// 0.4.5 addendum: the running game exe + detected classic-D2 patch —
// "Game.exe Retail 1.14d", "d2.exe 1.14d", "Game.exe v1.0.13.55",
// "d2.exe [CRC 3A5D2B1F]" — composed
// by g3x_hostver and pushed once per process from grSstWinOpen. Drawn
// directly below the tag line (the two identity lines sit together; the
// shader line and toast move down one step). NULL/empty hides the line and
// shrinks the box. Copies internally; whatever fits (63 chars) displays.
void g3x_overlay_set_hostver(const char* text);

// On/off switch. glide_exports wires [glide3xvk] overlay=0/1 to this; with no
// ini key the value is auto — on (the box is the version stamp since the
// drivertag patch was removed). Hides the conf panel too (overlay=0 means no
// overlay boxes at all). Re-pushed on every context open so a relaunch picks
// up ini edits without touching the DLL.
void g3x_overlay_set_enabled(bool on);
bool g3x_overlay_enabled();

// 0.4.6: the Scroll Lock toggle — flip the overlay on/off at runtime and
// return the NEW state. A session override, not a re-push of the ini value:
// D2 re-opens Glide contexts constantly (the 640x480 probes, menu, game), and
// an ini re-push on each open would pop the boxes back mid-session, so the
// toggle latches until process end while the ini still decides the state at
// startup and after a relaunch (file wins at load, key wins in flight — the
// same split as the F11/F12 hotkeys vs d2slangvk.conf). Hides and restores
// EVERY overlay box: the tag, the shader/hostver/FPS lines, the toast and the
// Shift+V conf panel (its visible flag is untouched — a panel up before the
// toggle is up again after toggling back). While hidden nothing draws, so the
// vanishing/reappearing box is the toggle's own feedback; g3x_frame toasts
// "overlay on" on the show press and logs both directions.
bool g3x_overlay_toggle();

// ---- 0.4.5: the Shift+V conf panel -----------------------------------------
//
// A toggleable box on the LEFT edge (the tag box owns the top-right, the
// game's /fps line the very top-left — the panel starts one line below it)
// listing the current d2slangvk.conf options as resolved. Content is owned
// by g3x_slang (it has the parser, the live F11/F12 state and the conf
// path): it formats the lines and pushes them here; this module only stores,
// toggles and draws. Same look as the tag box — 65%-black backdrop, 8x8 font
// with 1px drop shadow — so the two boxes read as one system.
//
// Capacity limits, shared with the formatter (it sizes its buffers to these;
// the setter silently clamps anything past them).
enum {
    G3XVK_CONF_PANEL_MAX_LINES = 18,
    G3XVK_CONF_PANEL_LINE_CAP  = 72
};

// Replace the panel's line set (copied; each line clamped to LINE_CAP, the
// list to MAX_LINES). NULL/0 clears the content (panel then draws nothing
// even while visible).
void g3x_overlay_conf_panel_set(const char* const* lines, unsigned count);

// Show/hide. Toggle is an explicit user action (Shift+V); the panel stays up
// across conf reloads and window (re)opens until toggled off.
void g3x_overlay_conf_panel_toggle();
bool g3x_overlay_conf_panel_visible();

// 0.4.5: the arrow-key selection cursor. line indexes the CURRENT panel
// lines as pushed by g3x_overlay_conf_panel_set (0 = the first line); -1 =
// no highlight. The selected line draws as a dim-green row band with the
// glyphs in bright green — a menu cursor against the white text. Up/Down
// handling lives in g3x_slang's conf-panel poll (it knows which lines are
// options); this module only stores and paints. Out-of-range values draw
// nothing, so an index that goes stale between a refresh and the next push
// can never scribble outside the box.
void g3x_overlay_conf_panel_set_selected(int line);
int  g3x_overlay_conf_panel_selected();
