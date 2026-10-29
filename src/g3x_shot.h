// glide3xvk — 0.4.6: the PrintScreen screenshot key.
//
// One press saves WHAT THE SCREEN SHOWS as a PNG into screenshots\ beside the
// game exe: the PRESENTED raster — slang-FILTERED pixels on the preset path
// (a GPU readback of the rendered swapchain image, taken between the chain
// render and the present), the compose raster on the legacy path — overlay
// boxes included, at swapchain/window resolution. WYSIWYG. For clean shots
// (no tag/panel/toast) toggle the overlay off with Scroll Lock first: the
// boxes hidden = nothing drawn into the frame the shot copies. The gamma-
// corrected game raster is always in there (the chain input is built from it
// on the CPU side; the legacy path IS it).
//
// Seam and rules (mirroring the Scroll Lock toggle):
//   - polled from the present seam (g3x_frame_present) on every presented
//     frame, with or without a preset chain, and deliberately NOT parked
//     while the overlay is toggled off (a hidden overlay then simply makes
//     every shot clean),
//   - foreground-gated: a press made while another window is focused is
//     latched off, never queued (GetAsyncKeyState is process-global),
//   - edge-latched: one press = one shot, held PrintScreen does not machine-gun,
//   - conf-gated via [slangvk] screenshots= (1 default, 0 disables the key;
//     g3x_slang pushes the parsed flag here on every conf read),
//   - the encode + write run synchronously on the game thread (~a few ms;
//     the GPU readback itself is a sub-millisecond fenced copy) — a
//     deliberate trade: no detached-thread lifetime questions at process
//     exit, at most one frame of hitch per shot.
//
// The PNG encoder is dependency-free by house rule (this DLL imports nothing
// beyond winmm's counterparts and version): 8-bit RGB, filter type 0 per
// row, zlib stream made of STORED deflate blocks (no compression — a
// window-res shot is ~1-6 MB; determinism and tiny code beat ratio here),
// table-driven CRC-32 and the standard adler-32.
#pragma once

#include <cstdint>

// Push the parsed [slangvk] screenshots= flag (g3x_slang's reread_conf).
// Default state is enabled — a conf without the key keeps the hotkey live.
void g3x_shot_set_enabled(bool on);
bool g3x_shot_enabled();

// Edge-poll PrintScreen from the present seam. Sets the internal pending
// flag on a fresh focused press; a press while disabled logs once and is
// dropped. Consumed by g3x_shot_take_pending() at one of the two capture
// points below.
void g3x_shot_poll();

// Returns and clears the pending flag. The caller submits the frame copy
// through g3x_shot_submit() at the point it has the right buffer in hand.
bool g3x_shot_take_pending();

// Encode + write one BGRA8 frame (w*h*4 bytes, stride w*4 — the compose
// layout). Fills name_out with the saved file's BASE name ("D2_....png",
// toast-sized) and returns true on success; failures log and return false.
bool g3x_shot_submit(const uint8_t* bgra, unsigned w, unsigned h,
                     char* name_out, unsigned name_cap);
