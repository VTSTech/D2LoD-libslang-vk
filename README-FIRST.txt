d2linux - M2 overlay (round 1): the user32 surface goes real
============================================================
commit 5e1fe37c (on top of the m14 banner, 323e1803)

WHAT THIS IS
------------
M2 ("title screen offline", PLAN.md) round 1. Before any DirectDraw
pixel can reach the screen, the boot chain needs the Win32 windowing
surface it is built on: D2GFX_Initialize registers the "Diablo II"
class with D2WinMain's WndProc, D2GFX_CreateWindow creates the window,
and three pump shapes (D2WinMain::MessagePump's Peek-probe + GetMessage,
MainMenus' GetAndDispatchAllPossible PM_REMOVE drain, D2Client's
cinematic pump) dispatch into the handler table (globals/D2Win.cpp's
gaUIWindowHandlers).

M1 fail-softed ALL of that - every stub returned FALSE/NULL: the M1
build could register nothing, create nothing, pump nothing. This round
makes that surface real over SDL3:

  - a window registry (HWND -> class proc, client size, shown state);
    CreateWindowExA sizes a real SDL window (WS_POPUP = the client size
    handed in directly - the game's only creation shape);
  - a message queue fed by an SDL event pump that runs INSIDE
    PeekMessageA/GetMessageA - every pump shape works because each call
    first drains SDL into the queue, then serves from it (PM_NOREMOVE
    peeks, PM_REMOVE pops, GetMessage blocks; WM_QUIT is delivered
    Win32-style once the queue drains);
  - input translation to the exact WM_* stream the handler table
    registers: WM_MOUSEMOVE/NCMOVE/LEAVE, buttons 0x201-0x208, wheel,
    WM_CHAR, WM_KEYDOWN/UP with VK wParams, WM_ACTIVATEAPP, WM_CLOSE;
  - the load-bearing small stuff: ShowCursor's display-count semantics
    (the game's HideOSCursor loops on the RETURN value until the sign
    flips), GetKeyState's 0x8000 bit (edit-box shift-select), FindWindowA
    as the single-instance mutex (a flock(2) claim - the probe IS the
    claim), MessageBoxA as an SDL dialog + the stderr line;
  - clipboard (D2WinEditBox copy/paste) via the moveable-memory family;
  - GDI stays fail-soft BY DESIGN: its only slice consumer is D2DDraw's
    debug-font rasterizer, which tolerates a NULL HDC (the mask fills
    blank = no debug text) - verified against D2ddDebugFont_Alloc's
    null paths. The game's real fonts are DC6-based and do not touch GDI.

Plus the video side: Video_Attach/Video_Detach - the presentation
plumbing (renderer + INDEX8 staging + palette + streaming texture) can
now attach to the GAME's window when the user32 shim creates it. The
DirectDraw shim (next round) drives Video_Frame from there; until then
nothing attaches and the M0 window stays the only presenter. Nothing
changes on your desktop run yet: same rainbow window, same --smoke -
but the pump the game will drive is now verified in CI.

WHAT THE OVERLAY CHANGES (10 files)
-----------------------------------
- platform/linux/d2_linux_shim_user32.cpp   NEW (1110 lines): the
  user32/GDI shim - window registry, SDL-fed pump, input translation,
  cursor counts, clipboard, fail-soft GDI.
- platform/linux/d2_linux_api.h             M2 declarations: the
  RegisterClass/CreateWindow/pump/cursor/GDI/clipboard/RECT families,
  each with its named call sites.
- platform/linux/d2_linux_compat.h          REAL x86 layouts where M1
  had opaque stand-ins: WNDCLASSA (D2GFX fills it field-by-field - the
  old 4-byte stand-in would have smashed the stack object), tagMSG
  stays owned by D2Client/Message.h, TRACKMOUSEEVENT, WINDOWPLACEMENT,
  MONITORINFO, DEVMODEA (the real 148 bytes - D2GFX's small-screen
  fallback fill would have smashed the old 4-byte stand-in), and
  POINT/RECT now owned here (the M2 structs embed them BY VALUE).
- platform/linux/mk-platform-shadow.sh      shadow patches 13-16: drop
  the POINT/RECT/tagWINDOWPLACEMENT stubs (compat.h owns them now,
  compat.h is force-included first), retarget WNDPROC to
  LRESULT (__stdcall *)(...) - the game's class procs return LRESULT;
  the void-returning stub failed every D2Launch/D2Win handler-table
  assignment. The configure line moves "12 patches" -> "16 patches".
- platform/linux/d2_linux_main.cpp          the user32 contract rides
  --smoke: a 10-check self-test (class proc fires, queue preserves
  order + payloads, DispatchMessage routes into the proc, SendMessageA
  is synchronous, the ShowCursor hide-loop terminates, WM_DESTROY is
  queued, WM_QUIT carries the exit code). Every check is one the M1
  fail-soft stubs would have FAILED.
- platform/linux/d2_linux_video.{h,cpp}     Video_Attach/Video_Detach -
  the presentation plumbing splits out of Video_Init so a game-created
  window can become the presenter (double-create and unowned-destroy
  are rejected, paired like attach/detach).
- platform/linux/d2_linux_shim_win32.cpp    the pump family's fail-soft
  stubs MOVED to the user32 shim (nothing else in this file touches
  windowing).
- platform/linux/build.cmake                the new TU in the shim set.
- tools/check-build-linux.sh                the smoke now greps for the
  user32 verdict line - a stale tree FAILS loudly ("FAIL: no user32
  self-test verdict line").

VERIFY (sandbox of record, this round)
--------------------------------------
  - native x86-64 skeleton: builds; ./d2linux --smoke ->
      d2linux user32 self-test OK (class/window/pump/cursor/destroy/quit)
      d2linux smoke OK (3 frames, dummy video/audio)        exit 0
  - i386 slice: the M1 44-TU game build + the shim layer (incl. the new
    TU) links - static ELF 32-bit, unchanged warning set.
  - tools/check-build-linux.sh . HEAD from a clean clone -> "OK: d2linux
    built and smoke passed" with the verdict line in the log.
  - PE files untouched: the diff is linux-only (10 files, all under
    platform/linux/ + tools/).

APPLY
-----
From the repo root (branch linux):

  unzip -o d2linux-m2-user32-overlay.zip    # writes the 10 files over
                                            # their paths + this README
  git add -A && git commit -m "linux: M2 round 1 - the user32 surface goes real (window registry, SDL-fed pump, cursor/input, presentation attach)"

Reconfigure before rebuilding if the build dir predates this round (the
platform shadow is a configure-time product):

  cmake -S . -B build-native -DD2LINUX_ARCH=native
  cmake --build build-native
  ./build-native/d2linux --smoke

Then push - CI builds both arches and smokes; the i386 artifact stays
console-only by design (see the M13 README for the video verdicts).

WHAT'S NEXT (M2 round 2)
------------------------
The DirectDraw COM shim (d2_linux_shim_ddraw.cpp): the game's
CreateSurface / Lock / Unlock / Blt / Flip / SetPalette semantics onto
Video_Attach's INDEX8 staging surface - the first round where a real
game pixel can reach the screen and the M0 pattern producer retires.
Then Storm/MPQ file routing, then the boot-chain slice growth
(globals.cpp step 2 + D2Launch/D2Client/D2Multi/D2Common). Exit per
PLAN.md: title screen renders, main menu navigable, character select
visible.
