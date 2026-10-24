// glide3xvk — P2.10/P2.11: on-screen version tag.
//
// A single line — "glide3xvk 0.3.1 (77cb059-dirty)" — drawn into the present
// buffer every frame, so a field screenshot always proves which build produced
// it (rounds 6-11 repeatedly opened with "which dll was that?" ambiguity).
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
#define G3XVK_VERSION "0.4.1"

// Render the tag into a BGRA8 buffer (w*h*4 bytes, stride = w*4). Safe to call
// with any buffer size; the box anchors 4px from the right/top edge and clamps
// to the buffer edges. No-op when disabled.
void g3x_overlay_render(uint8_t* bgra, unsigned w, unsigned h);

// The composed tag line — "glide3xvk <ver> (<build id>)". Static storage,
// valid for the process lifetime. g3x_drivertag hands the same text to the
// game's "Video:" line, so the two stamps can never disagree.
const char* g3x_overlay_tagline();

// On/off switch. glide_exports wires [glide3xvk] overlay=0/1 to this; with no
// ini key the value is auto — off when the drivertag patch owns the game's
// "Video:" line, on as a fallback when it doesn't.
void g3x_overlay_set_enabled(bool on);
bool g3x_overlay_enabled();
