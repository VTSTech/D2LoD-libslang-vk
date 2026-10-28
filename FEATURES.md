# FEATURES.md — D2R backport slate (vanilla-plus)

*v1, 2026-10-29. Scoped by research: D2R official QoL behavior + prior art from the
classic-D2 modding scene (PD2, BH/slashdiablo maphack, Sting's hackmap, d2dx). Ranked
**low-risk first** per owner call. **Code-only** per charter Principle 4: no MPQ/data
edits, no new assets — every feature is pure code + a `d2vp.ini` gate. Hook sites below
are **candidate regions** taken from the known feature-era map (handoff.md rows 28–31);
each feature gets a Task-C-style tree-verify pass (exact anchors, grep gates) before any
implementation round. Live tasks: handoff.md §9. Charter: VANILLA-PLUS.md.*

## The feature contract (house rules, from shipped rows 28–31)

Every slate row must satisfy all of these before it ships:

1. **Default OFF = vanilla byte-for-byte.** Missing ini/section/key/garbage value means
   retail behavior at every touched site (predicate-false gating).
2. **Config gate**: `[features] <key>=0` in `d2vp.ini`, exe-dir anchored,
   `GetPrivateProfileIntA`, hot-re-read at most every 2 s (GetTickCount gate,
   first-read sentinel).
3. **ini template**: the key is added to `ALTTOGGLE_WriteIniTemplate` (D2Client.cpp) with
   a documenting comment; auto-created file stays the only user documentation needed.
4. **One choke point where possible** (the `GetAndCreateSavePath` lesson): prefer a
   single hook all consumers route through over scatter-shot patches.
5. **No balance, no save-format change.** Gold-only, display-only, or input-only
   behavior; `.d2s`/`.d2x` layouts stay untouched.
6. **QA**: per-TU llvm-mingw compile of every touched TU + full gcc build (Win11 gate or
   CI) + the §11 field loop; handoff ledger row + worklog entry per round.

## The slate (ranked low-risk first)

| # | Feature | Area | D2R parity source | Effort | Risk | Gate key |
|---|---|---|---|---|---|---|
| 0a | Z-key camera zoom | Controls | D2R launch feature (Z / wheel) | M | Med | `camera_zoom` |
| 0b | Shift equipment-compare | Controls | D2R hover-compare | M | Med | `shift_compare` |
| 1 | HP/MP globe hover values | HUD | D2R globe hover shows current/max | S | Low | `globe_values` |
| 2 | Clock / game-timer overlay | HUD | D2R Tab-screen game timer; BH wall-clock | S | Low | `clock_overlay` |
| 3 | Item ilvl on tooltips | Loot | D2R 2.5 display-item-level toggle | S–M | Low | `ilvl_tooltip` |
| 4 | Monster HP bar on hover | HUD | D2R enemy health bar (PC) | M | Low–Med | `monster_hp_bar` |
| 5 | Auto gold pickup | Loot | D2R "Gold Pickup" gameplay option | M | Med | `auto_gold` |
| 6 | Automap scale + position | HUD | D2R larger overlay map | M | Med | `automap_scale` |
| 7 | Quick-cast keys | Controls | D2R-adjacent (PD2 quick cast) | M–L | Med–High | `quick_cast` |

Rows 0a/0b are **Task C** (handoff §9 item 3) — already scoped, awaiting go; they are
the slate's head, not new scope. Rows 1–7 are the new candidates.

---

### 1. HP/MP globe hover values — `[features] globe_values`

- **D2R behavior**: hovering the life/mana globes shows numeric current/max values.
- **Proposed**: while the cursor is over either bulb, draw `cur / max` text beside it,
  using the same tooltip/text primitives the alt-toggle label path already uses
  (`D2Client/UI/Text.cpp`, `D2Client/UI/Draw.cpp` draw layer). Values come from the
  player unit's stat list (life/mana current + max stats).
- **Candidate regions**: bulb hover detection near the panel hit-test paths
  (`D2Client/UI/Panel.cpp` / `Inv.cpp` region — verify); text draw via the existing
  client text helpers. No new state beyond the gate flag.
- **Prior art**: D2R official; PD2 numeric globes.
- **Risk**: pure additive draw; only care point is hover-vs-click (must not consume the
  click — the alt-toggle click-through fix, row 31, already mapped this machinery).
- **Exit**: hover shows values in-town and in-world, hide on leave, `0` = vanilla.

### 2. Clock / game-timer overlay — `[features] clock_overlay`

- **D2R behavior**: the Tab screen carries a game-length timer; wall-clock is a maphack
  staple (BH, hackmap) rather than a D2R option — ship both, one key each if wanted.
- **Proposed**: small fixed text top-right (or under the automap): local wall clock via
  `GetLocalTime`, and/or elapsed game time; drawn in the client overlay layer.
- **Candidate regions**: the same overlay draw stage the automap uses
  (`D2Client/UI/Automap.cpp` draws last-in-frame; draw ours before/after it — verify
  z-order), one timer initialized at game join.
- **Prior art**: D2R game timer; BH maphack clock; hackmap.
- **Risk**: none beyond draw-order; zero interaction surface.
- **Exit**: clock visible in-game only, no input eaten, `0` = vanilla.

### 3. Item ilvl on tooltips — `[features] ilvl_tooltip`

- **D2R behavior**: 2.5 shipped a toggleable display-item-level (advanced item stats).
- **Proposed**: append `Item Level: N` as the last tooltip line for inventory and
  ground-label tooltips; the ilvl lives on the item unit data
  (`D2UnitDataItemStrc`, accessor family in `D2Common/ITEMS/Items.*` — grep hit
  confirmed).
- **Candidate regions**: tooltip text assembly for inventory items
  (`D2Client/UI/Inv.cpp` region) + ground labels (`D2Client/UI/ShowItems.cpp`, the
  row-31 mapped path). Both funnels presumably share the stat-line builder — verify the
  choke point before patching both.
- **Prior art**: D2R 2.5; BH ilvl display; hackmap.
- **Risk**: string-append only; watch fixed tooltip buffer sizes (the recovered code
  allocates tight); S–M.
- **Exit**: ilvl matches ATMA/goMule-reported values on a sample, `0` = vanilla.

### 4. Monster HP bar on hover — `[features] monster_hp_bar`

- **D2R behavior**: hovering/targeting an enemy shows a health bar (PC feature; players
  literally ask Blizzard for a toggle — it exists).
- **Proposed**: while the hover target is a monster (the client already resolves the
  hovered unit for name tooltips — `HitTestScreen` machinery in
  `D2Client/UNIT/CUnit.cpp`, row-31 mapped), draw a small two-rect bar (frame + fill by
  current/max HP from the unit stat list) at a fixed screen position (D2R style, top
  area) or above the monster name. Bosses/ champs colored like their name.
- **Candidate regions**: hover-name draw path (same place the monster name renders —
  verify), stat reads via the client stat list accessors used by the light/HP code.
- **Prior art**: D2R official; BH/PD2 monster HP display; hackmap.
- **Risk**: additive draw but reads live unit stats every frame — keep it read-only, no
  selection/consumption changes; Low–Med.
- **Exit**: bar tracks a fighting monster's HP 1:1 (compare vs. vanilla tooltip on
  same frame), hidden on nothing-hovered, `0` = vanilla.

