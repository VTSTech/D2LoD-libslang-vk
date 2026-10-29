// vanilla-plus: "Vanilla+ Options" in-game settings page (FEATURES.md row 10).
// Design contract: DRAFT_vanilla_plus_options.md (Route B, above Previous Menu).
// Charter: VANILLA-PLUS.md - additive UI only; every toggle stays opt-in with
// 0-defaults; no D2Game/D2Common edits; no loader edits; no shipped assets.
//
// ===== Route B: the row label (field-QA revision, 2026-10-29) ===============
// Route A (synthesizing a text DC6 from font glyph frames) was retired after
// the first field build returned "label invisible, row clickable". The
// synthesis had fallen back to its blank image, and it carried three latent
// defects that would have broken it even on success:
//   1. DC6 RLE streams are bottom-up - FASTCMP_DecompressRLEWithAlpha walks
//      the destination with -nDstPitch - while the Route A encoder emitted
//      rows top-down, so the label would have rendered vertically flipped;
//   2. font glyph pixels are meant to be drawn through D2GFX_DrawShiftedImage
//      with the font's palette-shift index; a plain DrawImage of copied glyph
//      indices renders them unshifted;
//   3. the 1240-byte encode scratch buffer could not hold the label's RLE
//      stream (~2 KB for a 0x100-wide raster - worst case far more), a
//      stack-smashing bug hiding behind the fallback path.
// The retail esc-menu loop already invokes a per-row fpDrawCallback every
// frame (ESCMENU_DrawMenu, before the row's image draw), so the label is
// rendered there with DRAW_text (font resolved at runtime by measuring the
// retail neighbor label image - see the tunables block and
// VPLABEL_DecideMetrics). The row's
// fpInitCallback still seeds pDC6Main, but only with the field-proven 8x8
// blank placeholder:
// type-0 rows draw pDC6Main unconditionally (a null pointer would halt the
// draw), and the blank frame is exactly the image the first field build drew
// invisibly-but-safely. Same allocator + CELCMP_FixupPointersAndPrepare
// shape as a file-loaded image, so the retail free path (DC6_FreeAndDelete
// -> DeleteCellFile + FreeClientMemory) still tears it down with zero
// special-casing.
//
// ===== cursor handling (field-QA revision, 2026-10-29) ======================
// The first field build registered a WM_MOUSEMOVE (0x200) handler that ended
// in SMsgBreakHandlerChain - it ran first in the message chain and starved
// ui::UI_HandleMousePositionUpdate (the always-on gUIFlagGeneralInit batch),
// which is what updates MouseX/MouseY and refreshes the OS-cursor visibility
// on every move. Result: the on-screen cursor froze ("locked") while the page
// was open. Neither ESCMENU nor ControlConfig registers a 0x200 handler, so
// neither does this page: hover state is derived per frame in
// DRAW_VanillaPlusOptions from ui::GetMouseX/GetMouseY, and the mouse down/up
// handlers keep reading coordinates from the message itself.

#include "D2Client/UI/VanillaPlusOptions.h"

#include "D2CMP/SRC/CelCmp.h"
#include "D2Client/D2Client.h"
#include "D2Client/UI/VanillaPlusCore.h"
#include "D2Client/D2GfxDataStrc.h"
#include "D2Client/Engine/GfxInfo.h"
#include "D2Client/Message.h"
#include "D2Client/Sound/Sound.h"
#include "D2Client/UI/Draw.h"
#include "D2Client/UI/EscMenu.h"
#include "D2Client/UI/ui.h"
#include "D2Sound/Src/D2SoundFast.h"
#include "D2Win/DC6.h"
#include "D2Win/DC6Block.h"
#include "D2Win/Src/D2WinFont.h"
#include "D2gfx/D2GFX.h"
#include "Fog/Fog.h"
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

