// glide3xvk — libslang-vk preset chain at the present pass (P3).
//
// Owns everything slang: the adopted Vulkan context, the d2slangvk.conf
// settings, the game-resolution input image, the loaded preset and the render
// chain. The present seam (g3x_frame_present) feeds the gamma-corrected
// NATIVE-RESOLUTION game frame here; this module uploads it through
// slangvk_image and renders the preset chain straight into the acquired
// swapchain image (slangvk_chain_render_swapchain), so the preset's own
// scaling algorithms see the true source size — 640x480 in, 1440p out is the
// P3 exit shape (PLAN.md section 5).
//
// Fail-soft everywhere: any slang failure downgrades that frame (or the
// session) to the legacy unfiltered present path; the game never notices
// beyond the look. Everything here is silent when d2slangvk.conf carries no
// preset= line — zero cost, zero behavior change vs P2.
//
// Build modes: with G3XVK_HAVE_SLANGVK=1 the real library is linked; =0 gives
// a stub build (same API, always inactive) so the game tree and CI keep
// building when the libslang-vk checkout is absent. The pure logic (conf
// parse, output-rect math) lives outside the ifdef and is covered by the
// host tests (scripts/fxtest).
#pragma once

#include <stddef.h>
#include <stdint.h>

// Scaling modes (d2slangvk.conf "scale=").
enum {
    G3XSLANG_SCALE_FIT = 0,      // fit + center, letterbox (library default)
    G3XSLANG_SCALE_STRETCH = 1,  // fill the whole swapchain image
    G3XSLANG_SCALE_INTEGER = 2   // largest whole-number multiple, centered
};

// Content aspect (d2slangvk.conf "aspect="; also cycles live on F11).
// auto keeps the scale= geometry; an explicit ratio letterboxes the chain's
// output into the largest centered rect of that ratio inside the swapchain
// (4:3 on a 16:9 window = pillarbox; 16:9 on a 16:9 window = full-bleed).
enum {
    G3XSLANG_ASPECT_AUTO = 0,    // follow scale= (startup default)
    G3XSLANG_ASPECT_4_3   = 1,
    G3XSLANG_ASPECT_16_10 = 2,
    G3XSLANG_ASPECT_16_9  = 3,
    G3XSLANG_ASPECT_COUNT
};

// Pixel rectangle for the chain's output (library semantics: all-zero =
// fit + center letterbox; x/y may be negative when host-supplied).
struct G3xSlangRect {
    int32_t  x, y;
    uint32_t w, h;
};

// d2slangvk.conf body, parsed. Defaults: preset disabled, fit, vsync default,
// aspect auto, base res 1x, window native, borderless off.
struct G3xSlangConf {
    char preset[512];   // "" = chain disabled; relative paths resolve against the game exe dir
    int  scale;         // G3XSLANG_SCALE_*
    int  vsync;         // -1 = swapchain default (FIFO), 0 = off, 1 = on
    int  aspect;        // G3XSLANG_ASPECT_* (0.4.3; default auto)
    int  base_res;      // shader base resolution multiplier 1..4 (0.4.3; default 1)
    char window[32];    // 0.4.5 present-window preset name ("" = native; consumed by
                        // g3x_window — native|240p|480p|576p|720p|768p|1080p|desktop|WxH)
    int  borderless;    // 0.4.5: strip the frame on preset windows (0/1; "desktop" is
                        // borderless regardless)
    int  controller;        // 0.4.5: gamepad-to-mouse/keys master switch (0/1; consumed
                            // by g3x_controller — off by default)
    float controller_sens;  // 0.4.5: left-stick speed multiplier (<=0 = default 1.0)
    int  controller_deadzone; // 0.4.5: radial deadzone percent (<=0 = default 15,
                              // clamped 1..80)
    float controller_aimsens; // 0.4.5: right-stick fine-aim multiplier (<=0 = default 0.35)
    int  controller_moveclick; // 0.4.5: stick-walk flag; parser writes 1 (present
                               // non-zero value), -1 (present "0"), 0 = key absent;
                               // fixup maps >=0 -> on (the default), <0 -> off
    int  controller_home;      // 0.4.5: cursor-home flag; same parser convention as
                               // moveclick — absent/default = on, present "0" = off
};

