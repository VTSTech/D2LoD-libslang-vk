// glide3xvk — 0.4.6: the PrintScreen screenshot key.
//
// One press saves the CURRENT game frame as a PNG into screenshots\ beside
// the game exe. The frame captured is the gamma-corrected game raster at
// NATIVE game resolution (s_w x s_h in g3x_frame terms), taken BEFORE the
// overlay draws — a saved PNG never contains the version tag, the conf panel
// or the toast, with or without the Scroll Lock toggle. On the P3 preset
// path this is the chain's input frame (the chain itself renders straight
// into the GPU swapchain image, which a CPU-side capture cannot reach — a
// slang-FILTERED capture needs a Vulkan readback pass and stays future work;
// see "What is deliberately NOT here yet" in README.md). On the legacy path
// the same native-res copy is made, so the output shape is identical
// wherever a preset chain runs.
//
// Seam and rules (mirroring the 0.4.6 Scroll Lock toggle):
//   - polled from the present seam (g3x_frame_present) on every presented
//     frame — works with or without a preset chain, and is deliberately NOT
//     parked while the overlay is Scroll-Lock-toggled off (clean shots are
//     the point; the toast simply cannot draw while the boxes are hidden,
//     so the log line is the only feedback there),
//   - foreground-gated: a press made while another window is focused is
//     latched off, never queued (GetAsyncKeyState is process-global),
//   - edge-latched: one press = one shot, held PrintScreen does not machine-gun,
//   - conf-gated via [slangvk] screenshots= (1 default, 0 disables the key;
//     g3x_slang pushes the parsed flag here on every conf read),
//   - the encode + write run synchronously on the game thread (~a few ms for
//     the 800x600 RGB payload; a deliberate trade: no detached-thread
//     lifetime questions at process exit, one frame of hitch per shot at
//     most) — the pixels are COPIED first, so the raster reuses its buffer
//     the very next frame with zero tearing risk.
//
// The PNG encoder is dependency-free by house rule (this DLL imports nothing
// beyond winmm's counterparts and version): 8-bit RGB, filter type 0 per
// row, zlib stream made of STORED deflate blocks (no compression — a
// 800x600 shot is ~1.4 MB; determinism and tiny code beat ratio here),
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