// Esc-menu row band. The retail draw loop bottom-anchors every row image at
// nItemBottomY = nRowTop + nRowHeight; DC6 draws (font glyph frames included)
// extend upward from the y passed to the renderer, i.e. the y passed to
// DRAW_text anchors the glyphs' bottoms (the baseline for non-descending
// frames). The label baseline is inset from the band bottom so the text sits
// visually centered in the 0x27 band (tunable).
//
// Field-QA timeline for the row label FONT (rev3 -> revE):
//   rev3: font 0xd (FontInGameChat, chat cell)  -> "label shows, ~10% size"
//   rev4: font 2   (Font30)                     -> "closer, ~50% size"
// The five retail rows (SoundOptions / VideoOptions / AutoMapOptions /
// CfgOptions / Previous) are DC6 label IMAGES (DATA\LOCAL\{LANG}\UI\*.dc6),
// so no font id is authoritative for them - only their pixel height is, and
// that data lives in the MPQ, not in this tree. revE therefore stops guessing:
// on the row's first draw it measures the retail neighbor label image's frame
// height and picks the in-tree font whose capital-letter glyph frame height
// matches it (candidates below, probed ascending; ties resolve to the larger
// font). The pick is cached for the process lifetime; later draws just use
// it. If measurement or glyph probes fail wholesale, the fallback is
// Font42 - the largest menu font, which is where the "go bigger" answer the
// 50% datapoint points at lives.
//
// Manual override (escape hatch, no rebuild needed): add to d2vp.ini (the
// same file the [features] gates live in):
//   [ui]
//   label_font = <aFontNames index>
// 1 = Font16, 2 = Font30, 3 = Font42, 7 = Font24; anything outside 0..13
// falls back to the measured pick. Read once, when the metrics are decided.
static const int gaVpLabelFontProbe[3] = { 7, 2, 3 };  // Font24, Font30, Font42 (ascending)
static const int VPLABEL_FONT_FALLBACK = 3;            // Font42 - largest menu font

// Resolved once per process by VPLABEL_DecideMetrics (first row draw).
// -2 = undecided; afterwards a valid aFontNames index 0..13.
static int gnVpLabelFontId = -2;

// Baseline inset above the band bottom. Kept at the rev4 field-accepted
// value: glyph frames are bottom-anchored, so a taller font extends upward
// from the same anchor and the accepted placement carries over to whatever
// font the measurement picks ('p' descenders may reach a few px into the
// inter-row gap - the retail images bottom-anchor at the band bottom and
// behave the same way).
static const int VPLABEL_BASELINE_INSET = 4;

// The Vanilla+ page itself is a ControlConfig mirror and keeps ControlConfig's
// own font (FontInGameChat) - its 0x1a row bands are sized for it. Deliberately
// separate from the row label's resolved font so the esc-menu row and the
// page can't drag each other's metrics around.
static const uint16_t VPPAGE_FONT_ID = 0xd;        // FontInGameChat - ControlConfig's font

// Page layout (mirrors the ControlConfig panel box for a familiar look).
static const int VPPAGE_BOX_WIDTH = 0x26c;
static const int VPPAGE_BOX_HEIGHT = 0x171;
static const int VPPAGE_ROW_PITCH = 0x20;
static const int VPPAGE_ROW_HEIGHT = 0x1a;
static const int VPPAGE_FIRST_ROW_Y = 0x50;        // from box top
static const int VPPAGE_BACK_ROW_Y = 0x124;        // from box top
static const int VPPAGE_TEXT_BASELINE_INSET = 4;   // baseline above row bottom

// [features] keys rendered by the page. Adding a slate feature = +1 entry
// here and +1 template block in ALTTOGGLE_WriteIniTemplate - nothing else.
struct VPPFeatureRow {
    const char* szKey;        // d2vp.ini [features] key
    const wchar_t* wszLabel;  // row label
};
static const VPPFeatureRow gaVanillaPlusRows[4] = {
    { "alt_toggle", L"Alt-Show Items" },
    { "auto_gold", L"Auto Gold Pickup" },
    { "portable_save", L"Portable Saves" },
    { "globe_persistent", L"Persistent Globe Text" },
};
static const int VPP_ROW_COUNT = 4;
static const int VPP_ROW_BACK = 4; // hit-test index of the Previous Menu row

