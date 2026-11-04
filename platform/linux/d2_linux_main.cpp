// d2linux - native Linux host, milestone 0 scaffold (branch `linux`).
//
// What this binary IS: the SDL3 host skeleton of the native-Linux port -
// window + renderer + 8-bit palettized presentation + audio device stream,
// with a headless --smoke self-test (SDL dummy drivers, exit code 0) that CI
// runs. What it is NOT: the game. The decompiled game logic is not compiled
// here yet; the port plan, shim inventory and milestone ladder are in
// docs/linux-port.md. The Windows PE line (d2.exe) is untouched and remains
// the shipped artifact on main.
//
// Design anchor (docs/linux-port.md): the game renders 8-bit palettized
// frames and its lighting lives in palette ramps, so the host presents one
// INDEX8 surface through one streaming texture - the DirectDraw shim in M2
// drives exactly this path with the game's own frames.
//
// Failures carry their own evidence (the -m32-gate philosophy at runtime):
// a video-less run prints which backends THIS binary carries, what the
// session offers, and the fix that follows - --list-video prints the same
// report without running.
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

#include "d2_linux.h"
#include "d2_linux_video.h"
#include "d2_linux_audio.h"
#include "d2_linux_shim_ddraw.h"   // [d2linux M2 round 2] the COM shapes the
                                    // self-test drives (and the game will)
#include "D2Client/Message.h"   // [d2linux M2] the GAME's tagMSG — the self-test
                                // fills the exact type the boot-chain pumps use

namespace d2linux {
namespace {

const char* EnvOr(const char* name, const char* fallback) {
    const char* v = SDL_getenv(name);
    return (v && *v) ? v : fallback;
}

// True when the kernel self-identifies as WSL - the display advice differs
// (Windows 11 WSLg provides the session; plain WSL2 needs an X server on the
// Windows side with DISPLAY set).
bool RunningUnderWSL() {
    FILE* f = std::fopen("/proc/sys/kernel/osrelease", "r");
    if (!f) {
        return false;
    }
    char buf[128] = {0};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] >= 'A' && buf[i] <= 'Z') {
            buf[i] = static_cast<char>(buf[i] + 32);
        }
    }
    return std::strstr(buf, "microsoft") != nullptr;
}

// The compiled-in driver enumeration is the ground truth about what THIS
// binary can do (SDL_video.h/SDL_audio.h bootstrappers; safe before SDL_Init).
// The vendored SDL builds these to match the dev libraries present at
// configure time - a dummy-only list is a build-time fact, not a runtime one.
void PrintCompiledDrivers() {
    std::printf("  video drivers compiled in:");
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
        std::printf(" %s", SDL_GetVideoDriver(i));
    }
    std::printf("\n  audio drivers compiled in:");
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        std::printf(" %s", SDL_GetAudioDriver(i));
    }
    std::printf("\n");
}

bool HasVideoDriver(const char* want) {
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
        const char* d = SDL_GetVideoDriver(i);
        if (d && SDL_strcmp(d, want) == 0) {
            return true;
        }
    }
    return false;
}

void PrintSessionEnv() {
    std::printf("  DISPLAY=%s\n", EnvOr("DISPLAY", "(unset)"));
    std::printf("  WAYLAND_DISPLAY=%s\n", EnvOr("WAYLAND_DISPLAY", "(unset)"));
    std::printf("  XDG_SESSION_TYPE=%s\n", EnvOr("XDG_SESSION_TYPE", "(unset)"));
    std::printf("  SDL_VIDEODRIVER=%s\n", EnvOr("SDL_VIDEODRIVER", "(unset)"));
    if (RunningUnderWSL()) {
        std::printf("  note: WSL kernel detected - Windows 11 WSLg provides the display\n"
                    "    session; plain WSL2 needs an X server on Windows with DISPLAY set\n");
    }
}

}  // namespace

// --list-video: the first move for any display problem. Works without
// SDL_Init (bootstrapper enumeration); exit 0.
void PrintVideoReport() {
    std::printf("d2linux video report:\n");
    PrintCompiledDrivers();
    PrintSessionEnv();
}

