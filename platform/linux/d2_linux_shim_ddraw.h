// platform/linux/d2_linux_shim_ddraw.h — [d2linux M2 round 2] the DirectDraw
// COM shim's internal declarations: the DX1 object/vtable shapes, the
// surface-desc/BLTFX memory layouts, and the DD_* constants the shim and its
// self-test share. Implemented in d2_linux_shim_ddraw.cpp.
//
// WHY OWN TYPES, not the game's D2DDraw/Renderer/DirectDraw.h: that header is
// the ghidra reconstruction, and its IDirectDrawVtbl spells SetDisplayMode
// with the LATER-SDK 5-argument signature while the game binary — like every
// real DX1 ddraw.dll — uses the DX1 3-argument ABI (dwWidth, dwHeight,
// dwBPP). On i386 __stdcall the callee's epilogue pops what ITS OWN signature
// says: a 5-arg callee reached through a 3-arg call site pops 8 extra bytes
// and corrupts the stack. The shim therefore models the ABI the game actually
// pushes, and the slot ORDER the game's header lays out (they agree through
// GetAvailableVidMem, 0x00..0x5C).
//
// All names are D2-prefixed, so a future TU that includes both this header
// and the game's reconstruction can never ODR-clash. The object memory is the
// contract: the game casts its IDirectDraw* onto D2DDObj and calls through
// slot offsets; the D2DD* vtables are laid out at exactly those offsets.
//
// LAYOUT AND THE ARCH CHARTER: the struct offsets below are the i386 ABI
// (the game's). A native x86-64 build sees its own consistent layout (void*
// is 8 bytes) — harmless, because game TUs are i386-only by charter (PLAN.md
// R1): the only native-mode writer/reader of these structs is this shim and
// its self-test, which compile together. The i386 static_asserts pin the
// game-facing layout exactly.
#ifndef D2_LINUX_SHIM_DDRAW_H
#define D2_LINUX_SHIM_DDRAW_H

#include <cstdint>

// ---------------------------------------------------------------------------
// HRESULT values (ddraw.h spellings — several DDERR_* alias the E_* codes).
// The failure codes are HRESULT-typed (d2_platform.h: long) so comparisons
// against API returns stay sign-clean on both gcc (-Wsign-compare) and the
// decomp's signed HRESULT reads.
// ---------------------------------------------------------------------------
#ifndef DD_OK
#define DD_OK 0
#endif
#define D2DD_E_NOINTERFACE        ((HRESULT)0x80004002)  // E_NOINTERFACE
#define D2DD_E_OUTOFMEMORY        ((HRESULT)0x8007000E)  // E_OUTOFMEMORY
#define D2DD_E_INVALIDARG         ((HRESULT)0x80070057)  // E_INVALIDARG (DDERR_INVALIDPARAMS)
#define D2DD_E_FAIL               ((HRESULT)0x80004005)  // E_FAIL (DDERR_GENERIC)
#define D2DD_DDERR_NOTFOUND       ((HRESULT)0x887601F9)  // MAKE_DDHRESULT(505)
#define D2DD_DDERR_ALREADYINITIALIZED ((HRESULT)0x88760005)  // MAKE_DDHRESULT(5)
#define D2DD_DDERR_SURFACELOST    ((HRESULT)0x887601C2)  // MAKE_DDHRESULT(450)
#define D2DD_DDERR_WASSTILLDRAWING ((HRESULT)0x8876021C) // MAKE_DDHRESULT(540)

