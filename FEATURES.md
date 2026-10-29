# FEATURES.md — D2R backport slate (vanilla-plus)

*v3, 2026-10-29. Row 9 (`shared_stash`) added from an owner suggestion — side-file design, `.d2s`/`.d2x` untouched. Row 4 (`transparent_hud`) added from an owner suggestion — no D2R parity, engine-native draw modes. Scoped by research: D2R official QoL behavior + prior art from the
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
| 1 | Persistent globe text (was: hover values) | HUD | D2R globe hover shows current/max | S | Low | **Repurposed 2026-10-30** — orb-face readout removed (retail already prints cur/max above each orb on hover); `globe_persistent` now pins that text on (ledger row 40; options page row 4) |
| 2 | Clock / game-timer overlay | HUD | D2R Tab-screen game timer; BH wall-clock | S | Low | `clock_overlay` |
| 3 | Item ilvl on tooltips | Loot | D2R 2.5 display-item-level toggle | S–M | Low | `ilvl_tooltip` |
| 4 | Transparent bottom bar | HUD | None — owner suggestion; see-through-HUD staple elsewhere | S–M | Low–Med | `transparent_hud` |
| 5 | Monster HP bar on hover | HUD | D2R enemy health bar (PC) | M | Low–Med | `monster_hp_bar` |
| 6 | Auto gold pickup | Loot | D2R "Gold Pickup" gameplay option | M | Med | **Implemented** (2026-10-29: ledger row 33; field-verified, coexists with alt_toggle) |
| 7 | Automap scale + position | HUD | D2R larger overlay map | M | Med | `automap_scale` |
| 8 | Quick-cast keys | Controls | D2R-adjacent (PD2 quick cast) | M–L | Med–High | `quick_cast` |
| 9 | Shared side-file stash | Stash | None — owner suggestion (PlugY-style side file) | L | Med–High | `shared_stash` |
| 10 | Vanilla+ Options menu page | UI | None — owner suggestion 2026-10-29; in-game settings surface for the gated features | M | Med→Low | **Implemented** (field rev C 2026-10-29: Route B label + cursor fix, field round 2 pending; land: ledger row 34) |

Rows 0a/0b are **Task C** (handoff §9 item 3) — already scoped, awaiting go; they are
the slate's head, not new scope. Rows 1–9 were the new candidates; row 6 shipped for 0.0.4, and row 1 shipped as the orb-face readout and was repurposed post-release (see §1).

---

### 1. Persistent globe text (was: HP/MP globe hover values) — `[features] globe_persistent`

- **REPURPOSED 2026-10-30 (post-0.0.4 field round, ledger row 40)**: the orb-face
  readout (patches 0013+0014, shipped 2026-10-29, field-verified) duplicated what
  retail already does — PANEL_DrawPanelLayout prints the same cur/max ABOVE each orb
  on hover (locale 0x1045/0x1046, baseline ScreenSizeY-0x5f), and retail even ships
  its own always-on toggle ("Show HP/MP Text", the bShowHpText/bShowmpText registry
  keys). The orb-face draw, its decl, the DRAW_UI call and the old ini key are
  removed; the row is now `[features] globe_persistent` ("Persistent Globe Text"):
  the gate is ||'d into Panel.cpp's two draw conditions so retail's above-orb text
  shows without hovering. `0` = vanilla hover-only; retail's own toggle still works;
  a stale `globe_values=1` ini line is inert. The as-built notes live in handoff
  Rev 22 / Rev 26.
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

### 4. Transparent bottom bar — `[features] transparent_hud`

- **Behavior (owner suggestion, 2026-10-29 — not a D2R option)**: the bottom control
  panel's stone texture is not rendered — the game world shows through the band it
  used to cover. Everything drawn on top keeps rendering: orb fills, gold readout,
  run/menu buttons, stamina + XP bars, skill icons, belt. Hit-tests are rect-based
  (`PANEL_IsMouseIn*`), so clicks keep working with the stone gone.
- **Proposed**: gate `transparent_hud`: `0` = vanilla; `1` = skip the stone background
  blits (clean see-through); `2` = draw the same blits through one of the engine's own
  transparency draw modes instead of `DRAW_SOLID` — the same stipple family the panel
  already uses for orb fills and the stamina bar ("D2-authentic" dithered
  transparency; no true alpha in the paletted path, same caveat as row 7).
- **Candidate regions (tree-scan 2026-10-29 — the cleanest choke point on the slate)**:
  the entire bottom bar is one function, `D2Client/UI/Panel.cpp`
  `PANEL_DrawCharacterInfo()` (scan: lines 1329–1382) — 5 `DRAW_SOLID` frames of
  `Panel\CtrlPnl7` (non-800 modes) or 6 frames of `Panel\800CtrlPnl7` (800x600),
  called from `DRAW_UI_BordersAndOrbsAndFillers()` immediately before the orb/button/
  bar draws it overlays. Gate the blits inside this one function; nothing else is
  touched. Optional scope variants, same pattern: the 800-only stone side rails
  (`DRAW_UI_BorderLeft`/`DRAW_UI_BorderRight`, border-type gated in the same
  dispatcher) and the mini-panel background.