// The SDL_Init(SDL_INIT_VIDEO) failure verdict: the binary states its own
// evidence (backends, request, session) and the fix that follows from the
// combination - the runtime twin of the -m32 configure gate's verdict.
void PrintVideoVerdict() {
    std::printf("d2linux: SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
    std::printf("d2linux video diagnosis:\n");
    PrintCompiledDrivers();
    PrintSessionEnv();

    const bool x11 = HasVideoDriver("x11");
    const bool wayland = HasVideoDriver("wayland");
    const bool kmsdrm = HasVideoDriver("kmsdrm");
    const char* display = EnvOr("DISPLAY", "");
    const char* wl_display = EnvOr("WAYLAND_DISPLAY", "");

    if (!x11 && !wayland && !kmsdrm) {
        std::printf(
            "  verdict: no desktop or console video backend is compiled into this\n"
            "    binary - only SDL's dummy/offscreen test drivers - so it cannot\n"
            "    open a window on ANY machine, desktop or not. A CI artifact is\n"
            "    console-only by design (SDL_X11=OFF SDL_WAYLAND=OFF: no :i386\n"
            "    X11/Wayland dev libraries exist); a local build landing here means\n"
            "    SDL configured without the dev libraries. Fix: install the X11/\n"
            "    Wayland dev set (docs/linux-port.md, Build) and reconfigure on a\n"
            "    FRESH build dir (rm -rf), e.g.:\n"
            "      cmake -S . -B build-native -G Ninja -DD2LINUX_ARCH=native\n");
        return;
    }
    if (!x11 && !wayland) {
        std::printf(
            "  verdict: this binary is KMS/DRM-only - it draws on the Linux console\n"
            "    (a real VT), not inside a desktop session. Run it from a TTY (may\n"
            "    need video-group/root for DRM master), or rebuild with the X11/\n"
            "    Wayland dev set for a desktop window.\n");
        return;
    }
    if (*display == '\0' && *wl_display == '\0') {
        std::printf(
            "  verdict: a desktop backend is compiled in, but this shell has no\n"
            "    display session (DISPLAY/WAYLAND_DISPLAY unset). Run from inside\n"
            "    a desktop session; over SSH use 'ssh -X' (or run it on the\n"
            "    machine's console); on a bare TTY the KMS path needs a\n"
            "    kmsdrm-enabled build: SDL_VIDEODRIVER=kmsdrm ./d2linux\n");
        return;
    }
    std::printf(
        "  verdict: both backend and session exist - force the backend the\n"
        "    session actually runs and compare the error:\n"
        "      SDL_VIDEODRIVER=x11 ./d2linux      (X11 / XWayland sessions)\n"
        "      SDL_VIDEODRIVER=wayland ./d2linux  (native Wayland)\n"
        "    On a Wayland-only session without XWayland the x11 attempt fails\n"
        "    while the wayland attempt connects (GNOME also needs libdecor-0\n"
        "    for decorations).\n");
}

bool ParseArgs(int argc, char** argv, HostOptions* out) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (SDL_strcmp(a, "--smoke") == 0) {
            out->smoke = true;
        } else if (SDL_strcmp(a, "--list-video") == 0) {
            out->list_video = true;
        } else if (SDL_strcmp(a, "-h") == 0 || SDL_strcmp(a, "--help") == 0) {
            std::printf("usage: d2linux [--smoke] [--list-video]\n"
                        "  --smoke       headless self-test (dummy video/audio, %d frames, exit 0)\n"
                        "  --list-video  print the video/audio drivers compiled into this binary\n"
                        "                and the display-session environment, then exit\n",
                        out->smoke_frames);
            return false;
        } else {
            std::printf("d2linux: unknown argument '%s' (-h for usage)\n", a);
            return false;
        }
    }
    return true;
}

void PrintBanner(const HostOptions& opts) {
    // Self-ID, d2boot.log style - but TWO facts, never fused into one token
    // (the single "milestone 0" line read the project as stalled at M0 on
    // every native build; owner round-3 finding). Line 1 names the build;
    // line 2 is the PROJECT stage (D2LINUX_PROJECT_STAGE, hand-bumped every
    // code round in build.cmake); line 3 is THIS binary's shape
    // (D2LINUX_BINARY_SHAPE, computed - the i386 charter keeps game TUs out
    // of the 64-bit binary, so "no game TUs" is the binary's shape, not the
    // port's status).
    std::printf("d2linux scaffold %s | Vanilla+ native Linux host\n",
                D2LINUX_BUILD_ID);
    std::printf("%s\n", D2LINUX_PROJECT_STAGE);
    std::printf("%s%s\n", D2LINUX_BINARY_SHAPE,
                opts.smoke ? " (smoke: dummy drivers)" : "");
}