// Input handlers (definitions below; the batch array references them).
// Deliberately NO WM_MOUSEMOVE (0x200) entry - see the header comment block:
// the always-on gUIFlagGeneralInit batch owns 0x200 and must keep running.
void __stdcall INPUT_VP_MouseDown(D2UIMouseMsgStrc * pMsg);
void __stdcall INPUT_VP_MouseUp(D2UIMouseMsgStrc * pMsg);
void __stdcall INPUT_VP_EscapeKey(D2UIMouseMsgStrc * pMsg);

// Input batch registered while the page is open (cube/panel style: dedicated
// handlers, self-contained registration). eEventType: 0=window message,
// 3=key down (ui.h D2UIFlagInitStrc contract).
static D2UIFlagInitStrc gaVanillaPlusInit[3] = {
    { 0x0, 0x201, &INPUT_VP_MouseDown },
    { 0x3, 0x01b, &INPUT_VP_EscapeKey },
    { 0x0, 0x202, &INPUT_VP_MouseUp },
};

// ===== page geometry helpers (kept in one place for draw and hit tests) =====

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

// ===== row placeholder (Route B) =============================================

// The esc-menu draw path for type-0 rows calls IMAGE_DrawDC6ImageOnScreen
// unconditionally (ESCMENU_DrawMenu case 0), so pDC6Main must be a renderable
// DC6 or the draw halts. This placeholder is the exact image the first field
// build ended up drawing (Route A's blank fallback): an 8x8 fully transparent
// frame. Its RLE stream is the deterministic encoding of an all-zero 8x8
// raster - per row one 0x80|8 transparent-run token plus the 0x80 end-of-row
// token - so it is hard-coded instead of runtime-encoded (the Route A encoder
// and its undersized scratch buffer are gone from the shipped code path).
//
// Byte shape (file-loaded DC6 layout):
//   0x00  24-byte DC6 header   (version 6, flags 1, format 0, 1 dir, 1 frame)
//   0x18  frame-offset slot    (0x1C, file-absolute like every loaded image)
//   0x1C  0x20-byte frame header (nFlip=1 retail terminator, 8x8, nLength=16)
//   0x3C  RLE frame data       (16 bytes)
// CELCMP_FixupPointersAndPrepare converts the offset slot to an absolute
// pointer and allocates the per-frame 0x2C bookkeeping row, exactly as it
// does for file-loaded images. FALSE = out of memory (caller leaves pDC6Main
// null; the type-0 draw would halt, but allocation failure is failing anyway).
static BOOL VPLABEL_BuildBlankPlaceholder(DC6** ppOut) {
    *ppOut = nullptr;
    static const uint8_t gaBlankRle[16] = {
        0x88, 0x80, 0x88, 0x80, 0x88, 0x80, 0x88, 0x80,
        0x88, 0x80, 0x88, 0x80, 0x88, 0x80, 0x88, 0x80,
    };
    const int nTotalSize = 0x3C + (int)sizeof(gaBlankRle);
    DC6* pRaw = (DC6*)D2_ALLOC(nTotalSize);
    if (!pRaw) {
        return FALSE;
    }
    memset(pRaw, 0, nTotalSize);
    pRaw->nVersion = 6;       // required by the fixup (nExpectedFormat == -1)
    pRaw->nFlags = 1;         // bit0 required, bit1 clear (pre-fixup state)
    pRaw->nFormat = 0;
    /* nTerm stays zeroed */
    pRaw->nDirections = 1;
    pRaw->nFrames = 1;
    *(uint32_t*)((char*)pRaw + 0x18) = 0x1C; // file-absolute frame offset

    DC6Block* pFrame = (DC6Block*)((char*)pRaw + 0x1C);
    pFrame->nFlip = 1;        // retail frame termination value
    pFrame->nWidth = 8;
    pFrame->nHeight = 8;
    pFrame->nOffsetX = 0;
    pFrame->nOffsetY = 0;
    pFrame->nAllocSize = 8 * 8;
    pFrame->nNextBlock = 0;   // fixup patch slot: overwritten with the row ptr
    pFrame->nLength = (int)sizeof(gaBlankRle);
    memcpy((char*)pRaw + 0x3C, gaBlankRle, sizeof(gaBlankRle));

    // Retail fixup: converts the file-relative offset to an absolute pointer,
    // allocates the 1x1 block row, patches the frame's +0x18 slot. Idempotent
    // (nFlags bit1), exactly what every file-loaded DC6 goes through.
    D2CMP::SRC::CelCmp::CELCMP_FixupPointersAndPrepare(pRaw, ppOut, ".\\UI\\VANILLAPLUS.CPP", 0x0, -1, "vanilla-plus options placeholder");
    if (!*ppOut) {
        D2_FREE(pRaw);
        return FALSE;
    }
    return TRUE;
}

