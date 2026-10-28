// vanilla-plus: "Vanilla+ Options" in-game settings page (FEATURES.md row 10).
// Design contract: DRAFT_vanilla_plus_options.md (Route A, above Previous Menu).
// Charter: VANILLA-PLUS.md - additive UI only; every toggle stays opt-in with
// 0-defaults; no D2Game/D2Common edits; no loader edits; no shipped assets.
//
// ===== Route A: the synthesized row label ===================================
// The Options-page row (index 4 of gaEscMenuOptionsTable) carries an empty
// image path. ESCMENU_LoadLocalizedImages skips rows whose
// szLocalizedImagePath[0] == '\0' WITHOUT halting and WITHOUT assigning
// pDC6Main, then still fires fpInitCallback - so ESCMENU_InitVanillaPlusLabel
// seeds pItem->pDC6Main with a DC6 built entirely in memory:
//   1. rasterize "Vanilla+ Options" from the font-0xd glyph frames (the same
//      frames D2WINFONT_DrawWideString blits), copying the glyph pixels' own
//      palette indices so the label renders like in-game white text;
//   2. RLE-encode one 1-direction 1-frame DC6 whose bytes are shaped exactly
//      like a file-loaded image (24-byte header, file-relative frame-offset
//      table, 0x20-byte frame header, 0x80-terminated run rows);
//   3. allocate the raw image with the same client allocator
//      D2FILE_AllocAndLoadD2File uses (AllocClientMemory) and run the retail
//      CELCMP_FixupPointersAndPrepare over it.
// The retail free path (ESCMENU_FreeLoadedImages -> DC6_FreeAndDelete ->
// DeleteCellFile + FreeClientMemory) then tears it down with zero
// special-casing: same allocator, same shape, same fixup.

#include "D2Client/UI/VanillaPlusOptions.h"

#include "D2CMP/SRC/CelCmp.h"
#include "D2Client/D2Client.h"
#include "D2Client/D2GfxDataStrc.h"
#include "D2Client/Engine/GfxInfo.h"
#include "D2Client/Sound/Sound.h"
#include "D2Client/UI/Draw.h"
#include "D2Client/UI/EscMenu.h"
#include "D2Client/UI/ui.h"
#include "D2Sound/Src/D2SoundFast.h"
#include "D2Win/DC6.h"
#include "D2Win/DC6Block.h"
#include "D2Win/Src/D2WinArchive.h"
#include "D2Win/Src/D2WinFont.h"
#include "D2gfx/D2GFX.h"
#include "Fog/Memory.h"
#include "Storm/Storm.h"
#include "globals/globals.h"

#define DEFAULT_SECURITY_COOKIE 0xBB40E64E

#ifndef D2_ALLOC
#define D2_ALLOC(size) Fog::Memory::AllocClientMemory((size), __FILE__, __LINE__, 0)
#endif

#ifndef D2_FREE
#define D2_FREE(p) Fog::Memory::FreeClientMemory((p), __FILE__, __LINE__, 0)
#endif

