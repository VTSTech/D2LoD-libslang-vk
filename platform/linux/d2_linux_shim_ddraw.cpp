// platform/linux/d2_linux_shim_ddraw.cpp — [d2linux M2 round 2] the
// DirectDraw COM shim: the game's 8-bit palettized surface model carried onto
// the d2linux INDEX8 presentation path. First real game pixels.
//
// The boot chain's renderer (D2DDraw::Renderer::DirectDraw) drives DirectDraw
// through one exact ladder (its InternalOpenWindow / BeginScene / EndScene2
// are the reference call sites; the flag values below are the game's own):
//
//   DirectDrawCreate(NULL, &dd, NULL)
//   dd->SetCooperativeLevel(hwnd, 0x11)        fullscreen-exclusive
//   dd->SetDisplayMode(640, 480, 8)            (3-arg DX1 ABI — see header)
//   dd->CreateSurface(primary, 2 back buffers) → falls back to 1, then to a
//                                              single-buffered primary
//   primary->GetAttachedSurface(BACKBUFFER)    the render-target back buffer
//   dd->CreateSurface(offscreen, sysmem)       the compose surface, same size
//   primary->SetPalette(pal)                   DDPCAPS_8BIT, 256 PALETTEENTRY
//   BeginScene:  offscreen->Lock(0x21) → game writes indices → EndScene1
//                offscreen->Unlock
//   EndScene2:   back->BltFast(0,0, offscreen, full, 0x10) →
//                primary->Flip(NULL, 1) — THE present — and a deferred
//                palette->SetEntries(0, 1, 255, entries+1) on palette change
//   cutscenes:   primary->Blt(NULL..0x400) DDBLT_COLORFILL black before Bink
//
// This file implements exactly those semantics on host memory: every surface
// owns a system-memory 8-bit index buffer (pitch == width — the game stores
// Lock's lPitch into what its BltFast rect then uses as a width, so any
// padding would leak into blit geometry), the palette object is a 256-entry
// table mirrored into the SDL staging palette on SetPalette/SetEntries, and
// Flip copies the back buffer into the host's INDEX8 staging surface and
// presents. The front-buffer colorfill and the flip-chain rotation the game
// cannot observe through its own calls are modeled, not emulated: there is
// one back buffer regardless of the requested count (every frame is fully
// recomposed by the game's ClearScreen + full-rect BltFast, so rotation would
// be invisible), and BltFast to a chain-less primary presents immediately
// (the single-buffered fallback path — real DD shows the blit at once).
//
// Calls outside the ladder return DD_OK-noops or a one-shot-diagnosed
// DDERR_UNSUPPORTED — never silent lies (the -m32-gate philosophy: failures
// carry their own evidence).
//
// Threading: the game drives the whole ladder from its main thread; SDL
// presentation happens on the caller of Flip. No locks (documented model).

#include <cstdio>
#include <cstring>
#include <vector>

#include "platform/linux/d2_linux_compat.h"
#include "platform/linux/d2_linux_api.h"
#include "platform/linux/d2_linux_video.h"
#include "platform/linux/d2_linux_shim_ddraw.h"

// [d2linux M2 round 2] internal cross-shim hook (link-level, NOT game API):
// SetCooperativeLevel's hwnd must be a window this process actually created
// (the user32 shim's registry is the truth). Defined in
// d2_linux_shim_user32.cpp.
extern "C" bool d2linux_user32_window_registered(HWND hwnd);

