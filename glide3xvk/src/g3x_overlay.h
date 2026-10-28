// glide3xvk — P2.10/P2.11: on-screen version tag + active-shader line.
//
// Line 1 — "glide3xvk 0.4.3 (181780c)" — drawn into the present buffer every
// frame, so a field screenshot always proves which build produced it (rounds
// 6-11 repeatedly opened with "which dll was that?" ambiguity).
//
// Line 2 — the active slang preset's file name, since 0.4.2 — so a field
// screenshot also proves WHICH shader produced the look. g3x_slang pushes the
// name when a chain goes ready (including hot reload) and clears it when the
// chain dies; with no preset the line is absent and the box shrinks to the
// tag alone (the clean P2 baseline look).
//
// Host-version line — since the 0.4.5 addendum the box also names the exe
// it is running inside ("d2.exe 1.14d", "Game.exe v1.0.13.55",
// "d2.exe [PE 3A5D2B1F]"), detected once per process by g3x_hostver at the
// first grSstWinOpen. Absent until detection reports something.
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
// and pushed in via g3x_overlay_set_enabled.
#pragma once

#include <cstdint>

// Release version of this glide3xvk build. Shown by the overlay, printed by
// the session banner and reported through grGetString(GR_RENDERER). Bump on
// every field-delivered change set.
#define G3XVK_VERSION "0.4.5"

// Render the tag into a BGRA8 buffer (w*h*4 bytes, stride = w*4). Safe to call
// with any buffer size; the box anchors 4px from the right/top edge and clamps
// to the buffer edges. No-op when disabled.
void g3x_overlay_render(uint8_t* bgra, unsigned w, unsigned h);

// The composed tag line — "glide3xvk <ver> (<build id>)". Static storage,
// valid for the process lifetime. The overlay box draws it; it is also the
// grGetString(GR_RENDERER) text, so the two stamps can never disagree.
const char* g3x_overlay_tagline();

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
// "d2.exe 1.14d", "Game.exe v1.0.13.55", "d2.exe [PE 3A5D2B1F]" — composed
// by g3x_hostver and pushed once per process from grSstWinOpen. Drawn
// directly below the tag line (the two identity lines sit together; the
// shader line and toast move down one step). NULL/empty hides the line and
// shrinks the box. Copies internally; whatever fits (63 chars) displays.
void g3x_overlay_set_hostver(const char* text);

// On/off switch. glide_exports wires [glide3xvk] overlay=0/1 to this; with no
// ini key the value is auto — on (the box is the version stamp since the
// drivertag patch was removed). Hides the conf panel too (overlay=0 means no
// overlay boxes at all).
void g3x_overlay_set_enabled(bool on);
bool g3x_overlay_enabled();

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