namespace D2Client::UI::VanillaPlus {

// ===== tunables (single place, field-QA) ====================================

// Row label geometry. The synthesized image is bottom-anchored at
// nItemBottomY by the renderer, so a VPLABEL_HEIGHT-pixel image spans the
// whole 0x27 row band; the glyphs are vertically centered inside it.
static const int VPLABEL_HEIGHT = 0x27;            // matches nRowHeight
static const uint16_t VPLABEL_FONT_ID = 0xd;       // ControlConfig's row font

// Page layout (mirrors the ControlConfig panel box for a familiar look).
static const int VPPAGE_BOX_WIDTH = 0x26c;
static const int VPPAGE_BOX_HEIGHT = 0x171;
static const int VPPAGE_ROW_PITCH = 0x20;
static const int VPPAGE_ROW_HEIGHT = 0x1a;
static const int VPPAGE_FIRST_ROW_Y = 0x50;        // from box top
static const int VPPAGE_BACK_ROW_Y = 0x124;        // from box top

// [features] keys rendered by the page. Adding a slate feature = +1 entry
// here and +1 template block in ALTTOGGLE_WriteIniTemplate - nothing else.
struct VPPFeatureRow {
    const char* szKey;        // d2vp.ini [features] key
    const wchar_t* wszLabel;  // row label
};
static const VPPFeatureRow gaVanillaPlusRows[3] = {
    { "alt_toggle", L"Alt-Show Items" },
    { "auto_gold", L"Auto Gold Pickup" },
    { "portable_save", L"Portable Saves" },
};
static const int VPP_ROW_COUNT = 3;
static const int VPP_ROW_BACK = 3; // hit-test index of the Previous Menu row

// Input handlers (definitions below; the batch array references them).
void __stdcall INPUT_VP_MouseMove(D2UIMouseMsgStrc * pMsg);
void __stdcall INPUT_VP_MouseDown(D2UIMouseMsgStrc * pMsg);
void __stdcall INPUT_VP_MouseUp(D2UIMouseMsgStrc * pMsg);
void __stdcall INPUT_VP_EscapeKey(D2UIMouseMsgStrc * pMsg);

// Input batch registered while the page is open (cube/panel style: dedicated
// handlers, self-contained registration). eEventType: 0=window message,
// 3=key down (ui.h D2UIFlagInitStrc contract).
static D2UIFlagInitStrc gaVanillaPlusInit[4] = {
    { 0x0, 0x200, &INPUT_VP_MouseMove },
    { 0x0, 0x201, &INPUT_VP_MouseDown },
    { 0x3, 0x01b, &INPUT_VP_EscapeKey },
    { 0x0, 0x202, &INPUT_VP_MouseUp },
};

// ===== page state ============================================================

static int gnVanillaPlusHoverRow = -1;  // hit-test row under the cursor

// Page geometry helpers (kept in one place for the draw and the hit tests).
static void VPPAGE_GetBoxOrigin(int* pnX, int* pnY) {
    *pnX = (ScreenSizeX - VPPAGE_BOX_WIDTH) / 2;
    *pnY = (WindowSizeY - VPPAGE_BOX_HEIGHT) / 2;
}

static int VPPAGE_RowTopY(int nRowIndex) {
    int nBoxX = 0;
    int nBoxY = 0;
    VPPAGE_GetBoxOrigin(&nBoxX, &nBoxY);
    if (nRowIndex == VPP_ROW_BACK) {
        return nBoxY + VPPAGE_BACK_ROW_Y;
    }
    return nBoxY + VPPAGE_FIRST_ROW_Y + nRowIndex * VPPAGE_ROW_PITCH;
}

static int VPPAGE_HitTest(int nMouseX, int nMouseY) {
    int nBoxX = 0;
    int nBoxY = 0;
    VPPAGE_GetBoxOrigin(&nBoxX, &nBoxY);
    int nRowLeft = nBoxX + 0x12;
    int nRowRight = nBoxX + VPPAGE_BOX_WIDTH - 0x12;
    if (nMouseX < nRowLeft || nMouseX >= nRowRight) {
        return -1;
    }
    for (int i = 0; i <= VPP_ROW_BACK; i++) {
        int nTop = VPPAGE_RowTopY(i);
        if (nMouseY >= nTop && nMouseY < nTop + VPPAGE_ROW_HEIGHT) {
            return i;
        }
    }
    return -1;
}

// ===== label synthesis (Route A) =============================================

// Decode one DC6 frame (retail RLE: 0x01..0x7F literal run, 0x81..0xFF
// transparent run of (token & 0x7F), 0x80 end-of-row) into an 8-bit raster.
// Semantics pinned from FASTCMP_DecompressRLEWithAlpha (D2CMP/SRC/FastCmp.cpp).
static void VPLABEL_DecodeFrameRLE(const DC6Block* pFrame, uint8_t* pOut, int nOutWidth, int nOutHeight) {
    const uint8_t* pCursor = (const uint8_t*)pFrame + 0x20;
    int nWidth = pFrame->nWidth;
    int nHeight = pFrame->nHeight;
    if (nWidth > nOutWidth) {
        nWidth = nOutWidth;
    }
    if (nHeight > nOutHeight) {
        nHeight = nOutHeight;
    }
    for (int y = 0; y < nHeight; y++) {
        uint8_t* pRow = pOut + y * nOutWidth;
        int x = 0;
        while (x < nWidth) {
            uint8_t byToken = *pCursor++;
            if (byToken & 0x80) {
                byToken &= 0x7f;
                if (!byToken) {
                    break; // 0x80 = end of row
                }
                x += byToken; // transparent run: leave the raster zeroed
            } else {
                if (!byToken) {
                    break; // defensive: retail encoders never emit 0x00
                }
                if (x + byToken > nWidth) {
                    byToken = (uint8_t)(nWidth - x);
                }
                for (uint8_t i = 0; i < byToken; i++) {
                    pRow[x + i] = *pCursor++;
                }
                x += byToken;
            }
        }
    }
}

// Encode an 8-bit raster as DC6 rows (inverse of the decoder above). Returns
// the encoded size; pnDataSizeOut receives it for the frame's nLength field.
static int VPLABEL_EncodeFrameRLE(const uint8_t* pRaster, int nWidth, int nHeight, uint8_t* pOut, int* pnDataSizeOut) {
    uint8_t* pWrite = pOut;
    for (int y = 0; y < nHeight; y++) {
        const uint8_t* pRow = pRaster + y * nWidth;
        int x = 0;
        while (x < nWidth) {
            if (!pRow[x]) {
                int nRun = 1;
                while (x + nRun < nWidth && !pRow[x + nRun] && nRun < 0x7f) {
                    nRun++;
                }
                *pWrite++ = (uint8_t)(0x80 | nRun); // transparent run 1..127
                x += nRun;
            } else {
                int nRun = 1;
                while (x + nRun < nWidth && pRow[x + nRun] && nRun < 0x7f) {
                    nRun++;
                }
                *pWrite++ = (uint8_t)nRun; // literal run 1..127
                for (int i = 0; i < nRun; i++) {
                    *pWrite++ = pRow[x + i];
                }
                x += nRun;
            }
        }
        *pWrite++ = 0x80; // end of row
    }
    *pnDataSizeOut = (int)(pWrite - pOut);
    return *pnDataSizeOut;
}

// Build the byte-exact raw DC6 file image around one encoded frame and run
// the retail fixup. Allocation matches D2FILE_AllocAndLoadD2File (D2_ALLOC),
// so the retail free path (DeleteCellFile + FreeClientMemory) is compatible
// without special-casing. FALSE = out of memory (the caller keeps the row
// label blank rather than risk a null pDC6Main draw halt).
static BOOL VPLABEL_BuildDc6(const uint8_t* pRaster, int nWidth, int nHeight, DC6** ppOut) {
    *ppOut = nullptr;
    uint8_t aEncoded[VPPAGE_BOX_WIDTH * 2];
    int nEncodedSize = 0;
    VPLABEL_EncodeFrameRLE(pRaster, nWidth, nHeight, aEncoded, &nEncodedSize);

    // 24-byte header + 4-byte frame-offset entry + 0x20-byte frame header.
    int nTotalSize = 0x18 + 4 + 0x20 + nEncodedSize;
    DC6* pRaw = (DC6*)D2_ALLOC(nTotalSize);
    if (!pRaw) {
        return FALSE;
    }
    memset(pRaw, 0, nTotalSize);
    pRaw->nVersion = 6;       // required by the fixup (nExpectedFormat == -1)
    pRaw->nFlags = 1;         // bit0 required, bit1 clear (pre-fixup state)
    pRaw->nFormat = 0;
    /* nTerm stays zeroed (unused by the fixup for file-shaped images) */
    pRaw->nDirections = 1;
    pRaw->nFrames = 1;
    *(uint32_t*)((char*)pRaw + 0x18) = 0x1C; // file-relative frame offset

    DC6Block* pFrame = (DC6Block*)((char*)pRaw + 0x1C);
    pFrame->nFlip = 1;        // retail frame termination value
    pFrame->nWidth = nWidth;
    pFrame->nHeight = nHeight;
    pFrame->nOffsetX = 0;
    pFrame->nOffsetY = 0;
    pFrame->nAllocSize = nWidth * nHeight;
    pFrame->nNextBlock = 0;   // fixup patch slot: overwritten with the row ptr
    pFrame->nLength = nEncodedSize;
    memcpy((char*)pRaw + 0x3C, aEncoded, nEncodedSize);

    // Retail fixup: converts the file-relative offset to an absolute pointer,
    // allocates the 1x1 block row, patches the frame's +0x18 slot. Idempotent
    // (nFlags bit1), exactly what every file-loaded DC6 goes through.
    D2CMP::SRC::CelCmp::CELCMP_FixupPointersAndPrepare(pRaw, ppOut, ".\\UI\\VANILLAPLUS.CPP", 0x0, -1, "vanilla-plus options label");
    if (!*ppOut) {
        D2_FREE(pRaw);
        return FALSE;
    }
    return TRUE;
}

// Rasterize the label from the font's own glyph frames and synthesize the
// DC6. On any failure a small blank image is synthesized instead so the row
// stays drawable (never a null pDC6Main under the type-0 draw path).
static void VPLABEL_SynthesizeInto(DC6** ppOut) {
    *ppOut = nullptr;

    // Font selection: LookupGlyphEntry reads the CURRENT font cache, so
    // select font 0xd first (load-on-demand) and restore afterwards.
    int32_t nPrevFont = D2Win::Src::D2WinFont::SetAndReturnLastFont(VPLABEL_FONT_ID);

    do {
        // Load our own copy of the font image so glyph pixels can be decoded
        // without reaching into D2WinFont's internal cache globals.
        char szFontPath[80];
        D2Win::Src::D2WinFont::D2WINFONT_BuildFontPath(szFontPath, VPLABEL_FONT_ID, (int32_t)sizeof(szFontPath));
        DC6* pFontDC6 = D2Win::Src::D2WinArchive::IMAGE_LoadDC6(szFontPath, 0);
        if (!pFontDC6) {
            break;
        }

        static const wchar_t gwszLabel[] = L"Vanilla+ Options";
        int nLabelChars = (int)(sizeof(gwszLabel) / sizeof(gwszLabel[0])) - 1;

        // Measure: glyph advance widths from the .tbl entries, frames from
        // the font image (D2WINFONT_DrawWideString's exact pairing).
        int nTotalWidth = 0;
        int nMaxGlyphHeight = 0;
        int anFrameIndex[64];
        int anGlyphWidth[64];
        if (nLabelChars > 64) {
            nLabelChars = 64;
        }
        for (int i = 0; i < nLabelChars; i++) {
            D2FontGlyphStrc* pGlyph = D2Win::Src::D2WinFont::D2WINFONT_LookupGlyphEntry((uint16_t)gwszLabel[i]);
            if (!pGlyph) {
                anFrameIndex[i] = -1;
                anGlyphWidth[i] = 0;
                continue;
            }
            anFrameIndex[i] = *(const uint16_t*)(pGlyph->abUnverified + 6);
            anGlyphWidth[i] = pGlyph->abUnverified[1];
            nTotalWidth += anGlyphWidth[i];
        }
        if (nTotalWidth <= 0 || nTotalWidth > 0x100) {
            D2Win::Src::D2WinArchive::IMAGE_FreeImage(pFontDC6);
            break;
        }

        // Compose: decode each glyph frame, blit into the shared raster at
        // the running pen x, vertically centered (the renderer bottom-anchors
        // the whole image at nItemBottomY, so centered-in-band matches the
        // row art). Glyph pixels keep the font's own palette indices.
        int nGlyphPitch = 0x40;
        static uint8_t aGlyphScratch[0x40 * 0x40];
        static uint8_t aLabelRaster[0x100 * VPLABEL_HEIGHT];
        memset(aLabelRaster, 0, sizeof(aLabelRaster));
        int nPenX = 0;
        for (int i = 0; i < nLabelChars; i++) {
            if (anFrameIndex[i] < 0 || anGlyphWidth[i] <= 0 || anGlyphWidth[i] >= nGlyphPitch) {
                continue;
            }
            // Frame lookup: direction 0 + the .tbl frame index - the exact
            // (DC6, frame) pair D2WINFONT_DrawWideString renders.
            D2GfxDataStrc sGfxData;
            memset(&sGfxData, 0, sizeof(sGfxData));
            sGfxData.pDC6 = pFontDC6;
            sGfxData.nDirection = 0;
            sGfxData.nFrameNumber = anFrameIndex[i];
            DC6Block* pGlyphFrame = D2CMP::SRC::CelCmp::CELCMP_GetDC6BlockFromGfxData(&sGfxData);
            if (pGlyphFrame) {
                int nGlyphHeight = pGlyphFrame->nHeight;
                if (nGlyphHeight > nGlyphPitch) {
                    nGlyphHeight = nGlyphPitch;
                }
                memset(aGlyphScratch, 0, sizeof(aGlyphScratch));
                VPLABEL_DecodeFrameRLE(pGlyphFrame, aGlyphScratch, nGlyphPitch, nGlyphPitch);
                int nYOffset = (VPLABEL_HEIGHT - pGlyphFrame->nHeight) / 2;
                if (nYOffset < 0) {
                    nYOffset = 0;
                }
                for (int y = 0; y < nGlyphHeight; y++) {
                    int nDstY = nYOffset + y;
                    if (nDstY >= VPLABEL_HEIGHT) {
                        break;
                    }
                    uint8_t* pDst = aLabelRaster + nDstY * nTotalWidth + nPenX;
                    const uint8_t* pSrc = aGlyphScratch + y * nGlyphPitch;
                    for (int x = 0; x < anGlyphWidth[i]; x++) {
                        if (pSrc[x]) {
                            pDst[x] = pSrc[x]; // keep the font's own palette index
                        }
                    }
                }
            }
            // Advance the pen by the glyph's .tbl width (renderer parity).
            nPenX += anGlyphWidth[i];
        }

        D2Win::Src::D2WinArchive::IMAGE_FreeImage(pFontDC6);

        if (!VPLABEL_BuildDc6(aLabelRaster, nTotalWidth, VPLABEL_HEIGHT, ppOut)) {
            break;
        }
        D2Win::Src::D2WinFont::SetAndReturnLastFont(nPrevFont);
        return;
    } while (0);

    // Fallback: blank 8x8 image keeps the row drawable (and invisible)
    // instead of a null pDC6Main halting the type-0 draw path.
    {
        static uint8_t aBlank[8 * 8]; // zeroed = fully transparent
        if (!VPLABEL_BuildDc6(aBlank, 8, 8, ppOut)) {
            *ppOut = nullptr; // out of memory: the process is failing anyway
        }
    }
    D2Win::Src::D2WinFont::SetAndReturnLastFont(nPrevFont);
}

// ===== Esc-menu row callbacks ================================================

void __fastcall ESCMENU_InitVanillaPlusLabel(D2EscMenuItemStrc * pItem, D2UIMouseMsgStrc * pMsg) {
    (void)pMsg;
    if (!pItem || pItem->pDC6Main) {
        return; // idempotent: ESCMENU_Initialize runs once, but stay safe
    }
    VPLABEL_SynthesizeInto(&pItem->pDC6Main);
}

void __fastcall ESCMENU_ShowVanillaPlusOptions(D2EscMenuItemStrc * pItem, D2UIMouseMsgStrc * pMsg) {
    (void)pItem;
    (void)pMsg;
    // ESCMENU_ShowControlConfig's body, with our flag in UI_CONFIG's place.
    if (ui::GetUIFlag(UI_ESCMENU)) {
        Draw::SetUIFlag(UI_ESCMENU, UI_RESET, 0);
    }
    Draw::SetUIFlag(UI_VANILLAPLUS, UI_SET, 0);
}

// ===== page: back + toggles ==================================================

static void VANILLAPLUS_BackToOptionsMenu() {
    // INPUT_ConfigCancel's shape (minus the key-binding revert): close the
    // page, re-open the Esc menu on the Options table.
    Draw::SetUIFlag(UI_VANILLAPLUS, UI_RESET, 0);
    EscMenu::ESCMENU_ShowMenu(0, 1);
    Sound::SOUND_PlaySound(SOUND_CURSOR_PASS, nullptr, 0, 0, 0);
}

static void VANILLAPLUS_ToggleRow(int nRowIndex) {
    if (nRowIndex < 0 || nRowIndex >= VPP_ROW_COUNT) {
        return;
    }
    const char* szKey = gaVanillaPlusRows[nRowIndex].szKey;
    int nValue = D2VP_GetFeatureToggle(szKey);
    // WritePrivateProfileStringA through D2Client's bridge: writes d2vp.ini
    // and flushes every gate's 2 s hot-read cache (same-frame effect for the
    // draw, <= 1 frame for auto_gold, next save-path resolution for
    // portable_save - matching its ini template wording).
    D2VP_SetFeatureToggle(szKey, nValue ? 0 : 1);
    Sound::SOUND_PlaySound(SOUND_CURSOR_SELECT, nullptr, 0, 0, 0);
}

// ===== page: input handlers ==================================================

void __stdcall INPUT_VP_MouseMove(D2UIMouseMsgStrc * pMsg) {
    if (GAME::Game::GAMEDATA_GetExitFlag3()) {
        pMsg->bHandled = 0;
        return;
    }
    gnVanillaPlusHoverRow = VPPAGE_HitTest(ui::GetMouseX(), ui::GetMouseY());
    pMsg->bHandled = 1;
    pMsg->dwResult = 0;
    Storm::SMsgBreakHandlerChain(pMsg);
}

void __stdcall INPUT_VP_MouseDown(D2UIMouseMsgStrc * pMsg) {
    if (GAME::Game::GAMEDATA_GetExitFlag3()) {
        pMsg->bHandled = 0;
        return;
    }
    int nHit = VPPAGE_HitTest((uint32_t)pMsg->uLParam.sMousePos.nMouseX, (uint32_t)pMsg->uLParam.sMousePos.nMouseY);
    if (nHit >= 0 && nHit < VPP_ROW_COUNT) {
        VANILLAPLUS_ToggleRow(nHit);
    } else if (nHit == VPP_ROW_BACK) {
        VANILLAPLUS_BackToOptionsMenu();
    }
    pMsg->bHandled = 1;
    pMsg->dwResult = 0;
    Storm::SMsgBreakHandlerChain(pMsg);
}

void __stdcall INPUT_VP_MouseUp(D2UIMouseMsgStrc * pMsg) {
    // Swallow the matching button-up so no click leaks to the game layer.
    pMsg->bHandled = 1;
    pMsg->dwResult = 0;
    Storm::SMsgBreakHandlerChain(pMsg);
}

void __stdcall INPUT_VP_EscapeKey(D2UIMouseMsgStrc * pMsg) {
    if (GAME::Game::GAMEDATA_GetExitFlag3()) {
        pMsg->bHandled = 0;
        return;
    }
    VANILLAPLUS_BackToOptionsMenu();
    pMsg->bHandled = 1;
    pMsg->dwResult = 0;
    Storm::SMsgBreakHandlerChain(pMsg);
}

// ===== page: lifecycle =======================================================

void __stdcall VANILLAPLUS_OnToggleUI(int32_t nParam) {
    HWND hWnd = D2gfx::D2GFX::GetHwnd();
    if (nParam) {
        gnVanillaPlusHoverRow = -1;
        _Message::MESSAGE_RegisterEventHandlerBatch(hWnd, gaVanillaPlusInit, 4);
    } else {
        gnVanillaPlusHoverRow = -1;
        _Message::MESSAGE_UnregisterEventHandlerBatch(hWnd, gaVanillaPlusInit, 4);
    }
}

// ===== page: draw ============================================================

static void VPPAGE_DrawCenteredText(const wchar_t* wszText, int nY, int nColor) {
    int nWidth = D2Win::Src::D2WinFont::DRAW_text0((uint16_t*)wszText);
    D2Win::Src::D2WinFont::DRAW_text((uint16_t*)wszText, ScreenSizeX / 2 - nWidth / 2, nY, nColor, 0);
}

static void VPPAGE_DrawValueText(int nBoxX, int nValue, int nRowY) {
    // On = green (2), Off = red (1) - classic D2 locale color codes.
    D2Win::Src::D2WinFont::DRAW_text((uint16_t*)(nValue ? L"On" : L"Off"), nBoxX + VPPAGE_BOX_WIDTH - 0x5a, nRowY, nValue ? 2 : 1, 0);
}

void __stdcall DRAW_VanillaPlusOptions() {
    int nBoxX = 0;
    int nBoxY = 0;
    VPPAGE_GetBoxOrigin(&nBoxX, &nBoxY);

    int32_t nPrevFont = D2Win::Src::D2WinFont::SetAndReturnLastFont(VPLABEL_FONT_ID);

    // Panel: the ControlConfig box geometry, so the page reads as a sibling.
    Engine::GfxInfo::D2GFX_DrawSolidRectExBySize(nBoxX, nBoxY, VPPAGE_BOX_WIDTH, VPPAGE_BOX_HEIGHT, 0, DRAW_TRANSPARENT);
    RECT sFrame;
    sFrame.left = nBoxX;
    sFrame.right = nBoxX + VPPAGE_BOX_WIDTH - 3;
    sFrame.top = nBoxY + 3;
    sFrame.bottom = nBoxY + VPPAGE_BOX_HEIGHT;
    ui::DRAW_Direct_RectFrame(&sFrame);

    VPPAGE_DrawCenteredText(L"Vanilla+ Options", nBoxY + 0x1e, 4);

    // Feature rows: label left, value right, hover highlight.
    int nMouseX = ui::GetMouseX();
    int nMouseY = ui::GetMouseY();
    int nHover = VPPAGE_HitTest(nMouseX, nMouseY);
    for (int i = 0; i < VPP_ROW_COUNT; i++) {
        int nRowY = VPPAGE_RowTopY(i);
        if (nHover == i) {
            Engine::GfxInfo::D2GFX_DrawSolidRectExBySize(nBoxX + 0x12, nRowY, VPPAGE_BOX_WIDTH - 0x24, VPPAGE_ROW_HEIGHT, 0, DRAW_TRANSPARENT);
        }
        D2Win::Src::D2WinFont::DRAW_text((uint16_t*)gaVanillaPlusRows[i].wszLabel, nBoxX + 0x1e, nRowY + 2, 0, 0);
        VPPAGE_DrawValueText(nBoxX, D2VP_GetFeatureToggle(gaVanillaPlusRows[i].szKey), nRowY + 2);
    }

    // Previous Menu row (same highlight/click contract as the toggles).
    int nBackY = VPPAGE_RowTopY(VPP_ROW_BACK);
    if (nHover == VPP_ROW_BACK) {
        Engine::GfxInfo::D2GFX_DrawSolidRectExBySize(nBoxX + 0x12, nBackY, VPPAGE_BOX_WIDTH - 0x24, VPPAGE_ROW_HEIGHT, 0, DRAW_TRANSPARENT);
    }
    VPPAGE_DrawCenteredText(L"Previous Menu", nBackY + 2, 0);

    D2Win::Src::D2WinFont::SetAndReturnLastFont(nPrevFont);
}

} // namespace D2Client::UI::VanillaPlus
