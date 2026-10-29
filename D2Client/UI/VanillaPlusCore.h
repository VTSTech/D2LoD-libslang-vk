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
