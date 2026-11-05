# FEATURES.md — D2R backport slate (vanilla-plus)

*v6, 2026-11-05. Rows 14–15 added on owner ask — **planned, not implemented**: resolution
modes. Row 14 activates retail's own **dormant 1344x700** (mode 3 — wired in the binaries,
never a UI stop); row 15 adds **1920x1080**. Video-options slider stops, not `[features]`
keys. Tree-verified same-day against the pushed tree (1344x700-PLAN.md §8): a third gap
joined row 14 — the mini-panel's own position→mode re-apply (`SetupMiniPanel`) — and the
dims-pair count corrected to seven. **Row 14 then IMPLEMENTED the same day (R109)**: the
three-gap code round — gates 394/394, syntax-check digests identical, llvm-mingw build
gate + field QA pending (owner). Earlier: v5, 2026-11-05. Rows 12–13 (`stack_gems`, `stack_runes`) added from an owner suggestion —
**planned, not implemented**: no D2R parity (Blizzard never shipped rune/gem stacking — a live
player request through 2.7), PD2 ships it. Earlier: v4, 2026-11-05. Status sweep: shipped rows
marked Shipped with their first release (1, 3, 6, 9, 10, 11); row 9's SMOKE debts cleared (§H items 25/27 in the R83 full-belt pass [#24], the ten-page items in the R104 pass [#28]); §1/§6/§9/§10 as-built notes refreshed, §9's pre-ship draft marked superseded by the shipped in-panel tab. Earlier: v3, 2026-10-29 — row 9 (`shared_stash`) added from an owner suggestion — side-file design, `.d2s`/`.d2x` untouched; row 4 (`transparent_hud`) added from an owner suggestion — no D2R parity, engine-native draw modes. Scoped by research: D2R official QoL behavior + prior art from the
classic-D2 modding scene (PD2, BH/slashdiablo maphack, Sting's hackmap, d2dx). Ranked
**low-risk first** per owner call. **Code-only** per charter Principle 4: no MPQ/data
edits, no new assets — every feature is pure code + a `d2vp.ini` gate. Hook sites below
are **candidate regions** taken from the known feature-era map (handoff.md rows 28–31);
each feature gets a Task-C-style tree-verify pass (exact anchors, grep gates) before any
implementation round. Live tasks: handoff.md §9. Charter: VANILLA-PLUS.md (as-built
feature docs); this file is the slate + per-row engineering record.*

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
| 1 | Persistent globe text (was: hover values) | HUD | D2R globe hover shows current/max | S | Low | **Shipped 0.0.5** — repurposed 2026-10-30 pre-ship: the orb-face readout duplicated retail's own above-orb hover text, so `globe_persistent` just pins that text on (ledger row 40; options page row 4; see §1) |
| 2 | Clock / game-timer overlay | HUD | D2R Tab-screen game timer; BH wall-clock | S | Low | `clock_overlay` |
| 3 | Item ilvl on tooltips | Loot | D2R 2.5 display-item-level toggle | S–M | Low | **Shipped** (R55 standalone line → R56 name-suffix → R57 bare digits → R59 single-space trim — `item_ilvl`, see §3; first shipped in release 0.0.7) |
| 4 | Transparent bottom bar | HUD | None — owner suggestion; see-through-HUD staple elsewhere | S–M | Low–Med | `transparent_hud` |
| 5 | Monster HP bar on hover | HUD | D2R enemy health bar (PC) | M | Low–Med | `monster_hp_bar` |
| 6 | Auto gold pickup | Loot | D2R "Gold Pickup" gameplay option | M | Med | **Shipped 0.0.4** (2026-10-29: ledger row 33; field-verified — "Auto-Gold Pickup works!" — coexists with alt_toggle) |
| 7 | Automap scale + position | HUD | D2R larger overlay map | M | Med | `automap_scale` |
| 8 | Quick-cast keys | Controls | D2R-adjacent (PD2 quick cast) | M–L | Med–High | `quick_cast` |
| 9 | Shared side-file stash | Stash | None — owner suggestion (PlugY-style side file) | L | Med–High | **Round 1 FIELD-PROVEN (R63, Rev 82; build-fix R64, Rev 83; tab-anchor fixes R65+R66, Revs 84-85; owner on R66: "Great job. I see it. It works.")** — 1 retail-shaped page via a stash-panel "Shared" tab (no hotkey), items + gold, own `_d2vp_shared.sss` file, `.d2s` retail-pure (save-flush design); SP-only v1; mechanism field-proven on the R65 build blind (page swap + cross-char item/gold round trip), tab visibility confirmed on R66. First shipped in release **0.0.7** (cut at R66); in-game **Shared Stash** row on the Vanilla+ Options page added R67 (no longer ini-only). **R75 (Rev 97): a SECOND shared page + pager** (owner: "give the current stash tab label an outline" + "add a 2nd page and buttons/arrows to navigate pages") — the label draws with a 1-px black halo on all four sides, `<`/`>` arrows beside the word step personal → Shared 1 → Shared 2, the word itself toggles back to personal, `_d2vp_shared.sss` v2 stores both pages (v1 files load as page 1), the 0x4F/0x7D param word carries the absolute desired page. (The R68–R71 tab polish — Font24, floating row, backdrop pill — was rolled back by owner decision at `59ff645`; R75 builds on the R66 in-panel tab.) **R82 (Rev 105): TEN shared pages** (owner: "increase the shared stash pages to 10") — the planned v2-header widening: same magic + version, `pageCount` 10 (header 28 → 92 bytes), pre-R82 two-page files read back with pages 3-10 empty (next save rewrites ten), labels `Shared 1`…`Shared 10`, arrows still step one page, one gold pool across all ten. SMOKE debts cleared: §H items 25 (`.d2s` in retail) + 27 (degrade drill) confirmed in the R83 full-belt pass ([#24](https://github.com/VTSTech/D2VP-QoL/issues/24), 2026-11-02), the ten-page items 33-35 rode the R104 full pass ([#28](https://github.com/VTSTech/D2VP-QoL/issues/28), 2026-11-04); the "Then 100" leg widens the same two constants. Plan: SHARED-STASH-PLAN.md |
| 10 | Vanilla+ Options menu page | UI | None — owner suggestion 2026-10-29; in-game settings surface for the gated features | M | Med→Low | **Shipped** (landed 2026-10-30, ledger row 34; first release **0.0.6**); **eight rows as of R108** — the slate's five shipped keys + Shared Stash (R67: the row busts the client stash gate cache too, so the tab follows the toggle the same frame) + No Panel World Shift (R101, issue #26 round 9: the write bridge already busts the PANELSHIFT cache, so the flip applies same-frame; the seventh row moved the Previous Menu row down one pitch, 0x124→0x144) + Mana Burn Fix (R108: the R106 combat gate joins the panel — the eighth row moved the Previous Menu row down one pitch again, 0x144→0x164, and grew the box 0x171→0x191; the server gate keeps its 2 s poll by design, so a flip lands on the next drain roll within 2 s while the panel shows it same-frame) |
| 11 | Mana Burn fix (melee drain 256x bug) | Combat (D2Game) | D2R 2.4 patch notes ("fixed melee hits draining 256 times too much") + Basin wiki Mana Burn | S | Low | **Implemented** (R106 — `mana_burn_fix`; ini-only at ship, in-game **Mana Burn Fix** row on the Vanilla+ Options page since R108; server-side gate TU + VP-marked drain-roll shim in SUnitDmg.cpp; field QA + llvm-mingw gate pending) |
| 12 | Stackable gems | Inventory | None — owner suggestion 2026-11-05; PD2 stacks gems in its currency tab | M | Med | `stack_gems` |
| 13 | Stackable runes | Inventory | None — owner suggestion 2026-11-05; PD2 stacks runes in its currency tab | M | Med | `stack_runes` |
| 14 | 1344x700 resolution (retail's dormant mode 3) | Video / renderers | Retail-dormant engine mode — the binaries carry the tables, no UI stop ever shipped; D2MultiRes-era community prior art | S–M | Low–Med | **Implemented** (R109 — Gap A `case 3` + Gap B slider stop 3 + Gap C mini-panel mapping; gates 394/394; llvm-mingw gate + field QA pending (owner); plan: 1344x700-PLAN.md) |
| 15 | 1920x1080 resolution | Video / renderers | None — owner suggestion 2026-11-05; SGD2FreeRes/D2DX-class arbitrary-res prior art | M | Med | video-options slider stop 4 (not `[features]`-gated) |

Rows 0a/0b are **Task C** (handoff §9 item 3) — already scoped, awaiting go; they are
the slate's head, not new scope. Rows 1–9 were the new candidates; row 6 shipped for 0.0.4, and row 1 shipped as the orb-face readout and was repurposed post-release (see §1).
Rows 12–13 are **planned scope** (owner suggestion 2026-11-05, not yet implemented): one
mechanism — letting a class of items merge into stacks the way arrows/tomes already do —
behind two separate gates, drafted together in §12/§13. Rows 14–15 are planned scope too
(owner ask 2026-11-05): 1344x700 activates a retail-dormant engine mode — the whole
site-1..6 pipeline already routes it, three gaps close it (§14, [1344x700-PLAN.md](1344x700-PLAN.md)) —
and 1920x1080 is one full site-1..6 round on the 720p playbook (§15).
**Shipped as of R108**: rows 1, 3, 6, 9, 10 and 11 — plus, outside this slate, the
pre-slate pair `alt_toggle`/`portable_save` (handoff ledger rows 28–31, releases
0.0.2/0.0.3) and `no_panel_shift` (born from issue #26, R95–R101). As-built docs for
everything shipped: [VANILLA-PLUS.md](VANILLA-PLUS.md) Features section.

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
- **Shipped (0.0.5, 2026-10-29)**: exactly the repurposed design — the gate is ||'d
  into `D2Client/UI/Panel.cpp`'s two above-orb draw conditions, so retail's own
  cur/max text shows without hovering and nothing else is drawn. Managed from the
  Vanilla+ Options page (row 4, "Persistent Globe Text") and
  `[features] globe_persistent`; retail's "Show HP/MP Text" toggle keeps working.
  The Proposed/Candidate/Exit blocks below are the pre-repurpose draft (the original
  orb-face readout), kept for record.
- **Proposed (pre-repurpose draft)**: while the cursor is over either bulb, draw `cur / max` text beside it,
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

### 3. Item ilvl on tooltips — `[features] item_ilvl` (SHIPPED R55, name-suffix presentation R56, bare digits R57, single-space trim R59)

- **D2R behavior**: 2.5 shipped a toggleable display-item-level (advanced item stats).
- **Shipped (R55 + R56 + R57 + R59)**: with `item_ilvl=1`, the tooltip's **name line** reads
  `Long Sword (12)` — the ilvl is appended to the item name in all three
  tooltip builders (`LoadItemDesc` identified + unidentified paths,
  `ITEM_BuildSetItemTooltip`), rendering in the name's own color. R55 shipped a
  standalone `Item Level: <ilvl>` line below Required Level; R56 moved it into
  the name per owner preference; R57 dropped the `ilvl` label word — players
  know what the bare `(N)` means. R59 trims the name's trailing spaces
  before appending: `GetItemName` glues string-table entries (magic affix
  words, unique/set names, separators) and some of those entries end with
  their own space, which used to render a double gap (`Name  (12)`); the
  gap before `(N)` is now always exactly one space. The ilvl lives on the item unit data
  (`D2Common::ITEMS::Items::ITEM_GetItemData_nItemLevel`, item-data offset 0x2C
  — the same parsed-item struct the retail req-level line reads client-side);
  ilvl travels in the item bitstream on every path that reaches the tooltip.
  Shop-narrow / book / transmogrify tooltips stay vanilla by design.
- **Prior art**: D2R 2.5; BH ilvl display; hackmap.
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
  **Implemented + field-verified, shipped 0.0.4**: `D2Client.cpp` (gate +
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

- **SHIPPED — field-proven end to end**: first in release **0.0.7** (cut at R66),
  grown to **ten** shared pages (R82); the as-built record — the outlined in-panel
  tab, the `‹`/`›` pager, the `_d2vp_shared.sss` v2 header and the ten-page widening,
  plus the SMOKE confirmations — lives in slate row 9 above and in
  [SHARED-STASH-PLAN.md](SHARED-STASH-PLAN.md). The design text below is the
  pre-implementation draft: its hotkey idea was superseded by the in-panel tab, and
  the rule-5 contract note was owner-signed off by shipping (`.d2s` stays
  retail-pure; one server-side move path; deleting the side file deletes its items,
  documented in the ini template).
- **Behavior (owner suggestion, 2026-10-29 — not a D2R option)**: an in-game stash
  shared across all single-player characters — deposit/withdraw between saves without
  GoMule/ATMA muling or a second client. **No `.d2s`/`.d2x` layout change**: items only
  move in/out of the existing container lists, which the format already supports (a
  char that never touched the stash is just an empty list). The stash contents live in
  their own **mini save** — versioned header + checksum, atomic write + `.bak` — that
  retail ignores. PlugY ships the same idea as save-dir side files.
- **Proposed (draft — superseded by the shipped in-panel tab)**: gate `shared_stash`: `0` = vanilla; `1` = the stash panel gains a
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
- **Contract note**: not display- or input-only — items move
  between characters, so this is an explicit amendment to rule 5's spirit. It honors
  rule 5's letter (`.d2s`/`.d2x` byte-compatible) and cannot duplicate items (single
  move path, server-side). Deleting the side file deletes its items — document in the
  ini template comment. *(Owner sign-off: shipped as row 9 above.)*
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
  page text. **Shipped in 0.0.6** (2026-10-30, ledger row 34); field-proven since
  via the R83 and R104 full smoke belts ([#24](https://github.com/VTSTech/D2VP-QoL/issues/24),
  [#28](https://github.com/VTSTech/D2VP-QoL/issues/28)); the page carries eight rows
  as of R108 (slate row 10 above).
- **Risk**: Med→Low. Novel Route A synthesis retired; everything now mirrors
  proven in-tree patterns (ControlConfig text/rows, esc-menu row callbacks, blank
  placeholder = the byte shape field-proven in round 1). Zero D2Game/D2Common
  edits, no loader edits, all retail rows byte-identical.

---

### 11. Mana Burn fix — `[features] mana_burn_fix` (SHIPPED R106; Options-page row R108)

- **The bug (retail 1.14d, verified in this tree 2026-11-05)**: monsters with
  the Mana Burn affix drain 256x the intended mana with melee hits — 512x in
  Hell — emptying a mana pool per blow. Two retail defects compound:
  (1) the affix stores `UNITSTAT_manadrainmindam/maxdam` already in the mana
  1/256 fixed point (`D2Game/Monsters/Umod.cpp` ManaBurn scales the
  MonUMod.txt 66/100 / 33/50 percentages by `0x100`), and the melee drain
  roll (`D2Game/UNIT/SUnitDmg.cpp` `DAMAGE_CalculateOffensiveDamage` →
  `DAMAGE_RollElementalDamageWithBonus`) shifts them ANOTHER 8 bits — that
  helper's `<<8`/`*0x100` inputs are right for the plain-point
  fire/cold/lightning stats it also serves, wrong for the drain family. The
  missile path (`D2Game/MISSILES/MissilesMode.cpp`) reads the same stats raw
  and was always correct. (2) `DAMAGE_CalculateResistance` applies the
  difficulty resist penalty (-40 NM / -100 Hell) to the drain fields' zero
  resist base (their table entries carry no resist stat), multiplying the
  drain x1.4 / x2 — the "hidden" Hell doubling. Community record:
  r/ProjectDiablo2 ("applies this multiplication twice … 256 times … 512
  times"); Basin wiki: "it is multiplied by 256 when applied by a melee
  attack. Fixed in D2R."
- **Shipped (R106)**: gate `mana_burn_fix` (ini-only at ship, server side —
  new fork TU `D2Game/UNIT/VanillaPlusCombat.{h,cpp}`, the shared-stash
  server-gate shape: own statics + 2 s GetTickCount hot-read + the shared
  `ALTTOGGLE_BuildIniPath` helper). In `D2Game/UNIT/SUnitDmg.cpp`, one
  VP-marked static shim (`DAMAGE_RollDrainStatFixedPoint`) plus three
  VP-marked call sites (life/mana/stamina drain rolls): gate on → the raw
  fixed-point stats are rolled through the SAME file statics (same RNG,
  same shape, same `byScale` consumer); gate off → the retail call,
  byte-for-byte. The life/stamina sites share the latent double-scale but
  nothing server-side sets those stats on melee attackers today (missiles
  carry them via the missile path) — covered anyway for consistency. The
  difficulty x1.4/x2 resist penalty is left alone ON PURPOSE (owner call,
  2026-11-05): with the gate on, Hell Mana Burn drains up to 200% of the
  monster's MonLvl DM per hit — exactly D2R's post-2.4 behavior.
- **Options-page row (R108, owner ask: "the mana_burn_fix toggle is not in
  the Vanilla+ Options panel ... For R108 let's add it to the Options
  Panel")**: the toggle is no longer ini-only — an eighth row **Mana Burn
  Fix** on the Vanilla+ Options page rides the shared `D2VP_*` bridge (one
  `gaVanillaPlusRows` entry; the ini template block shipped with R106, so
  nothing else). The server gate keeps its 2 s poll (the stash-server
  shape — D2Game cannot import D2Client helpers), so a panel flip lands on
  the next drain roll within 2 s, the same latency as an external ini edit;
  the panel draw re-reads the key fresh every frame and shows the flip
  immediately. The eighth row moved the Previous Menu row down one pitch
  (0x144 → 0x164) and grew the box (0x171 → 0x191, same 0x13 bottom
  margin; fits 640x480).
- **Prior art**: D2R 2.4 fix; Basin wiki Mana Burn page.
- **Risk**: Low. One roll-site branch, no state, no save surface; off =
  retail byte-for-byte (charter rule 1).
- **Exit**: a Hell Mana Burn unique with monlvl DM 40 drains 26-40 mana per
  hit (not 13,517-20,480); `0` = vanilla byte-for-byte. Field QA + the
  llvm-mingw build gate pending (owner).

---

### 12. Stackable gems — `[features] stack_gems` (PLANNED — owner suggestion 2026-11-05, not implemented)

- **No D2R parity — researched before drafting**: D2R never shipped rune/gem stacking. It is
  one of the longest-running D2R player requests — 2021 launch-window petitions, "Really?
  2023 and no stackable runes/gems?" on the Blizzard forums, "stackable runes and gems … they
  did nothing" replies on the 2.7 notes thread — so this row is a **prior-art backport**, not
  retail parity: Project Diablo 2 stacks gems (and runes) in its currency tab, and its patch
  notes read "Hellforge will now drop stacked gems and runes". Same framing as rows 4 and 9
  (owner suggestion, no-parity).
- **Behavior**: identical gems occupy one inventory/stash/cube cell with a count; the freed
  grid squares are the entire payoff (ten perfect amethysts take one cell, not ten). No
  balance surface: a stack is exactly the sum of its members.
- **Proposed**: gate `stack_gems`: `0` = vanilla; `1` = gems of the same type merge
  server-side when one lands in a container that already holds one (pickup, drop, stash
  move), tracked by the existing stack machinery — the same quantity bookkeeping stacked
  arrows/tomes already use — with the count drawn by the existing stack-count text. Stack
  splitting is NOT v1 (picking a stack up takes the whole stack; splitting can ride a later
  round if wanted). Stack cap is an owner decision at draft time; the vanilla quantity field
  arrows already use sets the hard upper bound (exact width = tree-verify item).
- **Candidate regions (all "verify" — no tree-verify pass yet)**: the per-item
  stackable/maxstack fields on the item unit data (`D2Common/ITEMS/Items.*` — the gem code
  family gcv/gcw/gcg/gcb/gcs/…; flipping the in-memory fields at item-unit init is code-only
  and reuses every downstream stack path); the merge-on-add branch beside the row-9
  inventory ops (`D2Common/INVENTORY/Inventory.*` — add-to-grid already merges stackables —
  verify); the client stack-count draw (`D2Client/UI/Inv.cpp` region — whatever arrows
  render today); the cube's partial-consume path (gem recipes must take 1 of N off a stack —
  the quantity-decrement machinery belt items already exercise — verify the cube calls it).
- **Contract note (rule 5, row-9 disclosure shape)**: a stacked gem is written to the
  bitstream the vanilla way — stackable bit + quantity field, the exact encoding retail
  already reads on arrows/tomes — so `.d2s` layouts stay untouched and retail/ATMA/GoMule
  keep loading the char. Existing stacks survive a gate flip OFF (they are just legal
  stackable items; they simply stop growing). Disclose in the ini template comment, like
  row 9 did.
- **Prior art**: PD2 currency tab (stacked gems/runes); D3 gem stacking (the model players
  cite in the D2R request threads).
- **Risk**: Med. The first row since 9 that changes item state server-side: merges must be
  server-authoritative (no client-predicted quantities), the one-cell-freed grid math and
  the cube/vendor consume paths need the QA loop, and duplication is again the worst-case
  bug class — a merge is destroy-two-add-one through a single code path.
- **Exit**: twenty identical gems picked up in a row end as one cell with the correct count
  on the item and in the tooltip; a gem cube recipe consumes from the stack; the stack
  survives save → load bit-identical (ATMA compare); retail loads the same `.d2s`; `0` from
  a fresh start = vanilla byte-for-byte (no merge ever happens).

### 13. Stackable runes — `[features] stack_runes` (PLANNED — owner suggestion 2026-11-05, not implemented)

- **Same mechanism as row 12, separate gate** — one tree-verify pass and one implementation
  round cover both rows; this row exists so gems and runes toggle independently (the ini
  template ships both keys). §12's contract note, risk shape and exit loop apply verbatim
  to runes.
- **Rune-specific surface**: the item codes are `r01`–`r33` (verify in-tree). Two paths
  matter beyond §12's: runeword/cube recipes consume exactly 1 of a rune off what may be a
  large stack (the partial-consume path is THE critical verify item — a wrong decrement
  here is an item-printer bug), and the Hellforge quest reward (runes drop there; v1
  proposal: always drops fresh, no quest-path edits — merging it into an existing stack,
  PD2-style, is a later call).
- **Prior art**: PD2 (runes stack in the currency tab; "Hellforge will now drop stacked
  runes"); the GoMule/ATMA muling practice of one-cell-per-rune-type this replaces.
- **Risk / Exit**: as §12. Rune-flavored exit adds: an upgrade recipe (3× Thul → 1× Amn
  class) consumes 3 off a stack of N without dupe or loss, and two of the same rune in one
  container always end as one stack with the right count.

### 14. 1344x700 resolution — activate retail's dormant mode 3 (IMPLEMENTED R109 — code in tree; llvm-mingw gate + field QA pending)

- **As-built (R109, the three-gap round)**: **Gap A** — `CLIENT_SetScreenResolution`
  case 3 sets ScreenSize 1344x700 + `nScreenMode = 1`, riding the same retail downstream
  flow (WindowSize, border type, automap recenter, `D2GFX_SetVideoMode(3)`) as the 720p
  case. **Gap B** — the video-options slider's fourth stop: a second Route-B blank
  attaches into `pDC6SubImages[3]` at init (count 4; the slot was pre-reserved in
  EscMenu.h), both position→mode mappings add `3 → mode 3` ahead of the `>= 2` arm, and
  `UI_SLIDER_DrawResolutionValue` picks from a per-stop label table (`L"1280x720"` /
  `L"1344x700"`) — the issue-#23 right-align geometry proved label-agnostic. **Gap C**
  (the tree-verify catch) — `SetupMiniPanel`, the game-enter/cutscene-exit re-apply,
  clamp `> 2` → `> 3` plus a `== 3 → mode 3` arm ahead of the `>= 2` arm. Failed stop-3
  attach degrades gracefully: the count stays 3 and the load clamp folds position 3 to
  720p. Drive-by: the stale GDI decompiler comment "1360x700" corrected to 1344x700
  (`0x540`). Gates: 7 new `res3:` rows, `init attach` re-counted (3), minipanel clamp
  gate re-pinned (`> 3`), self-ID `Vanilla+0.0.7-R109` → **394/394**.
- **The dormant mode**: retail 1.14d ships mode-3 plumbing end to end but no UI stop — the
  size table maps 3 → 1344x700 (`D2gfx/D2GFX.cpp` `D2GFX_GetWindowSizeByResolutionMode`,
  case 3 = `0x540` x 700), GDI windowed has native `0x540`-wide branches (DIB alloc +
  scale factor), DDraw carried a `{0}` sentinel slot until the 720p round filled it with
  1344x700, and the D3D path clamped registry modes 1/3 back to 800x600 — while the video
  slider only ever offered two stops. The owner framing ("planned by Blizzard … never
  implemented or shipped") matches the mechanism half: wired, never exposed. The
  classic-D2 community has played on it for years via patches (D2MultiRes 2009-era;
  SlashDiablo HD1 "up to 1344x700"; Puppeteer's "1344x700 and 1068x600" dlls).
- **Why it is cheap**: the 1280x720 round (sites 1–6,
  [docs/resolution-widelayout-audit.md](docs/resolution-widelayout-audit.md)) built the
  wide-UI class generically and reserved mode 3 everywhere — `VPRES_IsWideLayoutMode`
  includes 3, offsets compute themselves (X 272 / Y 100), and every renderer dispatch and
  dims guard already lists (1344,700). Three gaps close it: the missing `case 3` in
  `CLIENT_SetScreenResolution`, the slider's missing fourth stop, and the mini-panel's own
  position→mode re-apply (`SetupMiniPanel` — runs at every game-enter and cutscene-exit).
  Plan: [1344x700-PLAN.md](1344x700-PLAN.md).
- **Proposed**: append a fourth slider stop mapping to mode 3. Existing registry positions
  0–2 keep their meaning (720p users unaffected); not `[features]`-gated — opt-in by
  picking the stop, same as 720p.
- **Risk**: Low–Med. Three files plus a label table; every renderer path was field-proven at
  720p. DDraw 8-bit fullscreen at 1344x700 is hardware-dependent (may be N/A on modern
  systems — record, don't block).
- **Exit**: pick the stop → 1344x700 with the 800-class UI centered (272/100); in-game
  switch 800↔1344↔720 clean; cutscene round-trip returns; save → relaunch keeps the stop;
  old registry values behave exactly as before.

### 15. 1920x1080 resolution (PLANNED — owner ask 2026-11-05, not implemented)

- **No retail parity**: 640x480/800x600 are retail's stops and 1344x700 is retail's dormant
  one — 1080p is pure owner scope, in the SGD2FreeRes/D2DX class of arbitrary-resolution
  play.
- **Why it rides the 720p playbook**: the same site-1..6 checklist per new mode —
  `CLIENT_SetScreenResolution` + D2GFX table cases, D3D size case + wide-set + dims guards,
  DDraw `[6]` tables + horizontal bounds, GDI windowed branches, Glide wrapper
  `g3x_resolution_size` entry 18 + client mapping, `VPRES_IsWideLayoutMode`, slider stop 5.
  The wide-UI offsets are formulas (X 560 / Y 480).
- **Order**: schedule AFTER row 14 — 1344x700 exercises the wide class at a non-1280 width
  first, cheap insurance; and the round should start by consolidating the copy-pasted
  dims-pair tests (`(1280,720) || (1344,700)` across Glide.cpp ×2, Direct3D.cpp ×2,
  DirectDraw.cpp ×1, Windowed.cpp ×1, VanillaPlusCore.cpp ×1 — seven copies) into one
  predicate, so per-mode cost stops growing.
- **Open verify items (tree-verify pass)**: D3D texture-cache sizing
  (`gnD3DTextureCacheBlockSize/Count`, floor/subtile caches — field-proven at 720p, unknown
  whether they scale from screen dims at 1080p); DDraw 8-bit fullscreen 1920x1080 (expect
  N/A by hardware); front-end menus; CPU-side tile dispatch at ~2.25× the 720p pixel
  count.
- **Risk**: Med. **Exit**: row 14's loop at 1920x1080 — UI centered (560/480), switch
  matrix clean, caches sane (no tile artifacts), save → relaunch keeps the stop.

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
  shared side-file stash (2026-10-29 — no-parity row, PlugY-style side file);
  stackable gems + runes (2026-11-05 — no-parity rows 12–13: D2R never shipped stacking —
  2021 petitions, the "Really? 2023 and no stackable runes/gems?" Blizzard forum thread,
  "they did nothing" replies on the 2.7 notes thread — while PD2 ships it, "Hellforge will
  now drop stacked gems and runes" in the patch notes linked above).
- Engine grounding: Phrozen Keep draw-mode documentation (25%/50% stipple, color
  blends; 1.10/1.13-era threads) — the paletted transparency family row 4 mode 2 uses.
- Resolution prior art: retail's dormant mode 3 (in-tree: size-table case, DDraw sentinel
  slot, GDI `0x540` branches); D2MultiRes (moddb, 2009 — "resolutions other than 640x480
  and 800x600"); SlashDiablo HD (slashdiablo.net/about — "HD1 with up to 1344x700");
  Puppeteer extended-resolution dlls ("1344x700 and 1068x600"); SGD2FreeRes
  (github mir-diablo-ii-tools — arbitrary-resolution backport project).
- In-tree grounding: handoff.md rows 28–31 (the feature-era map), fix-14 Drop.cpp map;
  FEATURES row-4 scan of `D2Client/UI/Panel.cpp` (2026-10-29); resolution site-1..6 record
  (docs/resolution-widelayout-audit.md) + the mode-3 recon in 1344x700-PLAN.md §3.
