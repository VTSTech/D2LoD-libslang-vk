// vanilla-plus core gates: d2vp.ini plumbing shared by the opt-in features.
//
// Fork-owned TU (picked up by CMake's GLOB_RECURSE) - extracted from
// D2Client.cpp so the upstream merge surface stays minimal. Do not move this
// code back into upstream files; upstream files only ever call the entry
// points declared here (see docs/merge-upstream.md for the merge playbook).
//
// Charter: every feature is opt-in via [features] keys in d2vp.ini next to
// d2.exe; missing file/section/key or a garbage value always reads as 0 =
// vanilla byte-for-byte. The ini is hot-re-read at most every 2 s
// (GetTickCount gate), and D2VP_ConfigReloaded busts those caches after a
// write so the options page takes effect on the next poll.

#pragma once

struct DC6;

// [resolution-mode] Route-B label primitives for the 1280x720 stop of the
// video-options Resolution row (attached by the row's fpInitCallback
// UI_SLIDER_LoadAndApplyResolution in Storm/_unnamespaced.cpp and drawn by
// its fpDrawCallback UI_SLIDER_DrawResolutionValue in
// D2Client/UI/EscMenu.cpp). The static esc-menu table keeps nSubImageCount
// = 2 on purpose: ESCMENU_LoadLocalizedImages loads one label DC6 per stop
// from the MPQs and no "1280x720" asset exists, so a third static entry
// would halt the menu load. Instead the init callback attaches the blank
// placeholder below into pDC6SubImages[2] at runtime (retail-shape 8x8
// transparent DC6; the menu DC6s are process-lifetime, same as every
// file-loaded label) and the visible label is font-drawn per frame. Same
// Route B the Vanilla+ row label was field-proven with (see
// VanillaPlusOptions.cpp for the Route A post-mortem).
// FALSE = allocation/fixup failure; caller keeps nSubImageCount = 2 and the
// init callback clamps the slider position back to 800x600.
BOOL __stdcall VPRES_BuildBlankPlaceholder(struct DC6** ppOut);
// Pick the in-tree menu font whose capital 'S' glyph frame height best
// matches the frame-0 pixel height of pTargetDC6 (the retail "800x600"
// value image of the same row). Probes Font24/Font30/Font42 ascending
// (same probe list as the Vanilla+ row label), ties resolve to the larger
// font, fallback Font42. Saves/restores the current font around the probes
// and caches the result per process for the given target pointer (the
// retail label DC6 lives from ESCMENU_Initialize to shutdown).
int __stdcall VPRES_PickFontMatchingDC6(struct DC6* pTargetDC6);
// [resolution site-5] TRUE when eResolutionMode renders the retail 800x600
// UI class (the "800*" art set + nScreenMode=1 D2Common layout tables +
// count-based stat/skill button input): mode 2 (retail 800x600), mode 3
// (dormant 1344x700, same class) and mode 4 (Vanilla Plus 1280x720). Every
// retail D2GFX_GetResolutionMode() consumer that tests == 2 / != 2 to pick
// the 800-vs-640 layout routes through here, so a future wide mode is
// added in exactly one place.
BOOL __stdcall VPRES_IsWideLayoutMode(DWORD eResolutionMode);
// [resolution site-5] Horizontal offset that centers the exactly-800-px-wide
// retail UI class on the current playfield: (ScreenSizeX - 800) / 2, clamped
// to 0 on narrower widths. 0 at modes 0/1/2 (retail math stays
// byte-identical), 240 at mode 4 (1280x720), 272 at mode 3 (1344x700).
// Add it to left-anchored X coordinates and subtract it from
// ScreenSizeX-relative right anchors. Screen-center-anchored widgets and
// everything hanging off gnPanelOffsetX/gnPanelOffsetY (set once per frame
// in DRAW_UI) needs no per-site change.
int __stdcall VPRES_GetWideUiOffsetX();
// [resolution site-6] Vertical companion to VPRES_GetWideUiOffsetX: the Y
// half of the "move the 800-class UI as one rigid unit" rule. The 800-class
// panel art is bottom-anchored ("gnPanelOffsetY - const + ScreenSizeY",
// bar pieces at ScreenSizeY) and follows the screen height on its own, but
// every FIXED Y coordinate - table rects from Belts.txt/inventory.txt and
// "const - gnPanelOffsetY" text/hit-test expressions - stays in 800x600
// space and lands exactly one height-delta too high. This offset is that
// delta: ScreenSizeY - 600, clamped to 0. 0 at modes 0/1/2 (retail math
// stays byte-identical), 100 at mode 3 (1344x700), 120 at mode 4 (1280x720),
// 168 at mode 5 (1366x768) [R112], 480 at mode 6 (1920x1080) [R115].
// Add it to fixed Y coordinates of the 800-class UI. DO NOT add it to
// bottom-anchored expressions (anything already containing "+ ScreenSizeY")
// or change gnPanelOffsetY (-60) - double-shifting is the failure mode.
int __stdcall VPRES_GetWideUiOffsetY();

