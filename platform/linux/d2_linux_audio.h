// d2linux audio host - playback device stream stub. See d2_linux_main.cpp
// for the milestone-0 scope note.
#pragma once

namespace d2linux {

// Opens the default playback device as an S16-stereo 22.05 kHz stream (the
// retail D2Sound output shape) fed with silence. Failures are reported and
// treated as non-fatal by the caller.
bool Audio_Init();
void Audio_Shutdown();

}  // namespace d2linux