- **Key verify items**: (a) the world stage fills the full frame every draw, so a
  skipped blit reveals the world, not garbage — confirm in-game; (b) both image
  variants (`CtrlPnl7` + `800CtrlPnl7`) covered — the function's two branches already
  span them; (c) nothing reads back the stone pixels (orbs/buttons draw independently
  in the sibling `PANEL_Draw*` calls) — verify; (d) the call site passes
  `nPanelOffsetY = (eRendererMode == RENDERED_DirectDraw)`, i.e. the panel anchor
  differs per renderer — test DirectDraw vs the other renderer.
- **Prior art**: no D2R parity — owner suggestion; the see-through-HUD look is a
  staple of other games' HUD packs, and see-through D2 UI is engine-native: Phrozen
  Keep documents the draw-mode table (25%/50% stipple, color blends) modders have
  used for transparent panels since the 1.10 era.
- **Risk**: Low–Med. Pure draw-gating — no input, balance, or save surface. Care
  points: taste (mode 1 leaves orbs/buttons floating over live world — mode 2 is the
  softer look) and the renderer × resolution QA matrix.
- **Exit**: gate on → world visible through the bottom band in 640/800 and both
  renderers; every button/orb/tooltip behaves exactly as vanilla (row-31
  click-through check); `0` = vanilla byte-for-byte.

### 5. Monster HP bar on hover — `[features] monster_hp_bar`

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

### 6. Auto gold pickup — `[features] auto_gold`

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
  *(Revised by the draft: the trigger is client-only and the server 0x16 flow is
  reused untouched — effective risk Low–Med.)*
- **Exit**: walk-over picks gold exactly like click (full-inventory: pile stays),
  items untouched, `0` = vanilla.
- **Draft**: `DRAFT_auto_gold.md` (v2, 2026-10-29) — tree-verify pass complete:
  **zero D2Game/D2Common edits** (the 0x16 interact flow already does
  walk-then-pick server-side); client-only trigger in 3 TUs. Owner decisions
  2026-10-29: **strict radius** — fire only at `< 5`, the server's own instant-pick
  distance, mirrored together with the server's `0x804` collision test, so a fire
  can NEVER start retail's walk-to-item (the character is never moved) — and
  **combat guard on** — fires only from Neutral/TownNeutral/Walk/Run.
  **Implemented + field-verified** (uncommitted): `D2Client.cpp` (gate +
  per-frame tick + dedupe ring + ini template block), `D2Client.h` (2 decls),
  `UI/Draw.cpp` (1 call line in `DRAW_UI`, outside the AltShowItems gate).
  Sandbox syntax-verified against the real signatures; llvm-mingw i686 build
  green (user Win11); **field-verified 2026-10-29** (owner: "Auto-Gold Pickup
  works!" — session ran with `alt_toggle=1` enabled alongside, row-31
  coexistence). Optional deep-QA V2/V5/V6 rides a later round, non-blocking.
  Land: ledger row 33 per handoff §7.

### 7. Automap scale + position — `[features] automap_scale`

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

### 8. Quick-cast keys — `[features] quick_cast`

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

### 9. Shared side-file stash — `[features] shared_stash`

- **Behavior (owner suggestion, 2026-10-29 — not a D2R option)**: an in-game stash
  shared across all single-player characters — deposit/withdraw between saves without
  GoMule/ATMA muling or a second client. **No `.d2s`/`.d2x` layout change**: items only
  move in/out of the existing container lists, which the format already supports (a
  char that never touched the stash is just an empty list). The stash contents live in
  their own **mini save** — versioned header + checksum, atomic write + `.bak` — that
  retail ignores. PlugY ships the same idea as save-dir side files.
- **Proposed**: gate `shared_stash`: `0` = vanilla; `1` = the stash panel gains a
  shared mode toggled by a hotkey while the panel is open (registration via the row-28
  `CmdTbl` path) — same grid UI, different backing container, a small text caption
  marks the mode (row-1 text helpers, no new assets). Deposits/withdrawals are
  server-side container-to-container **moves** (SP is in-process) using the existing
  D2Common inventory ops; the side file is written on change (or at save time — verify
  the hook) and re-read when the stash opens.
- **Candidate regions**: container ops `D2Common/INVENTORY/Inventory.*`
  (`INVENTORY_AppendItemToList`, `INVENTORY_PlaceItemInGrid`,
  `INVENTORY_FindFirstEmptySlot`, inventory-create family — the stash page is already
  "just" an inventory container per the recovered header docs); item bit
  pack/unpack in `D2Common/ITEMS/Items.*` (anchors live deeper — `Items.h` exports
  only `FreeItems` per the fix-14 map); file IO + save dir through the
  `GetAndCreateSavePath` choke-point family (`D2Game/PLAYER/PlayerSave.cpp`,
  `D2Launch/CharSel.cpp`, Fog `SFile`); stash panel + click handling
  `D2Client/UI/Inv.cpp`; hotkey registration `D2Client/UI/CmdTbl.cpp` (row-28
  proven).