namespace {

// ---------------------------------------------------------------------------
// The vtables (definitions live at the bottom of each section; forward-declared
// here because CreateSurface/CreatePalette hand the static tables to objects).
// ---------------------------------------------------------------------------
extern const D2DDVtbl         g_dd_vtable;
extern const D2DDSurfaceVtbl  g_surface_vtable;
extern const D2DDPaletteVtbl  g_palette_vtable;

// ---------------------------------------------------------------------------
// Full objects: the COM view (vtable pointer, offset 0 — pointer
// interconvertible with the D2DD* header structs) + shim state. The view
// members point at CONST vtables (real COM tables live in read-only memory);
// the public header structs type the same address non-const, which only
// affects compile-time checks inside the caller's TU.
// ---------------------------------------------------------------------------
enum class SurfKind { kPrimary, kBack, kOffscreen };

struct D2DDObjFull;
struct D2DDSurfaceObjFull;
struct D2DDPaletteObjFull;

struct D2DDObjFull {
    const D2DDVtbl* view;
    ULONG           refs;
    HWND            hwnd;       // SetCooperativeLevel's window
    uint32_t        coop;       // DDSCL_* recorded
    int             mode_w, mode_h, mode_bpp;   // SetDisplayMode state
    D2DDSurfaceObjFull* primary;
};

struct D2DDSurfaceObjFull {
    const D2DDSurfaceVtbl* view;
    ULONG           refs;
    D2DDObjFull*    dd;
    SurfKind        kind;
    int             w, h, pitch;    // pitch == w (the no-padding invariant)
    std::vector<uint8_t> px;        // 8-bit palette indices
    D2DDPaletteObjFull* pal;        // attached palette (primary)
    bool            has_ck;
    uint32_t        ck_lo;          // src color key (8-bit: low byte matters)
    D2DDSurfaceObjFull* back;       // primary's back buffer
};

struct D2DDPaletteObjFull {
    const D2DDPaletteVtbl* view;
    ULONG           refs;
    uint8_t         rgb[256][4];    // PALETTEENTRY copy (r,g,b,peFlags)
    D2DDSurfaceObjFull* attached_to;  // the primary this palette sits on
};

// ---------------------------------------------------------------------------
// Diagnostics. Init-ladder calls print (they fire once — the d2boot-style
// evidence lines); per-frame paths NEVER print (a flip-per-frame shim would
// spam). "Unsupported" stubs print once each — one line says what the game
// (or a future caller) reached for, silence would say nothing.
// ---------------------------------------------------------------------------
void DdLog(const char* what) {
    std::printf("d2linux ddraw: %s\n", what);
}

void DdUnsupported(const char* what) {
    static const char* g_reported[16];
    static int g_reported_n = 0;
    for (int i = 0; i < g_reported_n; ++i) {
        if (g_reported[i] == what) return;
    }
    if (g_reported_n < 16) g_reported[g_reported_n++] = what;
    std::printf("d2linux ddraw: unsupported call (one-shot): %s\n", what);
}

// The mode table: the game's own video modes (D2DDraw's mode tables: mode 0
// 640x480, mode 1/2 800x600, Vanilla+ mode 3 1344x700, mode 4 1280x720),
// all 8bpp — EnumDisplayModes enumerates these; anything else at
// SetDisplayMode is rejected with a loud diagnostic (the port presents
// 8-bit palettized; higher depths are the Direct3D renderer's business).
struct DdMode { int w, h; };
const DdMode kModes[] = {{640, 480}, {800, 600}, {1344, 700}, {1280, 720}};
const int kModeCount = (int)(sizeof(kModes) / sizeof(kModes[0]));

bool ModeIsKnown(int w, int h, int bpp) {
    if (bpp != 8) return false;
    for (int i = 0; i < kModeCount; ++i) {
        if (kModes[i].w == w && kModes[i].h == h) return true;
    }
    return false;
}

void FillPixelFormat(D2DDPixelFormat* pf) {
    pf->dwSize          = sizeof(D2DDPixelFormat);
    pf->dwFlags         = D2DD_DDPF_RGB | D2DD_DDPF_PALETTEINDEXED8;
    pf->dwFourCC        = 0;
    pf->dwRGBBitCount   = 8;
    pf->dwRBitMask      = 0;
    pf->dwGBitMask      = 0;
    pf->dwBBitMask      = 0;
    pf->dwRGBAlphaBitMask = 0;
}

void FillSurfaceDesc(const D2DDSurfaceObjFull* s, D2DDSurfDesc* d) {
    d->dwFlags = D2DD_DDSDD_CAPS | D2DD_DDSD_HEIGHT | D2DD_DDSD_WIDTH |
                 D2DD_DDSD_PITCH | D2DD_DDSD_PIXELFORMAT;
    d->dwHeight = (uint32_t)s->h;
    d->dwWidth  = (uint32_t)s->w;
    d->lPitch   = s->pitch;
    FillPixelFormat(&d->ddpfPixelFormat);
    switch (s->kind) {
    case SurfKind::kPrimary:
        d->ddsCaps = D2DD_DDSCAPS_PRIMARYSURFACE | D2DD_DDSCAPS_FLIP |
                     D2DD_DDSCAPS_COMPLEX | D2DD_DDSCAPS_VIDEOMEMORY;
        break;
    case SurfKind::kBack:
        d->ddsCaps = D2DD_DDSCAPS_BACKBUFFER | D2DD_DDSCAPS_FLIP |
                     D2DD_DDSCAPS_COMPLEX | D2DD_DDSCAPS_VIDEOMEMORY;
        break;
    default:
        d->ddsCaps = D2DD_DDSCAPS_SYSTEMMEMORY | D2DD_DDSCAPS_OFFSCREENPLAIN;
        break;
    }
}

// ---------------------------------------------------------------------------
// Rect-blit core: 8-bit index copy, both sides clipped, optional src key.
// Pitch == width on every surface, but the loops stay pitch-general.
// ---------------------------------------------------------------------------
void BlitIndexed(D2DDSurfaceObjFull* dst, int dx, int dy,
                 const D2DDSurfaceObjFull* src, int sx, int sy,
                 int w, int h, bool keyed) {
    if (dx < 0) { int c = -dx; sx += c; dx = 0; w -= c; }
    if (dy < 0) { int c = -dy; sy += c; dy = 0; h -= c; }
    if (sx < 0) { int c = -sx; dx += c; sx = 0; w -= c; }
    if (sy < 0) { int c = -sy; dy += c; sy = 0; h -= c; }
    if (dx + w > dst->w) w = dst->w - dx;
    if (dy + h > dst->h) h = dst->h - dy;
    if (sx + w > src->w) w = src->w - sx;
    if (sy + h > src->h) h = src->h - sy;
    if (w <= 0 || h <= 0) return;

    const uint8_t ck = (uint8_t)(src->ck_lo & 0xFFu);
    for (int row = 0; row < h; ++row) {
        const uint8_t* s = src->px.data() + (size_t)(sy + row) * src->pitch + sx;
        uint8_t* d = dst->px.data() + (size_t)(dy + row) * dst->pitch + dx;
        if (keyed) {
            for (int c = 0; c < w; ++c) {
                if (s[c] != ck) d[c] = s[c];
            }
        } else {
            std::memcpy(d, s, (size_t)w);
        }
    }
}

void FillIndexed(D2DDSurfaceObjFull* dst, int x, int y, int w, int h,
                 uint8_t index) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dst->w) w = dst->w - x;
    if (y + h > dst->h) h = dst->h - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; ++row) {
        std::memset(dst->px.data() + (size_t)(y + row) * dst->pitch + x,
                    index, (size_t)w);
    }
}