// ===== Esc-menu row callbacks ================================================

void __fastcall ESCMENU_InitVanillaPlusLabel(D2EscMenuItemStrc * pItem, D2UIMouseMsgStrc * pMsg) {
    (void)pMsg;
    if (!pItem || pItem->pDC6Main) {
        return; // idempotent: ESCMENU_Initialize runs once, but stay safe
    }
    // Route B: the visible label is drawn by VANILLAPLUS_DrawRowLabel (the
    // row's fpDrawCallback); this pointer only satisfies the unconditional
    // type-0 image draw with a fully transparent frame.
    VPLABEL_BuildBlankPlaceholder(&pItem->pDC6Main);
}

// ===== row label metrics (revE: measure the retail neighbor) ================

// Height of one glyph's frame in font nFontId (0 = not measurable). Forces
// the standard lazy load through SetAndReturnLastFont (the exact path every
// text draw takes when a font is first used), then reads the glyph's frame
// from the font DC6 through the retail accessors - the same glyph-entry ->
// frame-number lookup D2WINFONT_DrawWideString performs for every drawn
// character. Leaves the current font switched; callers save/restore around
// probe batches.
static int VPLABEL_GlyphFrameHeight(int nFontId, uint16_t wCharCode) {
    if (nFontId < 0 || nFontId >= 14) {
        return 0;
    }
    D2Win::Src::D2WinFont::SetAndReturnLastFont(nFontId);
    // A failed load leaves the cache entry null (SetAndReturnLastFont assigns
    // the loader result unconditionally); FONT_GetCharWidthCallback would
    // then index pGlyphs + ch against a null/garbage table - never
    // dereference that.
    if (!gaFontCache[nFontId].pFontDC6 || !gaFontCache[nFontId].pGlyphs) {
        return 0;
    }
    D2FontGlyphStrc* pGlyph = (*gpfnFontGetCharWidth)(wCharCode);
    if (!pGlyph) {
        return 0;
    }
    DC6* pFontDc6 = (DC6*)gaFontCache[nFontId].pFontDC6;
    D2GfxDataStrc sGfxData;
    memset(&sGfxData, 0, sizeof(sGfxData));
    sGfxData.pDC6 = pFontDc6;
    sGfxData.nDirection = 0;
    sGfxData.nFrameNumber = (int)*(const uint16_t*)(pGlyph->abUnverified + 6);
    DC6Block* pBlock = D2CMP::SRC::CelCmp::CELCMP_GetDC6BlockFromGfxData(&sGfxData);
    if (!pBlock) {
        return 0;
    }
    int nHeight = (int)D2CMP::SRC::CelCmp::GetDC6Height(pBlock);
    return nHeight > 0 ? nHeight : 0;
}

