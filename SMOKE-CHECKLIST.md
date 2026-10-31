# Vanilla+ Smoke Test — in-game checklist

Build: **Vanilla+0.0.6-R54** (confirm via line 1 of `d2boot.log` before starting).
Core path takes ~15 min. Run top to bottom — the fast checks come first.

**Known-fail — skip, don't burn time:** Necro skeleton spawn tint (issue #8). R51 spawn-polarity fix is pending re-verify; symptom = own raised skeletons spawn yellow and (per R51 theory) should never re-tint. One useful datum for the next round: raise a skeleton and note whether it *ever* re-tints (e.g. after a roster-update event) or stays yellow forever — "never re-tints" would falsify the R51 theory.

**Also wanted this pass — Den of Evil turn-in (issue #13, ROOT-CAUSED + FIXED R54):** section F below. The R53 marks caught the failing site (client never echoed c2s 0x31) and R54 both fixed the cause (the Den reward gossip line didn't exist in the data) and instrumented the whole gossip chain (`quest: gossip add` / 0x27 / `npc menu:` marks). The rejoin-repro run alone verifies the fix.

---

## Setup (once, ~1 min)

- [ ] **0.** Confirm the right exe: line 1 of `d2boot.log` must say `Vanilla+0.0.6-R54`. If it says R53 or lower, wrong exe — don't test further.

## A. Boot & menus (~3 min)

- [ ] **1. Intro & title:** intro video plays *with sound* → title music plays → character select renders.
- [ ] **2. Options page:** `Esc → Options → "Vanilla+ Options"` — page appears, toggles flip when clicked.
- [ ] **3. Video modes:** `640x480`, `800x600`, `1280x720` — at least fullscreen **and** windowed (`-w`) at 720p. No black screen / freeze on switch.

## B. In-game core (~5 min)

- [ ] **4. Load a char → Blood Moor.** Kill 2–3 Fallen. Attack swings, hit sounds, and ambient/music all present (R50 restored per-class player sounds — listen for *your* class's swing/grunt).
- [ ] **5. Unique colors (R49 fix):** find a unique or blue-minion pack — tint correct, not bone-default. (Regression watch: R50 briefly broke roster *pets* the same way.)
- [ ] **6. Pickups:** grab a weapon + gold pile by hand. No crash with inventory deliberately full (0.0.5 fixed the full-inventory pickup crash).

## C. Vanilla+ features (needs `d2vp.ini`)

- [ ] **7. Auto-create:** with no `d2vp.ini` present, first run creates it (documented, all-defaults) and the game plays vanilla.
- [ ] **8. `alt_toggle`:** each Alt press flips ground labels on/off. Click a *label* to pick up. Labels stay latched after you click something.
- [ ] **9. `auto_gold`:** walk over a gold pile → auto-collected. Negative checks: does **not** move your character, does **not** fire mid-attack, picks up **gold only** (drop a gem, walk over — gem stays).
- [ ] **10. `globe_persistent`:** HP/MP text above orbs always visible, no hover needed.
- [ ] **11. Hot-reload:** edit `d2vp.ini` while in-game (flip a key) → applies within ~2 s, no restart.
- [ ] **12. `portable_save`:** enabled → save & exit → `.d2s` lands in `<game folder>\save`, re-enters fine.

## D. 720p alignment regressions (R47/R48 territory)

- [ ] **13.** At 1280x720: inventory + skill flyouts align; open stash → **deposit/withdraw gold dialogs** centered properly (the R48 casualty — easy to regress).
- [x] **13b. Belt hover — CONFIRMED on R52, [issue #14](https://github.com/VTSTech/D2LoD-libslang-vk/issues/14) CLOSED:** hover the belt HUD → expanded belt popup opens **and stays open** while the cursor is inside it; clicking a slot in the popup uses/drops that potion; hovering a filled slot shows the item-name tooltip anchored to the slot (not 60px too high). Keep as a per-build regression check.

## E. Wrapper route (optional, if testing `-3dfx`)

- [ ] **14.** Launch with `-3dfx` → glide3xvk renders, `glide3x.log` gets written, presets/chain load.

## F. Quest trace — A1Q1 Den of Evil ([issue #13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13), R53/R54 boot marks)

Every step writes to `d2boot.log`. One char that clears the Den and rejoins covers both scenarios.

- [ ] **15. Fresh clear (same-game turn-in):** on a char whose Den is NOT done: clear the Den → log must show `quest: den kill left=...` per kill (ticking down), then `quest: den CLEARED -> JustCompleted set...`, then `quest: den final state 5 broadcast (8-tick timer)`, and the completion sting plays. Talk to Akara **in the same game** and expect the R54 chain: `quest: akara activate ... rewardAwaits=1` → `gossip line 3` → **`quest: gossip add quest=0x.. slot=3 npc=0x0094`** → **`str=0x004c menu=0 -> added`** → **`quest: npc interact 0x27 send npc=0x0094 type=1 msgs=1`** → **`s2c 0x27 overhead ... msgType0=0x004c`** → `npc menu: msgType0=0x004c` → `c2s 0x31 gossip send ... gossip=0x004c` → `quest: akara gossip msg=0x004c` → `quest: den reward GRANTED (+1 skill...)`.
- [x] **16. Rejoin turn-in (the issue #13 repro — now the FIX verification):** with a char that cleared but never claimed: join a new game → `quest: rejoin Den ...` line (state note only — R53 proved the server bit arrives correct). Talk to Akara → **the same 7-step chain as item 15 must appear, ending `quest: den reward GRANTED`, and the +1 skill point must land on the character.** If instead you see `quest: gossip add slot=3 EMPTY (count=...)` or `NO CLASS MATCH`, the running exe predates the fix (check the self-ID line). Any `npc menu:` mark naming a no-echo branch (`interaction path` / `generic menu` / `hand-item bail`) with `msgType0=0xffff` is the old defect shape — attach the log. — **CONFIRMED 2026-10-31 (R54 field log, owner: "Great Job. Works.")**: the full chain ran first try, ending `quest: den reward GRANTED (+1 skill, Den->Completed, Respec armed)`; post-grant activate reads `rewardAwaits=0 completed=1`. Item 15 (same-game) still pending its pass.
- [ ] **17. Accept regression (R54 touched the accept table's neighbors):** a fresh character talks to Akara → Den quest accepts as before (`quest: gossip add ... slot=0 ... str=0x0040 -> added` → echo 0x40). Slot 0 was already in-bounds pre-fix — this is a cheap regression guard.

---

## Evidence to keep if anything breaks

| Artifact | Comes from | Tells us |
| --- | --- | --- |
| `d2boot.log` | `d2.exe` | build self-ID (which round you actually ran) |
| `d2crash-<date>-<time>.txt` | `d2.exe` (auto-written next to the exe; hangwatch dumps one after 12 s) | symbolised crashed module/function/offset |
| `glide3x.log` | glide3xvk wrapper | frame, preset, chain state; whether the faulting thread was wrapper code |

A crash without both artifacts cannot be attributed — attach all to the issue.
