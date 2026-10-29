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
