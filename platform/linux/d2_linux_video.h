// d2linux video host - window, renderer, and the 8-bit palettized
// presentation path. See d2_linux_main.cpp for the milestone-0 scope note.
#pragma once

struct SDL_Window;

namespace d2linux {

// Opens the window + renderer, creates the INDEX8 staging surface, its
// palette and the streaming texture. The window handle is returned for the
// event loop (and, later, for the input shim).
bool Video_Init(int w, int h, SDL_Window** out_window);

// Builds one frame in the INDEX8 surface (the palette is the light-ramp
// carrier), uploads it through the streaming texture, presents.
void Video_Frame(int frame);

// [d2linux M2] attaches the presentation plumbing (renderer + INDEX8 staging
// + palette + streaming texture) to an EXISTING window — the user32 shim's
// CreateWindowExA calls this for the game window the boot chain creates.
// Fails if something is already hosting (the M0 window, or a double-create).
bool Video_Attach(SDL_Window* window);

// [d2linux M2] tears the presentation plumbing down (window excluded — the
// user32 shim's DestroyWindow owns that lifecycle). The window argument pairs
// detach with attach: only the window THIS layer is hosting gets torn down,
// so DestroyWindow of an unattached window (or a NULL — shutdown) is a no-op
// for everything else.
void Video_Detach(SDL_Window* window);

// [d2linux M2 round 2] installs the game's 256-entry palette (PALETTEENTRY
// layout: peRed, peGreen, peBlue, peFlags — the IDirectDrawPalette object's
// table) into the staging surface's palette. False when nothing is hosting.
bool Video_SetPalette(const void* entries256);

// [d2linux M2 round 2] the DirectDraw shim's present: one 8-bit index frame
// (row-pitched) into the staging surface — centered on black when the frame
// is smaller than the hosted window (the fullscreen-exclusive analogue: a
// centered, letterboxed mode) — then converted straight into the streaming
// texture (no intermediate surface) and presented. False when nothing hosts.
bool Video_PresentIndexed(const uint8_t* indices, int pitch, int w, int h);

// [d2linux M2 round 2] presents completed so far — the self-test's probe
// that Flip really drove the presentation chain.
int Video_PresentCount();

void Video_Shutdown();

}  // namespace d2linux