// Pure parser for a d2slangvk.conf body: "[slangvk]" section with
// preset=/scale=/vsync= keys; ';' and '#' comments; blanks ignored; later
// assignments win. Missing keys keep defaults. Never fails on content —
// unknown keys are ignored, bad values fall back to the default.
bool g3x_slang_conf_parse(const char* text, G3xSlangConf* out);

// Pure output-rect math. fit -> {0,0,0,0}; stretch -> full target; integer ->
// k = floor(min(out/in)) clamped to >= 1, rect centered (clamped to the
// target when the input is larger than it, which degenerates to fill).
G3xSlangRect g3x_slang_output_rect(int mode, unsigned in_w, unsigned in_h,
                                   unsigned out_w, unsigned out_h);

// Pure aspect-rect math (0.4.3). Largest centered rect of the requested
// ratio inside the out_w x out_h target; G3XSLANG_ASPECT_AUTO returns the
// all-zero sentinel meaning "caller keeps its scale= geometry". The chain
// output is letterboxed into this rect by the library.
G3xSlangRect g3x_slang_aspect_rect(int aspect, unsigned out_w, unsigned out_h);

// ---- lifecycle (called from vk_backend / glide_exports) --------------------
//
// Handles are the REAL Vulkan objects as void* so this header stays
// window/Vulkan-free (the stub build compiles on the Linux test host):
// instance, physical device, device, graphics queue, queue family index.

bool g3x_slang_init(void* instance, void* physical_device, void* device,
                    void* queue, unsigned queue_family);
void g3x_slang_shutdown();                       // chains -> presets -> image -> adopt-release

void g3x_slang_context_open();                   // re-read d2slangvk.conf (grSstWinOpen)

// Called after every swapchain (re)creation: retargets the chain (slangvk_chain_resize).
void g3x_slang_swapchain_resized(unsigned out_w, unsigned out_h, int swap_format);

// Per-frame: upload the game-res BGRA frame (post-gamma, overlay already
// drawn) and render the chain into an acquired swapchain image, then present.
// Returns true when THIS module handled the frame; false = caller must run
// the legacy unfiltered present path.
bool g3x_slang_present(const uint8_t* bgra, unsigned w, unsigned h);

// True when the chain will take the next frame (gates g3x_frame_present's path).
bool g3x_slang_active();

// Vsync preference for swapchain creation: -1 default (FIFO), 0 = off, 1 = on.
int g3x_slang_vsync_pref();

// 0.4.3 hotkeys, polled from the present path while a chain filters frames
// (mirrors Chocolate-Doom-VK): F11 cycles the content aspect
// 4:3 -> 16:10 -> 16:9 -> auto, F12 cycles the shader base resolution
// 1x -> 2x -> 3x -> 4x -> 1x. Both are session-only (never written back to
// d2slangvk.conf; a conf reload resets them to the file's values).
void g3x_slang_hotkeys_poll();

// 0.4.5 Shift+V conf panel, polled from the present path on EVERY frame —
// unlike F11/F12 this works with no chain active (the controller=/window=
// options matter there too). A press toggles the overlay's left-edge panel
// listing the resolved d2slangvk.conf values (defaults filled in, live
// F11/F12 picks marked with their file: values when they differ). Content is
// re-read from disk at every open — the file is the truth — and refreshed
// while visible whenever the conf hot-reloads or an F11/F12 press lands.
// Honors the [glide3xvk] overlay= ini switch (overlay=0 = no overlay boxes).
// 0.4.5: with the panel up, Up/Down move a green selection cursor through
// the option lines (header and hint footer skipped; clamped at the ends, no
// wrap; each move re-parses the conf), and Left/Right CHANGE the selected
// option: one press = one step (enum neighbor, +/- one increment, or a
// toggle), written straight into d2slangvk.conf as a surgical single-key
// rewrite (comments/spacing/order survive) and pushed through the normal
// conf machinery, so it lands live like a hand edit of the file. While the
// panel is hidden the arrow keys are untouched — the game keeps them.
void g3x_slang_conf_panel_poll();

#ifndef G3XVK_HAVE_SLANGVK
// Stub-build test hooks (host tests only; not compiled into the real DLL).
void g3x_slang_test_set_active(bool on);
void g3x_slang_test_set_present(bool on);        // false -> present() fails like the real thing
const uint8_t* g3x_slang_test_last_frame(unsigned* w, unsigned* h); // last upload attempt
#endif
