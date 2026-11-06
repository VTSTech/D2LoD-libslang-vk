// vanilla-plus shared stash: server-side container (track N, round 1).
//
// Fork-owned TU (picked up by CMake's GLOB_RECURSE) - do not move this code
// into upstream files; upstream files only ever call the entry points
// declared here from thin VP-marked sites (see docs/merge-upstream.md).
//
// Owner constraints (SHARED-STASH-PLAN.md, 2026-11-01):
//   1. "the stash should have it's own save"  - the shared pool lives in its
//      own file (<save dir>\_d2vp_shared.sss), never inside the character
//      .d2s.
//   2. "vanilla+ character saves need to be compatible with retail" - hard
//      invariant I1: a Vanilla+ save loads in retail, byte-for-byte. The
//      shared page therefore NEVER survives into a .d2s: the save-flush
//      entry point below parks the shared set into the .sss and restores
//      the personal set before MakeSaveFile runs.
//
// Design (round 1 - one page, retail-shaped; two shared pages since R75,
// ten since R82):
//   While the shared page is active, the player's INVPAGE_STASH (page 4)
//   inventory holds the SHARED set - materialized from the .sss file - and
//   the personal set waits in a memory snapshot. Every retail stash flow
//   (grid draw, 0x19 pick, 0x18 place, gold buttons) works on it unchanged.
//   Shared gold borrows the retail UNITSTAT_goldbank stat while the page is
//   active (retail deposit/withdraw buttons + display work as-is); the
//   swap/restore happens only at the toggle and the save flush. Toggling
//   back (or saving) serializes the shared set back into the .sss using the
//   retail per-item encoder, so an item can move file -> .d2s -> file
//   forever without a conversion step.
//
// Scope guard: the feature engages only when [features] shared_stash=1 in
// d2vp.ini AND the game is SGAMETYPE_SINGLEPLAYER. Every other game type is
// inert (Battle.net / TCP-IP open never see the toggle handled).

#pragma once

struct SGAMEDATA;
struct D2UnitStrc;

// [vanilla-plus shared-stash] The one out-of-retail-range button id the
// client sends via C->S 0x4F (Click Button) to toggle the shared page.
// Retail ClickButton routes unknown ids to the trade-path rejection with an
// interact-object target, so a retail server silently ignores it; our VP
// site intercepts it first. Param word = desired page (0 personal, 1..10
// shared pages 1..10 - ten since R82).
constexpr uint16_t VPSHARED_BUTTON_TOGGLE = 0x7D;

// TRUE when the VP site in PlayerTrade::ClickButton must divert this button
// to VPSHARED_OnClickButton. Requires: the VP toggle button id, the feature
// ini gate on, and a single-player game. Zero state reads when the ini gate
// is off (I2: vanilla parity when off).
BOOL __stdcall VPSHARED_ShouldInterceptClickButton(struct SGAMEDATA * pGame, struct D2UnitStrc * pUnit, int16_t nButtonId);

// Toggle handler for the VP button. dwParam = the ABSOLUTE desired page
// (0 personal, 1..10 shared pages 1..10 - two since R75, ten since R82;
// anything above the page count is ignored with a d2boot mark). Idempotent: requesting the
// already-active page is a no-op. Refuses while the player is dead or
// trading. Does the full swap: snapshot + dematerialize the current page-4
// set, materialize the other set, resync the client (the removal packets
// ride ITEM_RemoveItemFromCurrentLocation; the additions are pushed with
// Send0x9CItemPutInContainer), swap the goldbank borrow on
// personal<->shared edges, and on shared->personal also writes the .sss
// file (all ten pages, v2 format).
void __stdcall VPSHARED_OnClickButton(struct SGAMEDATA * pGame, struct D2UnitStrc * pUnit, int16_t nButtonId, uint32_t dwParam);

// Park the shared page before any .d2s is built (I1). Called from the thin
// VP site at the top of PlayerSave::SaveGameAllGameTypes. When the shared
// page is active for this game: dematerialize the shared set into the .sss
// (with the shared gold absorbed out of UNITSTAT_goldbank), restore the
// personal set + the personal goldbank snapshot, so MakeSaveFile serializes
// retail-only bytes. When the page is inactive or the feature is off this
// is a zero-op (no file IO, no marks).
void __stdcall VPSHARED_FlushBeforeSave(struct SGAMEDATA * pGame, struct D2UnitStrc * pUnit);