// The present: back buffer (or a chain-less primary) → INDEX8 staging →
// screen. One-shot diagnose when nothing is hosting (the game creates its
// window before DirectDraw init, so this should never fire).
void PresentSurface(D2DDSurfaceObjFull* s) {
    if (!d2linux::Video_PresentIndexed(s->px.data(), s->pitch, s->w, s->h)) {
        DdUnsupported("present with no attached window (Video_PresentIndexed)");
    }
}

// The palette mirror: a palette ATTACHED to the hosting primary feeds the
// staging palette immediately (SetPalette) and after every mutation
// (SetEntries — EndScene2's deferred-palette path). Unattached palettes are
// pure tables until SetPalette.
void MirrorPalette(const D2DDPaletteObjFull* pal) {
    if (pal && pal->attached_to) {
        d2linux::Video_SetPalette(pal->rgb);
    }
}

// ---------------------------------------------------------------------------
// IDirectDraw (D2DDObj). Slot order = the game's reconstruction 0x00..0x5C.
// ---------------------------------------------------------------------------
ULONG __stdcall Dd_AddRef(D2DDObj* self) {
    D2DDObjFull* dd = (D2DDObjFull*)self;
    return ++dd->refs;
}

ULONG __stdcall Dd_Release(D2DDObj* self) {
    D2DDObjFull* dd = (D2DDObjFull*)self;
    if (--dd->refs) return dd->refs;
    // Refcount zero: the object goes. Its surfaces hold no back-reference to
    // keep alive (the game's release ladder drops every one of them), but a
    // surface outliving its DirectDraw would dangle — the game never does
    // that; the self-test pins the ladder that makes it true.
    delete dd;
    return 0;
}

HRESULT __stdcall Dd_QueryInterface(D2DDObj*, void*, void**) {
    // The game never QueryInterfaces (no call site). Any IID: no.
    DdUnsupported("QueryInterface (no COM identity queries are modeled)");
    return D2DD_E_NOINTERFACE;
}

HRESULT __stdcall Dd_Compact(D2DDObj*) { return DD_OK; }

