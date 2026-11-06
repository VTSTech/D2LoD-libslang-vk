# FEATURES.md — D2R backport slate (vanilla-plus)

> **Agent lane.** This file is the feature slate for the agent (verbose by design:
> per-row scoping, implementation notes, round records). Humans: the feature table and
> status live in [README.md](README.md), per-release changes in [CHANGELOG.md](CHANGELOG.md).

*v6, 2026-11-05. Rows 14–15 added on owner ask — **planned, not implemented**: resolution
modes. Row 14 activates retail's own **dormant 1344x700** (mode 3 — wired in the binaries,
never a UI stop); row 15 adds **1920x1080**. Video-options slider stops, not `[features]`
keys. Tree-verified same-day against the pushed tree (plans/1344x700-PLAN.md §8): a third gap
joined row 14 — the mini-panel's own position→mode re-apply (`SetupMiniPanel`) — and the
dims-pair count corrected to seven. **Row 14 then IMPLEMENTED the same day (R109)**: the
three-gap code round — then the field round (owner QA): **Glide GREEN** (owner screenshot,
stop live in the menu), **D3D crash caught and fixed in-round** — Vista+ display-mode lists
omit 1344x700, so the exclusive DD7 `SetDisplayMode` rejected it and D2Win's fatal-resize
path exited the process; fix = a D3D host-mode letterbox ladder (1366x768 → 1360x768) plus
a client-side mode-list probe that refuses the stop cleanly when nothing fits (§14).
Gates 401/401; llvm-mingw build gate pending (owner; D3D re-test owed). **Field round 2
(owner QA, R110 → R111)**: the D3D crash fix held ("doesn't crash"), but the stop came up
as a **borderless window instead of fullscreen** — the Win10/11 DDraw fullscreen
emulation can also ACCEPT a mode it cannot actually set (fake success, no error to
react to), so R110's retry ladder never fired and the emulation's fallback (borderless
window at the logical size on the unchanged desktop) showed. R111 probes the mode list
BEFORE the mode-set — an unlisted stop goes straight to a listed host mode (a real mode
set) — and sizes the window to the HOST so the emulated-fullscreen window covers the
screen; frame stays 1344x700 with black letterbox bands (§14). Gates 405/405; llvm-mingw
build gate + D3D re-test pending (owner). **Field round 3 (owner QA, R111 → R112)**: the
fullscreen look held ("ok, i see the black bars") — the bands ARE the R111 letterbox on
the 1366x768 host mode, so R112 does two things the owner asked for in the same breath:
(1) **hosted stops stretch** — the logical frame is stretched over the whole host mode at
present (a stretched Blt instead of the Flip) and the cursor capture maps host pixels
back into the logical frame, so clicks land where the stretched pixels are (§14); (2)
**1366x768 becomes the slider's fifth stop** — resolution mode 5 (`0x556 x 0x300`), the
panel-standard 16:9 timing most laptop panels list natively, so D3D usually enters it
TRUE-exclusive (no hosting, retail Flip) and only exotic drivers fall back to the stretch
ladder (§14). Gates 432/432 (27 net-new `res5:`/`stretch:` rows); llvm-mingw build gate +
D3D re-test pending (owner). Earlier: v5, 2026-11-05. Rows 12–13 (`stack_gems`, `stack_runes`) added from an owner suggestion —
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
| 0b | Shift equipment-compare | Controls | D2R hover-compare | M | Med | **Implemented** (R125 — hold-Shift second tooltip, D2R side-by-side parity; shop stock included per owner; R126 field catch — shields compared the primary weapon, fixed with the same-kind slot pick — see §0b; `shift_compare`; llvm-mingw gate + field QA pending) |
| 1 | Persistent globe text (was: hover values) | HUD | D2R globe hover shows current/max | S | Low | **Shipped 0.0.5** — repurposed 2026-10-30 pre-ship: the orb-face readout duplicated retail's own above-orb hover text, so `globe_persistent` just pins that text on (ledger row 40; options page row 4; see §1) |
| 2 | Clock / game-timer overlay | HUD | D2R Tab-screen game timer; BH wall-clock | S | Low | `clock_overlay` |
| 3 | Item ilvl on tooltips | Loot | D2R 2.5 display-item-level toggle | S–M | Low | **Shipped** (R55 standalone line → R56 name-suffix → R57 bare digits → R59 single-space trim — `item_ilvl`, see §3; first shipped in release 0.0.7) |
| 4 | Transparent bottom bar | HUD | None — owner suggestion; see-through-HUD staple elsewhere | S–M | Low–Med | `transparent_hud` |
| 5 | Monster HP bar on hover | HUD | D2R enemy health bar (PC) | M | Low–Med | `monster_hp_bar` |
| 6 | Auto gold pickup | Loot | D2R "Gold Pickup" gameplay option | M | Med | **Shipped 0.0.4** (2026-10-29: ledger row 33; field-verified — "Auto-Gold Pickup works!" — coexists with alt_toggle) |
| 7 | Automap scale + position | HUD | D2R larger overlay map | M | Med | `automap_scale` |
| 8 | Quick-cast keys | Controls | D2R-adjacent (PD2 quick cast) | M–L | Med–High | `quick_cast` |
| 9 | Shared side-file stash | Stash | None — owner suggestion (PlugY-style side file) | L | Med–High | **Round 1 FIELD-PROVEN (R63, Rev 82; build-fix R64, Rev 83; tab-anchor fixes R65+R66, Revs 84-85; owner on R66: "Great job. I see it. It works.")** — 1 retail-shaped page via a stash-panel "Shared" tab (no hotkey), items + gold, own `_d2vp_shared.sss` file, `.d2s` retail-pure (save-flush design); SP-only v1; mechanism field-proven on the R65 build blind (page swap + cross-char item/gold round trip), tab visibility confirmed on R66. First shipped in release **0.0.7** (cut at R66); in-game **Shared Stash** row on the Vanilla+ Options page added R67 (no longer ini-only). **R75 (Rev 97): a SECOND shared page + pager** (owner: "give the current stash tab label an outline" + "add a 2nd page and buttons/arrows to navigate pages") — the label draws with a 1-px black halo on all four sides, `<`/`>` arrows beside the word step personal → Shared 1 → Shared 2, the word itself toggles back to personal, `_d2vp_shared.sss` v2 stores both pages (v1 files load as page 1), the 0x4F/0x7D param word carries the absolute desired page. (The R68–R71 tab polish — Font24, floating row, backdrop pill — was rolled back by owner decision at `59ff645`; R75 builds on the R66 in-panel tab.) **R82 (Rev 105): TEN shared pages** (owner: "increase the shared stash pages to 10") — the planned v2-header widening: same magic + version, `pageCount` 10 (header 28 → 92 bytes), pre-R82 two-page files read back with pages 3-10 empty (next save rewrites ten), labels `Shared 1`…`Shared 10`, arrows still step one page, one gold pool across all ten. SMOKE debts cleared: §H items 25 (`.d2s` in retail) + 27 (degrade drill) confirmed in the R83 full-belt pass ([#24](https://github.com/VTSTech/D2VP-QoL/issues/24), 2026-11-02), the ten-page items 33-35 rode the R104 full pass ([#28](https://github.com/VTSTech/D2VP-QoL/issues/28), 2026-11-04); the "Then 100" leg widens the same two constants. Plan: plans/SHARED-STASH-PLAN.md |
| 10 | Vanilla+ Options menu page | UI | None — owner suggestion 2026-10-29; in-game settings surface for the gated features | M | Med→Low | **Shipped** (landed 2026-10-30, ledger row 34; first release **0.0.6**); **eight rows as of R108** — the slate's five shipped keys + Shared Stash (R67: the row busts the client stash gate cache too, so the tab follows the toggle the same frame) + No Panel World Shift (R101, issue #26 round 9: the write bridge already busts the PANELSHIFT cache, so the flip applies same-frame; the seventh row moved the Previous Menu row down one pitch, 0x124→0x144) + Mana Burn Fix (R108: the R106 combat gate joins the panel — the eighth row moved the Previous Menu row down one pitch again, 0x144→0x164, and grew the box 0x171→0x191; the server gate keeps its 2 s poll by design, so a flip lands on the next drain roll within 2 s while the panel shows it same-frame) |
| 11 | Mana Burn fix (melee drain 256x bug) | Combat (D2Game) | D2R 2.4 patch notes ("fixed melee hits draining 256 times too much") + Basin wiki Mana Burn | S | Low | **Implemented** (R106 — `mana_burn_fix`; ini-only at ship, in-game **Mana Burn Fix** row on the Vanilla+ Options page since R108; server-side gate TU + VP-marked drain-roll shim in SUnitDmg.cpp; field QA + llvm-mingw gate pending) |
| 12 | Stackable gems | Inventory | None — owner suggestion 2026-11-05; PD2 stacks gems in its currency tab | M | Med | `stack_gems` |
| 13 | Stackable runes | Inventory | None — owner suggestion 2026-11-05; PD2 stacks runes in its currency tab | M | Med | `stack_runes` |
| 14 | 1344x700 resolution (retail's dormant mode 3) + 1366x768 fifth stop | Video / renderers | Retail-dormant engine mode — the binaries carry the tables, no UI stop ever shipped; D2MultiRes-era community prior art; 1366x768 is the panel-standard the field box letterboxes onto | S–M | Low–Med | **Implemented** (R109 — Gap A `case 3` + Gap B slider stop 3 + Gap C mini-panel mapping; field: Glide GREEN, D3D switch crash fixed in-round via mode-list probe + host-mode letterbox ladder; field round 2 (R111): D3D borderless-window look fixed — probe-first presentation + host-sized window; field round 3 (R112): hosted stops STRETCH the frame over the host mode + **1366x768 joins as the slider's fifth stop (mode 5)**, gates 432/432; llvm-mingw gate + D3D re-test pending (owner); plan: plans/1344x700-PLAN.md §10) |
| 15 | 1920x1080 resolution | Video / renderers | None — owner suggestion 2026-11-05; SGD2FreeRes/D2DX-class arbitrary-res prior art | M | Med | video-options slider stop 6 / mode 6 (not `[features]`-gated; slot bookkeeping after R112 — see §15) |

Rows 0a/0b are **Task C** (handoff §9 item 3) — already scoped, awaiting go; they are
the slate's head, not new scope. Rows 1–9 were the new candidates; row 6 shipped for 0.0.4, and row 1 shipped as the orb-face readout and was repurposed post-release (see §1).
Rows 12–13 are **planned scope** (owner suggestion 2026-11-05, not yet implemented): one
mechanism — letting a class of items merge into stacks the way arrows/tomes already do —
behind two separate gates, drafted together in §12/§13. Rows 14–15 are planned scope too
(owner ask 2026-11-05): 1344x700 activates a retail-dormant engine mode — the whole
site-1..6 pipeline already routes it, three gaps close it (§14, [plans/1344x700-PLAN.md](plans/1344x700-PLAN.md)) —
and 1920x1080 is one full site-1..6 round on the 720p playbook (§15).
**Shipped as of R108**: rows 1, 3, 6, 9, 10 and 11 — plus, outside this slate, the
pre-slate pair `alt_toggle`/`portable_save` (handoff ledger rows 28–31, releases
0.0.2/0.0.3) and `no_panel_shift` (born from issue #26, R95–R101). As-built docs for
everything shipped: [README.md](README.md) (feature table) + the sections here.

---

### 0b. Shift equipment-compare — `[features] shift_compare` (IMPLEMENTED R125; R126 shield-slot fix FIELD-VERIFIED 2026-11-06 — "Great job, works")

**What.** With the gate on, holding **Shift** (the key bound to retail cmd 0x24 —
`CMD_SetClickModifier`, default VK_SHIFT, a keybind remap is honored, read per frame with
retail's own `UpdateMouseModifierKeyState` idiom) while hovering an equippable item in any
panel draws a **second tooltip** — the equipped item for the matching body slot — alongside
the hovered item's tooltip. D2R hover-compare parity: side-by-side viewing only, no
stat-delta lines (that is D2R's own compare scope — diablo2.io launch-era statement in
Sources). Opt-in via `[features] shift_compare=1` + an in-game row (row 9, "Shift Item
Compare") on the Vanilla+ Options page.

**Coverage** (owner decision 2026-11-06, "shop stock included"): every context that
publishes through Inv.cpp's single `LoadItemDesc` call site — player inventory, stash,
cube, vendor shop (BOTH your own items and the vendor's stock: the stock hover takes the
shop-narrow name+price builder branch and the compare capture runs right after either
branch publishes), trade screen. Hovering the paperdoll never compares; non-equippables
(potion, charm, jewel) resolve no body slot and draw no box.

**Slot rules** (plans/SHIFT-COMPARE-PLAN.md §2, tree-verified before coding): the hovered item's
intended slot comes from its ItemTypes row (`ITEM_GetTxtItemTypes_BodyLoc1andBodyLoc2`,
bl1/bl2), never from the item instance; rings (bl1=6/bl2=7) compare against the first
occupied of {6,7} (retail's first-fit order); **arm items (R126)** — vanilla
ItemTypes.txt gives every weapon class AND the Shield row the same body pair
(body1=rarm=4, body2=larm=5), so bl order alone cannot tell the primary weapon box from
the off-hand box: when BOTH arm cells are occupied the same-kind slot wins — a
shield/quiver hover compares the off-hand (bodyloc 5), a weapon hover the primary
(bodyloc 4), the same resolution retail's own auto-equip applies to the identical
ambiguity (`ItemMode.cpp` choose_body_loc's type-conflict check; the shield detector is
`ITEMS_IsA(item, ITEMTYPE_AnyShield)`, the same one retail's Panel.cpp uses); with one
arm occupied that item compares regardless of kind (the only possible comparison);
weapons stay 2H-aware — when `INVENTORY_GetWeaponHandedness` reports 2 (a 2H occupying
both arms) the compare describes slot 4's item, the same paired-cell convention the
equipped-slot hover path uses; everything else (armor 3, helm 1, boots 9, gloves 10,
belts 8, amulet 2) is a straight bl1 lookup falling back to bl2. Empty target slot = no
box.

**Mechanics** (the item_ilvl lesson: hook, don't refactor): a VP block right after Inv.cpp
:9422's `LoadItemDesc(pSlotItem, nDrawTextWidth)` calls the core hook
`SHIFTCOMPARE_CompareForHover()` (VanillaPlusCore.cpp). The hook saves the minimal global
set (`pItemToDescribe` + `g_nCurrentInventoryPage` + the primary's published
`DisplayedTooltip`), forces the page to 0 (which takes the FULL builder at :8532 and
suppresses the vendor price line the full path appends on shop pages :8783-8787), swaps
`pItemToDescribe` to the equipped item, calls `LoadItemDesc(pPlayer, 1)` (owner = the
player), captures the result into the second channel, then restores everything — the
primary draws unchanged. Equipped set items ride LoadItemDesc's own quality-5 route into
`ITEM_BuildSetItemTooltip` (:8540-8546), so the compare box carries the set-bonus lines.
The second channel (`gVpCompareTooltip` + `TEXT_CompareTooltipSetAttributes` +
`DRAW_CompareTooltipUnderCursor`) lives in D2WinFont.cpp beside the file-local metric
statics: the draw is a VP-marked clone of `Draw_UnitNameUnderCursor` (font-downsize loop,
X/bottom clamps, translucent-black `DRAW_LESSTRANSPARENT` background, embedded 0xFF color
code handling), placed LEFT of the primary with a 4 px gap (wide enough that two adjacent
translucent boxes never visually merge), falling back RIGHT of the primary when that
leaves the screen, bottoms aligned — and it self-clears, so nothing outlives the frame.
The placement uses a capture-time snapshot of the primary's rect, so the Alt-labels path
wiping the primary between capture and draw (ShowItems.cpp, DRAW_AltItems) cannot distort
it — Alt-latched labels + Shift compare coexist.

**Files**: VanillaPlusCore.{h,cpp} (gate + resolver + compose + ini template + bust),
Inv.cpp (one VP block after the publish), D2WinFont.{h,cpp} (second channel), Draw.cpp
(one draw call), VanillaPlusOptions.cpp (row 9 + 0x184/0x1b1 layout move). No new TUs, no
input-path changes (held-Shift is inert in world space while a panel is open; click
semantics untouched).

**Status**: tree-verify pass done (every §3 anchor re-pinned at source level; the swap
set pinned to exactly { pItemToDescribe, g_nCurrentInventoryPage }; set-item question
resolved BETTER than drafted — LoadItemDesc routes equipped set items through the set
builder itself, no gap). Gates +19 `shiftcmp:` pins, 2 R108-era options shape pins
retired (superseded by the R125 pins) → 516/516; self-ID `Vanilla+0.0.7-R124` →
`-R125`. **R126 field catch** (owner, first field pass on the R125 build: "if i try to
compare a shield, it compares to my primary hand weapon, not my offhand shield"): the §2
draft's "first occupied of {bl1,bl2}" assumed shields were bl1=5 — the data says bl1=4,
same as weapons, so shield hovers hit the weapon box first; fixed with the same-kind
slot preference (Slot rules above), +1 `shiftcmp:` gate (shield-kind slot pick) →
**517/517**; release line rolls 0.0.7 → **0.0.8**, self-ID `-R125` →
`Vanilla+0.0.8-R126` (509/509 after the R127 vpdiag retirement, 510/510 with the
docs-budget gate). **FIELD-VERIFIED 2026-11-06** ("Great job, works" — the shield
slot legs ran on the owner builds). Nothing owed.

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
  [plans/SHARED-STASH-PLAN.md](plans/SHARED-STASH-PLAN.md). The design text below is the
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
  gate re-pinned (`> 3`), self-ID `Vanilla+0.0.7-R109` → 394/394.
- **Field round (owner QA, same day)**: **Glide GREEN** — the stop works end to end under
  glide3xvk 0.4.6 (owner screenshot: video menu at 1344x700, world rendering, wide-UI
  offsets correct). **D3D crashed on switching** — root cause is NOT the mode-3 code (the
  D3D size case, wide-set and dims guards all carried 1344x700 already): on Vista+ the
  exclusive DDraw `SetDisplayMode` only accepts timings from the driver's display-mode
  list; 1344x700 is a non-standard timing retail never shipped, so no driver lists it
  (1280x720 is CEA-standard and enters everywhere). The rejected mode-set failed
  `RENDERER_D3D_InitializeDevice`, and D2Win's `D2GFX_SetVideoMode` treats a failed
  resize as fatal (`"Failed to resize window... this is fatal!"` → `exit`). Glide was
  immune — the wrapper sizes its own Vulkan swapchain and never touches the display
  mode. **Fix (two layers, same candidate set)**: (1) `RENDERER_D3D_InitializeDevice`
  retries a rejected mode with the smallest standard host modes that fully contain the
  logical frame — 1366x768, then 1360x768 — while every render coordinate and the
  viewport stay 1344x700, so the larger flip chain shows as inert black letterbox bands
  (`RENDERER_D3D_ClearBackbuffer` now clears the LOCKED surface's row count, byte-identical
  for exact-fit modes); (2) `CLIENT_SetScreenResolution` probes the driver's mode list
  (32 bpp, `EnumDisplaySettingsA`) before committing stops 3/4 on DDraw/D3D and refuses
  cleanly — screen keeps the running mode and the saved slider position snaps back so
  registry, slider and screen agree — when neither the exact mode nor a host mode is
  listed, so no path can reach the fatal resize. Boot is covered by layer 1 (the game
  boots at 640x480 and only re-applies the saved stop through the probed choke point).
  Gates: 7 new `res3:` D3D/probe rows → **401/401**; D3D re-test owed (owner).
- **Field round 2 (owner QA, R110 → R111)**: R110 (the crash fix, delivered as a hotfix
  overlay — mark bumped only at R111) held: "doesn't crash". But fullscreen D3D + the
  1344x700 stop came up as a **borderless window on the unchanged desktop**. Second
  emulation behavior pinned: the Win10/11 DDraw fullscreen emulation can ACCEPT a
  `SetDisplayMode` it cannot actually set (fake success) and fall back to a borderless
  window at the requested size — R110's ladder only fired on a REJECTED call, so there
  was nothing to retry on. Two code shapes fixed in R111: (1) **probe-first** —
  `RENDERER_D3D_InitializeDevice` asks the driver's mode list (new
  `VPRES3D_IsDisplayModeListed`, twin of the client helper) BEFORE the mode-set; an
  unlisted stop goes straight to a listed host mode (1366x768 → 1360x768, same candidate
  set, now a real mode-set every time) and the doomed call is never made; (2) **the
  window covers the HOST** — R110 `MoveWindow`'d the game window to the logical
  1344x700 inside the 1366x768 host mode, which under the emulated fullscreen left the
  desktop peeking past the window's right/bottom edges (the other half of the reported
  look); hosted stops now move to the host size, true exclusive paths stay retail
  byte-identical. Same commit: the client probe gained a depth-aware base
  (`VPRES_IsDisplayModeListedAtDepth`) and a **DirectDraw exact-only branch** — the
  8-bit palettized DDRAW renderer has no letterbox mechanism, so it enters stops 3/4
  only when the driver genuinely lists the mode at 8 bpp (extinct on modern drivers →
  graceful refusal there too, closing the same crash class DDRAW was still carrying).
  Presentation ladder log line renamed (`[vanilla-plus vpres] stop WxH is not in the
  display mode list: hosting fullscreen at WxH`). Gates: retry-log pin replaced,
  counts re-pinned, 4 new rows (`d3d probe-first log`, `d3d probe helper`, `d3d window
  covers host`, `probe depth-aware helper`, `ddraw exact-8bpp arm` minus the retired
  retry log = +4) → **405/405**; syntax-check error digests identical pre/post on the
  three touched TUs (the pre-existing D2Hell.h host-shim noise only). D3D re-test owed
  (owner; expected: display switches to 1366x768, game frame top-left 1344x700 with
  black bands right/bottom, no desktop visible, mouse confined to the logical frame).
- **Field round 3 (owner QA, R111 → R112)**: the R111 fullscreen look held — "ok, i see
  the black bars" — and those bands ARE the R111 letterbox: the field box hosts 1344x700
  on its native 1366x768 panel, the frame renders top-left, the extra host rows/cols stay
  black. Owner asked for two things in one breath: "can we center and stretch it to
  1366x768 when 1344x700 is not valid. Might as well add 1366x768 as a resolution option
  as well." **(1) Hosted stops stretch**: the scene still renders into the top-left
  logical rect (viewport untouched, R111), but the PRESENT is now a stretched Blt of that
  rect onto the primary — new `D2Direct3D::Renderer::VPRES3D_PresentHostedFrame`, wired
  into `EndScene2` (the per-frame present), `CreateWindow`, `ResizeWindow` and
  `PlayCutScene` (cutscenes stretch too — Bink rides the normal frame pump); FALSE falls
  through to the retail Flip, byte-identical on every true-exclusive stop, and a Blt
  refusal on an exotic backend degrades to the R111 letterbox instead of a lost frame.
  The cursor follows: the renderer arms a D2gfx stretch channel
  (`D2GFX_SetPresentationStretch`) with the host size and `MOUSE_CaptureMousePosition`
  maps raw host pixels back into the logical frame (`x * logical / host`) BEFORE the
  retail clamp — clicks land where the stretched pixels are, the bottom 68 px stay
  reachable, the drawn cursor sits under the physical one. Trade-off, documented: hosted
  presents have no vblank sync (DDBLT_WAIT only); true-exclusive keeps Flip. **(2)
  1366x768 = mode 5, the slider's FIFTH stop**: `D2GFX` case 5 (`0x556 x 0x300`) + the
  D3D mirror, `CLIENT_SetScreenResolution` case 5, probe stop-5 dims (exact-or-host, the
  same candidate set), slider position 4 → mode 5 in both mappings (ahead of the `>= 2`
  arm), label table `L"1366x768"`, mini-panel `== 4 → mode 5` + clamp `> 4`, renderer
  branches (D3D perspective-class gate, DDraw `[6]` display tables + horizontal bound,
  GDI DIB + scale width, Glide `grSstWinOpen` enum **18** + perspective path), wide class
  (`VPRES_IsWideLayoutMode` + all seven dims pairs), and the esc-menu slot array grown
  `pDC6SubImages[4]` → `[5]` (the one retail-struct change: the fifth stop's blank
  attaches at runtime; the loader walk still caps at the four real path fields, count-5
  rows are legal, the draw-site clamp covers slot 4). Unlike dormant mode 3, 1366x768 is
  a CEA-standard timing most panels list natively — D3D usually enters it TRUE-exclusive
  (retail Flip, 1:1 cursor); the stretch ladder only fires on drivers that omit it.
  Glide note: the paired [Glide3xVK](https://github.com/VTSTech/Glide3xVK) wrapper needs
  `g3x_resolution_size` entry 18 = `{0x556, 0x300}` — wrappers older than 0.4.7 fail soft
  (640x480 surface + "unknown resolution" log line), same contract as entries 16/17; the
  entry shipped in wrapper 0.4.7 (the R114 round, below). Gates: 21
  `res5:` + 6 `stretch:` rows, `init attach` re-counted (4), minipanel gate re-pinned
  (`> 4` + mode-5 arm), self-ID `Vanilla+0.0.7-R112` → **432/432**; syntax-check: zero
  errors in the twelve touched TU bodies (the pre-existing `__time32_t`/D2Hell.h
  host-shim header noise only). llvm-mingw build gate + D3D re-test owed (owner;
  expected at 1344x700: full-screen stretched frame, edge-to-edge, no bands; at 1366x768:
  true-exclusive native mode, retail presentation).
- **R113 field round 4 (regression hotfix, no behavior change)**: R112's struct growth
  (`D2EscMenuItemStrc.pDC6SubImages [4] → [5]`, items 0x550 → 0x554 bytes) left three
  retail-shaped consumers of the old stride/count live in `EscMenu.cpp` — the mouse-Y
  row walk (`pItemYPosition += 0x154`) read rows ≥ 1 four bytes per-item early, so the
  mouse could not hit-test any menu row at ANY stop (keyboard nav kept working — it
  walks by array indexing; owner: "The mouse can no longer navigate or select options
  in the menu ... It no longer says 'Resolution' but '640x480' instead of the word
  Resolution"); the loader's sub-image slot-write counter (`nSubImageOffset += 0x154`)
  spilled every item's value-DC6 writes 4·N bytes into the previous item (the
  "640x480" value image parked in the background row's slot 4, drawn over the baked
  "Resolution" caption; later items scribbled pointer bytes into `szSubImage4Path`
  tails — the visible corruption); and `ESCMENU_FreeLoadedImages` kept the retail
  `nSubImageCount > 4 → HALT` that R112's legal count-5 row trips on every menu
  teardown. Fix: both raw walks re-pointed to `0x155` ints (the R112 0x554-byte
  stride), the free path accepts count ≤ 5. Stops/mappings/labels/stretch untouched —
  640x480/800x600 stay byte-identical retail, the wide stops behave exactly as R112
  shipped. Gates: `res5: escmenu count 5 legal` re-counted 2 → 3 (loader + free), +2
  `r113:` stride pins, self-ID `Vanilla+0.0.7-R113` → **434/434**; syntax-check error
  digest identical pre/post on the two touched TUs (pre-existing D2Hell.h host-shim
  noise only). llvm-mingw build gate + menu re-test owed (owner).
- **R114 field round 5 (Glide 1366x768 — wrapper-side, no game change)**: owner field
  matrix on the R113 build — D3D held ("ok, working good in D3D now"), but in Glide
  "when the Resolution lands on 1366x768 it only displays the top left 1/4 of the screen
  and its very zoomed in". Root cause sat in the WRAPPER, exactly the dependency R112
  flagged: the game's fifth stop sends `grSstWinOpen` enum 18, but Glide3xVK 0.4.6's
  `g3x_resolution_size` only knew 0-17 — the stop took the documented soft-fail (640x480
  surface + `unknown resolution 18` in glide3x.log) while the game laid out for
  1366x768, and the wrapper presented that surface — holding only the top-left 640x480
  of the 1366x768 layout — stretched over the full host: top-left quarter, ~2x zoom.
  D3D unaffected (mode table lives in d2vp.exe), Glide stops 3/4 fine (wrapper entries
  17/16 shipped in 0.4.6). Fix — [Glide3xVK 0.4.7](https://github.com/VTSTech/Glide3xVK)
  (patch [`patch/glide3xvk-0.4.7-entry18.patch`](patch/glide3xvk-0.4.7-entry18.patch)):
  `g3x_resolution_size` entry 18 = `{0x556, 0x300}` + unknown-resolution guard
  `> 17` → `> 18`; no other wrapper code is resolution-sensitive. Host check
  (`Glide3xVK/scripts/test_res18.cpp`): enums 7/8/16/17/18 map exact, 19/255 keep the
  soft-fail. d2vp.exe UNCHANGED — the R113 build stays current (self-ID
  `Vanilla+0.0.7-R113`, gates 434/434 untouched). Field re-test owed (owner,
  glide3x.dll 0.4.7): Glide 1366x768 = native fullscreen, full frame; then the R112
  field matrix.
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
  Plan: [plans/1344x700-PLAN.md](plans/1344x700-PLAN.md).
- **Proposed**: append a fourth slider stop mapping to mode 3. Existing registry positions
  0–2 keep their meaning (720p users unaffected); not `[features]`-gated — opt-in by
  picking the stop, same as 720p.
- **Risk**: Low–Med. Three files plus a label table; every renderer path was field-proven at
  720p. DDraw 8-bit fullscreen at 1344x700 is hardware-dependent (may be N/A on modern
  systems — record, don't block).
- **Exit**: pick the stop → 1344x700 with the 800-class UI centered (272/100); in-game
  switch 800↔1344↔720 clean; cutscene round-trip returns; save → relaunch keeps the stop;
  old registry values behave exactly as before.

### 15. 1920x1080 resolution (Implemented — R115, owner ask 2026-11-05 "Now lets do 1080p")

- **No retail parity**: 640x480/800x600 are retail's stops and 1344x700 is retail's dormant
  one — 1080p is pure owner scope, in the SGD2FreeRes/D2DX class of arbitrary-resolution
  play.
- **Why it rode the 720p playbook**: the same site-1..6 checklist per new mode —
  `CLIENT_SetScreenResolution` + D2GFX table cases, D3D size case + wide-set + dims guards,
  DDraw display tables + horizontal bounds, GDI windowed branches, Glide wrapper
  `g3x_resolution_size` entry + client mapping, `VPRES_IsWideLayoutMode`, slider stop.
  **Slot bookkeeping (as consumed)**: 1366x768 took mode 5 / slider stop 5 / wrapper
  entry 18 / the `[5]` sub-image slot (R112); 1080p took **mode 6 / slider stop 6 / wrapper
  entry 19 / `pDC6SubImages[6]`** / DDraw `[7]` tables. The wide-UI offsets are formulas
  (X 560 / Y 480). The R112 stretch ladder gives 1080p the same D3D safety net: an
  unlistable stop hosts (and downscales) over the 1366x768/1360x768 hosts instead of
  crashing.
- **R115 as-built (the sixth stop)**: `D2GFX` case 6 + D3D mirror (`0x780 x 0x438`),
  `CLIENT_SetScreenResolution` case 6 + probe stop-6 dims + refusal snap-back mode 6 →
  position 5, slider position 5 → mode 6 in both EscMenu mappings + `L"1920x1080"` label +
  FOURTH runtime blank (count 6), mini-panel `== 5 → mode 6` + clamp `> 5`, D3D
  perspective-class gate `!= 6`, DDraw `[7]` tables + horizontal bound `0x780`, GDI DIB +
  scale width, Glide enum **19** + perspective path, wide class in all eight dims pairs +
  both client classifiers. The struct growth repeated R112's — `pDC6SubImages [5] → [6]`
  (0x554 → 0x558 bytes) — with the R113 lesson applied IN THE SAME COMMIT: both raw
  strides `0x155 → 0x156`, loader count clamp `> 6` (walk cap stays 4 real path fields),
  free-path halt clamp `> 6`, draw-site clamp `> 5`. Wrapper side shipped in-round:
  Glide3xVK **0.4.8** `g3x_resolution_size` entry 19 = `{0x780, 0x438}` + guard `> 19`
  (patch verified to apply on the published 0.4.7; host check 16-19 exact, 20/255
  soft-fail). Gates: +21 `res6:` rows + re-pins, self-ID `Vanilla+0.0.7-R115` → **454/454**.
- **R116 field catch (windowed 1366x768 shear, fixed in-round)**: owner on the R114-era
  build — "in D3D windowed mode 1366x768 everything is distorted and slanted". `-w` never
  reaches the D3D renderer (`Game/Main.cpp` maps `bWINDOW → RENDERER_Windowed` ahead of
  any `bD3D` check), so windowed has always been the GDI DIB path — and that renderer had
  been riding the logical width as scanline pitch since retail: GDI DWORD-aligns every
  8bpp row, so the real stride is `(width + 3) & ~3` — 1368 at the 1366-wide stop, ==
  width at every other stop, which is why the defect hid through R112–R115 (windowed was
  only field-tested fullscreen). The width-as-pitch slip drifted the Y-buffer row table,
  the D2CMP `nDstPitch` slots and every raw address 2px/row (bottom row 1,534px off).
  Fix: `RENDERER_WINDOWED_nDIBPitch` (one global, computed at DIB alloc, used at all 24
  raw-buffer sites in `D2Gdi/Renderer/Windowed.cpp`) — byte-identical at every %4 stop.
  The DirectDraw windowed renderer already passed its real `lPitch`; this catches the GDI
  path up to the same contract. Gates +4 `r116:` pins → **458/458**.
- **R117 field catch (edge-void fill, fixed in-round)**: owner at the new 1920x1080 stop
  (PERSPECTIVE on) — "there's some blackness at the very edge. Is there any way to fix
  that?". Pixel-measured the field screenshot first: **0px solid-black band on every
  screen edge** — not a present/letterbox artifact. It is the D2 **level-boundary void**:
  `DrawFloorByViewAndFloor` (D2Client/GAME/Draw.cpp) draws only the current room +
  adjacent rooms, and retail's void skirt (`DrawSubTile` → `DRAW_DrawEdgeFloorTile`
  stamping the act edge tile `&pDrlgAct->sTileCache` past the drawn bounds) is ONE
  3-tile strip per open side on the player row, behind 30/33 + 25/23 subtile triggers —
  sized for 800x600 (~25-subtile reach) while a 1080p view reaches
  (1920/16 + 1080/8)/2 ≈ **127 subtiles** into the void. (Decompile trap worth the
  record: the `bIsPerspective` term in the skirt's enable gate is the **PANEL border
  type** — the var is reassigned to `PANELSHIFT_BorderForWorld` at line 728 — so the
  skirt already ran in perspective; it was simply far too small.) R117 replaces the
  strip with a **band fill scaled from the live backbuffer**: reach =
  `W/32 + H/16 + 10` subtiles (screen half-reach in world coords + parity margin),
  depth = `reach/5 + 1` tiles stamped beyond the open bound, span = `reach*2/5 + 1`
  tiles each way along it; four open-side bands (left/right ride anchorY, top/bottom
  ride anchorX); bounds/anchors captured to locals BEFORE the first stamp
  (`DrawFloorTile` → `DRAW_UpdateTileVisibilityBounds` grows the bounds mid-loop with
  the fill's own tiles); off-screen stamps cost one `CoordsMapToScreen` + the
  `rcExtendedView` cull and nothing else, so per-frame cost is bounded by the visible
  fill area (worst host-measured case: 12,432 stamps at 1080p on a all-sides-open
  sliver, zero visible-tile waste). Gates unchanged: `DrawEdges` levels only, VPRES
  wide stops only (800x600 keeps retail byte-identical), border-0 world view, ortho +
  perspective. Host geometric proof (`scripts/test_r117_fill.cpp`, g++ -Wall -Wextra
  clean): **100% of the on-screen void diamond covered at every wide stop at any
  camera parity; interior frames stamp 0**. Gates +4 `r117:` pins + self-id roll →
  **462/462**, self-ID `Vanilla+0.0.7-R116` → `-R117`.
- **R118 field catch (the every-mode black rim + the R117 harness boot break, fixed in-round)**: owner after deleting the R117 harness and rebuilding — "runs now. The blackness at the edges is still there. and is present in all video modes 720p and up. 1080p sees way beyond 720p's border and yet when i switch to 720p it has a blackness at the edges. This seems like some kind of render of view limit, can we extend it?"; on the boot break: "ok that hypothesis was correct the test .cpp in /scripts/ broke the entire build." **The boot break**: the root CMakeLists globs `*.cpp` tree-wide, so R117's host harness (`scripts/test_r117_fill.cpp`, own `main()` + stub globals) compiled INTO d2.exe (GUI subsystem → the stray `main` links clean) and the build died after `static-init ok`, before `WinMain`, in every renderer. Fenced: `vanilla-plus/cmake/sources.cmake` now literal-prefix-excludes `scripts/` — nothing parked there can reach the game link again. **The blackness (owner was right — a render-of-view limit)**: ruled out the light grid first (`LIGHTMAP_InitFromPlayerAndRooms` pre-fills the whole 48×48 ambient grid with the player room's ambient — cells past the rooms are lit, not black) and R117's fill (stamps `DrawEdges` boundaries only). The real limit: **both render walks stop one hop out** — the floor pass loops exactly `DRLGROOM_GetAdjacentRoomsList` (3×3 room block) and `WALL2_BuildSceneDrawList` walks the same set via `CallRoomCallback` (a literal `for` over `pRoom->ppRoomList`) — floors, walls, roofs, units, shadows all stop at the block; past it the cleared backbuffer is the black rim. 800x600's ~25-subtile reach always fit inside the block (±60–100 subtiles), which is why retail never needed more; the wide stops reach 85 (720p) to ~127 (1080p) subtiles — rim at 720p, half the screen void at 1080p. The rooms exist: `InitNearRooms` materializes the RoomEx graph three hops out (hop-2 rooms get `DRLGROOMEX_ActivateRoomEx`). Fix: new `WALL2_CollectRoomsBeyondAdjacency` (Wall2.cpp/.h, shared) — BFS depth 3, hop ≤ 1 skipped, world-square culled at `W/32+H/16+90` subtiles, 96-room cap; floors draw the hop-2/3 rooms through `DRAW_DrawWideViewRoomFloors` (the retail two-pass shape verbatim); walls/roofs/units/shadows ride the existing `WALL2_CollectRoomTilesAndUnits` (its `rcCull`/grid/entry-cap culls bound the cost). Extra visible tiles also grow R117's visibility bounds, so its fill now keys on real level boundaries instead of the block edge. Wide stops only — 640x480/800x600 byte-identical. Gates +6 `r118:` pins + self-id `Vanilla+0.0.7-R117` → `-R118`. Field matrix owed (owner): no black rim at any wide stop in normal play; edge fill only at true map boundaries; FPS watch at 1080p in dense areas; a far-corner void at 1080p would be the DRLG's 3-hop materialization depth — the next knob.
- **R119 field catch (the R118 half-fix — reach in the wrong coordinate space)**: owner — "ok that half fixed it, i don't see any blackness on the bottom or sides, but there is still blackness on the top edge and top corners" (perspective) and "without perspective mode it's the opposite issue, rolling black triangle at bottom" (ortho). ONE bug, two faces: DRLG room coords / draw bounds / `UNIT_CenterXCoordsDiv5 * 5` are game coords (5 per subtile — retail compares the identical expression against `dwXStart`), but R118's reach (`ScreenSizeX/32 + ScreenSizeY/16 + 10 + 80`) was subtile-scaled: the walk's prune square covered 43 of the 127+ subtiles a 1080p view reaches, so hop-2/3 rooms only patched a ~700px band past the retail 3x3 block (bottom/sides = the block's own big-room coverage — "half fixed"; perspective's warp-loaded top edge/corners black), and R117's four fill open-bound triggers compared coord-space deltas against the same subtile reach — under-fired with bounds 43–127 subtiles out (the rolling ortho bottom triangle over small rooms). Reach now in game coords at both call sites (`W*5/32 + H*5/16 + 450` = 1087 coords at 1080p), fill triggers vs `nFillReachCoords = nFillReach * 5` (stamp depth/span stay subtile-derived — dividing by 5 was always correct), BFS queue 128→512, both out caps 96→256, collector params renamed to the coord space (`nCenterX/Y`, `nReach`). Gates +6 `r119:` pins + self-id roll → **475/475**.
- **R120 (wide-view diagnostics — vpdiag -> d2boot.log, zero behavior change)**: owner after R119 — "same behavior, blackness on top with perspective, blackness on bottom without" and "Is there anything we could add to d2boot.log to help diagnose the issue?". Four candidate mechanisms, two field modes, one visible symptom — so instrument all four and print numbers: every 32nd floor frame (~2x/second) plus a forced dump on every perspective flip, the floor pass writes three `vpdiag` lines carrying (1) mode/perspective/backbuffer/the retail `rcExtendedView` cull rect/lightmap origin, (2) the fill-trigger bounds + the R118/R119 BFS walk's real coverage (emitted/hop mix/room-union AABB/both silent-drop counters) for floor and scene passes, (3) tile seen/drawn/culled counts, culled-anchor screen extremes, the drawn-floor px AABB vs the backbuffer as per-edge gaps, fill stamps/fired bits/anchor, and the ambient-lightmap 48x48 clamp rate. Read map: gaps>0 + short drawn AABB = world absent on that edge; rc large + culled extremes past rcv = retail cull rect eating drawn world; qf/of>0 = collector drops; fire=0000 with gaps>0 = fill never fired; lm misses high = drawn-but-dark clamp class. Counters shared via Draw.h (`VPDIAG_WalkStats`), collectors increment-only (Wall2.cpp, View.cpp), Draw.cpp owns reset+dump. Gates +14 `r120:` pins, self-ID `-R119` → `-R120` → **489/489**; owner d2boot.log capture owed (both modes + one perspective toggle), fix round = R121.
- **R121 (the R120 log verdicts — fill un-gated, hop-4 walk, snapshot triggers)**: the owner's R120 vpdiag capture read four verdicts. (1) `edge=0` in both modes with `persp=1` — `pLevelTxt->DrawEdges==0` on A1 outdoor and the retail flag gated the whole R117 fill+bounds block (all dumps phase 0, `fire=0000`, `st=0`, `wb` sentinels): the band fill was dead code in the exact test area, and the R118 "half fix" was the wide walk alone. Fix: at wide stops the bounds grow and the fill runs keyed on `VPRES_IsWideLayoutMode` alone; the flag and retail modes stay byte-identical. (2) `gaps=0/52..86/0/0` — the ortho bottom void measured, every frame. (3) The depth-3 walk union stopped 30-70 coords short of the 1080p bottom screen corner — the corner room is the hop-4 diagonal; both call sites now walk depth 4 (unmaterialized hop-4 yields nothing). (4) The 74% lightmap misses are benign — the grid pre-fills with the player room's ambient, so out-of-window samples equal retail. Bonus: the four fill triggers now read bounds snapshotted before the first stamp (a band's own stamps grow the live members mid-fill — earlier-band y-growth could push a later trigger past the reach and skip it). Gates +6 `r121:`, re-pins to depth 4 and the widened classifier counts, self-ID `-R121` → **495/495**; field: phase-1 vpdiag lines with real `wb`/`fire`/`st` and the bottom gap at/near 0.
- **R122 (the R121-WIP field verdict — `rcExtendedView` re-fit to the backbuffer)**: the owner's R121-build capture (1080p, both modes, one flip) proved every R121 fix in the field — `vpdiag[1]` with `edge=0` (un-gate live), `fire=1248`/`st=12432` (all four bands firing, ~12k stamps/frame), `f:n=22-29 qf=0 of=0` (hop-4 rooms emitting, no drops) — and still `gaps=0/51..82/0/0`. The discriminator: the drawn-floor px AABB hugs the cull rect on every edge (persp `px=[-119 2024 -109 1024]` inside `rcv=[-125 -110 2030 1033]`; ortho `px=[-64 1981 -80 1000]` inside `rcv=[-80 -80 2000 1033]`) while culled-anchor extremes run to `cx=[-6784 8656]` — the rect, not the walk, is the binding cull. Root: `SetViewRectangleSize` builds the rect as `H - 0x2f` at the bottom — 47px ABOVE the backbuffer, the 800x600 panel-era wedge — so `DrawFloorTile` and the wall2 scene cull kill every tile anchored past it, fill stamps included. Fix: one wide-classifier block in `SetViewRectangleSize` re-fits the rect to `±0xc0` around the live backbuffer (covers retail's widest margin, persp left −0x7d, with warp slack; the scene pass shares the rect so bottom walls un-cull too). Retail modes byte-identical. Gates +4 `r122:` pins + summit-clouds classifier count 2→3, self-ID `-R122` → **499/499**; field: next log's `rcv=` should read `[-192 -192 2112 1272]` at 1080p in both modes and `gaps=` should collapse to `0/0/0/0`.
- **Deferred to its own round (kept from the plan)**: consolidating the copy-pasted
  dims-pair tests (`(1280,720) || (1344,700) || (1366,768) || (1920,1080)` across Glide.cpp
  ×2, Direct3D.cpp ×2, DirectDraw.cpp ×1, Windowed.cpp ×1, VanillaPlusCore.cpp ×1 — now
  EIGHT copies) into one predicate, so per-mode cost stops growing. R115 followed the
  copy-paste shape one more time for a surgical diff; the consolidation round is now
  genuinely owed.
- **Open verify items (field matrix owed)**: D3D texture-cache sizing
  (`gnD3DTextureCacheBlockSize/Count`, floor/subtile caches — field-proven at 720p, unknown
  whether they scale from screen dims at 1080p); DDraw 8-bit fullscreen 1920x1080 (expect
  N/A by hardware — the probe refuses gracefully); front-end menus; CPU-side tile dispatch
  at ~2.25× the 720p pixel count.
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
  (docs/resolution-widelayout-audit.md) + the mode-3 recon in plans/1344x700-PLAN.md §3.