// Frame-0 height of a retail row's label image (the measurement target: what
// the user visually compares our label against). Type-0 rows bottom-anchor
// this exact image into the band, so its height IS the retail label height.
static int VPLABEL_MeasureRowImageHeight(D2EscMenuItemStrc* pItem) {
    if (!pItem || !pItem->pDC6Main) {
        return 0;
    }
    D2GfxDataStrc sGfxData;
    memset(&sGfxData, 0, sizeof(sGfxData));
    sGfxData.pDC6 = pItem->pDC6Main;
    sGfxData.nDirection = 0;
    sGfxData.nFrameNumber = 0;
    DC6Block* pBlock = D2CMP::SRC::CelCmp::CELCMP_GetDC6BlockFromGfxData(&sGfxData);
    if (!pBlock) {
        return 0;
    }
    int nHeight = (int)D2CMP::SRC::CelCmp::GetDC6Height(pBlock);
    return nHeight > 0 ? nHeight : 0;
}

// Resolve the row label font, once per process, on the row's first draw.
//   1. [ui] label_font override from d2vp.ini (validated 0..13).
//   2. Measure the retail neighbor label (row [5] "Previous" - the direct
//      visual neighbor below us, always visible, no expansion/visibility
//      gates; fallback row [0] "SoundOptions") and pick the probed font whose
//      'S' frame height best matches the image height. Capital height is the
//      comparison metric: "Previous" has no descenders, so its baked image
//      height IS its capital extent (+1-2 px of bake padding - the same bias
//      for every candidate, so the argmin stays stable).
//   3. Fallbacks: no measurable image or no probeable glyph -> Font42.
static void VPLABEL_DecideMetrics() {
    if (gnVpLabelFontId != -2) {
        return; // already decided this process
    }

    // Provisional fallback; the paths below keep or replace it.
    gnVpLabelFontId = VPLABEL_FONT_FALLBACK;

    char szIniPath[260];
    int nOverride = -1;
    if (ALTTOGGLE_BuildIniPath(szIniPath)) {
        nOverride = (int)GetPrivateProfileIntA("ui", "label_font", -1, szIniPath);
    }
    if (nOverride >= 0 && nOverride < 14) {
        gnVpLabelFontId = nOverride;
        return;
    }

    int nTargetHeight = VPLABEL_MeasureRowImageHeight(&gaEscMenuOptionsTable[5]);
    if (nTargetHeight <= 0) {
        nTargetHeight = VPLABEL_MeasureRowImageHeight(&gaEscMenuOptionsTable[0]);
    }
    if (nTargetHeight <= 0) {
        return; // keep Font42, nothing to measure against
    }

    int32_t nSavedFont = D2Win::Src::D2WinFont::SetAndReturnLastFont(gaVpLabelFontProbe[0]);
    int nBestFont = -1;
    int nBestDist = 0;
    for (int i = 0; i < 3; i++) {
        int nFontId = gaVpLabelFontProbe[i];
        int nCapsHeight = VPLABEL_GlyphFrameHeight(nFontId, (uint16_t)L'S');
        if (nCapsHeight <= 0) {
            continue;
        }
        int nDist = nCapsHeight > nTargetHeight ? nCapsHeight - nTargetHeight : nTargetHeight - nCapsHeight;
        // "<=" on purpose: the probe order is ascending, so a distance tie
        // resolves to the larger font (the retail labels are the big menu
        // family - when in doubt, go bigger).
        if (nBestFont == -1 || nDist <= nBestDist) {
            nBestFont = nFontId;
            nBestDist = nDist;
        }
    }
    D2Win::Src::D2WinFont::SetAndReturnLastFont(nSavedFont);

    if (nBestFont != -1) {
        gnVpLabelFontId = nBestFont;
    }
}

