# DRAFT_auto_gold.md — slate row 6 implementation draft (v1, 2026-10-29)

*Status: Task-C-style tree-verify pass **complete**. Awaiting owner go — then one
implementation round per handoff §11 (tree-verify → implement → QA → land). Charter:
VANILLA-PLUS.md. Slate: FEATURES.md row 6. Delivery conventions: handoff §7.*

**Headline of the pass: this feature needs ZERO changes to D2Game or D2Common.**
The walk-over pickup rides retail's own, fully recovered server flow — the same 0x16
interact packet a gold-pile click sends, which the server answers either with an
immediate pick (adjacent) or its own walk-to-item mode. All failure semantics (gold
cap, obstruction, busy) stay retail because we never touch them. The entire change is
a client-side trigger + the standard ini gate, in 3 TUs.

## 1. Verified anchor map

| # | Anchor | Evidence (scan 2026-10-29) | Grep gate (must hold at impl time) |
|---|---|---|---|
| A1 | **Client send** — `INTERACT_AnyObject` UNIT_ITEM case | `D2Client/UNIT/UnitMode.cpp:150-153`: `NET_D2GS_CLIENT_Send_INT_INT_INT(0x16, 4, nUnitGUID, bInventoryOpen & 0xff)` | `Send_INT_INT_INT(0x16, 4` in UnitMode.cpp → exactly 1 hit |
| A2 | **Server dispatcher (read-only)** | `D2Game/PLAYER/PlayerMsg.cpp:689` `SERVER_InteractOrPick`; ITEM branch **791-811**: target must be `eAnimMode == Run` (ground item), `nDist < 0x33` bail, `nDist < 5 && !TestCollisionBetweenInteractingUnits(pUnit, pTargetUnit, 0x804)` → `SERVER_PickToInventory` (bSetting 0) / `SERVER_PickToCursor` (bSetting 1), **else `StartModeID(...)` = retail walk-to-item** | `case UNIT_ITEM:` + `nDist < 5` in PlayerMsg.cpp → lines ~791-811 |
| A3 | **Packet chain (read-only)** | `D2Game/GAME/CCmd.cpp:245` `NET_D2GS_SERVER_ProcessClientMessage_InGame` → `:263` `PlayerMsg::NET_D2GS_SERVER_HandleIncomingPacketAfterClientCheck` → `SCMD_0x16_InteractWithEntityEx` (PlayerMsg.cpp:2224) → `SERVER_InteractOrPick` | grep `SCMD_0x16_InteractWithEntityEx` in PlayerMsg.* |
| A4 | **Gold code test** | `'gld '` little-endian = **`0x20646c67`**; server precedents `D2Game/ITEMS/Items/Drop.cpp:389/2901` (`gnGoldItemClassId`); **client-side dword-code-test precedent in our own trigger TU**: UnitMode.cpp:110/127 (`ITEM_GetItemsTxt_szCode(x) != 0x20326671` quiver check) | `0x20646c67` count across repo ≥ 12 files |
| A5 | **Client ground-item scan model** | `D2Client/UI/ShowItems.cpp:91` `DRLGROOM_GetAdjacentRoomsList(pPlayerRoom, ...)`; `:105-107` per-room `pUnitList` walk, filter `pItem->eUnitFlagsEx >> 7 & 1 && eUnitType == UNIT_ITEM`; ground-mode check precedent `D2Client/UNIT/CUnit.cpp:2452` (`eAnimMode == Run`) | grep `eUnitFlagsEx >> 7 & 1` in ShowItems.cpp |
| A6 | **Per-frame host** | `D2Client/UI/Draw.cpp:2041` `DRAW_UI(pGameView, pnNpcParam)` is the per-frame in-game UI stage; `DRAW_AltItems()` is called at `:2240` **inside** the `GFX_bShowUIFlag.AltShowItems` gate (`:2237`) — Alt-only, so NOT our host. `AUTO_GOLD_Tick()` goes in the same `DRAW_UI` stage, **outside** that gate | `DRAW_UI(D2GameViewStrc` in UI/Draw.cpp → 1 hit; insert line pinned at impl |
| A7 | **Throttle precedent** | `UnitMode.cpp:87-92`: per-target `GetTickCount()` vs `pTargetUnit->dwTickCount`, 200 ms (monster interact). We do NOT write unit fields — see D3 | grep `dwTickCount < 200` in UnitMode.cpp |
| A8 | **Gate + template precedents** | row 28 `ALTTOGGLE_Active` (GetPrivateProfileIntA "features", 2 s GetTickCount, `!lastCheck` sentinel), row 29 `ALTTOGGLE_EnsureIni`/`ALTTOGGLE_WriteIniTemplate`, decls `D2Client.h:120-131`, boot hook `ApplicationMain` | grep `ALTTOGGLE_Active` in D2Client.cpp/h |

## 2. Design

### D1 — Gate (house pattern, verbatim)

`AUTO_GOLD_Active()` in `D2Client/D2Client.cpp` + decl in `D2Client/D2Client.h`:
`GetPrivateProfileIntA("features", "auto_gold", 0, szIniPath)`, exe-dir anchored,
re-read at most every 2 s (GetTickCount gate, `!lastCheck` sentinel — byte-for-byte
the row-28/30 pattern). Missing file/section/key/garbage = 0 = vanilla. Template:
documented `auto_gold=0` block appended to `ALTTOGGLE_WriteIniTemplate`
("walk over gold piles to pick them up; same path as clicking them; 0 = vanilla").

