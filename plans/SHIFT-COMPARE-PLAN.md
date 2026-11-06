# SHIFT-COMPARE — equipment-compare plan (slate row 0b)

*Drafted 2026-11-06 on owner ask ("Lets draft a plan for shift_compare"). Slate row 0b in
[FEATURES.md](FEATURES.md) — "Shift equipment-compare | Controls | D2R hover-compare | M | Med |
gate `shift_compare`" — one of the two Task-C rows already scoped and awaiting go. D2R parity
grounding: launch-era comparison tooltips are deliberately basic — "no stat comparisons, just
the ability to view the other item alongside" (diablo2.io, Sep 2021, linked in Sources); the
Shift-hold trigger is this fork's shape (D2R shows the pair on hover; here the pair only
appears while Shift is held, so the default view stays retail). Status: **IMPLEMENTED (R125, 2026-11-06) — the §9 tree-verify pass ran first (every §3 anchor re-pinned; the swap set pinned to exactly { pItemToDescribe, g_nCurrentInventoryPage }; §9 item 5 resolved better than drafted — LoadItemDesc already routes equipped set items through ITEM_BuildSetItemTooltip :8540-8546, no gap), then the §5 round landed: gate + resolver + compose (VanillaPlusCore), the Inv.cpp :9422 hook, the D2WinFont second channel, the Draw.cpp call, options row 9 + ini template + bust line, the shiftcmp: gate cluster (+19, 2 R108-era shape pins retired) → 516/516, self-ID `Vanilla+0.0.7-R125`. §10 asks ANSWERED by the owner (2026-11-06): side-by-side, hold, label "Shift Item Compare", **shop stock included** (a delta from this draft's v1 — see the §2/§5/§8 amendments below), scope = one round with the row. **R126 FIELD CATCH (owner, first field pass on the R125 build: "if i try to compare a shield, it compares to my primary hand weapon, not my offhand shield"):** this plan's §2 slot map was wrong about shields — vanilla ItemTypes.txt gives weapons AND the Shield row the SAME body pair (body1=rarm=4, body2=larm=5), so "first occupied of {bl1,bl2}" hit the primary weapon box first; fixed same-round with the same-kind slot preference (both arm cells occupied → shield/quiver hovers try the off-hand bodyloc 5 first, weapon hovers the primary 4 — the resolution retail's own auto-equip applies to the identical ambiguity, ItemMode.cpp choose_body_loc type-conflict check; detector ITEMS_IsA ITEMTYPE_AnyShield, retail Panel.cpp:1252's own). self-ID `Vanilla+0.0.8-R126` (release line rolls 0.0.7 → 0.0.8), gates 517/517 (509/509 after the R127 vpdiag retirement, +1 docs-budget gate = 510/510). **CLOSED — FIELD-VERIFIED 2026-11-06**: the owner confirmed the R126 fix in the field ("Great job, works") — the §8 matrix legs ran on the owner builds (shield→shield, weapon→weapon, quiver→quiver); R127 retired the vpdiag probes that shipped beside the wide-view rounds (zero behavior change). Nothing owed.*

---

## 1. Goal and non-goals (v1)

**Goal.** With the gate on, holding **Shift** while hovering an equippable item in any
panel draws a **second tooltip** — the currently equipped item for the matching body slot —
alongside the hovered item's tooltip. D2R hover-compare parity: view the other item
alongside, nothing fancier. Opt-in via `[features] shift_compare=1` + an in-game row
(row 9) on the Vanilla+ Options page.

**Non-goals v1 (each a deferred round only if the field asks):**

- No stat-delta lines (green/red diffs) — D2R's own compare is side-by-side viewing only.
- No compare while hovering the paperdoll — those items are already equipped.
- No mercenary-inventory compare, no ground-item compare (ground items ride the name-only
  `DRAW_UnitUnderMouse` path, not the panel tooltip pipeline).
- No weapon-set-2 (bodylocs 11/12) compare targets — the parked set, stats detached.
- No always-on mode and no latch mode (one full Alt-style press to pin) — v1 is hold-to-show.
- No input-path changes: the click-modifier semantics of Shift (stand still / force attack)
  are untouched; while a panel is open, world clicks don't fire anyway.

## 2. Behavior spec

- **Trigger.** Shift held at tooltip-draw time, read the way retail reads it:
  `GetAsyncKeyState(CMD_GetKeyForCommand(0x24, ...))` — the key bound to retail cmd 0x24
  (`CMD_SetClickModifier`, default `0x10` = VK_SHIFT), so a user keybind remap is honored.
  Per-frame poll in the compare path, exactly the `UpdateMouseModifierKeyState` idiom
  (`Game.cpp:1193-1221` ? `gClickModifierDown`); no CmdTbl entry, no latch, no keyup hook.
- **Placement.** The equipped item's tooltip draws **left of the primary tooltip** with a
  4 px gap; when that would leave the screen it falls back **right of the primary**; the
  same X/bottom clamps `Draw_UnitNameUnderCursor` applies apply to it. Translucent-black
  background (`D2GFX_DrawSolidRectEx`, `DRAW_LESSTRANSPARENT`) — the same look as every
  item tooltip, with the ?4 px gap so two adjacent translucent boxes never visually merge.
- **Slot mapping (hovered item ? compare target).** The hovered item's intended slot comes
  from its ItemTypes row — `ITEM_GetTxtItemTypes_BodyLoc1andBodyLoc2` (bl1, bl2) — never
  from the item instance (`GetBodyLocation` is only set once equipped):
  - bl1 first; if that bodyloc is empty, bl2; both empty ? no compare box.
  - **Rings** (bl1=6, bl2=7): compare against the first occupied of {6, 7} — deterministic,
    matches retail's first-fit auto-equip order (`ITEMMODE_FindBestBodyLocForItem`).
  - **Weapons** (bl1/bl2 ? {4, 5}): first occupied of {bl1, bl2}; if the equipped item at
    that slot has `INVENTORY_GetWeaponHandedness(...) == 2` (a 2H occupying both arms),
    describe slot 4's item — the same "2H describes from the paired cell" convention the
    equipped-slot hover path already uses (`INV_UpdateBodySlotUnderCursor:3440-3475`).
  - Shields/ammo resolve to 5, armor 3, helm 1, boots 9, gloves 10, belts 8, amulet 2 —
    all straight bl1 lookups; per-class data values get confirmed in the tree-verify pass.
- **Panels in scope.** Player inventory, stash, cube, vendor shop, trade — one hook covers
  all of them because they all render through `DRAW_UI_Inventory` ? `LoadItemDesc`
  (`g_nCurrentInventoryPage` 0 / 4 / 3 / 1-8 / 0xB). The inventory panel is open in every
  one of these contexts (D2 opens it alongside), so the hover machinery and paperdoll rect
  table always exist. **R125 owner amendment (2026-11-06, "shop stock included"): hovering vendor *stock* — the
  shop-narrow name+price branch (:8828-8899) — compares TOO: both branches publish through
  the same TEXT_TooltipSetAttributes tail, the capture runs right after the single :9422
  call site, and the compose forces `g_nCurrentInventoryPage = 0` (full builder, no price
  line on the equipped item).**
- **Negative behavior (the feature contract).** Gate off / missing key / garbage value =
  retail byte-for-byte at every touched site; Shift up = exactly today's single tooltip;
  gate flips apply within the standard 2 s ini hot-read; hovering an equipped paperdoll
  slot produces no compare box.

## 3. Recon anchors (what the tree already gives us)

*Source-level recon 2026-11-06; the tree-verify pass (§9) re-pins every anchor before coding.
Line numbers can drift ±2.*

**The tooltip pipeline (D2Client/UI/Inv.cpp + D2Win/Src/D2WinFont.cpp):**

- The three tooltip builders the shipped `item_ilvl` feature hooks are the same surface this
  feature rides: `ITEM_BuildSetItemTooltip` (`Inv.cpp:7790-8170`), `LoadItemDesc` full path
  (`:8252-8824`) and the shop-narrow else-branch (`:8828-8899`). The ilvl suffix call sites
  sit at `:8071` / `:8736` / `:8830`; ilvl rides along automatically for the compare tooltip.
- The described item is the **file-scope global `pItemToDescribe`** (`Inv.cpp:226`), not a
  parameter — both builders take the *owner* unit (`pUnit`) and gate on hover flags
  (`:8514`) + inventory membership (`:8527`, `INVENTORY_CheckDoItemBelongToInventory`).
  Equipped items pass membership: body-grid items live in `pUnit->pInventory`, which is why
  hovering the paperdoll renders them today.
- Composition is stack-local end to end (`szItemDescription` / `wszFinalTooltip`) until the
  single publish call `TEXT_TooltipSetAttributes` (`:8896-8901`, `:8165-8169`) — which
  writes the **one global** `DisplayedTooltip` (`D2WinFont.cpp:74`; struct `D2WinFont.h:39-46`:
  `wszString[1024]` + nX/nY/nColor/nCenterFlag/nLoadProgress). Frame order: cleared every
  frame (`Draw.cpp:532`) ? panel draws set it ? `Draw_UnitNameUnderCursor`
  (`D2WinFont.cpp:1385-1476`) draws it from `Draw.cpp:739`. One tooltip per frame by design.
- Draw primitives are reusable as-is: width/height via `TEXT_CalcTextDimensions`
  (`D2WinFont.cpp:859`) and the file-local `TEXT_CalculateTextWidth/Height`; background
  `D2GFX_DrawSolidRectEx` (`D2GFX.cpp:1012`); text `D2WINFONT_DrawWideString`
  (`D2WinFont.cpp:385`, handles embedded 0xFF color codes); font save/restore via
  `SetAndReturnLastFont` when a tall tooltip downsizes.
- **A second one-shot tooltip channel already exists in the binary**: `UI_DrawStoredTooltip`
  (`ui.cpp:3872`, drawn at `Draw.cpp:741` right after the primary) — own buffer, own
  screen coords, self-clearing. The frame pipeline already runs two tooltip draws back to
  back; this feature formalizes the second one behind a gate.
- Alt-labels interference to coexist with: the Alt-labels path wipes the primary channel
  (`ShowItems.cpp:309/413/420`, `TEXT_TooltipSetAttributes(nullptr,0,0,0,0)`). The compare
  capture happens immediately after the primary publish, so later wipes don't matter —
  coexistence gets a smoke item (§8).
- Precedent for injected tooltip text inside `LoadItemDesc`: the weapon-swap hint block
  (`:8479-8508`, string 0x58c6).

**Equipped-slot access (D2Common):**

- Equipped gear is a 13-slot pointer table indexed by `eD2BodyLoc`:
  `D2InventoryGridStrc` (`D2Common/INVENTORY/Inventory.h:53-67`; 1 head, 2 neck, 3 torso,
  4/5 arms, 6/7 rings, 8 belt, 9 feet, 10 gloves, 11/12 swap arms).
- One call fetches any slot: `INVENTORY_GetItemByGridOffset(pInventory, slot)`
  (`Inventory.cpp:1397-1418`, NULL = empty). This is what retail's paperdoll draw
  (`InitInventory`, `Inv.cpp:1573-1580`), stat window and weapon swap all use.
- `eD2BodyLoc` enum (`d2_enums.h:2753-2768`); storage class on the item:
  `ITEM_GetItemData_eD2InventoryGridType` (`Inventory.cpp:4727-4738`; 3 = equipped,
  4 = swap-equipped) — the cheap "is what I'm hovering already equipped?" check if ever
  needed.

**Trigger precedent:**

- Retail already polls Shift per frame for the click modifier:
  `UpdateMouseModifierKeyState` (`D2Client/GAME/Game/Game.cpp:1193-1221`),
  `GetAsyncKeyState(CMD_GetKeyForCommand(0x24, ...))` ? static `gClickModifierDown`
  (`:259`), consumed by the click-flag paths. cmd 0x24 defaults to VK_SHIFT
  (`CmdTbl.cpp:138`). Held-Shift compare reuses this exact idiom.
- Contrast: `alt_toggle` never polls — it rides the CmdTbl dispatch (keydown/keyup handlers,
  `D2Client.cpp:774-785`). That is the latch pattern; v1 deliberately does NOT use it.

**Gate + options-page surface (D2Client/UI/VanillaPlusCore.cpp, VanillaPlusOptions.cpp):**

- Gate shape to clone: `ITEMILVL_Active` (`VanillaPlusCore.cpp:497-507`) — statics + 2 s
  GetTickCount + `D2VP_GetFeatureToggle("shift_compare")` (`:625-633`).
- Cache-bust registry for the Options-row bridge: `D2VP_ConfigReloaded`
  (`VanillaPlusCore.cpp:652-660`) — one new `dw…LastCheck = 0;` line.
- ini template: documented block in `ALTTOGGLE_WriteIniTemplate`
  (`VanillaPlusCore.cpp:179-259`; mana_burn block at `:244-251` is the format model).
- Options page row 9 (all in `VanillaPlusOptions.cpp`, values current as of R108):
  `gaVanillaPlusRows[8] ? [9]` + entry (`:150-172`); `VPP_ROW_COUNT` 8?9 (`:173`);
  `VPP_ROW_BACK` 8?9 (`:174`, drives hit-test + draw special-case); `VPPAGE_BACK_ROW_Y`
  0x164 ? **0x184** (`:138`); `VPPAGE_BOX_HEIGHT` 0x191 ? **0x1b1** (`:134`); rewrite the
  layout-math comment (`:175-184`). 640x480 still fits (box bottom ?456 < 480). No new UI
  flag, no esc-menu table edit (that's the separate 6-row table, `EscMenu.cpp:4231-4262`).

## 4. Design decisions (recommendations, owner-overridable)

| # | Decision | Recommendation | Why | Alternative |
|---|---|---|---|---|
| 1 | Presentation | **Second tooltip box** (side-by-side, D2R look) | True parity; the single `DisplayedTooltip` global is one-per-frame by design, but the binary already runs a second channel (`UI_DrawStoredTooltip`) right after the primary | Append the equipped item's stats into the same box under a separator — cheaper, but not D2R's shape and makes very tall tooltips |
| 2 | Composing the equipped item's text | **Save ? swap `pItemToDescribe` ? call `LoadItemDesc(pPlayer, 1)` ? capture ? restore** | Zero-footprint on an upstream-owned 650-line function (merge hygiene, the item_ilvl lesson: hook, don't refactor); equipped items pass the membership gate; the ilvl suffix rides along | Tail-refactor `LoadItemDesc` into a `(pItem, pOwner, wszOut)` compose split — cleaner long-term, large upstream conflict surface; revisit only if the swap proves fragile |
| 3 | Trigger | **Hold Shift** | Matches the slate name; zero input-path/CmdTbl changes; panel-open contexts make held-Shift inert in world space | Latch on full press (alt_toggle shape — round-2 candidate); always-show (D2R's actual trigger — clutters 640x480) |
| 4 | Which key | **The key bound to cmd 0x24** (default VK_SHIFT), via `CMD_GetKeyForCommand` | Honors keybind remaps, identical to retail's own Shift read | Raw `GetKeyState(0x10)` (belt-hotkey precedent) — simpler, ignores remaps |
| 5 | Options row | **Ship row 9 in the same round** | The R101/R108 row-growth pattern is mechanical and re-pinned by gates; the exact touch list is six constants + one table entry | ini-only first (mana_burn R106 precedent), row later |

## 5. Implementation sketch (one code round, ~5 TUs, all VP-marked)

1. **`D2Client/UI/VanillaPlusCore.cpp` / `.h`** — the feature logic:
   - `SHIFTCOMPARE_Active()` — the `ITEMILVL_Active` shape, key `"shift_compare"`.
   - `SHIFTCOMPARE_ResolveSlot(pItem)` ? bodyloc — ItemTypes bl1/bl2 + the §2 rules
     (rings first-occupied, 2H-aware weapon pairing).
   - `SHIFTCOMPARE_CompareForHover()` — the compose round trip: save the minimal global
     set (`pItemToDescribe`, `gpInvItemUnderCursor` if the :8514 gate reads it, plus the
     `DisplayedTooltip` struct itself) ? swap in the equipped item ?
     `LoadItemDesc(pPlayer, 1)` ? copy `DisplayedTooltip` into the compare channel ?
     restore everything. §9 pins the exact gate reads before this is written.
   - The bust line in `D2VP_ConfigReloaded`, the template block, the `.h` declarations.
2. **`D2Client/UI/Inv.cpp`** — one VP block in `DRAW_UI_Inventory` immediately after the
   `LoadItemDesc` call (`~:9422`): if gate on + Shift down + a grid/stash/cube/shop/trade
   item is hovered (`gbInvBodySlotUnderCursorExists == 0`, not the shop-narrow branch) +
   an equipped item exists for the resolved slot ? compose. Hook placement is the context
   filter: this code only runs when the primary publish just came from an item hover in
   this panel family. R125 amendment (owner: "shop stock included"): the narrow-branch
   exclusion is dropped — the hook fires after BOTH builder shapes, and the compose forces
   the page to 0 so the compare box is the full equipped-item tooltip in every context.
3. **`D2Win/Src/D2WinFont.cpp`** — the second channel, living here so it reuses the
   file-local metric statics: a `gVpCompareTooltip` struct (same fields minus
   `nLoadProgress`, hard-zero so the progress-bar branch can never fire), a
   `TEXT_CompareTooltipSetAttributes` publish setter, and `DRAW_CompareTooltipUnderCursor`
   — a VP-marked clone of `Draw_UnitNameUnderCursor`'s ~90 lines (measure, clamps, bg
   rect, wide-string draw, font save/restore) plus the left-of placement rule computed
   from the primary's own rect (recomputed from the still-populated `DisplayedTooltip`).
4. **`D2Client/UI/Draw.cpp`** — one VP line after the primary draw call (`~:739`):
   `DRAW_CompareTooltipUnderCursor();` — slots into the existing pipeline order
   (primary tooltip `:739` ? stored tooltip `:741`).
5. **`D2Client/UI/VanillaPlusOptions.cpp`** — row 9 per §3's touch list; label
   **"Shift Item Compare"** (wording = owner ask, §10).

Equipped **set items**: `LoadItemDesc` may not carry the green set-bonus lines
(`ITEM_BuildSetItemTooltip` is a separate builder) — the tree-verify pass decides between
accepting the gap (v1 note in the tooltip) or routing equipped set items through the set
builder instead. Pre-pinned as a §9 item, not left to discovery mid-round.

## 6. Rounds

- **Round 1 (self-ID ? R125)** — everything in §5: gate + resolve + compose + second
  channel + row 9 + ini template + pin cluster + docs. Exit: gates green at the new total,
  negative checks byte-identical, field matrix (§8) owner-verified.
- **Round 2 (deferred, only on field ask)** — latch mode; both-rings refinement (compare
  against the less-similar ring); merc-inventory compare; always-on sub-key;
  stat-delta presentation if the owner ever wants to exceed D2R parity.

## 7. Gates + docs touch list (per the feature contract)

- **`tools/check-vanilla-plus.sh`** — new `shiftcmp:` pin cluster (~11 gates, the
  mana_burn-cluster shape `:1349-1368`): gate accessor, compose call site, draw call,
  channel struct, template key, options-row entry, the re-pinned page constants
  (`VPP_ROW_COUNT = 9`, `VPPAGE_BACK_ROW_Y = 0x184`), marker counts. 499 ? ~510-512; run
  to confirm the reported total.
- **self-ID roll** — `Vanilla+0.0.7-R124` ? `-R125` (`Game/Main.cpp` + gate re-pin).
- **`SMOKE-CHECKLIST.md`** — new §C item (the `12b` item_ilvl format) + one sentence in
  the §B round-trip paragraph for the options row.
- **`handoff.md`** — line-1 NEWEST row + §2 feature-ledger row.
- **`FEATURES.md`** — row 0b status + a dedicated §0b section (this plan gets linked from
  the row now, at draft status).
- **`VANILLA-PLUS.md`** — Features bullet (the surface goes eight ? nine deep; the "eight"
  counts in Status get touched too).
- **`README.md`** — feature-table row. **`CHANGELOG.md`** — the R125 entry.
- **ini template** — documented `shift_compare=0` block in `ALTTOGGLE_WriteIniTemplate`.

## 8. Field QA matrix (owner, after Round 1)

- Compare shows in: player inventory, stash, cube, vendor shop (hovering own inventory AND the shop
  stock — R125 owner decision: stock hover compares too), trade screen. Glide **and** D3D, at
  800x600 and a wide stop (720p/1080p).
- Slot cases: helm, armor, belt, boots, gloves, amulet; ring with one equipped and with
  both equipped (deterministic pick, §2); 1H weapon + shield; 2H weapon (compare target
  describes the 2H); hovering a potion/non-equippable = no box.
- Negatives: gate off ? tooltips exactly retail; Shift up ? exactly today; empty target
  slot ? no box; hovering the paperdoll ? no box; gate flip applies ?2 s, also from the
  Options row (round trip).
- Coexistence: Alt-latched labels + Shift compare at once; Shift held while left-clicking
  the hovered item (vanilla click behavior unchanged); FPS unchanged while hover-comparing
  in a busy area.

## 9. Tree-verify checklist (run before the code round; re-pin every anchor)

1. Re-check every §3 file:line at source level (the 1344x700-PLAN §8 discipline).
2. Pin exactly which globals the `LoadItemDesc` gates read (`:8514` hover flags,
   `:8527` membership, `:8532` shop-narrow condition) and confirm the swap set is
   `pItemToDescribe` (+ `gpInvItemUnderCursor` only if the gate reads it).
3. Confirm `LoadItemDesc` writes **no** file-scope static other than the
   `TEXT_TooltipSetAttributes` publish (the restore protocol depends on it).
4. Verify equipped-item membership passes `:8527` for every panel family (player inv,
   stash, cube, shop, trade — the owner inventory is the same unit throughout).
5. Set-item equipped path: does `LoadItemDesc` carry set-bonus lines, or route through
   `ITEM_BuildSetItemTooltip` (§5 decision).
6. Per-weapon-class bl1/bl2 values from the bins (bows vs 1H vs 2H vs shields) against
   the §2 mapping rules; confirm `INVENTORY_GetWeaponHandedness` is client-callable at
   draw time.
7. Confirm `CMD_GetKeyForCommand(0x24, ...)` is callable from the draw-time context and
   `gaPlayerBodySlotScreenCoords` is populated in every compare context.
8. Confirm the `ShowItems` wipe ordering relative to the compose hook (capture is
   immediate, but pin the frame order anyway).
9. Cap check: an equipped item's full text already fits `DisplayedTooltip`'s 0x400
   wchars today (paperdoll hovers publish there) — confirm no set-item exception.

## 10. Open asks for the owner — ANSWERED 2026-11-06

1. **Presentation** — **side-by-side second box** (owner picked the recommendation).
2. **Trigger** — **hold Shift** (owner picked the recommendation).
3. **Row label** — **"Shift Item Compare"** (owner's wording).
4. **Shop stock** — **INCLUDED** (owner overrode the draft's skip; implemented via the
   post-publish capture, see the §2/§5/§8 amendments).
5. **Scope** — **one round including the Options row 9** (the plan's recommendation; row 9
   shipped in the same R125 round).

## Sources

- D2R parity: "Item comparison tooltips are going to be very basic — no stat comparisons,
  just the ability to view the other item alongside" — diablo2.io forum,
  https://diablo2.io/forums/how-does-the-item-comparison-tooltip-work-in-resurrected-t8788.html
  (Sep 2021); D2R controller compare-button PSA (r/Diablo, 2021) for the hold-to-compare
  interaction shape; D4's Shift-toggle compare naming precedent (Blizzard forums, 2023).
- In-tree grounding: the item_ilvl hook set (R55-R59, the tooltip-builder map), the
  alt_toggle latch vs the click-modifier poll, the R101/R108 options-page row-growth
  constants, `UI_DrawStoredTooltip` (the existing second channel), handoff §8 conventions
  and FEATURES.md's feature contract (rows 28-31 house rules).