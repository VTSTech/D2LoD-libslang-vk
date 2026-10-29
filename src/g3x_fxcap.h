// g3x_fxcap.h — Route-B Stage 1 fxparity capture (DLL side, ROUTE-B.md §6).
//
// g3x_fxcap_start(N) attaches a G3xRouteBSink that serializes the frame
// module's api-level call stream to fxcapture.cap (exe directory) and stops
// itself after N frames (or at grSstWinClose). The file is the replay the
// routeb CI gate reruns through both raster backends.
#pragma once

void g3x_fxcap_start(unsigned max_frames);
void g3x_fxcap_stop();
