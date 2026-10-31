# Vanilla+ Smoke Test — in-game checklist

Build: **Vanilla+0.0.6-R52** (confirm via line 1 of `d2boot.log` before starting).
Core path takes ~15 min. Run top to bottom — the fast checks come first.

**Known-fail — skip, don't burn time:** Necro skeleton spawn tint (issue #8). R51 spawn-polarity fix is pending re-verify; symptom = own raised skeletons spawn yellow and (per R51 theory) should never re-tint. One useful datum for the next round: raise a skeleton and note whether it *ever* re-tints (e.g. after a roster-update event) or stays yellow forever — "never re-tints" would falsify the R51 theory.

---

## Setup (once, ~1 min)

- [ ] **0.** Confirm the right exe: line 1 of `d2boot.log` must say `Vanilla+0.0.6-R52`. If it says R51 or lower, wrong exe — don't test further.

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
- [ ] **13b. Belt hover (720p regression found in the field, FIXED R52):** hover the belt HUD → expanded belt popup opens **and stays open** while the cursor is inside it; clicking a slot in the popup uses/drops that potion; hovering a filled slot shows the item-name tooltip anchored to the slot (not 60px too high).

## E. Wrapper route (optional, if testing `-3dfx`)

- [ ] **14.** Launch with `-3dfx` → glide3xvk renders, `glide3x.log` gets written, presets/chain load.

---

## Evidence to keep if anything breaks

| Artifact | Comes from | Tells us |
| --- | --- | --- |
| `d2boot.log` | `d2.exe` | build self-ID (which round you actually ran) |
| `d2crash-<date>-<time>.txt` | `d2.exe` (auto-written next to the exe; hangwatch dumps one after 12 s) | symbolised crashed module/function/offset |
| `glide3x.log` | glide3xvk wrapper | frame, preset, chain state; whether the faulting thread was wrapper code |

A crash without both artifacts cannot be attributed — attach all to the issue.