### 5. Auto gold pickup — `[features] auto_gold`

- **D2R behavior**: the "Gold Pickup" gameplay option vacuum-picks gold piles walked
  over (Blizzard shipped it; console got it first).
- **Proposed**: with the gate on, gold piles within the vanilla item-interaction radius
  are picked up when the player walks over them — the same code path a click-pickup
  takes (server-side, which in single player is in-process), so inventory-full, dropped
  -in-town, and gold-cap semantics stay retail. No item pickup — gold only.
- **Candidate regions**: `D2Game/ITEMS/` (drop/pickup namespace — `Items/Drop.cpp` is
  fix-14 mapped; the walk-over trigger likely pairs a client proximity check with the
  existing pickup request handler — the exact request function needs the tree-verify
  pass; `D2Game/ITEMS/Items.h` currently exposes only `FreeItems`, so the real anchors
  live deeper).
- **Prior art**: D2R official; PD2 ("Improved auto gold pickup"); PlugY-era plugins.
- **Risk**: touches the gameplay pickup path — the first slate row that is not
  draw/input-only; still no balance surface (gold is currency, not stats). Med.
- **Exit**: walk-over picks gold exactly like click (full-inventory: pile stays),
  items untouched, `0` = vanilla.

### 6. Automap scale + position — `[features] automap_scale`

