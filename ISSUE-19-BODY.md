# ISSUE-19-BODY.md — paste-ready body for issue #19

> Paste everything below the line into https://github.com/VTSTech/D2LoD-libslang-vk/issues/19 .
> Status header for the issue body: **ROOT-CAUGHT + FIXED R61 — FIELD-CONFIRMED (owner: "R61 Confirmed fixes #19") — the save was never the trigger; the resolution row was.**

---

**[Bug] Saving a game and then trying to enter a game again crashes the game** (ROOT-CAUGHT + FIXED + FIELD-CONFIRMED R61 — CLOSED)

## Symptom

Owner report (verbatim): *"Saving a game and then trying to enter a game again crashes the game."*

Shape: a session is played, saved (Save & Exit), the client returns to the main menu / character select in the same process — and the next **enter-game on that process** crashes. First entry after a fresh process boot is fine.

## Root cause (R61 — crash report + `d2exe.map` symbolization + source)

The field crash report (`HALT line=1281 (0x501)` + stack walk to `ESCMENU_LoadLocalizedImages_HaltTail` ← `ESCMENU_Initialize` ← `InitializeUI`) lands in the **escape-menu localized-image loader**, at its fatal guard for an **empty sub-image path**. The Vanilla+ **Resolution row** (VIDEO OPTIONS, 640x480 / 800x600 / **Vanilla Plus 1280x720**) is the trigger:

- The row is a static table entry with `nSubImageCount = 2` and two MPQ-backed stops (`640x480`, `800x600`). The third stop has **no MPQ asset** — the "1280x720" value is a runtime-synthesized blank DC6, attached by the row's init callback (`UI_SLIDER_LoadAndApplyResolution`): it writes the blank into `pDC6SubImages[2]` and bumps `nSubImageCount` to **3**, inside the static table. `szSubImage3Path` stays **empty** (there is no file path for a synthesized stop).
- **`ESCMENU_Initialize` re-runs on every game entry** (the 0x01 new-game packet → `InitializeUI`). Game entry #1: the loader runs with count 2 (clean), *then* the callback mutates the table. Game entry #2: the loader now reads count 3, walks three sub-paths, hits the empty third path — and retail's fatal guard `ESCMENU_LoadLocalizedImages_HaltTail(0x501)` kills the process. Deterministic. **The save is irrelevant: any second game entry in one process dies.**
- This also explains the field log exactly: session 1 joined clean (init #1 = the process's first `InitializeUI`), and session 2 died 47 ms after the first server tick with **no** `s2c recv 0x01` line (the crash is inside the 0x01 handler's `InitializeUI`, before the dispatch mark re-prints in the non-load state).

## What R61 ships (the fix)

- **`D2Client/UI/EscMenu.cpp` — the loader's two data guards degrade instead of halt** (the halt tail stays for the two programmer-error sites, 0x4e8/0x4e9):
  - empty sub-image path (the 0x501 site): one `escmenu: <table> item <i> stop <j> path empty - prior dc6 kept [R61]` mark, skip the load, **keep the previously loaded DC6 in the slot** — the runtime blank survives every re-init, so the 720p stop keeps working;
  - sub-image count out of range (the 0x4fe site): clamp to [0,4] + mark — the slot writes stay in-bounds no matter what the field now holds.
- **The type-1 draw site** (the row renderer for toggle-style rows) now skips NULL sub-image slots and clamps the slot index — a degraded first init draws a missing toggle image instead of crashing.
- Zero change to healthy tables: every retail row (3DSound, EAX, NPCSpeech, AutoMap toggles) parses with non-empty paths and in-range counts and is untouched.

## Field verification (CONFIRMED 2026-11-01 — SMOKE-CHECKLIST §G items 18–20)

**Owner verdict (R61 build): "R61 Confirmed fixes #19."** The fix-proof run (two game entries in one process) passed — the second entry loaded; no `halt report:`, no `d2crash-*.txt`. The checks below are kept as the per-build regression protocol:

1. Boot → enter any game → ~30 s → Save & Exit → same process, Play again. `d2boot.log` line 1 reads `Vanilla+0.0.6-R61`; the second entry **loads and plays**; the log shows exactly one `escmenu: video item 1 stop 2 path empty - prior dc6 kept [R61]` at the second entry; no `halt report:` / `d2crash-*.txt`.
2. In game 2, VIDEO OPTIONS → Resolution: three stops, the 720p stop selectable, the "1280x720" label visible.
3. In-game ESC menu draws every row normally.

## Notes / residual watches

- The R60 field log's **`roster heal:` 64-node cap marks** (also present in the R54 log, always during the first session's load) are a **separate, standing watch** — the upstream corruptor (context H25) — and are NOT this crash: the crash reproduces with the roster fully out of the way, and the R60 exit probe showed clean all-NULL roster state. The corruptor watch stays armed (`roster heal:` in any log → send it).
- The R60 probes/guards (roster state at exit/join, merc petdata guard, corpse-flags determinism) all stay shipped — they were correct hardening regardless of this root cause.
- Archaeology note: the Resolution row ships with the 0.0.5-era 720p branch (Route-B label, `ddb2dbc`); why the R54 rejoin field pass did not trip the halt (the mutation must have been inactive or failing there) is unresolved under the squashed history — recorded as an open archaeology item; it does not affect the fix.

## Cross-refs

- Standing watch, roster corruptor: handoff §6 (first 0.0.6-era sighting 2026-10-31, R54 field log; again in the R60 log).
- Issue #13 (A1Q1 turn-in) — the R54 field log that exposed the corruptor's marks; closed separately.
- Suite: 252/252 (`tools/check-vanilla-plus.sh`), self-ID `Vanilla+0.0.6-R61`. Overlay: `vanilla-plus-0.0.6-R61-issue-19-rootcause-fix.zip`.