// [vanilla-plus no-panel-shift] (issue #26 / R95) d2vp.ini next to d2.exe,
// [features] no_panel_shift=0|1 - retail slides the whole game world a
// quarter screen sideways when exactly one panel (character screen /
// inventory) is open, so the action re-centers in the uncovered strip
// (both panels open = no slide). That retail assumption is exact only when
// the open panel reaches the screen edge (640x480/800x600); at the wide
// stops (1280x720 / 1344x700 / 1366x768 [R112] / 1920x1080 [R115]) the 800-class UI floats
// inboard, the
// slide drags world out from under the visible outer margins, and every
// margin artifact of issue #26 lives on that panned path (the perspective
// projection anchors on the player and expands from the screen center, so
// the pan rides half a dozen independent channels: MouseYRelatedB, the
// pView walk rect, the four roof/floor light-grid half-screen adjusts, the
// weather clip switches, the automap border offset and the renderer
// quarter offsets). no_panel_shift=1 renders the world exactly like the
// field-proven both-panels-open case (no slide, full unpanned view rect)
// on the wide stops and lets the panel float over it; retail 640x480 /
// 800x600 always keeps the vanilla slide. Default/missing/garbage = 0 =
// vanilla slide (charter rule 1). Hot-reads at most every 2 s like every
// gate.
BOOL __stdcall PANELSHIFT_NoShiftWorld();
// Split-side neutralizer for the world-draw call sites: returns 0 (= border
// none: full-width walk rect, zero world pan, zero renderer offset - the
// renderers receive 0 and take their own border-0 full-screen path) for
// border 1/2 while PANELSHIFT_NoShiftWorld() is active, else eBorderType
// unchanged. Border 3 passes through - it already renders unpanned. World
// culling and clipping only: UI/interaction consumers (panel click rects,
// label culls, selection guards) keep reading the real border type.
// R100: the unit-sprite draw path routes its real-border read through this
// too - the Ghidra-1864+ retail bodies hand-pan sprites/shadows/selection
// rings/hover text +/-quarter in the perspective branch and must follow the
// border-0 camera whenever the world does (no_panel_shift=1).
uint32_t __stdcall PANELSHIFT_BorderForWorld(uint32_t eBorderType);

