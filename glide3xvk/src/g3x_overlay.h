// glide3xvk — P2.10/P2.11: on-screen version tag + active-shader line.
//
// Line 1 — "glide3xvk 0.4.2 (181780c)" — drawn into the present buffer every
// frame, so a field screenshot always proves which build produced it (rounds
// 6-11 repeatedly opened with "which dll was that?" ambiguity).
//
// Line 2 — the active slang preset's file name, since 0.4.2 — so a field
// screenshot also proves WHICH shader produced the look. g3x_slang pushes the
// name when a chain goes ready (including hot reload) and clears it when the
// chain dies; with no preset the line is absent and the box shrinks to the
// tag alone (the clean P2 baseline look).
//
// Placement: since 0.4.1 the box is the FALLBACK, not the primary stamp. The
// primary is the game's own "Video:" debug line, which g3x_drivertag repoints
// at our tag string (see g3x_drivertag.cpp) — same info, the game's font, at
// the game's position (500,2), and no collision with anything. glide_exports
// turns the box off when that patch lands and keeps it on when it doesn't;
// [glide3xvk] overlay=1 in the ini forces the box back on, overlay=0 hides it.
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
#define G3XVK_VERSION "0.4.2"

// Render the tag into a BGRA8 buffer (w*h*4 bytes, stride = w*4). Safe to call
// with any buffer size; the box anchors 4px from the right/top edge and clamps
// to the buffer edges. No-op when disabled.
void g3x_overlay_render(uint8_t* bgra, unsigned w, unsigned h);

// The composed tag line — "glide3xvk <ver> (<build id>)". Static storage,
// valid for the process lifetime. g3x_drivertag hands the same text to the
// game's "Video:" line, so the two stamps can never disagree.
const char* g3x_overlay_tagline();

// The overlay's second line: the active slang preset's file name (basename
// only, no path). NULL or empty clears the line and shrinks the box back to
// the tag alone. Called by g3x_slang when a chain goes ready / dies; copies
// internally, caller's storage may be transient.
void g3x_overlay_set_shader(const char* name);

// On/off switch. glide_exports wires [glide3xvk] overlay=0/1 to this; with no
// ini key the value is auto — off when the drivertag patch owns the game's
// "Video:" line, on as a fallback when it doesn't.
void g3x_overlay_set_enabled(bool on);
bool g3x_overlay_enabled();
