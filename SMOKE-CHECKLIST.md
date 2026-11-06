# Vanilla+ Smoke Test — in-game checklist

> **Running this checklist:** GitHub renders the boxes below read-only in the file view — task-list checkboxes are only live inside issues. To run a pass, open **Issues → New issue → "Vanilla+ Smoke Test"**: the template ([.github/ISSUE_TEMPLATE/smoke-test.md](.github/ISSUE_TEMPLATE/smoke-test.md), generated from this file by [tools/gen-smoke-issue-template.py](tools/gen-smoke-issue-template.py)) pre-fills every check as a clickable box, with a Verdict section to fill at the end. This file stays the canonical reference; when a round changes a check, re-run the generator and commit both files.

Build: **Vanilla+0.0.8-R128** (confirm via line 1 of `d2boot.log` before starting). The per-round build history (R74 → R128, one line per round) and the closed-section narratives live in **HISTORY.md H43**. **The last full-belt pass: the R104 build (2026-11-04, [issue #28](https://github.com/VTSTech/D2VP-QoL/issues/28)) — every §A–§L check green**; the R105–R128 chain carries per-round field passes on the touched surfaces (resolution stops, wide-view, D3D fullscreen — R124 verified, shift-compare — R125/R126 verified 2026-11-06, vpdiag retirement R127 = no field ask, gamble fix — R128 verified 2026-11-06, §M, owner-closed [#32]).
Core path takes ~15 min. Run top to bottom — the fast checks come first.

**Known-fail — none.** (Resolved 2026-11-03, [issue #8](https://github.com/VTSTech/D2VP-QoL/issues/8): the necro skeleton spawn tint is FIELD-VERIFIED on the R90b/R91 build — "Skeletons are finally white again" — and is now the standing §B item-5 regression check. The 2026-11-03 D3D registry-corruption episode was owner-environment, not a build bug — see the watch note in §L.)

---

## Setup (once, ~1 min)

- [ ] **0.** Confirm the right exe: line 1 of `d2boot.log` must say `Vanilla+0.0.8-R128`. If it says an older round (or still carries the `0.0.6` prefix), wrong exe — don't test further.

## A. Boot & menus (~3 min)

- [ ] **1. Intro & title:** intro video plays *with sound* → title music plays → character select renders.
- [ ] **2. Options page:** `Esc → Options → "Vanilla+ Options"` — page appears, toggles flip when clicked (nine feature rows — Show Items Toggle / Auto Gold Pickup / Portable Saves / Persistent Globe Text / Item Level Tooltips / Shared Stash / No Panel World Shift / Mana Burn Fix / Shift Item Compare — plus Previous Menu), and the page fits 640x480 with no row overlap. Round trips: flip **Mana Burn Fix** On → a Mana Burn unique's melee hit stops emptying the mana pool within ~2 s; Off → the retail 256x drain returns. Flip **No Panel World Shift** On → at 1280x720 the world stays put with one panel open; Off → the vanilla slide returns. Flip **Shift Item Compare** On → the compare box appears on the next hover frame; Off → single tooltip as retail.
- [ ] **3. Video modes:** `640x480`, `800x600`, `1280x720`, `1344x700`, `1366x768`, `1920x1080` — at least fullscreen **and** windowed (`-w`) at one wide stop. No black screen / freeze on switch. (At 1344x700 hosted fullscreen the frame stretches edge-to-edge over the host mode with the cursor mapped back; at 1080p D3D usually enters true-exclusive. Glide needs the matching wrapper: 0.4.6 = 1344x700, 0.4.7 adds 1366x768, 0.4.8 adds 1920x1080.)
- [x] **36. Video-options label alignment (#23):** in-game `Esc → Options → Video Options`: the RESOLUTION value text right-aligns with the value column and sits on the neighbors' ink baseline; flipping stops leaves no stray text. — **CONFIRMED 2026-11-02 (R83 full smoke pass, issue #24).** Keep as a per-build regression check.

## B. In-game core (~5 min)

- [ ] **4. Load a char → Blood Moor.** Kill 2–3 Fallen. Attack swings, hit sounds, and ambient/music all present (R50 restored per-class player sounds — listen for *your* class's swing/grunt).
- [ ] **5. Unique colors (R49 fix):** find a unique or blue-minion pack — tint correct, not bone-default. Necro raised skeletons spawn and stay **white** through roster updates (the R50/R51 polarity fix, [#8](https://github.com/VTSTech/D2VP-QoL/issues/8) field-verified R91).
- [ ] **6. Pickups:** grab a weapon + gold pile by hand. No crash with inventory deliberately full (0.0.5 fixed the full-inventory pickup crash).

## C. Vanilla+ features (needs `d2vp.ini`)

- [ ] **7. Auto-create:** with no `d2vp.ini` present, first run creates it (documented, all-defaults) and the game plays vanilla.
- [ ] **8. `alt_toggle`:** each Alt press flips ground labels on/off. Click a *label* to pick up. Labels stay latched after you click something. **Stale-selection guard (R58, issue #15):** with labels latched, hover a label once, then walk-click on EMPTY ground far from any item — the click must **walk** (no pickup, no walk-to-loot); clicking the sprite still picks up; a combat click still attacks.
- [ ] **9. `auto_gold`:** walk over a gold pile → auto-collected. Negative checks: does **not** move your character, does **not** fire mid-attack, picks up **gold only** (drop a gem, walk over — gem stays).
- [ ] **10. `globe_persistent`:** HP/MP text above orbs always visible, no hover needed.
- [ ] **11. Hot-reload:** edit `d2vp.ini` while in-game (flip a key) → applies within ~2 s, no restart.
- [ ] **12. `portable_save`:** enabled → save & exit → `.d2s` lands in `<game folder>\save`, re-enters fine.
- [ ] **12b. `item_ilvl` (R55–R59):** set `item_ilvl=1` → every item tooltip's name line reads `Long Sword (12)` — bare digits, exactly one space before `(N)`, rendered in the name's own color. Sanity pairs: a set piece, an unidentified item, gold/potions (harmless suffix). Negative: key off/missing → tooltips exactly as retail; the Options row flips it live.
- [ ] **12c. `shift_compare` (R125/R126 — [plans/SHIFT-COMPARE-PLAN.md](plans/SHIFT-COMPARE-PLAN.md) row 0b):** set `shift_compare=1` → hold **Shift** while hovering an equippable item in a panel → a **second tooltip** draws beside the hovered item's: the item you have equipped for that body slot. Side-by-side, no stat-delta lines. Slot sanity pairs: helm → the equipped helm; a ring with one equipped → that ring, with both → deterministic first-occupied pick; 1H weapon + shield equipped → hovering the **shield** compares the shield and hovering the **weapon** compares the weapon (R126 same-kind slot pick); a quiver with a bow → the quiver; a dual-wielding barbarian → the hovered-kind matching hand; a 2H → described from the weapon cell; a potion/charm/jewel → NO box. Coverage: inventory, stash, cube, trade screen, vendor (your items AND the shop stock). Placement: LEFT of the primary with a 4 px gap (fallback RIGHT), bottoms aligned. Negative: gate off → retail; Shift up → single tooltip; empty target slot / paperdoll hover → no box. Coexistence: Alt-latched labels + Shift compare both show; the Options row flips it live; `item_ilvl` rides along on the compare box.

## D. 720p alignment regressions (R47/R48 territory)

- [ ] **13.** At 1280x720: inventory + skill flyouts align; open stash → **deposit/withdraw gold dialogs** centered properly (the R48 casualty — easy to regress).
- [x] **13b. Belt hover (R52, [issue #14](https://github.com/VTSTech/D2VP-QoL/issues/14)):** hover the belt HUD → expanded popup opens **and stays open** while the cursor is inside it; clicking a popup slot uses/drops that potion; the item-name tooltip anchors to the slot. — **CONFIRMED 2026-10-31.** Keep as a per-build regression check.

## E. Wrapper route (optional, if testing `-3dfx`)

- [ ] **14.** Launch with `-3dfx` → glide3xvk renders, `glide3x.log` gets written, presets/chain load.

## F. Quest trace — A1Q1 Den of Evil ([issue #13](https://github.com/VTSTech/D2VP-QoL/issues/13), R53/R54 boot marks)

Every step writes to `d2boot.log`. One char that clears the Den and rejoins covers both scenarios.

- [x] **15. Fresh clear (same-game turn-in):** clear the Den → `quest: den kill left=...` per kill → `quest: den CLEARED -> JustCompleted set...` → `quest: den final state 5 broadcast` → completion sting. Talk to Akara **in the same game**: the R54 chain (`quest: gossip add quest=0x.. slot=3` → `quest: npc interact 0x27 send` → `s2c 0x27 overhead ... msgType0=0x004c` → `c2s 0x31 gossip send` → `quest: den reward GRANTED (+1 skill...)`). — **CONFIRMED 2026-11-02 (R83 full smoke pass, issue #24).** Keep as a per-build regression check.
- [x] **16. Rejoin turn-in (the issue #13 repro):** char cleared but never claimed → join a new game → `quest: rejoin Den ...` → the same chain as item 15 ending `quest: den reward GRANTED`, and the +1 skill point lands. If you see `gossip add slot=3 EMPTY` or `NO CLASS MATCH`, the exe predates the fix (check the self-ID line). — **CONFIRMED 2026-10-31 (R54 field log, owner: "Great Job. Works.").** Keep as a per-build regression check.
- [ ] **17. Accept regression (R54 touched the accept table's neighbors):** a fresh character talks to Akara → Den quest accepts as before (`quest: gossip add ... slot=0 ... str=0x0040 -> added` → echo 0x40).

## G. Save → re-enter regression ([issue #19](https://github.com/VTSTech/D2VP-QoL/issues/19), R61 — FIELD-CONFIRMED 2026-11-01, issue CLOSED)

Root cause: the VP Resolution row's runtime blank-attach mutated the static esc-menu table, so ANY second game entry in one process hit the retail fatal `HaltTail(0x501)`. R61 degrades the loader guards to marks and guards the type-1 draw site. One char, two game entries in one process — the fix is proven by NOT crashing:

- [x] **18. Re-enter (the issue repro, now the fix proof):** boot → enter any game → ~30 s → Save & Exit → same process, same char → Play. **The second entry must load and be playable.** Expected: exactly one `escmenu: video item 1 stop 2 path empty - prior dc6 kept [R61]` mark (the fix working); no `halt report:`/`d2crash-*.txt`. — **CONFIRMED 2026-11-01 (owner: "R61 Confirmed fixes #19").** Keep as a per-build regression check.
- [x] **19. Resolution row still whole:** in game 2 → Options → Video → Resolution: the 720p stop selectable and its label visible (the blank slot survived the re-init). — **No regression reported since R61.** Keep as a per-build regression check.
- [x] **20. Esc menu + roster watches unchanged:** the ESC menu draws every row (type-1 toggles included); `roster heal:` cap marks during load are the SEPARATE standing corruptor watch (§6 of handoff) — if they appear, send the log. — **No regression reported since R61.** Keep as a per-build regression check.

## H. Shared stash — 10 pages (R82), outlined pager tab ([SHARED-STASH-PLAN.md](plans/SHARED-STASH-PLAN.md) track N)

Setup: `d2vp.ini` → `[features] shared_stash=1` (hot-reloads within ~2 s) — or flip the **Shared Stash** Options row (the tab follows the toggle the same frame client-side; give the server half its 2 s beat before clicking). Toggling OFF while the shared page is ACTIVE: the tab disappears — the grid keeps showing the shared page until you re-enter a game; nothing is lost (the save flush parks the shared set either way). Field-proven end to end: mechanism blind-tested R65, tab visible R66 ("Great job. I see it. It works."), ten pages R82, items 25/27 field-proven in the R83 full pass. Log chain: `shared stash:` marks cover every step below.

- [x] **21. Tab appears (gate on):** open any stash in town — the outlined **"Shared"** word + `>` arrow sit in the stash panel's **top-right corner**, above the gold display row, clear of the gold text and the panel art seam (white = personal page; green `‹ Shared N ›` = a shared page). Clicking the WORD toggles personal↔shared-1, the ARROWS step pages. — **CONFIRMED R66/R74/R83.** Keep as a per-build regression check.
- [ ] **22. Gate off = vanilla:** with `shared_stash=0`/missing (or before the 2 s poll), the tab is absent and the stash behaves exactly as retail. No `_d2vp_shared.sss` may exist in the save folder from this run.
- [ ] **23. Item deposit + cross-character round trip (the core test):** char A: personal stash → click **Shared** → drop an item in → tab back personal → Save & Exit. Char B (same PC): stash → **Shared** → **the same item is there** → withdraw → Save & Exit. Char B's first toggle logs `shared stash: loaded N items in M pages (gold G)`.
- [ ] **24. Gold deposit + withdraw:** with the shared page active, the stash gold buttons move gold into/out of the shared pool (the display reads the shared amount); tab back → the display returns to the character's own banked gold, exactly what it was.
- [x] **25. `.d2s` stays retail (I1, the hard invariant):** after a session that used the shared page, the character's `.d2s` **must still load in an unmodified retail client**. The save folder shows `_d2vp_shared.sss` beside the `.d2s`; the `.d2s` itself contains only personal items/gold. — **CONFIRMED 2026-11-02 (R83 full smoke pass, issue #24).** Keep as a per-build regression check.
- [ ] **26. Per-game reset:** enter a new game — the client starts on the personal page (tab white) and the server re-loads the pool from the file on the first Shared toggle, so the previous session's deposits are there.
- [x] **27. Degrade drill (one minute, worth it):** scribble one byte inside a file item block of `_d2vp_shared.sss` (offset ≥ 92 in the ten-page v2) → in game, toggle Shared → log: `shared stash: checksum fail - file - starting empty`, shared page 1 empty, **the file on disk untouched**; fix the byte and the pool behaves again. Delete the file → `no file - starting empty`, recreated on the next tab-back. — **CONFIRMED 2026-11-02 (R83 full smoke pass, issue #24).** Keep as a per-build regression check.
- [ ] **33. Pager navigation:** personal reads `Shared >` — click `>` → `‹ Shared 1 ›` (green); each further `>` steps one page (`Shared 2` … `Shared 10`, a log line per step; `‹ Shared 10` shows NO right arrow); `<` steps back; clicking the WORD from any shared page returns to personal. Separation: item A page 1, item B page 2, item C page 10 → each only shows on its own page.
- [ ] **34. Persistence + file upgrades:** an item on shared page 2 survives Save & Exit → re-enter. Upgrade checks: a **pre-R82** two-page file loads with pages 3-10 empty (`v2 file has 2 pages - pages 3..10 start empty`) and rewrites as ten-page after the next tab-back; a **pre-R75** v1 file still loads as page 1.
- [ ] **35. Ten-page reachability + one gold pool (R82):** click `>` ten times — every page renders its label, no crash, no ghost items; an item on **page 10** survives Save & Exit → re-enter; gold deposited on page 5 reads the SAME shared amount on every shared page and the personal bank returns unchanged.

## I. 1792 merge regression pass (merge `6c552f9` + the R73/R74 compile-fix rounds)

The Ghidra 1784–1792 regen re-spliced three pure-decompiled regions (Direct3D render, SFile, SpellSel) and re-resolved two fork-marked merge sites (Inv.cpp, SpellSel.cpp). The pass maps to what the merge touched — ride it on any merge round:

- [ ] **28. Direct3D:** VIDEO OPTIONS resolution switch 640x480 → 800x600 → VP 1280x720 and back; windowed ↔ fullscreen; alt-tab out/in; brightness slider. Terrain with perspective walls renders normally.
- [ ] **29. SFile:** load an EXISTING character; then the shared-stash persistence loop (deposit → save → re-enter → withdraw) — exercises the `_d2vp_shared.sss` read/write path.
- [ ] **30. SpellSel:** right-click skill select on both spell buttons, L/R swap, skill flyout tooltips at 720p and 800x600.
- [ ] **31. Inv:** inventory drag/drop belt ↔ inventory ↔ stash; the Shared tab loop.
- [ ] **32. Standing Vanilla+ set:** each Options toggle flips its ini key; auto-gold picks a pile; alt-toggle latches labels; ilvl suffix on hover; belt hover at 720p; A1Q1 Den turn-in.

Boot gate: the first `d2boot.log` line must print the round under test's self-ID (item 0 — R73 never booted, its overlay was compile-broken; R74 was the first post-1792 build).

## J. Item tooltip — elemental/poison zero fix ([issue #16](https://github.com/VTSTech/D2VP-QoL/issues/16), R84)

Single-value branches of the item-stat tooltip formatter printed stack residue when a roll collapsed to min==max ("0 poison damage over 0 seconds"). Tooltip-only — the stats were never wrong.

- [ ] **37. Cold tooltip:** hover an 'of Frost' weapon — the cold line shows real numbers, never `Adds 0 Cold Damage` / `+0`. Eyeball fire/lightning/magic lines too.
- [ ] **38. Poison tooltip:** hover any poison item — real values over real seconds, never `0 Poison Damage over 0 Seconds`. If a zero/zero line still appears, screenshot the item (that would be a genuinely length-0 mod — generation-side, a different follow-up).

## K. `-nointro` boot switch (R86)

`d2vp.exe -nointro`: the logo .biks and the auto-played title cinematic never enter cutscene mode — boot runs logo-free to the title. In-game quest cutscenes are NOT affected; boot WITHOUT the switch is byte-for-byte vanilla. First detection writes `[vp] cmdline -nointro: boot intro videos disabled` to `d2boot.log`.

- [ ] **39. Boot with the switch:** no logo videos, no auto `d2intro` — straight to the title; `d2boot.log` carries the `[vp] cmdline -nointro` mark.
- [ ] **40. Regression (no switch):** one boot WITHOUT `-nointro` — the logo videos still play with sound and the mark is absent.

## L. `no_panel_shift` — static world with one panel open (R95, [issue #26](https://github.com/VTSTech/D2VP-QoL/issues/26))

With `[features] no_panel_shift=1` (wide layouts only), a single open panel renders the world exactly like the both-panels case — no vanilla quarter-screen slide — and the panel floats over it. Missing key / 640x480 / 800x600 = the vanilla slide, byte-identical. Field-proven end to end R95–R101 (Glide + D3D), the §L standing asks cleared in the R104 full smoke pass.

- [x] **41. Static world:** at 1280x720, perspective ON, `no_panel_shift=1`: open the character screen — the world, player, NPCs stay **exactly** where they were; same with the inventory; automap, rain, click-to-move line up (click a unit standing in an outer margin). — **CONFIRMED 2026-11-03 (R95 Glide pass) + R104.** Keep as a per-build regression check.
- [x] **42. The last #26 blackness:** with the option on and perspective ON, open the inventory — no black strip beyond the panel's right edge. — **CONFIRMED 2026-11-03 (R95) + R104.**
- [x] **43. Perspective off:** same single-panel checks with perspective OFF — world static, both panels clean, no bottom strip. — **CONFIRMED 2026-11-04 (R104 full smoke pass, issue #28).**
- [x] **44. Regression (vanilla slide):** `no_panel_shift=0` — the retail quarter-screen slide returns with a single panel, renders as R94. — **CONFIRMED 2026-11-03/04.**
- [x] **45. Both panels:** option on, both panels open — world visible and correct in both outer margins. — **CONFIRMED 2026-11-04 (R104, issue #28).**
- [x] **41a. Unit lock re-check (R100):** repeat 41 — player, NPCs, objects, shadows, selection rings, hover text, merc HP bar, name plates, warp tooltip, ground labels all stay glued with a single panel open. — **CONFIRMED 2026-11-04 (R104, issue #28).**
- [x] **41b. Vanilla slide untouched (R100):** `no_panel_shift=0`, single panel: units and world slide together as one (retail behavior). — **CONFIRMED 2026-11-04 (R104, issue #28).**
- [x] **48. D3D perspective-toggle survival (R98 root fix):** in D3D, toggle Perspective ON→OFF and OFF→ON at the video options — no death either direction, no `promote out-of-range` / `texture cache dead` marks; then finish the D3D #26 matrix (41/42/43/45). (The R96/R97 probe rounds and the persisting-death read-out matrix are archived in **H43**.) — **CONFIRMED 2026-11-04 (R104, issue #28).** Keep as a per-build regression check.

**Registry watch (owner environment, 2026-11-03 — not a build bug):** if D3D ever again boots to a black screen / fails to enter game across builds that previously worked: restore the fresh-install video-settings registry values before suspecting the build (Vanilla+ writes no registry values). Full episode record: H43.

---

## M. Gamble at a gambling NPC ([issue #32](https://github.com/VTSTech/D2VP-QoL/issues/32), R128 — FIELD-VERIFIED 2026-11-06, owner-closed)

The one-click crash: talking to Gheed (or any gambling NPC) and clicking **Gamble** crashed the game on every round since the source regeneration. R128 fixed the broken item-table lookup and the §M pass went green the same day (window opens, sane `gamble:` bounds, one purchase, re-open clean). The items stay as the **standing gamble regression check** for future full-belt runs.

- [ ] **1.** Talk to Gheed in town → click **Gamble** → the gamble window opens with a mixed item list and **no crash** (pre-R128 this died with an AV READ ~1 s after the click).
- [ ] **2.** `d2boot.log` must show the fix chain: `gamble: gen start npc=0x.... level=<your level> cache=<count> total=<count>` → ~14 × `gamble: roll level=.. bound=.. pick=..` with **sane bounds** (`bound` is a small count, 2..total — never a huge pointer-looking value) → `gamble: gen done items=14`. A garbage-looking `bound=` means the running exe predates the fix (check item 0).
- [ ] **3.** Buy one gamble item — gold deducts, item lands in inventory (the purchase path was unreachable while the open crash stood; first field exercise here).
- [ ] **4.** Re-open the gamble screen a second time in the same session — fresh item list, still no crash.

---

## Evidence to keep if anything breaks

| Artifact | Comes from | Tells us |
| --- | --- | --- |
| `d2boot.log` | `d2vp.exe` | build self-ID (which round you actually ran) |
| `d2crash-<date>-<time>.txt` | `d2vp.exe` (auto-written next to the exe; hangwatch dumps one after 12 s) | symbolised crashed module/function/offset |
| `glide3x.log` | glide3xvk wrapper | frame, preset, chain state; whether the faulting thread was wrapper code |

A crash without both artifacts cannot be attributed — attach all to the issue.
