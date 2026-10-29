// g3x_raster_gpu.h — Route-B Stage 2: the DLL-side GPU raster path (§7).
//
// [glide3xvk] raster=gpu in glide3xvk.ini hands the game-frame raster to the
// binned compute kernel (spike/routeb.comp): the record sink feeds the shared
// packer (spike/routeb_pack.h — the SAME pack implementation the CI parity
// harness runs), one dispatch per frame renders the frame into a game-res
// image, and the backend presents it through the g3x_vk_present_image blit
// seam. Default raster=cpu — the CPU raster stays byte-identical until the
// flag is set (and stays the fallback on any GPU failure, fail-soft).
//
// Frame flow (all on the game thread, serialized with the present path):
//   grSstWinOpen -> g3x_raster_gpu_start(w, h): loads routeb.comp.spv from the
//                  DLL directory, builds pipeline + game-res image, attaches
//                  the record sink, switches the dispatcher to emit-only.
//   draw calls   -> sink accumulates post-dispatch primitives + versioned
//                  table snapshots (palette/chroma/texels), CPU raster off.
//   swap         -> g3x_frame_present -> g3x_raster_gpu_end_frame(): packs the
//                  frame, uploads, dispatches (tiles_x x tiles_y workgroups),
//                  presents. A frame with zero primitives (Bink/LFB) returns
//                  false and the CPU compose path presents as always — the
//                  mode switch is per-frame and free.
//   close/detach -> g3x_raster_gpu_stop (sink off) / g3x_raster_gpu_shutdown
//                  (objects freed, called from g3x_vk_shutdown with the
//                  device still alive).
#pragma once

void g3x_raster_gpu_start(unsigned w, unsigned h);
void g3x_raster_gpu_stop();
bool g3x_raster_gpu_active();
void g3x_raster_gpu_set_ghash(bool on);
bool g3x_raster_gpu_end_frame();     // true = this frame was rendered + presented
void g3x_raster_gpu_shutdown();