// --- DDSURFACEDESC dwFlags (ddraw.h) ---
#define D2DD_DDSDD_CAPS            0x00000001u
#define D2DD_DDSD_HEIGHT           0x00000002u
#define D2DD_DDSD_WIDTH            0x00000004u
#define D2DD_DDSD_PITCH            0x00000008u
#define D2DD_DDSD_BACKBUFFERCOUNT  0x00000020u
#define D2DD_DDSD_LPSURFACE        0x00000800u
#define D2DD_DDSD_PIXELFORMAT      0x00001000u
// --- DDSCAPS dwCaps (ddraw.h) ---
#define D2DD_DDSCAPS_BACKBUFFER      0x00000004u
#define D2DD_DDSCAPS_COMPLEX         0x00000008u
#define D2DD_DDSCAPS_FLIP            0x00000010u
#define D2DD_DDSCAPS_OFFSCREENPLAIN  0x00000040u
#define D2DD_DDSCAPS_PRIMARYSURFACE  0x00000200u
#define D2DD_DDSCAPS_SYSTEMMEMORY    0x00000800u
#define D2DD_DDSCAPS_VIDEOMEMORY     0x00004000u
// --- DDPIXELFORMAT dwFlags ---
#define D2DD_DDPF_RGB               0x00000040u
#define D2DD_DDPF_PALETTEINDEXED8   0x00000020u
// --- DDLOCK (ddraw.h): the game locks 0x21 (WAIT|WRITEONLY, BeginScene) and
// --- 0x11 (WAIT|READONLY, GetBackBuffer).
#define D2DD_DDLOCK_WAIT     0x00000001u
#define D2DD_DDLOCK_READONLY 0x00000010u
#define D2DD_DDLOCK_WRITEONLY 0x00000020u
// --- Blt dwFlags: the game uses DDBLT_COLORFILL (PlayCutScene's clear).
#define D2DD_DDBLT_COLORFILL 0x00000400u
#define D2DD_DDBLT_WAIT      0x01000000u
// --- BltFast dwTrans: the game uses DDBLTFAST_WAIT (0x10).
#define D2DD_DDBLTFAST_SRCCOLORKEY 0x00000001u
#define D2DD_DDBLTFAST_DESTCOLORKEY 0x00000002u
#define D2DD_DDBLTFAST_WAIT        0x00000010u
// --- Flip dwFlags: the game uses DDFLIP_WAIT.
#define D2DD_DDFLIP_WAIT 0x00000001u
// --- CreatePalette dwFlags: the game uses DDPCAPS_8BIT (0x4).
#define D2DD_DDPCAPS_8BIT 0x00000004u
// --- SetCooperativeLevel dwFlags: the game uses 0x11
// --- (DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE).
#define D2DD_DDSCL_FULLSCREEN 0x00000001u
#define D2DD_DDSCL_EXCLUSIVE  0x00000010u

// ---------------------------------------------------------------------------
// Memory layouts. D2DDSurfDesc is the game's DDSURFACEDESC (it passes
// dwSize=0x6C); D2DDBltFx is its packed DDBLTFX (dwSize=100, dwFillColor at
// 0x50). The color-key blobs are kept as raw dwords — the game never reads
// them back through the shim.
// ---------------------------------------------------------------------------
struct D2DDCaps {
    uint32_t dwCaps;                     /* DDSCAPS: the game casts a DWORD* */
};

struct D2DDPixelFormat {                 /* DDPIXELFORMAT, 32 bytes */
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;                 /* union dwYBitDepth/dwZBitDepth/... */
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwRGBAlphaBitMask;
};

struct D2DDSurfDesc {                    /* DDSURFACEDESC, 0x6C bytes i386 */
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    int32_t  lPitch;                     /* union dwLinearSize */
    uint32_t dwBackBufferCount;
    uint32_t dwMipMapCount;              /* union dwZBufferBitDepth/dwRefreshRate */
    uint32_t dwAlphaBitDepth;
    uint32_t dwReserved;
    void*    lpSurface;
    uint32_t ddckCKDestOverlay[2];
    uint32_t ddckCKDestBlt[2];
    uint32_t ddckCKSrcOverlay[2];
    uint32_t ddckCKSrcBlt[2];
    D2DDPixelFormat ddpfPixelFormat;
    uint32_t ddsCaps;
};

struct D2DDBltFx {                       /* DDBLTFX, 100 bytes, packed */
    uint32_t dwSize;
    uint32_t dwDDFX;
    uint32_t dwROP;
    uint32_t dwDDROP;
    uint32_t dwRotationAngle;
    uint32_t dwZBufferOpCode;
    uint32_t dwZBufferLow;
    uint32_t dwZBufferHigh;
    uint32_t dwZBufferBaseDest;
    uint32_t dwZDestConstBitDepth;
    uint32_t dwZDestConst;               /* union lpDDSZBufferDest */
    uint32_t dwZSrcConstBitDepth;
    uint32_t dwZSrcConst;                 /* union lpDDSZBufferSrc */
    uint32_t dwAlphaEdgeBlendBitDepth;
    uint32_t dwAlphaEdgeBlend;
    uint32_t dwReserved;
    uint32_t dwAlphaDestConstBitDepth;
    uint32_t dwAlphaDestConst;           /* union lpDDSAlphaDest */
    uint32_t dwAlphaSrcConstBitDepth;
    uint32_t dwAlphaSrcConst;            /* union lpDDSAlphaSrc */
    uint32_t dwFillColor;                /* union dwFillDepth/dwFillPixel */
    uint32_t ddckDestColorkey[2];
    uint32_t ddckSrcColorkey[2];
} __attribute__((packed));