- **D2R behavior**: the overlay map is bigger, positioned, and more readable than
  vanilla's corner blit.
- **Proposed**: integer scale + configurable corner/offset for the automap blit. True
  alpha is not available in the paletted overlay path — ship scale/position first;
  "opacity" only if the game's own stipple/transparency tables make it free.
- **Candidate regions**: automap draw site (`D2Client/UI/Automap.cpp`; the
  save-path row 30 confirms this TU + its GAME-side consumer), blit rect math.
- **Prior art**: d2hackmap automap config (colors/size); D2R official look.
- **Risk**: purely visual; watch edge-clipping at 640x480 vs 800x600. Med.
- **Exit**: 2x map in a custom corner without overlap artifacts in both resolutions,
  `0` = vanilla.

### 7. Quick-cast keys — `[features] quick_cast`

- **D2R-adjacent behavior** (PD2 ships it explicitly): casting a bound skill at the
  cursor **without** switching the active skill; primary bind = quick cast, secondary =
  regular select.
- **Proposed**: hold-modifier + skill hotkey (e.g. Ctrl+F1) casts that skill at the
  current cursor position client-side, then restores the active skill state untouched.
- **Candidate regions**: input dispatch (`D2Client/UI/CmdTbl.cpp` — row-28 proven
  registration path), skill-cast command building (`D2Client/SKILLS.cpp`,
  `D2Client/UNIT/PlrSkills.cpp` region), server skill execution untouched
  (`D2Game/SKILLS/SkillsServer.cpp` — in-process SP).
- **Prior art**: PD2 quick cast.
- **Risk**: input/skill-dispatch interaction is the widest surface on the slate; do it
  last, after the draw-only rows have hardened the pattern. Med–High.
- **Exit**: quick-cast fires the right skill at cursor, active skill button unchanged,
  mana costs normal, `0` = vanilla.

---

## Dropped candidates (and why)

| Candidate | Verdict | Reason |
|---|---|---|
| Expanded stash pages | **Dropped** | Save-format hazard (`.d2x` layout + inventory grid realloc); D2R did it with new UI + data. Revisit only if upstream ever blesses save-format work. |
| Socketing / unsocketing QoL recipes | **Dropped** | Cube recipes are data tables (`Txt/Cube.cpp` reads bins) — violates code-only. |
| Personal loot | **Dropped** | Multiplayer concept; charter is single-player focus. |
| Loot filter | **Parked** | Its own project (filter language + UI + config surface) — PD2 ships BH wholesale; would dominate the roadmap. Not a "backport", a subsystem. |
| Buff timers with icons | **Parked** | Icon assets violate code-only; text-only timers possible later if wanted (reuses row-2 overlay pattern). |
| Keybind remapping UI | **Dropped** | Vanilla 1.14 already ships customize-keys; D2R's delta is small. |
| Ctrl+click in trade screens | **Dropped** | A forum request, not a shipped D2R feature; trade screens are BNet-adjacent and upstream-locked. |

## How a slate row becomes a shipped feature

1. **Tree-verify** the row: exact anchors + grep gates (Task-C detail, handoff row-28
   style), TU list.
2. **Implement** against the feature contract above; one row per round.
3. **QA**: per-TU llvm-mingw + full gcc build gate + §11 field loop.
4. **Land**: handoff ledger row (`vanilla-plus feature: ...`), ini template entry,
   VANILLA-PLUS.md Features section, release note at the next tag.

## Sources

- D2R options/behavior: Blizzard D2R forums (auto-gold pickup, 2021; enemy health bars,
  2026; ilvl display, 2.5 era); D2R Tab-screen game timer threads.
- Prior art: [Project Diablo 2 patch notes](https://wiki.projectdiablo2.com/wiki/Patch_Notes)
  (quick cast, auto gold, buff timers, area-level display), slashdiablo maphack (BH),
  Sting's hackmap config surface, bayaraa/d2dx (renderer-side QoL precedents).
- In-tree grounding: handoff.md rows 28–31 (the feature-era map), fix-14 Drop.cpp map.