HRESULT __stdcall Dd_CreateClipper(D2DDObj*, uint32_t, void**, void*) {
    DdUnsupported("CreateClipper (no clipper model; the DDraw path never clips)");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Dd_CreatePalette(D2DDObj*, uint32_t dwFlags, void* lpColorTable,
                                   D2DDPaletteObj** lplpDDPalette, void*) {
    if (!lplpDDPalette) return D2DD_E_INVALIDARG;
    D2DDPaletteObjFull* pal = new D2DDPaletteObjFull();
    pal->refs = 1;
    pal->attached_to = nullptr;
    if (lpColorTable) {
        std::memcpy(pal->rgb, lpColorTable, sizeof(pal->rgb));
    } else {
        std::memset(pal->rgb, 0, sizeof(pal->rgb));
    }
    pal->view = &g_palette_vtable;
    *lplpDDPalette = (D2DDPaletteObj*)pal;
    static_cast<void>(dwFlags);  // DDPCAPS_8BIT assumed (the game's value)
    DdLog("CreatePalette (8bpp, 256 entries)");
    return DD_OK;
}

HRESULT __stdcall Dd_CreateSurface(D2DDObj* self, void* lpDDSurfaceDesc,
                                   void** lplpDDSurface, void*) {
    D2DDObjFull* dd = (D2DDObjFull*)self;
    if (!lpDDSurfaceDesc || !lplpDDSurface) return D2DD_E_INVALIDARG;
    const D2DDSurfDesc* d = (const D2DDSurfDesc*)lpDDSurfaceDesc;
    if (d->dwSize < 0x68) return D2DD_E_INVALIDARG;  // through ddsCaps
    const uint32_t caps = d->ddsCaps;

    D2DDSurfaceObjFull* s = new D2DDSurfaceObjFull();
    s->view = nullptr;
    s->refs = 1;
    s->dd = dd;
    s->pal = nullptr;
    s->has_ck = false;
    s->ck_lo = 0;
    s->back = nullptr;

    if (caps & D2DD_DDSCAPS_PRIMARYSURFACE) {
        // The flip chain. One back buffer regardless of the requested count —
        // see the header comment (the game fully recomposes every frame).
        s->kind = SurfKind::kPrimary;
        s->w = dd->mode_w;
        s->h = dd->mode_h;
        D2DDSurfaceObjFull* back = new D2DDSurfaceObjFull();
        back->view = nullptr;
        back->refs = 1;
        back->dd = dd;
        back->kind = SurfKind::kBack;
        back->w = s->w;
        back->h = s->h;
        back->pal = nullptr;
        back->has_ck = false;
        back->ck_lo = 0;
        back->back = nullptr;
        back->pitch = back->w;
        back->px.assign((size_t)back->w * back->h, 0);
        back->view = &g_surface_vtable;
        s->pitch = s->w;
        s->px.assign((size_t)s->w * s->h, 0);
        s->back = back;
        s->view = &g_surface_vtable;
        if (dd->primary) {   // double-create: the game's ladder never does
            delete back;
            delete s;
            return D2DD_E_FAIL;
        }
        dd->primary = s;
        std::printf("d2linux ddraw: CreateSurface primary %dx%d (+1 back buffer, "
                    "requested count %u)\n", s->w, s->h, d->dwBackBufferCount);
    } else {
        if (!(d->dwFlags & D2DD_DDSD_HEIGHT) || !(d->dwFlags & D2DD_DDSD_WIDTH) ||
            d->dwWidth == 0 || d->dwHeight == 0 ||
            d->dwWidth > 8192 || d->dwHeight > 8192) {
            delete s;
            return D2DD_E_INVALIDARG;
        }
        s->kind = SurfKind::kOffscreen;
        s->w = (int)d->dwWidth;
        s->h = (int)d->dwHeight;
        s->pitch = s->w;
        s->px.assign((size_t)s->w * s->h, 0);
        s->view = &g_surface_vtable;
        std::printf("d2linux ddraw: CreateSurface offscreen %dx%d (system memory)\n",
                    s->w, s->h);
    }
    *lplpDDSurface = (void*)s;
    return DD_OK;
}

HRESULT __stdcall Dd_DuplicateSurface(D2DDObj*, D2DDSurfaceObj*, D2DDSurfaceObj**) {
    DdUnsupported("DuplicateSurface");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Dd_EnumDisplayModes(D2DDObj*, uint32_t, void*, void* lpContext,
                                      void* lpEnumModesCallback) {
    typedef int (__stdcall *Cb)(D2DDSurfDesc*, void*);
    Cb cb = (Cb)lpEnumModesCallback;
    if (!cb) return D2DD_E_INVALIDARG;
    D2DDSurfDesc d;
    for (int i = 0; i < kModeCount; ++i) {
        std::memset(&d, 0, sizeof(d));
        d.dwSize = sizeof(D2DDSurfDesc);
        d.dwWidth = (uint32_t)kModes[i].w;
        d.dwHeight = (uint32_t)kModes[i].h;
        FillPixelFormat(&d.ddpfPixelFormat);
        if (!cb(&d, lpContext)) break;
    }
    return DD_OK;
}

HRESULT __stdcall Dd_EnumSurfaces(D2DDObj*, uint32_t, void*, void*, void*) {
    return DD_OK;  // no public surfaces to enumerate
}

HRESULT __stdcall Dd_FlipToGDISurface(D2DDObj*) { return DD_OK; }

HRESULT __stdcall Dd_GetCaps(D2DDObj*, void* lpDDDriverCaps, void* lpDDHELCaps) {
    // Capability bits deliberately zero: the system-memory model is fully
    // synchronous, every capability the game tests for is granted by
    // returning DD_OK from the call itself.
    if (lpDDDriverCaps) {
        uint32_t size = *(uint32_t*)lpDDDriverCaps;
        if (size > 256) size = 256;
        std::memset(lpDDDriverCaps, 0, size);
    }
    if (lpDDHELCaps) {
        uint32_t size = *(uint32_t*)lpDDHELCaps;
        if (size > 256) size = 256;
        std::memset(lpDDHELCaps, 0, size);
    }
    return DD_OK;
}

HRESULT __stdcall Dd_GetDisplayMode(D2DDObj* self, D2DDSurfDesc* d) {
    D2DDObjFull* dd = (D2DDObjFull*)self;
    if (!d) return D2DD_E_INVALIDARG;
    std::memset(d, 0, sizeof(*d));
    d->dwSize = sizeof(D2DDSurfDesc);
    d->dwWidth = (uint32_t)dd->mode_w;
    d->dwHeight = (uint32_t)dd->mode_h;
    d->lPitch = dd->mode_w;
    FillPixelFormat(&d->ddpfPixelFormat);
    return DD_OK;
}

HRESULT __stdcall Dd_GetFourCCCodes(D2DDObj*, uint32_t* lpNumCodes, uint32_t*) {
    if (lpNumCodes) *lpNumCodes = 0;
    return DD_OK;
}

HRESULT __stdcall Dd_GetGDISurface(D2DDObj*, D2DDSurfaceObj**) {
    DdUnsupported("GetGDISurface");
    return D2DD_DDERR_NOTFOUND;
}

HRESULT __stdcall Dd_GetMonitorFrequency(D2DDObj*, uint32_t* lpdwFrequency) {
    if (lpdwFrequency) *lpdwFrequency = 60;
    return DD_OK;
}

HRESULT __stdcall Dd_GetScanLine(D2DDObj*, uint32_t* lpdwScanLine) {
    if (lpdwScanLine) *lpdwScanLine = 0;
    return DD_OK;
}

HRESULT __stdcall Dd_GetVerticalBlankStatus(D2DDObj*, int* lpIsInVB) {
    if (lpIsInVB) *lpIsInVB = 1;
    return DD_OK;
}

HRESULT __stdcall Dd_Initialize(D2DDObj*, void*) {
    return D2DD_DDERR_ALREADYINITIALIZED;
}

HRESULT __stdcall Dd_RestoreDisplayMode(D2DDObj*) { return DD_OK; }

HRESULT __stdcall Dd_SetCooperativeLevel(D2DDObj* self, HWND hWnd, uint32_t dwFlags) {
    D2DDObjFull* dd = (D2DDObjFull*)self;
    if (!hWnd || !d2linux_user32_window_registered(hWnd)) {
        std::printf("d2linux ddraw: SetCooperativeLevel refused - hwnd is not a "
                    "window this process created\n");
        return D2DD_E_INVALIDARG;
    }
    dd->hwnd = hWnd;
    dd->coop = dwFlags;
    std::printf("d2linux ddraw: SetCooperativeLevel hwnd=%p flags=0x%x%s\n",
                (void*)hWnd, dwFlags,
                (dwFlags & D2DD_DDSCL_EXCLUSIVE) ? " (fullscreen-exclusive)" : "");
    return DD_OK;
}

HRESULT __stdcall Dd_SetDisplayMode(D2DDObj* self, uint32_t dwWidth,
                                    uint32_t dwHeight, uint32_t dwBPP) {
    D2DDObjFull* dd = (D2DDObjFull*)self;
    if (!ModeIsKnown((int)dwWidth, (int)dwHeight, (int)dwBPP)) {
        std::printf("d2linux ddraw: SetDisplayMode %ux%ux%u refused - the port "
                    "presents 8-bit palettized modes only (640x480/800x600/"
                    "1344x700/1280x720); other depths are the Direct3D "
                    "renderer's business\n", dwWidth, dwHeight, dwBPP);
        return D2DD_E_INVALIDARG;
    }
    if (dd->primary && ((int)dwWidth != dd->mode_w || (int)dwHeight != dd->mode_h)) {
        DdUnsupported("SetDisplayMode while surfaces exist (mode change; the "
                      "game's ResizeWindow ladder recreates instead)");
    }
    dd->mode_w = (int)dwWidth;
    dd->mode_h = (int)dwHeight;
    dd->mode_bpp = (int)dwBPP;
    std::printf("d2linux ddraw: SetDisplayMode %ux%ux%u\n", dwWidth, dwHeight, dwBPP);
    return DD_OK;
}

HRESULT __stdcall Dd_WaitForVerticalBlank(D2DDObj*, uint32_t) {
    return DD_OK;  // the present path vsyncs; the game's DDFLIP_WAIT says the rest
}

HRESULT __stdcall Dd_GetAvailableVidMem(D2DDObj*, void*, uint32_t* lpdwTotal,
                                        uint32_t* lpdwFree) {
    // Cosmetic numbers: every surface lands in system memory here.
    if (lpdwTotal) *lpdwTotal = 64u * 1024u * 1024u;
    if (lpdwFree) *lpdwFree = 60u * 1024u * 1024u;
    return DD_OK;
}

const D2DDVtbl g_dd_vtable = {
    Dd_QueryInterface,
    Dd_AddRef,
    Dd_Release,
    Dd_Compact,
    Dd_CreateClipper,
    Dd_CreatePalette,
    Dd_CreateSurface,
    Dd_DuplicateSurface,
    Dd_EnumDisplayModes,
    Dd_EnumSurfaces,
    Dd_FlipToGDISurface,
    Dd_GetCaps,
    Dd_GetDisplayMode,
    Dd_GetFourCCCodes,
    Dd_GetGDISurface,
    Dd_GetMonitorFrequency,
    Dd_GetScanLine,
    Dd_GetVerticalBlankStatus,
    Dd_Initialize,
    Dd_RestoreDisplayMode,
    Dd_SetCooperativeLevel,
    Dd_SetDisplayMode,
    Dd_WaitForVerticalBlank,
    Dd_GetAvailableVidMem,
};

// ---------------------------------------------------------------------------
// IDirectDrawPalette (D2DDPaletteObj).
// ---------------------------------------------------------------------------
ULONG __stdcall Pal_AddRef(D2DDPaletteObj* self) {
    D2DDPaletteObjFull* pal = (D2DDPaletteObjFull*)self;
    return ++pal->refs;
}

ULONG __stdcall Pal_Release(D2DDPaletteObj* self) {
    D2DDPaletteObjFull* pal = (D2DDPaletteObjFull*)self;
    if (--pal->refs) return pal->refs;
    if (pal->attached_to) pal->attached_to->pal = nullptr;
    delete pal;
    return 0;
}

HRESULT __stdcall Pal_QueryInterface(D2DDPaletteObj*, void*, void**) {
    DdUnsupported("palette QueryInterface");
    return D2DD_E_NOINTERFACE;
}

HRESULT __stdcall Pal_GetCaps(D2DDPaletteObj*, uint32_t* lpdwCaps) {
    if (lpdwCaps) *lpdwCaps = D2DD_DDPCAPS_8BIT;
    return DD_OK;
}

HRESULT __stdcall Pal_GetEntries(D2DDPaletteObj* self, uint32_t, uint32_t dwBase,
                                 uint32_t dwNumEntries, void* lpEntries) {
    D2DDPaletteObjFull* pal = (D2DDPaletteObjFull*)self;
    if (!lpEntries) return D2DD_E_INVALIDARG;
    if (dwBase >= 256) return D2DD_E_INVALIDARG;
    uint32_t n = dwNumEntries;
    if (dwBase + n > 256) n = 256 - dwBase;  // clamp, never overrun
    std::memcpy(lpEntries, pal->rgb + dwBase, (size_t)n * 4);
    return DD_OK;
}

HRESULT __stdcall Pal_Initialize(D2DDPaletteObj*, D2DDObj*, uint32_t, void*) {
    return D2DD_DDERR_ALREADYINITIALIZED;
}

HRESULT __stdcall Pal_SetEntries(D2DDPaletteObj* self, uint32_t, uint32_t dwStartingEntry,
                                 uint32_t dwCount, void* lpEntries) {
    D2DDPaletteObjFull* pal = (D2DDPaletteObjFull*)self;
    if (!lpEntries) return D2DD_E_INVALIDARG;
    if (dwStartingEntry >= 256) return D2DD_E_INVALIDARG;
    uint32_t n = dwCount;
    if (dwStartingEntry + n > 256) n = 256 - dwStartingEntry;
    if (n) {
        std::memcpy(pal->rgb + dwStartingEntry, lpEntries, (size_t)n * 4);
        MirrorPalette(pal);  // EndScene2's deferred-palette path lands here
    }
    return DD_OK;
}

const D2DDPaletteVtbl g_palette_vtable = {
    Pal_QueryInterface,
    Pal_AddRef,
    Pal_Release,
    Pal_GetCaps,
    Pal_GetEntries,
    Pal_Initialize,
    Pal_SetEntries,
};

// ---------------------------------------------------------------------------
// IDirectDrawSurface1 (D2DDSurfaceObj).
// ---------------------------------------------------------------------------
ULONG __stdcall Surf_AddRef(D2DDSurfaceObj* self) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    return ++s->refs;
}

ULONG __stdcall Surf_Release(D2DDSurfaceObj* self) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    if (--s->refs) return s->refs;
    if (s->pal) s->pal->attached_to = nullptr;
    if (s->dd && s->dd->primary == s) s->dd->primary = nullptr;
    if (s->back) {
        // Real DD flip-chain ownership: the chain holds a reference to its
        // back buffer, so the last one dies WITH the primary — an attached
        // back buffer can never be released to zero by the app alone. Drop
        // the chain's reference here (recursion depth is exactly 1).
        D2DDSurfaceObjFull* back = s->back;
        s->back = nullptr;
        back->dd = nullptr;   // the owning chain is going away
        Surf_Release((D2DDSurfaceObj*)back);
    }
    delete s;
    return 0;
}