#ifdef __i386__
#include <cstddef>
static_assert(sizeof(D2DDSurfDesc) == 0x6C, "DDSURFACEDESC must be the game's 0x6C");
static_assert(offsetof(D2DDSurfDesc, lpSurface) == 0x24, "DDSURFACEDESC.lpSurface offset");
static_assert(offsetof(D2DDSurfDesc, ddpfPixelFormat) == 0x48, "DDSURFACEDESC.ddpf offset");
static_assert(sizeof(D2DDBltFx) == 100, "DDBLTFX must be 100 bytes");
static_assert(offsetof(D2DDBltFx, dwFillColor) == 0x50, "DDBLTFX.dwFillColor offset");
#endif

// ---------------------------------------------------------------------------
// The COM objects, as the CALLERS see them: each is exactly one vtable
// pointer at offset 0 — the COM identity the game's IDirectDraw* cast reads.
// The shim's per-object state lives in the implementation file, in structs
// that START with these same views (pointer-interconvertible prefix) — the
// pointer handed out IS the view, the state rides behind it.
// (Forward declarations first: the vtables type their This pointers, the
// views type their vtable pointers.)
// ---------------------------------------------------------------------------
struct D2DDObj;            // IDirectDraw
struct D2DDSurfaceObj;     // IDirectDrawSurface1
struct D2DDPaletteObj;     // IDirectDrawPalette

// IDirectDraw vtable — DX1 slot order, DX1 argument ABIs (see the header
// comment for the SetDisplayMode signature decision). Slots 0x00..0x5C match
// the game's reconstruction; the DX1 vtable ends there.
struct D2DDVtbl {
    HRESULT (__stdcall *QueryInterface)      (D2DDObj*, void* riid, void** ppvObj);
    ULONG   (__stdcall *AddRef)              (D2DDObj*);
    ULONG   (__stdcall *Release)             (D2DDObj*);
    HRESULT (__stdcall *Compact)             (D2DDObj*);
    HRESULT (__stdcall *CreateClipper)       (D2DDObj*, uint32_t dwFlags, void** lplpDDClipper, void* pUnkOuter);
    HRESULT (__stdcall *CreatePalette)       (D2DDObj*, uint32_t dwFlags, void* lpColorTable, D2DDPaletteObj** lplpDDPalette, void* pUnkOuter);
    HRESULT (__stdcall *CreateSurface)       (D2DDObj*, void* lpDDSurfaceDesc, void** lplpDDSurface, void* pUnkOuter);
    HRESULT (__stdcall *DuplicateSurface)    (D2DDObj*, D2DDSurfaceObj* lpDDSurface, D2DDSurfaceObj** lplpDupDDSurface);
    HRESULT (__stdcall *EnumDisplayModes)    (D2DDObj*, uint32_t dwFlags, void* lpDDSurfaceDesc, void* lpContext, void* lpEnumModesCallback);
    HRESULT (__stdcall *EnumSurfaces)        (D2DDObj*, uint32_t dwFlags, void* lpDDSD, void* lpContext, void* lpEnumSurfacesCallback);
    HRESULT (__stdcall *FlipToGDISurface)    (D2DDObj*);
    HRESULT (__stdcall *GetCaps)             (D2DDObj*, void* lpDDDriverCaps, void* lpDDHELCaps);
    HRESULT (__stdcall *GetDisplayMode)      (D2DDObj*, D2DDSurfDesc* lpDDSurfaceDesc);
    HRESULT (__stdcall *GetFourCCCodes)      (D2DDObj*, uint32_t* lpNumCodes, uint32_t* lpCodes);
    HRESULT (__stdcall *GetGDISurface)       (D2DDObj*, D2DDSurfaceObj** lplpGDIDDSSurface);
    HRESULT (__stdcall *GetMonitorFrequency) (D2DDObj*, uint32_t* lpdwFrequency);
    HRESULT (__stdcall *GetScanLine)         (D2DDObj*, uint32_t* lpdwScanLine);
    HRESULT (__stdcall *GetVerticalBlankStatus)(D2DDObj*, int* lpIsInVB);
    HRESULT (__stdcall *Initialize)          (D2DDObj*, void* lpGUID);
    HRESULT (__stdcall *RestoreDisplayMode)  (D2DDObj*);
    HRESULT (__stdcall *SetCooperativeLevel) (D2DDObj*, HWND hWnd, uint32_t dwFlags);
    HRESULT (__stdcall *SetDisplayMode)      (D2DDObj*, uint32_t dwWidth, uint32_t dwHeight, uint32_t dwBPP);  // DX1 ABI — 3 args
    HRESULT (__stdcall *WaitForVerticalBlank)(D2DDObj*, uint32_t dwFlags);
    HRESULT (__stdcall *GetAvailableVidMem)  (D2DDObj*, void* lpDDSCaps, uint32_t* lpdwTotal, uint32_t* lpdwFree);
};

