// vanilla-plus: "Vanilla+ Options" in-game settings page (FEATURES.md row 10).
//
// Owner decisions (2026-10-29, DRAFT_vanilla_plus_options.md section 6, with
// the field-QA revision documented in DRAFT section 9):
//   - Route B label: the Options-page row's fpDrawCallback renders the label
//     with DRAW_text (font 0xd) every frame; the row's fpInitCallback seeds
//     pDC6Main with an 8x8 blank in-memory DC6 (type-0 rows draw pDC6Main
//     unconditionally). No shipped asset files, no loader edits, retail free
//     path frees the placeholder (Route A's glyph-synthesized DC6 was retired
//     in field QA: invisible in practice, plus bottom-up/palette-shift and
//     encode-buffer defects - see VanillaPlusOptions.cpp header).
//   - Placement: index 4 of gaEscMenuOptionsTable, directly above the
//     "Previous Menu" row.
//   - Input: the page registers WM_LBUTTONDOWN/UP and VK_ESCAPE only. No
//     WM_MOUSEMOVE handler: the always-on gUIFlagGeneralInit batch owns 0x200
//     (mouse position + OS-cursor refresh); a chain-breaking 0x200 handler
//     froze the cursor in field QA. Hover is computed per frame in the draw.
//
// The page itself mirrors the ControlConfig precedent (UI_CONFIG): a UI flag
// (UI_VANILLAPLUS) plus a draw callback and a message-handler batch. Rows
// toggle [features] keys in d2vp.ini through D2Client's exported helpers and
// flush every gate's 2 s hot-reload cache, so a toggle takes effect the same
// frame (auto_gold within its next 200 ms scan; portable_save on the next
// save-path resolution, as its ini template documents).

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include "d2_platform.h"

// Forward declarations
struct D2UIMouseMsgStrc;
struct D2EscMenuItemStrc;

namespace D2Client::UI::VanillaPlus {

// Esc-menu row callbacks (wired into gaEscMenuOptionsTable in
// globals/D2Client.cpp; signatures match pfnEscMenuItemAction).
// Seeds pItem->pDC6Main with the 8x8 blank placeholder DC6 (Route B; the
// visible label is the fpDrawCallback's job). Idempotent: a second call
// returns without touching the pointer.
void __fastcall ESCMENU_InitVanillaPlusLabel(struct D2EscMenuItemStrc * pItem, struct D2UIMouseMsgStrc * pMsg);
// Row draw callback (fpDrawCallback): renders "Vanilla+ Options" centered in
// the row band with DRAW_text (font 0xd), baseline anchored above the band
// bottom like every retail esc-menu sprite.
void __fastcall VANILLAPLUS_DrawRowLabel(struct D2EscMenuItemStrc * pItem, struct D2UIMouseMsgStrc * pMsg);
// Row click: close the Esc menu flag, open the Vanilla+ page. Mirrors
// ESCMENU_ShowControlConfig's body (UI_RESET the menu, UI_SET the page).
void __fastcall ESCMENU_ShowVanillaPlusOptions(struct D2EscMenuItemStrc * pItem, struct D2UIMouseMsgStrc * pMsg);

// DRAW_UI stage (keyed on GFX_bShowUIFlag.VanillaPlus, next to the
// ConfigControls dispatch). Title, one row per shipped [features] key with
// its current On/Off value, and a Previous Menu row. Click handling lives in
// the message handlers registered by VANILLAPLUS_OnToggleUI.
void __stdcall DRAW_VanillaPlusOptions();

// Panel lifecycle, called from Draw.cpp's SetUiFlagInit /
// UI_UnregisterHandlersForPanel switches (the UI_CONFIG pattern).
// nParam != 0: register this page's message handlers (mouse down/up + Esc).
// nParam == 0: unregister them.
void __stdcall VANILLAPLUS_OnToggleUI(int32_t nParam);

} // namespace D2Client::UI::VanillaPlus