HRESULT __stdcall Surf_QueryInterface(D2DDSurfaceObj*, void*, void**) {
    DdUnsupported("surface QueryInterface");
    return D2DD_E_NOINTERFACE;
}

HRESULT __stdcall Surf_AddAttachedSurface(D2DDSurfaceObj*, D2DDSurfaceObj*) {
    DdUnsupported("AddAttachedSurface");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_AddOverlayDirtyRect(D2DDSurfaceObj*, void*) {
    DdUnsupported("AddOverlayDirtyRect");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_Blt(D2DDSurfaceObj* self, void* lpDestRect,
                           D2DDSurfaceObj* lpDDSrcSurface, void* lpSrcRect,
                           uint32_t dwFlags, void* lpDDBltFx) {
    D2DDSurfaceObjFull* dst = (D2DDSurfaceObjFull*)self;
    const RECT* rdst = (const RECT*)lpDestRect;
    const RECT* rsrc = (const RECT*)lpSrcRect;

    if (dwFlags & D2DD_DDBLT_COLORFILL) {
        const D2DDBltFx* fx = (const D2DDBltFx*)lpDDBltFx;
        if (!fx || fx->dwSize < 0x50) return D2DD_E_INVALIDARG;
        int x = 0, y = 0, w = dst->w, h = dst->h;
        if (rdst) {
            x = (int)rdst->left; y = (int)rdst->top;
            w = (int)(rdst->right - rdst->left);
            h = (int)(rdst->bottom - rdst->top);
        }
        FillIndexed(dst, x, y, w, h, (uint8_t)(fx->dwFillColor & 0xFFu));
        return DD_OK;
    }

    if (!lpDDSrcSurface) return D2DD_E_INVALIDARG;
    D2DDSurfaceObjFull* src = (D2DDSurfaceObjFull*)lpDDSrcSurface;
    int sx = 0, sy = 0, w = src->w, h = src->h;
    if (rsrc) {
        sx = (int)rsrc->left; sy = (int)rsrc->top;
        w = (int)(rsrc->right - rsrc->left);
        h = (int)(rsrc->bottom - rsrc->top);
    }
    int dx = 0, dy = 0;
    if (rdst) { dx = (int)rdst->left; dy = (int)rdst->top; }
    bool keyed = (dwFlags & 0x00008000u) != 0;  // DDBLT_KEYSRC
    if (keyed && !src->has_ck) return D2DD_E_INVALIDARG;
    BlitIndexed(dst, dx, dy, src, sx, sy, w, h, keyed);
    if (dwFlags & 0x00000040u /*DDBLT_ASYNC*/) DdUnsupported("Blt DDBLT_ASYNC (run synchronous)");
    return DD_OK;
}

HRESULT __stdcall Surf_BltBatch(D2DDSurfaceObj*, void*, uint32_t, uint32_t) {
    DdUnsupported("BltBatch");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_BltFast(D2DDSurfaceObj* self, uint32_t dwX, uint32_t dwY,
                               D2DDSurfaceObj* lpDDSrcSurface, void* lpSrcRect,
                               uint32_t dwTrans) {
    D2DDSurfaceObjFull* dst = (D2DDSurfaceObjFull*)self;
    if (!lpDDSrcSurface) return D2DD_E_INVALIDARG;
    D2DDSurfaceObjFull* src = (D2DDSurfaceObjFull*)lpDDSrcSurface;
    if (src == dst) return D2DD_E_INVALIDARG;  // no same-surface blits in the model

    const RECT* r = (const RECT*)lpSrcRect;
    int sx = 0, sy = 0, w = src->w, h = src->h;
    if (r) {
        sx = (int)r->left; sy = (int)r->top;
        w = (int)(r->right - r->left);
        h = (int)(r->bottom - r->top);
    }
    bool keyed = (dwTrans & D2DD_DDBLTFAST_SRCCOLORKEY) != 0;
    if (keyed && !src->has_ck) return D2DD_E_INVALIDARG;
    BlitIndexed(dst, (int)dwX, (int)dwY, src, sx, sy, w, h, keyed);

    // Single-buffered fallback: a blit to a chain-less primary IS the frame
    // (EndScene2's gnDDrawFlipState==0 path — real DD shows it immediately).
    if (dst->kind == SurfKind::kPrimary && !dst->back) {
        PresentSurface(dst);
    }
    return DD_OK;
}

HRESULT __stdcall Surf_DeleteAttachedSurface(D2DDSurfaceObj*, uint32_t, D2DDSurfaceObj*) {
    DdUnsupported("DeleteAttachedSurface");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_EnumAttachedSurfaces(D2DDSurfaceObj*, void*, void*) {
    return DD_OK;  // the single back buffer is reachable via GetAttachedSurface
}

HRESULT __stdcall Surf_EnumOverlayZOrders(D2DDSurfaceObj*, uint32_t, void*, void*) {
    return DD_OK;
}

HRESULT __stdcall Surf_Flip(D2DDSurfaceObj* self, D2DDSurfaceObj* lpOverride,
                            uint32_t dwFlags) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    D2DDSurfaceObjFull* src = (D2DDSurfaceObjFull*)lpOverride;
    if (!src) src = s->back ? s->back : s;  // chain-less primary flips in place
    PresentSurface(src);
    static_cast<void>(dwFlags);  // DDFLIP_WAIT: we are synchronous, always "done"
    return DD_OK;
}

HRESULT __stdcall Surf_GetAttachedSurface(D2DDSurfaceObj* self, D2DDCaps* lpDDSCaps,
                                          D2DDSurfaceObj** lplpDDAttachedSurface) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    if (!lpDDSCaps || !lplpDDAttachedSurface) return D2DD_E_INVALIDARG;
    if ((lpDDSCaps->dwCaps & D2DD_DDSCAPS_BACKBUFFER) && s->back) {
        ++s->back->refs;  // GetAttachedSurface hands out a reference
        *lplpDDAttachedSurface = (D2DDSurfaceObj*)s->back;
        return DD_OK;
    }
    return D2DD_DDERR_NOTFOUND;
}

HRESULT __stdcall Surf_GetBltStatus(D2DDSurfaceObj*, uint32_t) { return DD_OK; }

HRESULT __stdcall Surf_GetCaps(D2DDSurfaceObj* self, D2DDCaps* lpDDSCaps) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    if (!lpDDSCaps) return D2DD_E_INVALIDARG;
    D2DDSurfDesc d;
    FillSurfaceDesc(s, &d);
    lpDDSCaps->dwCaps = d.ddsCaps;
    return DD_OK;
}

HRESULT __stdcall Surf_GetClipper(D2DDSurfaceObj*, void**) {
    DdUnsupported("GetClipper");
    return D2DD_DDERR_NOTFOUND;
}

HRESULT __stdcall Surf_GetColorKey(D2DDSurfaceObj* self, uint32_t, void* lpDDColorKey) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    if (!lpDDColorKey) return D2DD_E_INVALIDARG;
    uint32_t* ck = (uint32_t*)lpDDColorKey;
    ck[0] = s->ck_lo;
    ck[1] = s->ck_lo;
    return s->has_ck ? DD_OK : D2DD_DDERR_NOTFOUND;
}