// IDirectDrawSurface1 vtable — 36 slots, offsets 0x00..0x8C (the game's
// reconstruction agrees slot-for-slot).
struct D2DDSurfaceVtbl {
    HRESULT (__stdcall *QueryInterface)      (D2DDSurfaceObj*, void* riid, void** ppvObj);
    ULONG   (__stdcall *AddRef)              (D2DDSurfaceObj*);
    ULONG   (__stdcall *Release)             (D2DDSurfaceObj*);
    HRESULT (__stdcall *AddAttachedSurface)  (D2DDSurfaceObj*, D2DDSurfaceObj* lpDDSurface);
    HRESULT (__stdcall *AddOverlayDirtyRect) (D2DDSurfaceObj*, void* lpRect);
    HRESULT (__stdcall *Blt)                 (D2DDSurfaceObj*, void* lpDestRect, D2DDSurfaceObj* lpDDSrcSurface, void* lpSrcRect, uint32_t dwFlags, void* lpDDBltFx);
    HRESULT (__stdcall *BltBatch)            (D2DDSurfaceObj*, void* lpDDBltBatch, uint32_t dwCount, uint32_t dwFlags);
    HRESULT (__stdcall *BltFast)             (D2DDSurfaceObj*, uint32_t dwX, uint32_t dwY, D2DDSurfaceObj* lpDDSrcSurface, void* lpSrcRect, uint32_t dwTrans);
    HRESULT (__stdcall *DeleteAttachedSurface)(D2DDSurfaceObj*, uint32_t dwFlags, D2DDSurfaceObj* lpDDSurface);
    HRESULT (__stdcall *EnumAttachedSurfaces)(D2DDSurfaceObj*, void* lpContext, void* lpEnumSurfacesCallback);
    HRESULT (__stdcall *EnumOverlayZOrders)  (D2DDSurfaceObj*, uint32_t dwFlags, void* lpContext, void* lpfnCallback);
    HRESULT (__stdcall *Flip)                (D2DDSurfaceObj*, D2DDSurfaceObj* lpDDSurfaceTargetOverride, uint32_t dwFlags);
    HRESULT (__stdcall *GetAttachedSurface)  (D2DDSurfaceObj*, D2DDCaps* lpDDSCaps, D2DDSurfaceObj** lplpDDAttachedSurface);
    HRESULT (__stdcall *GetBltStatus)        (D2DDSurfaceObj*, uint32_t dwFlags);
    HRESULT (__stdcall *GetCaps)             (D2DDSurfaceObj*, D2DDCaps* lpDDSCaps);
    HRESULT (__stdcall *GetClipper)          (D2DDSurfaceObj*, void** lplpDDClipper);
    HRESULT (__stdcall *GetColorKey)         (D2DDSurfaceObj*, uint32_t dwFlags, void* lpDDColorKey);
    HRESULT (__stdcall *GetDC)               (D2DDSurfaceObj*, HDC* lphDC);
    HRESULT (__stdcall *GetFlipStatus)       (D2DDSurfaceObj*, uint32_t dwFlags);
    HRESULT (__stdcall *GetOverlayPosition)  (D2DDSurfaceObj*, int* lplX, int* lplY);
    HRESULT (__stdcall *GetPalette)          (D2DDSurfaceObj*, D2DDPaletteObj** lplpDDPalette);
    HRESULT (__stdcall *GetPixelFormat)      (D2DDSurfaceObj*, void* lpDDPixelFormat);
    HRESULT (__stdcall *GetSurfaceDesc)      (D2DDSurfaceObj*, D2DDSurfDesc* lpDDSurfaceDesc);
    HRESULT (__stdcall *Initialize)          (D2DDSurfaceObj*, D2DDObj* lpDD, D2DDSurfDesc* lpDDSurfaceDesc);
    HRESULT (__stdcall *IsLost)              (D2DDSurfaceObj*);
    HRESULT (__stdcall *Lock)                (D2DDSurfaceObj*, void* lpDestRect, D2DDSurfDesc* lpDDSurfaceDesc, uint32_t dwFlags, void* hEvent);
    HRESULT (__stdcall *ReleaseDC)           (D2DDSurfaceObj*, HDC hDC);
    HRESULT (__stdcall *Restore)             (D2DDSurfaceObj*);
    HRESULT (__stdcall *SetClipper)          (D2DDSurfaceObj*, void* lpDDClipper);
    HRESULT (__stdcall *SetColorKey)         (D2DDSurfaceObj*, uint32_t dwFlags, void* lpDDColorKey);
    HRESULT (__stdcall *SetOverlayPosition)  (D2DDSurfaceObj*, int lX, int lY);
    HRESULT (__stdcall *SetPalette)          (D2DDSurfaceObj*, D2DDPaletteObj* lpDDPalette);
    HRESULT (__stdcall *Unlock)              (D2DDSurfaceObj*, void* lpSurfaceData);
    HRESULT (__stdcall *UpdateOverlay)       (D2DDSurfaceObj*, void* lpSrcRect, D2DDSurfaceObj* lpDDDestSurface, void* lpDestRect, uint32_t dwFlags, void* lpDDOverlayFx);
    HRESULT (__stdcall *UpdateOverlayDisplay)(D2DDSurfaceObj*, uint32_t dwFlags);
    HRESULT (__stdcall *UpdateOverlayZOrder) (D2DDSurfaceObj*, uint32_t dwFlags, D2DDSurfaceObj* lpDDSReference);
};