// ---------------------------------------------------------------------------
// [d2linux M2] user32 shim self-test — the Win32 contract the game's three
// pump shapes depend on, exercised headlessly before the boot chain needs it.
// Every check here is one the M1 fail-soft stubs would have FAILED: a class
// proc that actually fires, a queue that preserves order and payloads, the
// cursor-count loop that terminates, the WM_DESTROY/WM_QUIT verdicts.
// ---------------------------------------------------------------------------
namespace {

int g_user32TestCalls = 0;

LRESULT __stdcall User32TestWndProc(HWND, UINT msg, WPARAM wp, LPARAM) {
    ++g_user32TestCalls;
    return (msg == 0x0401u /*WM_USER+1*/) ? (LRESULT)wp : 0;
}

const char* RunUser32SelfTest() {
    // 1. Class registration carries the proc through to CreateWindowExA.
    WNDCLASSA wc;
    SDL_zero(wc);
    wc.lpfnWndProc   = User32TestWndProc;
    wc.lpszClassName = "d2test";
    if (!RegisterClassA(&wc)) {
        return "RegisterClassA failed";
    }
    // 2. The single-instance probe acquires the flock claim (no phantom).
    if (FindWindowA("Diablo II", nullptr) != nullptr) {
        return "FindWindowA probe found a phantom instance";
    }
    // 3. The game's creation shape: WS_POPUP, client size handed in directly.
    HWND hwnd = CreateWindowExA(0, "d2test", "d2linux m2 self-test",
                                0x80000000u /*WS_POPUP*/, 0, 0, 320, 240,
                                nullptr, nullptr, nullptr, nullptr);
    if (!hwnd) {
        return "CreateWindowExA failed";
    }
    if (!IsWindow(hwnd)) {
        return "IsWindow false after CreateWindowExA";
    }
    // 4. GetClientRect carries the requested client size.
    RECT rc;
    if (!GetClientRect(hwnd, &rc) || rc.right - rc.left != 320 ||
        rc.bottom - rc.top != 240) {
        return "GetClientRect wrong after create";
    }
    // 5. PM_NOREMOVE peeks without popping; PM_REMOVE pops exactly once.
    tagMSG m;
    if (!PostMessageA(hwnd, 0x0401u, 42, 0)) {
        return "PostMessageA failed";
    }
    if (!PeekMessageA(&m, nullptr, 0, 0, 0 /*PM_NOREMOVE*/)) {
        return "PeekMessageA PM_NOREMOVE missed the post";
    }
    if (m.message != 0x0401u || m.wParam != 42) {
        return "PM_NOREMOVE payload wrong";
    }
    if (!PeekMessageA(&m, nullptr, 0, 0, 1 /*PM_REMOVE*/)) {
        return "PM_REMOVE missed the same post";
    }
    if (PeekMessageA(&m, nullptr, 0, 0, 1)) {
        return "queue not empty after PM_REMOVE";
    }
    // 6. DispatchMessageA routes into the registered class proc.
    PostMessageA(hwnd, 0x0401u, 7, 0);
    PeekMessageA(&m, nullptr, 0, 0, 1);
    const int calls_before = g_user32TestCalls;
    if (DispatchMessageA(&m) != 7) {
        return "DispatchMessageA lost the WndProc echo";
    }
    if (g_user32TestCalls != calls_before + 1) {
        return "WndProc not invoked";
    }
    // 7. SendMessageA is synchronous — a direct proc call, no queue hop.
    if (SendMessageA(hwnd, 0x0401u, 5, 0) != 5) {
        return "SendMessageA echo wrong";
    }
    if (PeekMessageA(&m, nullptr, 0, 0, 1)) {
        return "SendMessageA leaked into the queue";
    }
    // 8. The game's HideOSCursor loop terminates on the display count.
    while (ShowCursor(FALSE) > -1) {
    }
    if (ShowCursor(TRUE) != 0) {
        return "ShowCursor count broken after the hide loop";
    }
    // 9. DestroyWindow: the IsWindow verdict flips and WM_DESTROY is queued.
    if (!DestroyWindow(hwnd)) {
        return "DestroyWindow failed";
    }
    if (IsWindow(hwnd)) {
        return "IsWindow true after DestroyWindow";
    }
    if (!PeekMessageA(&m, nullptr, 0, 0, 1) || m.message != 0x0002u /*WM_DESTROY*/) {
        return "WM_DESTROY not delivered";
    }
    // 10. The GetMessage verdict: WM_QUIT carries the exit code, returns 0.
    PostQuitMessage(42);
    if (GetMessageA(&m, nullptr, 0, 0) != 0 || m.message != 0x0012u /*WM_QUIT*/ ||
        m.wParam != 42) {
        return "GetMessageA WM_QUIT verdict wrong";
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// [d2linux M2 round 2] the DirectDraw shim self-test — the game's own init
// ladder (D2DDraw::Renderer::DirectDraw::InternalOpenWindow's call order and
// flag values) plus the Lock/BltFast/Flip/palette semantics its scene loop
// leans on (BeginScene/EndScene1/EndScene2/GetBackBuffer/PlayCutScene are the
// reference call sites). Every check maps to a named game call site; a fail
// names the step. Runs headless on the dummy drivers like the user32 test.
// ---------------------------------------------------------------------------

LRESULT __stdcall DdrawTestWndProc(HWND, UINT, WPARAM, LPARAM) {
    return 0;
}

uint8_t g_ddrawTestPal[256 * 4];

void BuildDdrawTestPalette() {
    for (int i = 0; i < 256; ++i) {
        g_ddrawTestPal[i * 4 + 0] = (uint8_t)i;                 // peRed
        g_ddrawTestPal[i * 4 + 1] = (uint8_t)(255 - i);         // peGreen
        g_ddrawTestPal[i * 4 + 2] = (uint8_t)((i * 3) & 0xFF);  // peBlue
        g_ddrawTestPal[i * 4 + 3] = 0;                          // peFlags
    }
}

uint8_t DdrawTestPattern(int x, int y) {
    return (uint8_t)((x * 3 + y * 5) & 0xFF);
}

const char* RunDdrawSelfTest() {
    BuildDdrawTestPalette();

    // The test window: the game's creation shape at the game's mode size.
    // Own class ("d2ddtest") so this test is independent of the user32
    // test's registration order; a duplicate registration is benign.
    WNDCLASSA wc;
    SDL_zero(wc);
    wc.lpfnWndProc   = DdrawTestWndProc;
    wc.lpszClassName = "d2ddtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, "d2ddtest", "d2linux m2 ddraw self-test",
                                0x80000000u /*WS_POPUP*/, 0, 0, 640, 480,
                                nullptr, nullptr, nullptr, nullptr);
    if (!hwnd) {
        return "ddraw test CreateWindowExA failed";
    }

    // 1. The entry point the game links.
    D2DDObj* dd = nullptr;
    if (DirectDrawCreate(nullptr, &dd, nullptr) != DD_OK || !dd) {
        return "DirectDrawCreate failed";
    }
    // 2. QI stays honest: no COM identity queries are modeled.
    uint32_t fake_iid[4] = {0x12345678u, 0x9abcdef0u, 0x0fedcba9u, 0x87654321u};
    void* qv = nullptr;
    if (dd->lpVtbl->QueryInterface(dd, fake_iid, &qv) != D2DD_E_NOINTERFACE) {
        return "QueryInterface not refused";
    }
    // 3. Cooperative level: the game's 0x11 on a real window; garbage refused.
    if (dd->lpVtbl->SetCooperativeLevel(dd, hwnd, 0x11) != DD_OK) {
        return "SetCooperativeLevel(0x11) failed";
    }
    if (dd->lpVtbl->SetCooperativeLevel(dd, nullptr, 0x11) == DD_OK) {
        return "SetCooperativeLevel accepted a null hwnd";
    }
    // 4. Display mode: 8bpp granted (the game's SetDisplayMode(640,480,8),
    //    3-arg DX1 ABI), other depths refused loudly.
    if (dd->lpVtbl->SetDisplayMode(dd, 640, 480, 8) != DD_OK) {
        return "SetDisplayMode(640x480x8) failed";
    }
    if (dd->lpVtbl->SetDisplayMode(dd, 640, 480, 32) == DD_OK) {
        return "SetDisplayMode accepted a non-8bpp mode";
    }
    // 5. The primary: the game's exact desc (VIDEOMEMORY|PRIMARY|FLIP|COMPLEX,
    //    back count 2 — the first ladder rung; the shim grants one back
    //    buffer regardless, indistinguishable through the game's calls).
    D2DDSurfDesc sd;
    SDL_zero(sd);
    sd.dwSize = sizeof(D2DDSurfDesc);
    sd.dwFlags = D2DD_DDSDD_CAPS | D2DD_DDSD_BACKBUFFERCOUNT;
    sd.dwBackBufferCount = 2;
    sd.ddsCaps = D2DD_DDSCAPS_VIDEOMEMORY | D2DD_DDSCAPS_PRIMARYSURFACE |
                 D2DD_DDSCAPS_FLIP | D2DD_DDSCAPS_COMPLEX;
    D2DDSurfaceObj* primary = nullptr;
    if (dd->lpVtbl->CreateSurface(dd, &sd, (void**)&primary, nullptr) != DD_OK ||
        !primary) {
        return "CreateSurface(primary) failed";
    }
    // 6. The back buffer through the game's GetAttachedSurface(BACKBUFFER).
    D2DDCaps caps;
    caps.dwCaps = D2DD_DDSCAPS_BACKBUFFER;
    D2DDSurfaceObj* back = nullptr;
    if (primary->lpVtbl->GetAttachedSurface(primary, &caps, &back) != DD_OK ||
        !back || back == primary) {
        return "GetAttachedSurface(BACKBUFFER) failed";
    }
    // 7. GetSurfaceDesc reports the mode: 640x480, pitch 640, 8bpp indexed.
    D2DDSurfDesc pd;
    SDL_zero(pd);
    pd.dwSize = sizeof(D2DDSurfDesc);
    if (primary->lpVtbl->GetSurfaceDesc(primary, &pd) != DD_OK ||
        pd.dwWidth != 640 || pd.dwHeight != 480 || pd.lPitch != 640 ||
        !(pd.ddpfPixelFormat.dwFlags & D2DD_DDPF_PALETTEINDEXED8) ||
        pd.ddpfPixelFormat.dwRGBBitCount != 8) {
        return "GetSurfaceDesc(primary) wrong";
    }
    // 8. Palette: CreatePalette(DDPCAPS_8BIT) + SetPalette(primary).
    D2DDPaletteObj* pal = nullptr;
    if (dd->lpVtbl->CreatePalette(dd, D2DD_DDPCAPS_8BIT, g_ddrawTestPal, &pal,
                                  nullptr) != DD_OK || !pal) {
        return "CreatePalette failed";
    }
    if (primary->lpVtbl->SetPalette(primary, pal) != DD_OK) {
        return "SetPalette failed";
    }
    // 9. The compose surface: the game clones its GetSurfaceDesc(primary)
    //    result with SYSTEMMEMORY|OFFSCREENPLAIN caps (its exact ladder).
    SDL_zero(sd);
    sd.dwSize = sizeof(D2DDSurfDesc);
    sd.dwFlags = D2DD_DDSDD_CAPS | D2DD_DDSD_HEIGHT | D2DD_DDSD_WIDTH;
    sd.dwWidth = pd.dwWidth;
    sd.dwHeight = pd.dwHeight;
    sd.ddsCaps = D2DD_DDSCAPS_SYSTEMMEMORY | D2DD_DDSCAPS_OFFSCREENPLAIN;
    D2DDSurfaceObj* off = nullptr;
    if (dd->lpVtbl->CreateSurface(dd, &sd, (void**)&off, nullptr) != DD_OK || !off) {
        return "CreateSurface(offscreen) failed";
    }
    // 10. BeginScene's lock shape: Lock(WAIT|WRITEONLY) hands a stable
    //     pointer at pitch 640; the game writes indices; Unlock ignores arg.
    D2DDSurfDesc ld;
    SDL_zero(ld);
    ld.dwSize = sizeof(D2DDSurfDesc);
    if (off->lpVtbl->Lock(off, nullptr, &ld,
                          D2DD_DDLOCK_WAIT | D2DD_DDLOCK_WRITEONLY,
                          nullptr) != DD_OK || !ld.lpSurface || ld.lPitch != 640) {
        return "Lock(offscreen) failed";
    }
    for (int y = 0; y < 480; ++y) {
        uint8_t* row = (uint8_t*)ld.lpSurface + (size_t)y * ld.lPitch;
        for (int x = 0; x < 640; ++x) {
            row[x] = DdrawTestPattern(x, y);
        }
    }
    if (off->lpVtbl->Unlock(off, nullptr) != DD_OK) {
        return "Unlock(offscreen) failed";
    }
    // 11. EndScene2's blit: BltFast(back, 0,0, offscreen, full rect, WAIT).
    RECT full;
    full.left = 0; full.top = 0; full.right = 640; full.bottom = 480;
    if (back->lpVtbl->BltFast(back, 0, 0, off, &full, D2DD_DDBLTFAST_WAIT) != DD_OK) {
        return "BltFast(off->back) failed";
    }
    // 12. GetBackBuffer's read shape: Lock(WAIT|READONLY) round-trips the
    //     exact indices the game wrote.
    SDL_zero(ld);
    ld.dwSize = sizeof(D2DDSurfDesc);
    if (back->lpVtbl->Lock(back, nullptr, &ld,
                           D2DD_DDLOCK_WAIT | D2DD_DDLOCK_READONLY,
                           nullptr) != DD_OK || !ld.lpSurface) {
        return "Lock(back) failed";
    }
    for (int y = 0; y < 480; ++y) {
        const uint8_t* row = (const uint8_t*)ld.lpSurface + (size_t)y * ld.lPitch;
        for (int x = 0; x < 640; ++x) {
            if (row[x] != DdrawTestPattern(x, y)) {
                return "back-buffer round-trip mismatch";
            }
        }
    }
    if (back->lpVtbl->Unlock(back, nullptr) != DD_OK) {
        return "Unlock(back) failed";
    }
    // 13. THE present: Flip(primary) drives staging→texture→screen exactly
    //     once. In this run the M0 window owns the presentation, so this
    //     exercises the letterboxed 640-in-800 path too.
    const int pc0 = d2linux::Video_PresentCount();
    if (primary->lpVtbl->Flip(primary, nullptr, D2DD_DDFLIP_WAIT) != DD_OK ||
        d2linux::Video_PresentCount() != pc0 + 1) {
        return "Flip did not present";
    }
    // 14. PlayCutScene's clear: Blt(primary, COLORFILL black).
    D2DDBltFx fx;
    SDL_zero(fx);
    fx.dwSize = sizeof(D2DDBltFx);
    fx.dwFillColor = 0;
    if (primary->lpVtbl->Blt(primary, nullptr, nullptr, nullptr,
                             D2DD_DDBLT_COLORFILL, &fx) != DD_OK) {
        return "Blt(COLORFILL) failed";
    }
    // 15. EndScene2's deferred palette: SetEntries(0, 1, 255, entries+1).
    if (pal->lpVtbl->SetEntries(pal, 0, 1, 0xFF, g_ddrawTestPal + 4) != DD_OK) {
        return "SetEntries failed";
    }
    // 16. Status stubs stay green: nothing is ever "lost" on this path.
    if (primary->lpVtbl->IsLost(primary) != DD_OK ||
        primary->lpVtbl->Restore(primary) != DD_OK ||
        back->lpVtbl->GetBltStatus(back, 0) != DD_OK) {
        return "status stubs not green";
    }
    // 17. The release ladder (the game's InternalReleaseWindow order:
    //     offscreen, back, primary, palette, dd). The back buffer still
    //     carries the flip chain's reference when the game releases it —
    //     the chain's ref dies with the primary (real DD ownership), so
    //     Release(back) legitimately returns 1 here.
    if (off->lpVtbl->Release(off) != 0) return "offscreen release count wrong";
    if (back->lpVtbl->Release(back) != 1) return "back buffer release count wrong";
    if (primary->lpVtbl->Release(primary) != 0) return "primary release count wrong";
    if (pal->lpVtbl->Release(pal) != 0) return "palette release count wrong";
    if (dd->lpVtbl->Release(dd) != 0) return "DirectDraw release count wrong";
    // 18. The test window's teardown (DestroyWindow; the M0 presentation
    //     survives — Video_Detach only follows the hosting window).
    if (!DestroyWindow(hwnd)) {
        return "ddraw test DestroyWindow failed";
    }
    return nullptr;
}

}  // namespace

}  // namespace d2linux