// [vanilla-plus alt-toggle] Config-gated Alt-key ground-item-label toggle
// (D2R "Show Items: Toggle" parity). d2vp.ini next to d2.exe:
//   [features]
//   alt_toggle=1
// Default/missing file/missing section/missing key/garbage value = 0 =
// vanilla hold-Alt behavior byte-for-byte (Principle 1: opt-in, config-gated).
// The ini is re-checked at most every 2000 ms via GetTickCount, mirroring the
// wrapper's ~2 s conf hot-reload cadence. A missing d2vp.ini is created with
// a documented all-defaults template: once per process at client boot
// (ApplicationMain) and lazily before the first config read, so the opt-in
// keys are discoverable without reading any docs. Auto-repeat never reaches
// the handlers (CmdTbl.cpp filters lParam bit 30 upstream), so the keydown
// handler fires exactly once per physical press.
BOOL __stdcall ALTTOGGLE_Active();   // config: toggle mode on?
BOOL __stdcall ALTTOGGLE_Latched();  // current latch value
void __stdcall ALTTOGGLE_Flip();     // keydown: latch = !latch
void __stdcall ALTTOGGLE_Reset();    // InitializeUI: latch = FALSE
void __stdcall ALTTOGGLE_EnsureIni(); // create d2vp.ini template if missing
// [vanilla-plus alt-toggle] True while labels are shown by the toggle LATCH
// (never true in vanilla hold mode). While latched, the ground-label renderer
// must not own the click selection (vanilla DRAW_AltItems clears the selected
// unit every frame the cursor is not over a label, which turns every click
// into a walk), and ground items stay hit-testable so a label click picks the
// item up. D2R "Show Items: Toggle" parity: labels stay up, clicks still
// attack/interact.
BOOL __stdcall ALTTOGGLE_LabelClickThrough();
// Resolve "<exe dir>\\d2vp.ini". GetModuleFileNameA always yields a fully
// qualified path; refuse the (unreachable) dir-less case rather than fall
// back to a CWD-relative file. Shared by the [features] gates and the
// options page's [ui] label_font reader. FALSE = unresolvable.
BOOL __stdcall ALTTOGGLE_BuildIniPath(char* szIniPath);
// [vanilla-plus auto-gold] Config-gated walk-over gold pickup (D2R "Gold
// Pickup" parity). d2vp.ini next to d2.exe, [features] auto_gold=0|1 -
// default/missing file/missing key/garbage = 0 = vanilla byte-for-byte.
// While on, gold piles inside the server's own instant-pick radius are
// picked up through the literal click path (client 0x16 -> unmodified
// D2Game pick code); suppressed in combat modes; a fire can never start
// the retail walk-to-item mode (strict radius + collision parity).
BOOL __stdcall AUTO_GOLD_Active();   // config: auto_gold on?
void __stdcall AUTO_GOLD_Tick();     // per-frame trigger, called from DRAW_UI
// [globe-persistent] always-on orb text (FEATURES.md row 1, repurposed
// post-0.0.4). The 0.0.4 orb-face readout was removed after the first field
// round - retail already prints the same cur/max above each orb on hover,
// so ours was a duplicate display. With [features] globe_persistent=1,
// Panel.cpp's two orb-text draw conditions also fire without hover, so
// retail's above-orb Life/Mana text shows persistently; 0 = vanilla
// hover-only. Display-only, no state, no input changes.
BOOL __stdcall VANILLAPLUS_GlobeTextPersistent(); // per-frame, Panel.cpp draw conditions
// [vanilla-plus item-ilvl] Optional " (N)" suffix on the tooltip's item
// name (D2R "Advanced Tooltips" parity, the ilvl readout half). d2vp.ini next
// to d2.exe:
//   [features]
//   item_ilvl=1
// Default/missing file/section/key/garbage value = 0 = vanilla byte-for-byte:
// the formatter returns without touching the name buffer, so the upstream
// builders (Inv.cpp LoadItemDesc identified + unidentified paths and
// ITEM_BuildSetItemTooltip) proceed exactly as retail. While on, the hovered
// item's name line reads "Long Sword (12)" - the suffix is appended to
// the GetItemName buffer BEFORE the caller's AppendAsWideChar color wrap, so
// it renders in the item's own name color (gold on uniques, green on sets).
// One line, no extra tooltip row, same coverage as the tooltip line it
// replaces. Data source: the client's own item unit, D2Common's parsed item
// data (same struct that feeds the retail req-level line) - ilvl travels in
// the item bitstream on every path that reaches this tooltip (dropped,
// bought, gambled, traded, socketed, mercenary). The suffix is a hardcoded
// ASCII wide string: the vanilla string table has no such entry and the fork
// never edits MPQ data. Name buffers have big slack (real names cap near 40
// chars vs 0x80-0x400 buffers; GetItemName truncates at nSize), and the
// suffix adds <= 7 wchars. Display-only, no state, no input changes; the
// tooltip is rebuilt every frame the cursor dwells, so the 2 s hot-read gate
// below applies the ini flip without any rebuild trigger.
BOOL __stdcall ITEMILVL_Active(); // config: item_ilvl on?
// Appends " (N)" to the caller's item-name wide buffer, or returns
// untouched when the gate is off / the name is empty / the item carries no
// level. Called from Inv.cpp's three GetItemName sites right after the name
// is built and before the color wrap; the suffix inherits the name's color
// from the caller's own AppendAsWideChar. Gate on: the name's trailing
// spaces/tabs are trimmed off first (R59) - the GetItemName composition
// glues string-table entries and some (magic affix words, unique names) end
// with their own space, which read as a double gap before (N).
void __stdcall ITEMILVL_AppendNameSuffix(struct D2UnitStrc * pItem, uint16_t * wszName);
// [vanilla-plus shift-compare] Hold-Shift equipped-item compare tooltip
// (FEATURES.md row 0b, R125). D2R hover-compare parity: with
// [features] shift_compare=1, holding the key bound to retail cmd 0x24
// (CMD_SetClickModifier, default VK_SHIFT - a keybind remap is honored)
// while an equippable item is hovered in a panel draws a SECOND tooltip
// beside the hovered item's: the equipped item for the matching body slot.
// Side-by-side viewing only - no stat-delta lines, that is D2R's own
// compare scope (diablo2.io launch-era statement, SHIFT-COMPARE-PLAN.md
// Sources). Placement: left of the primary with a 4 px gap, falling back
// to the right when that leaves the screen; the same X/bottom clamp
// idioms Draw_UnitNameUnderCursor applies.
//
// Coverage (owner decision 2026-11-06, "shop stock included"): every
// context that publishes through Inv.cpp's single LoadItemDesc call site
// - player inventory, stash, cube, vendor shop (BOTH your own items and
// the vendor's stock: the stock hover takes the shop-narrow name+price
// builder branch, and the compare capture runs right after either branch
// publishes), trade screen. Hovering the paperdoll never compares (those
// items are already equipped); hovering a non-equippable (potion, charm,
// jewel) resolves no body slot and draws no box; rings compare against
// the first occupied of {6,7} (retail's first-fit order); with both arm
// cells occupied the same-kind slot wins (R126: a shield/quiver hover
// compares the off-hand bodyloc 5, a weapon hover the primary bodyloc 4 -
// weapons and shields share the ItemTypes body pair rarm/larm, so kind
// not bl order picks the box, retail's own auto-equip convention);
// weapons are 2H-aware (a 2H occupying both arms describes from slot 4, the
// same paired-cell convention INV_UpdateBodySlotUnderCursor uses). Equipped
// set items compose through LoadItemDesc's own quality-5 route into
// ITEM_BuildSetItemTooltip, so the compare box carries the set-bonus
// lines. The compare is one composed tooltip per frame: the capture
// saves pItemToDescribe + g_nCurrentInventoryPage (+ the primary's
// published DisplayedTooltip), swaps in the equipped item, calls
// LoadItemDesc(pPlayer, 1), copies the result into the second channel
// (D2WinFont.cpp gVpCompareTooltip, drawn by DRAW_CompareTooltipUnderCursor
// right after the primary draw call), then restores everything - zero
// footprint on the upstream builders themselves. Default/missing/garbage
// ini = 0 = retail byte-for-byte (charter rule 1); 2 s hot-read like
// every gate; while a panel is open held-Shift is inert in world space,
// and the click-modifier semantics of Shift are untouched.
BOOL __stdcall SHIFTCOMPARE_Active();      // config: shift_compare on?
void __stdcall SHIFTCOMPARE_CompareForHover(); // per-frame trigger, Inv.cpp after the LoadItemDesc publish
// [vanilla-plus options page] d2vp.ini feature-toggle bridge (row 10). The
// page reads/flips the same [features] keys the gates consume; writes use
// WritePrivateProfileStringA and flush every gate's 2 s hot-read cache.
int __stdcall D2VP_GetFeatureToggle(const char* szKey);   // 0 = off/missing/garbage
void __stdcall D2VP_SetFeatureToggle(const char* szKey, int nValue); // write + flush
void __stdcall D2VP_ConfigReloaded();                     // bust all 2 s gate caches
// Boot-time consistency audit of the Vanilla+ injection points (count bumps,
// table rows). Halts loudly with a d2boot.log line when a merge dropped an
// edit - see docs/merge-upstream.md. Cheap; called from ALTTOGGLE_EnsureIni.
void __stdcall VANILLAPLUS_SelfCheck();
// [vanilla-plus shared-stash] Client side of the shared stash (track N
// round 1 = one page; two shared pages + an outlined pager label since R75,
// TEN shared pages since R82; server half lives in
// D2Game/PLAYER/VanillaPlusStash.cpp).
// d2vp.ini next to d2.exe, [features] shared_stash=0|1 - default/missing
// file/missing key/garbage = 0 = vanilla byte-for-byte (no tab pixels, no
// clicks, no state). While on, the stash panel gains an OUTLINED "Shared"
// text tab (top-right, no hotkey by owner decision): clicking the word
// flips the visible stash page by sending the C->S 0x4F Click-Button packet
// with the out-of-retail-range button id 0x7D and the ABSOLUTE desired page
// in the param word (0 personal, 1..10 shared pages 1..10); the <
// and > arrows beside the word step one page at a time. The server
// materializes the requested shared page into the retail stash grid
// (page 4) - every retail stash flow keeps working - and borrows the
// goldbank stat for the shared gold pool while a shared page is active, so
// the retail deposit/withdraw gold buttons move shared gold untouched. The
// save flush (PlayerSave::SaveGameAllGameTypes VP site) parks all ten shared
// pages into _d2vp_shared.sss (v2 format; pre-R82 two-page and pre-R75
// one-page files read back - the pages the file doesn't carry start empty)
// and restores the personal page + personal gold before any .d2s is built
// (I1: character saves stay retail-pure).
BOOL __stdcall VPSHAREDUI_FeatureActive();   // config: shared_stash on?
int __stdcall VPSHAREDUI_IsSharedPage();     // client mirror: 0 personal, 1..10 = shared page on screen
void __stdcall VPSHAREDUI_ResetForNewGame(); // entering a game starts personal
// Hit-test + pager for the tab composite (word + < > arrows). TRUE = the
// click hit an element whose target differs from the current page: the
// mirror is updated, the 0x4F/0x7D toggle is sent with the desired page,
// and the caller must swallow the message (VP site in
// INV_HandleStashLeftClick). FALSE = miss or no-op - retail proceeds.
BOOL __stdcall VPSHAREDUI_HandleStashClick(int nMouseX, int nMouseY);
// Per-frame tab draw (VP site in Inv.cpp's stash draw branch, right after
// the gold-button draw). Draws the outlined word + pager arrows; draws
// nothing when the gate is off.
void __stdcall VPSHAREDUI_DrawTab();

// [vanilla-plus nointro] R86: the -nointro command-line switch. Opt-out by
// launcher argument (not d2vp.ini - it only means something when the user
// passes it), parsed ONCE per process from GetCommandLineA(): a
// whitespace-separated token exactly "-nointro" (case-insensitive; tokens
// of 32+ bytes can never match) sets the flag, anything else leaves the
// vanilla boot untouched. First query writes one [d2boot] mark so the field
// log pins the switch. Consumers: the two boot/title cutscene entries in
// D2Launch/MainMenus.cpp (PlaysBlizzardNorthCutScene and
// UIMENU_PlayIntroAndReturnToMainScreen) read this and, when it is set,
// never enter BeginCutSceneAtResolutionMode0 - the retail begin-failed
// fallthrough each site already has (no cutscene, straight to the next
// screen) becomes the boot path, which skips the Blizzard + Blizzard North
// logo .biks, the first-run D2/LoD intro .bik and the title-screen
// (click/timer) d2intro .bik. In-game quest cutscenes use a different call
// path and are NOT affected. Returns 0 without the switch: vanilla
// byte-for-byte.
BOOL __stdcall VPNOINTRO_IsRequested();