### D2 — Trigger: `AUTO_GOLD_Tick()` (new, D2Client.cpp)

Called once per frame from `DRAW_UI` (UI/Draw.cpp, A6 site — one call line). Guards,
in order; any fail = return, zero sends:

1. `AUTO_GOLD_Active()`
2. player unit + player room exist (`Player::GetPlayerUnit_UI`, `UNITS_GetRoom`)
3. player anim mode ∈ {Neutral, TownNeutral, Walk, Run} — **never fire mid-attack/
   cast/gethit/dead** (A5-mode precedent `eAnimMode.ePlayerMode`; exact enum names
   pinned at impl, V4). Combat stays untouched by construction.
4. scan throttle: static `dwLastScan`, 200 ms floor (A7 style)

Scan = A5 model: player room + adjacent rooms, walk `pUnitList`; an item qualifies iff
`eUnitType == UNIT_ITEM` && `eUnitFlagsEx >> 7 & 1` && `eAnimMode == Run` &&
`ITEM_GetItemsTxt_szCode(pItem) == 0x20646c67` (A4 — the exact test style already in
our trigger TU). Then:

- `nDist = D2Common::Collision::GetDistanceBetweenUnits(pPlayer, pItem)` — helper
  already linked into 6+ client TUs (Player.cpp:1053 etc.), no new deps.
- fire only when **`nDist < 5`** — the server's own instant-pick radius (A2), so every
  fired request completes immediately and NEVER starts a retail walk (no movement
  hijack, no fighting the player's path).
- per-item dedupe: client-side 16-entry ring buffer of `(dwUnitGUID, GetTickCount)`
  stamps; skip if same GUID stamped < 1000 ms ago. **No writes to any retail unit
  field** (the A7 field is readable precedent, not a write target).
- action: `UnitMode::INTERACT_AnyObject(4, pItem->dwUnitGUID)` — the literal click
  path (A1). One send per qualifying pile per frame at most; ring buffer caps bursts.

### D3 — What is explicitly NOT changed

`D2Game/**`, `D2Common/**` (zero edits — A2/A3 are read-only), `ShowItems.cpp` (label
renderer untouched), `CUnit.cpp` hit-tests (row-31 machinery untouched), cursor
handling, panel code. No unit-field writes anywhere. No new assets. Online note: the
0x16 flow is identical on realm games (no server edits), but per charter this ships
SP-focused; the ini comment says exactly that.

## 3. Semantics table (why zero server edits is the whole point)

| Situation | Behavior | Owner |
|---|---|---|
| Gold within 5 subtile-dist, clear | `SERVER_PickToInventory` → gold to stat, pile gone | retail (untouched) |
| Gold ≥ 5 away | client never fires (gate) — no walk started | our gate |
| Obstructed / unreachable | retail collision test inside the same branch | retail |
| Gold cap hit | retail clamp inside the pick path (V2 field-verifies remainder behavior) | retail |
| Inventory open while walking over | client flag byte passes through (A1); gold never sits on cursor — V3 verifies the `SERVER_PickToCursor` gold branch converges to gold-to-stat like a click with inv open | retail |
| Dead / mid-cast / mid-attack | client never fires (guard 3) | our gate |
| `auto_gold=0` / no ini | guard 1 false → `DRAW_UI` stage unchanged → vanilla byte-for-byte | charter Principle 1 |

## 4. Impl-round verify items (V1–V5)

- **V1** Read the gold branches of `SERVER_PickToInventory` / `SERVER_PickToCursor`
  (`D2Game/ITEMS/ItemMode.cpp` + PlayerMsg.cpp) — confirm gold→stat conversion and
  that no item-path code reads a field we might have been tempted to write (ring
  buffer makes this moot, but read anyway).
- **V2** Field: drop a pile larger than remaining gold cap → behavior must match a
  click on the same pile, frame-for-frame observable.
- **V3** Field: walk over gold with inventory open → gold to stat, nothing on cursor.
- **V4** Pin client-side `eAnimMode` names for guard 3 from Player.cpp usage.
- **V5** Field: flip `auto_gold` 1→0 mid-game → picks stop ≤ 2 s; flip 0→1 → start.

## 5. TU list, QA, delivery

- **Touched TUs (3)**: `D2Client/D2Client.cpp` (gate + tick + ring buffer + template
  block), `D2Client/D2Client.h` (2 decls), `D2Client/UI/Draw.cpp` (1 call line in
  `DRAW_UI`). Nothing else compiles differently.
- **QA**: per-TU llvm-mingw i686 compile + full build gate (Win11 / CI) + §11 field
  loop (V2/V3/V5 scenarios + row-31 click-through regression check: clicking gold
  still works with `alt_toggle=1`); A/B `auto_gold=0` = vanilla.
- **Land**: patch-first per §7 (unique anchors per site: A1/A6/A8 sites are all
  single-grep-gate), ledger row 33 + worklog entry, release note at next tag.

## 6. Open questions for the owner (one-word answers fine)

1. **Radius**: strict `< 5` instant-pick only (recommended — zero movement
   interference), or a wider radius that lets retail's own walk-to-item finish the
   pickup (D2R-er "vacuum" feel, but the character detours toward gold)?
2. **Combat guard**: skip picking while attacking/casting (recommended — combat
   untouchable), or pick regardless like D2R does?