HRESULT __stdcall Surf_GetDC(D2DDSurfaceObj*, HDC*) {
    DdUnsupported("GetDC (no GDI DC on the shim path)");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_GetFlipStatus(D2DDSurfaceObj*, uint32_t) { return DD_OK; }

HRESULT __stdcall Surf_GetOverlayPosition(D2DDSurfaceObj*, int*, int*) {
    DdUnsupported("GetOverlayPosition");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_GetPalette(D2DDSurfaceObj* self, D2DDPaletteObj** lplpDDPalette) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    if (!lplpDDPalette) return D2DD_E_INVALIDARG;
    if (!s->pal) return D2DD_DDERR_NOTFOUND;
    *lplpDDPalette = (D2DDPaletteObj*)s->pal;  // no AddRef: the game never calls this
    return DD_OK;
}

HRESULT __stdcall Surf_GetPixelFormat(D2DDSurfaceObj*, void* lpDDPixelFormat) {
    if (!lpDDPixelFormat) return D2DD_E_INVALIDARG;
    D2DDPixelFormat* out = (D2DDPixelFormat*)lpDDPixelFormat;
    uint32_t n = out->dwSize;  // the caller's declared size, read BEFORE the fill
    if (n == 0 || n > sizeof(D2DDPixelFormat)) n = sizeof(D2DDPixelFormat);
    D2DDPixelFormat pf;
    FillPixelFormat(&pf);
    std::memcpy(out, &pf, n);
    return DD_OK;
}

HRESULT __stdcall Surf_GetSurfaceDesc(D2DDSurfaceObj* self, D2DDSurfDesc* lpDDSurfaceDesc) {
    if (!lpDDSurfaceDesc) return D2DD_E_INVALIDARG;
    FillSurfaceDesc((D2DDSurfaceObjFull*)self, lpDDSurfaceDesc);
    return DD_OK;
}

HRESULT __stdcall Surf_Initialize(D2DDSurfaceObj*, D2DDObj*, D2DDSurfDesc*) {
    return D2DD_DDERR_ALREADYINITIALIZED;
}

HRESULT __stdcall Surf_IsLost(D2DDSurfaceObj*) { return DD_OK; }

HRESULT __stdcall Surf_Lock(D2DDSurfaceObj* self, void*, D2DDSurfDesc* lpDDSurfaceDesc,
                            uint32_t dwFlags, void*) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    if (!lpDDSurfaceDesc) return D2DD_E_INVALIDARG;
    // The whole surface, always: the game locks full-screen (nullptr rect).
    lpDDSurfaceDesc->dwFlags = D2DD_DDSD_PITCH | D2DD_DDSD_LPSURFACE;
    lpDDSurfaceDesc->lPitch = s->pitch;
    lpDDSurfaceDesc->lpSurface = s->px.data();
    static_cast<void>(dwFlags);  // WAIT: we are synchronous; R/O vs W/O is advisory
    return DD_OK;
}

