d2linux - M2 round 2 overlay: the DirectDraw COM shim — first game pixels
=========================================================================
on top of your current HEAD (91927833, the banner/warnings round your VM built green)

WHAT THIS ROUND IS
------------------
The banner's "next" pointer does what it says: the DirectDraw shim exists.
The game's 2D renderer (D2DDraw::Renderer::DirectDraw) talks DirectDraw 1 —
this round implements that COM surface as host code over SDL3, so the first
"game-shaped" pixels flow through the real pipeline: game-style ladder ->
8-bit surfaces -> palette -> INDEX8 staging -> one conversion pass into the
streaming texture -> screen. The rainbow M0 pattern now shares its present
path with a real DirectDraw flip.

THE LADDER THE SHIM IMPLEMENTS (your game's own call sites)
-----------------------------------------------------------
  DirectDrawCreate(NULL, &dd, NULL)              d2linux ddraw: DirectDrawCreate ...
  dd->SetCooperativeLevel(hwnd, 0x11)            fullscreen-exclusive, hwnd validated
                                                 against the user32 shim's window
                                                 registry (a window this process never
                                                 created is refused)
  dd->SetDisplayMode(640, 480, 8)                8bpp modes only, loudly
  dd->CreateSurface(primary + N back buffers)    falls back 2 -> 1 -> 0 like the game
  primary->GetAttachedSurface(BACKBUFFER)
  dd->CreateSurface(offscreen, SYSTEMMEMORY)     the compose surface, pitch == width
  primary->SetPalette(pal)                       DDPCAPS_8BIT, mirrored into the SDL
                                                 palette on SetPalette AND SetEntries
  BeginScene:  offscreen->Lock(WAIT|WRITEONLY) -> the game writes indices
  EndScene2:   back->BltFast(0,0, offscreen, full, WAIT) -> primary->Flip -> PRESENT
  cutscenes:   primary->Blt(DDBLT_COLORFILL) black
  deferred:    pal->SetEntries(0, 1, 255, entries+1) (EndScene2's palette path)

Semantics worth knowing (all documented in the source):
- SetDisplayMode keeps the DX1 3-ARG ABI. The ghidra header (DirectDraw.h)
  spells it with the later-SDK 5-arg signature — that is a reconstruction
  typo; the game pushes 3 args, so the shim's callee pops 3. On i386
  __stdcall the mismatch would corrupt the stack; it cannot here. (When the
  D2DDraw TUs join the slice, that one header typedef needs the same fix —
  the platform-shadow mechanism is the place.)
- pitch == width on every surface. The game stores Lock's lPitch into what
  EndScene2's BltFast rect uses as a WIDTH — any row padding would leak into
  blit geometry. There is none.
- one back buffer regardless of the requested count: the game fully
  recomposes every frame (ClearScreen + full-rect BltFast), so real DD's
  chain rotation is unobservable through its calls.
- Release(back) returns 1 while the chain lives — the flip chain owns the
  last reference and it dies with the primary (real DD ownership).
- out-of-ladder calls: DD_OK-noops where real DD is a no-op, otherwise
  DDERR_UNSUPPORTED with a ONE-SHOT log line. Never silent lies.

THE SELF-TEST (your --smoke now runs two Win32-contract gates)
--------------------------------------------------------------
"ddraw self-test OK" = 16 checks in the game's own call order: create, QI
refused, coop-level accept + refuse, mode accept + refuse (8bpp yes, 32bpp
no), primary desc, GetAttachedSurface, GetSurfaceDesc (640x480 / pitch 640 /
8bpp indexed), palette create + attach, offscreen create, the BeginScene
lock shape, BltFast, the GetBackBuffer readback ROUND-TRIP, Flip-present
count (+1 exactly), PlayCutScene COLORFILL, EndScene2 SetEntries, status
stubs, and the full release ladder. It also exercises the letterbox path
(640x480 mode presented inside the 800x600 M0 host — the fullscreen-
exclusive analogue: centered on black).

WHAT THE OVERLAY CHANGES (7 files)
----------------------------------
- platform/linux/d2_linux_shim_ddraw.h    NEW - the DX1 COM types (D2-prefixed,
                                          no ODR clash with the game header),
                                          DD_* constants, i386 layout
                                          static_asserts (DDSURFACEDESC 0x6C,
                                          DDBLTFX 100 / dwFillColor at 0x50).
- platform/linux/d2_linux_shim_ddraw.cpp  NEW - the shim (~950 lines).
- platform/linux/d2_linux_video.h/.cpp    Video_SetPalette / Video_PresentIndexed /
                                          Video_PresentCount; the shared present
                                          folds the 8->32 conversion into the
                                          texture lock (the per-frame
                                          SDL_ConvertSurface intermediate is
                                          GONE - one pass, zero extra allocs).
- platform/linux/d2_linux_main.cpp        RunDdrawSelfTest + smoke wiring.
- platform/linux/d2_linux_shim_user32.cpp the internal cross-shim hook
                                          d2linux_user32_window_registered()
                                          (link-level, NOT game API).
- platform/linux/build.cmake              the shim joins D2LINUX_SHIM_SOURCES;
                                          project stage bumped to
                                          "M2 round 2 - DirectDraw onto the
                                          INDEX8 surface".

HOW TO APPLY
------------
Copy the platform/ tree over your checkout (paths line up), then:

  rm -rf build && cmake -S . -B build -G "Unix Makefiles" && cmake --build build -j
  ./build/d2linux --smoke

Expected on top of your last green run: the banner's middle line now reads
"project: M2 round 2 - DirectDraw onto the INDEX8 surface (first game pixels)",
the ddraw ladder prints its evidence lines (DirectDrawCreate / coop / mode /
surfaces / palette), then:

  d2linux ddraw self-test OK (create/coop/mode/flipchain/palette/lock/blt/flip)
  d2linux smoke OK (3 frames, dummy video/audio)

And in a WINDOWED run (no args): 3 rainbow frames, then the last frames show
the DD test frame — a red/green/cyan index ramp letterboxed inside the
window — because the self-test's Flip presents to the same pipeline.

NEXT (per PLAN.md): Storm/MPQ file routing + the boot-chain slice
(globals.cpp step 2 + D2Launch/D2Client) - the M2 exit gate is the title
screen rendered through THIS shim.