// Row draw callback (fpDrawCallback), invoked by DRAW_EscMenu every frame
// before the row's image draw. Renders the label through the retail text
// path (DRAW_text, font resolved by VPLABEL_DecideMetrics) instead of a
// synthesized DC6. Geometry mirrors the DRAW_EscMenu per-item formula so the
// text lands in the same band the retail images bottom-anchor into;
// DRAW_text's y anchors the glyph bottoms (DC6 frames draw upward from y),
// hence the small inset above the band bottom.
void __fastcall VANILLAPLUS_DrawRowLabel(D2EscMenuItemStrc * pItem, D2UIMouseMsgStrc * pMsg) {
    (void)pMsg;
    if (!pItem) {
        return;
    }

    // Resolve the label font once (measures the retail neighbor label; no-op
    // after the first call). Must run before the SetAndReturnLastFont below.
    VPLABEL_DecideMetrics();

    // Row band: replicate ESCMENU_DrawMenu's per-item Y formula for the
    // Options table (this row only ever draws while that table is active).
    // nVisibleBefore counts visible rows above ours (expansion gate + per-row
    // fpVisibilityCheck), matching the retail loop's visibility accounting.
    int nVisibleBefore = 0;
    bool bFound = false;
    for (int i = 0; i < gaEscMenuHeaderOptions.nItemCount; i++) {
        if (&gaEscMenuOptionsTable[i] == pItem) {
            bFound = true;
            break;
        }
        if (gaEscMenuOptionsTable[i].nExpansionOnly && !Fog::D2CheckExpansionGame()) {
            continue;
        }
        pfnEscMenuItemVisible pfnVisible = gaEscMenuOptionsTable[i].fpVisibilityCheck;
        if (pfnVisible && !pfnVisible()) {
            continue;
        }
        nVisibleBefore++;
    }
    if (!bFound) {
        return; // foreign row: never ours, draw nothing
    }

    const int nMenuCenterY = ScreenSizeY - 0x50;
    const int nRowTop = gaEscMenuHeaderOptions.nVerticalSpacing * nVisibleBefore
        + (nMenuCenterY / 2 - gaEscMenuHeaderOptions.nVerticalSpacing * gaEscMenuHeaderOptions.nItemCount / 2);
    const int nBaselineY = nRowTop + gaEscMenuHeaderOptions.nRowHeight - VPLABEL_BASELINE_INSET;

    static const wchar_t gwszLabel[] = L"Vanilla+ Options";
    int32_t nPrevFont = D2Win::Src::D2WinFont::SetAndReturnLastFont(gnVpLabelFontId);
    int nTextWidth = D2Win::Src::D2WinFont::DRAW_text0((uint16_t*)gwszLabel);
    D2Win::Src::D2WinFont::DRAW_text((uint16_t*)gwszLabel, ScreenSizeX / 2 - nTextWidth / 2, nBaselineY, 0, 0);
    D2Win::Src::D2WinFont::SetAndReturnLastFont(nPrevFont);
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
// Coordinates come from the message, not from ui::GetMouseX/GetMouseY: the
// chain position of these handlers relative to UI_HandleMousePositionUpdate
// is not something this page controls, and the message payload is always the
// event's own position.

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
        _Message::MESSAGE_RegisterEventHandlerBatch(hWnd, gaVanillaPlusInit, 3);
    } else {
        _Message::MESSAGE_UnregisterEventHandlerBatch(hWnd, gaVanillaPlusInit, 3);
    }
}

// ===== page: draw ============================================================
// Text Y convention: DRAW_text's y is the glyph baseline (DC6 frames draw
// upward from y), so every text call anchors its baseline relative to the
// BOTTOM of its row band - the same convention ControlConfig's row renderer
// uses (baseline at the highlight bottom).

static void VPPAGE_DrawCenteredText(const wchar_t* wszText, int nBaselineY, int nColor) {
    int nWidth = D2Win::Src::D2WinFont::DRAW_text0((uint16_t*)wszText);
    D2Win::Src::D2WinFont::DRAW_text((uint16_t*)wszText, ScreenSizeX / 2 - nWidth / 2, nBaselineY, nColor, 0);
}