int main(int argc, char** argv) {
    d2linux::HostOptions opts;
    if (!d2linux::ParseArgs(argc, argv, &opts)) {
        return 2;
    }
    d2linux::PrintBanner(opts);

    if (opts.list_video) {
        d2linux::PrintVideoReport();
        return 0;
    }

    if (opts.smoke) {
        // Before SDL_Init: force the dummy backends so the self-test runs
        // headless (CI, containers). SDL reads these as environment variables
        // (SDL_hints.c: SDL_getenv("SDL_VIDEODRIVER")), so set the env vars -
        // a same-named hint alone is not what the driver picker consults.
        SDL_setenv_unsafe("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
        SDL_SetHint("SDL_VIDEODRIVER", "dummy");
        SDL_SetHint("SDL_AUDIODRIVER", "dummy");
    }

    // Video is fatal and self-diagnosing; audio is best-effort - splitting
    // the SDL_Init call means a soundcard-less box fails on video only (if
    // at all), instead of a combined failure with no clue which side died.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        d2linux::PrintVideoVerdict();
        return 1;
    }
    const bool audio_subsystem = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!audio_subsystem) {
        std::printf("d2linux: SDL audio subsystem unavailable (%s) - continuing without\n",
                    SDL_GetError());
    }

    SDL_Window* window = nullptr;
    if (!d2linux::Video_Init(opts.window_w, opts.window_h, &window)) {
        SDL_Quit();
        return 1;
    }
    // Audio is non-fatal for the scaffold: a silent host is still a host.
    if (audio_subsystem && !d2linux::Audio_Init()) {
        std::printf("d2linux: audio unavailable (%s) - continuing without\n", SDL_GetError());
    }

    int frame = 0;
    bool quit = false;
    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                quit = true;
            }
        }
        if (quit) {
            break;
        }
        d2linux::Video_Frame(frame++);
        if (opts.smoke && frame >= opts.smoke_frames) {
            break;
        }
        SDL_Delay(16);  // ~60 fps cap; M2 replaces this with the game's frame pacing
    }

    if (opts.smoke) {
        // [d2linux M2] the user32 contract rides the same headless run —
        // the pump the boot chain will drive is verified HERE, in CI, on
        // dummy drivers, before any game TU exists to drive it.
        const char* user32_fail = d2linux::RunUser32SelfTest();
        if (user32_fail) {
            std::printf("d2linux user32 self-test FAILED: %s\n", user32_fail);
            d2linux::Audio_Shutdown();
            d2linux::Video_Shutdown();
            SDL_Quit();
            return 1;
        }
        std::printf("d2linux user32 self-test OK (class/window/pump/cursor/destroy/quit)\n");
        // [d2linux M2 round 2] the DirectDraw contract: the game's init
        // ladder + scene-loop semantics, verified headless before a game TU
        // drives them for real. Flip's present rides the M0 window here.
        const char* ddraw_fail = d2linux::RunDdrawSelfTest();
        if (ddraw_fail) {
            std::printf("d2linux ddraw self-test FAILED: %s\n", ddraw_fail);
            d2linux::Audio_Shutdown();
            d2linux::Video_Shutdown();
            SDL_Quit();
            return 1;
        }
        std::printf("d2linux ddraw self-test OK (create/coop/mode/flipchain/palette/lock/blt/flip)\n");
        // The CI-verdict line: tools/check-build-linux.sh greps for this.
        std::printf("d2linux smoke OK (%d frames, dummy video/audio)\n", frame);
    }

    d2linux::Audio_Shutdown();
    d2linux::Video_Shutdown();
    SDL_Quit();
    return 0;
}