// IDirectDrawPalette vtable — 7 slots.
struct D2DDPaletteVtbl {
    HRESULT (__stdcall *QueryInterface)(D2DDPaletteObj*, void* riid, void** ppvObj);
    ULONG   (__stdcall *AddRef)        (D2DDPaletteObj*);
    ULONG   (__stdcall *Release)       (D2DDPaletteObj*);
    HRESULT (__stdcall *GetCaps)       (D2DDPaletteObj*, uint32_t* lpdwCaps);
    HRESULT (__stdcall *GetEntries)    (D2DDPaletteObj*, uint32_t dwFlags, uint32_t dwBase, uint32_t dwNumEntries, void* lpEntries);
    HRESULT (__stdcall *Initialize)    (D2DDPaletteObj*, D2DDObj* lpDD, uint32_t dwFlags, void* lpDDColorTable);
    HRESULT (__stdcall *SetEntries)    (D2DDPaletteObj*, uint32_t dwFlags, uint32_t dwStartingEntry, uint32_t dwCount, void* lpEntries);
};

// The COM identity views (defined AFTER the vtables, which type them).
struct D2DDObj {            // IDirectDraw
    D2DDVtbl* lpVtbl;
};

struct D2DDSurfaceObj {     // IDirectDrawSurface1
    D2DDSurfaceVtbl* lpVtbl;
};

struct D2DDPaletteObj {     // IDirectDrawPalette
    D2DDPaletteVtbl* lpVtbl;
};

// The game's entry point (ddraw.dll export). The game links the plain symbol;
// extern "C" keeps it undecorated on ELF (stdcall does not decorate there).
// Returns an AddRef'd D2DDObj.
extern "C" HRESULT __stdcall DirectDrawCreate(void* lpGuid, D2DDObj** lplpDD, void* pUnkOuter);

#endif  // D2_LINUX_SHIM_DDRAW_H
