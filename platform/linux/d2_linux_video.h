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

void Video_Shutdown();

}  // namespace d2linux