- **Contract note (needs owner sign-off)**: not display- or input-only — items move
  between characters, so this is an explicit amendment to rule 5's spirit. It honors
  rule 5's letter (`.d2s`/`.d2x` byte-compatible) and cannot duplicate items (single
  move path, server-side). Deleting the side file deletes its items — document in the
  ini template comment.
- **Risk**: Med–High — item loss/duplication is the worst bug class in the game.
  Mitigations: move semantics only, atomic write + backup, versioned header +
  checksum, and a dedicated §11 mule round-trip test (deposit → save → crash-kill →
  reload → withdraw). Effort **L** — the biggest row on the slate, alongside row 8;
  schedule after the draw-only rows have hardened the gate/overlay patterns.
- **Exit**: deposit on char A, save, load char B, withdraw — item stats/quality/
  sockets bit-identical (ATMA export compare), no dupes or losses on crash-kill
  during save, retail ignores the side file, `0` = vanilla.

### 10. Vanilla+ Options menu page — settings UI (not gated)

- **Behavior (owner suggestion, 2026-10-29)**: a "Vanilla+ Options" row on the Esc
  Options page (directly above "Previous Menu") opening a ControlConfig-style text
  screen with one toggle row per shipped `[features]` key (`alt_toggle`,
  `auto_gold`, `portable_save`, `globe_persistent`; grows as slate rows land). Click = flip + write
  `d2vp.ini` (`WritePrivateProfileStringA`, first ini writer in tree) + flush the
  2 s gate caches, so changes apply same-frame instead of on the next ini edit.
- **Tree-verify complete (2026-10-29, draft anchor map)**: the Esc-menu loader
  **skips rows with an empty image path** (no asset needed for the row itself). The
  row label ships as **Route B** (field-QA revision, draft §9): `fpDrawCallback`
  renders it with the retail `DRAW_text` path (font 0xd) every frame, and
  `fpInitCallback` seeds an 8×8 blank placeholder DC6 (raw file-shaped image →
  retail `CELCMP_FixupPointersAndPrepare` → freed by the retail
  `DC6_FreeAndDelete`) that satisfies the type-0 draw with zero visible pixels —
  no shipped assets, no loader edits, free-path compatible by construction.
  (Route A — a glyph-synthesized text DC6 — was retired in field QA: invisible in
  practice, plus bottom-up/palette-shift and encode-buffer defects; draft §9.1.)
  Page mirrors ControlConfig (`UI_CONFIG` precedent: escmenu row click swaps to a
  font-text screen; input = mouse down/up + Esc only — no 0x200 handler, draft
  §9.2). UI-flag store is a hard-asserted 38-slot array — `UI_VANILLAPLUS = 38`
  needed a 4-site mechanical extension (struct member, count assert, GetUIFlag
  bound, dismiss table).
- **Draft**: `DRAFT_vanilla_plus_options.md` (v1→v2, 2026-10-29) — 12 anchors,
  grep gates, V1–V6; owner decisions recorded: Route A + above-Previous placement.
- **Implemented (2026-10-29, draft v2 §7–§8; field-QA rev C §9)**: label route
  Route B + page + `UI_VANILLAPLUS = 38` extension landed across 11 retail files +
  2 new TUs (`D2Client/UI/VanillaPlusOptions.{h,cpp}`). V1–V5 pins resolved
  (DC6Block = frame header, bottom-anchored draw, same allocator pair,
  dismiss-table growth zero-fills). Sandbox harness (verbatim extraction,
  `g++ -fsyntax-only`) + 12 grep gates green. **Field round 1** (rev 2 overlay):
  builds + runs; label invisible (Route A fallback) + cursor lock (0x200
  chain-break) → rev 3 = Route B label + 3-entry input batch + baseline-anchored
  page text. Field round 2 + real llvm-mingw gate pending (owner).
- **Risk**: Med→Low. Novel Route A synthesis retired; everything now mirrors
  proven in-tree patterns (ControlConfig text/rows, esc-menu row callbacks, blank
  placeholder = the byte shape field-proven in round 1). Zero D2Game/D2Common
  edits, no loader edits, all retail rows byte-identical.

---

## Dropped candidates (and why)

| Candidate | Verdict | Reason |
|---|---|---|
| Expanded stash pages | **Dropped** | Save-format hazard (`.d2x` layout + inventory grid realloc); D2R did it with new UI + data. Revisit only if upstream ever blesses save-format work. The **side-file shared stash** — no `.d2x` change — is scoped separately as row 9. |
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
- Owner suggestions: transparent bottom bar (2026-10-29 — explicit no-parity row);
  shared side-file stash (2026-10-29 — no-parity row, PlugY-style side file).
- Engine grounding: Phrozen Keep draw-mode documentation (25%/50% stipple, color
  blends; 1.10/1.13-era threads) — the paletted transparency family row 4 mode 2 uses.
- In-tree grounding: handoff.md rows 28–31 (the feature-era map), fix-14 Drop.cpp map;
  FEATURES row-4 scan of `D2Client/UI/Panel.cpp` (2026-10-29).