static void VPPAGE_DrawValueText(int nBoxX, int nValue, int nBaselineY) {
    // On = green (2), Off = red (1) - classic D2 locale color codes.
    D2Win::Src::D2WinFont::DRAW_text((uint16_t*)(nValue ? L"On" : L"Off"), nBoxX + VPPAGE_BOX_WIDTH - 0x5a, nBaselineY, nValue ? 2 : 1, 0);
}

void __stdcall DRAW_VanillaPlusOptions() {
    int nBoxX = 0;
    int nBoxY = 0;
    VPPAGE_GetBoxOrigin(&nBoxX, &nBoxY);

    int32_t nPrevFont = D2Win::Src::D2WinFont::SetAndReturnLastFont(VPPAGE_FONT_ID);

    // Panel: the ControlConfig box geometry, so the page reads as a sibling.
    Engine::GfxInfo::D2GFX_DrawSolidRectExBySize(nBoxX, nBoxY, VPPAGE_BOX_WIDTH, VPPAGE_BOX_HEIGHT, 0, DRAW_TRANSPARENT);
    RECT sFrame;
    sFrame.left = nBoxX;
    sFrame.right = nBoxX + VPPAGE_BOX_WIDTH - 3;
    sFrame.top = nBoxY + 3;
    sFrame.bottom = nBoxY + VPPAGE_BOX_HEIGHT;
    ui::DRAW_Direct_RectFrame(&sFrame);

    // Title: baseline at boxTop + 0x1e, ControlConfig's exact title anchor.
    VPPAGE_DrawCenteredText(L"Vanilla+ Options", nBoxY + 0x1e, 4);

    // Feature rows: label left, value right, hover highlight. Hover is
    // derived live from the shared mouse position (kept fresh by the
    // always-on gUIFlagGeneralInit 0x200 handler - this page registers none).
    int nMouseX = ui::GetMouseX();
    int nMouseY = ui::GetMouseY();
    int nHover = VPPAGE_HitTest(nMouseX, nMouseY);
    for (int i = 0; i < VPP_ROW_COUNT; i++) {
        int nRowY = VPPAGE_RowTopY(i);
        if (nHover == i) {
            Engine::GfxInfo::D2GFX_DrawSolidRectExBySize(nBoxX + 0x12, nRowY, VPPAGE_BOX_WIDTH - 0x24, VPPAGE_ROW_HEIGHT, 0, DRAW_TRANSPARENT);
        }
        int nBaselineY = nRowY + VPPAGE_ROW_HEIGHT - VPPAGE_TEXT_BASELINE_INSET;
        D2Win::Src::D2WinFont::DRAW_text((uint16_t*)gaVanillaPlusRows[i].wszLabel, nBoxX + 0x1e, nBaselineY, 0, 0);
        VPPAGE_DrawValueText(nBoxX, D2VP_GetFeatureToggle(gaVanillaPlusRows[i].szKey), nBaselineY);
    }

    // Previous Menu row (same highlight/click contract as the toggles).
    int nBackY = VPPAGE_RowTopY(VPP_ROW_BACK);
    if (nHover == VPP_ROW_BACK) {
        Engine::GfxInfo::D2GFX_DrawSolidRectExBySize(nBoxX + 0x12, nBackY, VPPAGE_BOX_WIDTH - 0x24, VPPAGE_ROW_HEIGHT, 0, DRAW_TRANSPARENT);
    }
    VPPAGE_DrawCenteredText(L"Previous Menu", nBackY + VPPAGE_ROW_HEIGHT - VPPAGE_TEXT_BASELINE_INSET, 0);

    D2Win::Src::D2WinFont::SetAndReturnLastFont(nPrevFont);
}

} // namespace D2Client::UI::VanillaPlus