HRESULT __stdcall Surf_ReleaseDC(D2DDSurfaceObj*, HDC) {
    DdUnsupported("ReleaseDC");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_Restore(D2DDSurfaceObj*) { return DD_OK; }

HRESULT __stdcall Surf_SetClipper(D2DDSurfaceObj*, void*) {
    DdUnsupported("SetClipper");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_SetColorKey(D2DDSurfaceObj* self, uint32_t dwFlags, void* lpDDColorKey) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    if (!lpDDColorKey) return D2DD_E_INVALIDARG;
    const uint32_t* ck = (const uint32_t*)lpDDColorKey;
    s->ck_lo = ck[0];
    s->has_ck = true;
    static_cast<void>(dwFlags);  // DDCKEY_SRCBLT assumed (the 8-bit model)
    return DD_OK;
}

HRESULT __stdcall Surf_SetOverlayPosition(D2DDSurfaceObj*, int, int) {
    DdUnsupported("SetOverlayPosition");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_SetPalette(D2DDSurfaceObj* self, D2DDPaletteObj* lpDDPalette) {
    D2DDSurfaceObjFull* s = (D2DDSurfaceObjFull*)self;
    // No AddRef on attach: the game releases exactly the object CreatePalette
    // handed it, and the surface dies first in every ladder (documented
    // simplification — back-pointers keep both sides honest).
    if (s->pal) s->pal->attached_to = nullptr;
    s->pal = (D2DDPaletteObjFull*)lpDDPalette;
    if (s->pal) {
        s->pal->attached_to = s;
        MirrorPalette(s->pal);
    }
    DdLog("SetPalette attached");
    return DD_OK;
}

HRESULT __stdcall Surf_Unlock(D2DDSurfaceObj*, void* lpSurfaceData) {
    // The game passes (LPVOID)0 (its EndScene1/GetBackBuffer call sites) —
    // the DX1 contract wants Lock's pointer back, the game never does: the
    // argument is ignored, there is no lock bookkeeping to reconcile.
    static_cast<void>(lpSurfaceData);
    return DD_OK;
}

HRESULT __stdcall Surf_UpdateOverlay(D2DDSurfaceObj*, void*, D2DDSurfaceObj*, void*,
                                     uint32_t, void*) {
    DdUnsupported("UpdateOverlay");
    return D2DD_E_FAIL;
}

HRESULT __stdcall Surf_UpdateOverlayDisplay(D2DDSurfaceObj*, uint32_t) {
    return DD_OK;
}

HRESULT __stdcall Surf_UpdateOverlayZOrder(D2DDSurfaceObj*, uint32_t, D2DDSurfaceObj*) {
    DdUnsupported("UpdateOverlayZOrder");
    return D2DD_E_FAIL;
}

const D2DDSurfaceVtbl g_surface_vtable = {
    Surf_QueryInterface,
    Surf_AddRef,
    Surf_Release,
    Surf_AddAttachedSurface,
    Surf_AddOverlayDirtyRect,
    Surf_Blt,
    Surf_BltBatch,
    Surf_BltFast,
    Surf_DeleteAttachedSurface,
    Surf_EnumAttachedSurfaces,
    Surf_EnumOverlayZOrders,
    Surf_Flip,
    Surf_GetAttachedSurface,
    Surf_GetBltStatus,
    Surf_GetCaps,
    Surf_GetClipper,
    Surf_GetColorKey,
    Surf_GetDC,
    Surf_GetFlipStatus,
    Surf_GetOverlayPosition,
    Surf_GetPalette,
    Surf_GetPixelFormat,
    Surf_GetSurfaceDesc,
    Surf_Initialize,
    Surf_IsLost,
    Surf_Lock,
    Surf_ReleaseDC,
    Surf_Restore,
    Surf_SetClipper,
    Surf_SetColorKey,
    Surf_SetOverlayPosition,
    Surf_SetPalette,
    Surf_Unlock,
    Surf_UpdateOverlay,
    Surf_UpdateOverlayDisplay,
    Surf_UpdateOverlayZOrder,
};

}  // namespace

// ---------------------------------------------------------------------------
// The ddraw.dll entry point the game links.
// ---------------------------------------------------------------------------
extern "C" HRESULT __stdcall DirectDrawCreate(void* lpGuid, D2DDObj** lplpDD,
                                              void* pUnkOuter) {
    if (!lplpDD) return D2DD_E_INVALIDARG;
    if (pUnkOuter) {  // aggregation: never requested by the game
        DdUnsupported("DirectDrawCreate with pUnkOuter (aggregation)");
        return D2DD_E_FAIL;
    }
    D2DDObjFull* dd = new D2DDObjFull();
    dd->view = &g_dd_vtable;
    dd->refs = 1;
    dd->hwnd = nullptr;
    dd->coop = 0;
    dd->mode_w = 0;
    dd->mode_h = 0;
    dd->mode_bpp = 0;
    dd->primary = nullptr;
    *lplpDD = (D2DDObj*)dd;
    std::printf("d2linux ddraw: DirectDrawCreate guid=%s (the DX1 8bpp shim)\n",
                lpGuid ? "<game guid>" : "NULL");
    return DD_OK;
}
