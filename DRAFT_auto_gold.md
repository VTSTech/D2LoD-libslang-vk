# DRAFT_auto_gold.md — slate row 6 implementation draft (v2, 2026-10-29)

*Status: **FIELD VERIFIED** (2026-10-29). Tree-verify pass complete; owner
decisions recorded (§6 → both recommended options); code landed in 3 client
TUs + ini template; syntax-verified via a signature-mirror harness (sandbox
has no mingw); llvm-mingw i686 full build green (user Win11, the compile
gate); owner field session: "Auto-Gold Pickup works!", run with
`alt_toggle=1` enabled alongside (row-31 coexistence). Remaining: land per
handoff §7 (ledger row 33 + worklog entry); optional deep-QA V2/V5/V6 may
ride a later field round, non-blocking. Charter: VANILLA-PLUS.md. Slate:
FEATURES.md row 6. Delivery conventions: handoff §7.*

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
3. player anim mode ∈ {Neutral(1), TownNeutral(5), Walk(2), Run(3)} — **never fire
   mid-attack/cast/gethit/block/sequence/knockback/dead** (V4 pinned 2026-10-29
   from d2_enums.h:5339-5360). Combat stays untouched by construction.
4. scan throttle: static `dwLastScan`, 200 ms floor (A7 style)

Scan = A5 model: player room + adjacent rooms, walk `pUnitList`; an item qualifies iff
`eUnitType == UNIT_ITEM` && `eUnitFlagsEx >> 7 & 1` && `eAnimMode == Run` &&
`ITEM_GetItemsTxt_szCode(pItem) == 0x20646c67` (A4 — the exact test style already in
our trigger TU). Then:

- `nDist = D2Common::Collision::GetDistanceBetweenUnits(pPlayer, pItem)` — helper
  already linked into 6+ client TUs (Player.cpp:1053 etc.), no new deps.
- fire only when **`nDist < 5`** — the server's own instant-pick radius (A2) —
  **AND** `D2Common::Units::TestCollisionBetweenInteractingUnits(pPlayer, pItem,
  0x804)` is clear (v2 amendment: the exact second condition the server's ITEM
  branch checks before instant-picking; client-linkable, 6 precedents in
  D2Client/UNIT/Player.cpp:1011-1391). Mirroring BOTH conditions means the server
  ALWAYS answers a fired 0x16 with immediate `SERVER_PickToInventory` — NEVER the
  walk-to-item mode (no movement hijack, no fighting the player's path, even for
  gold behind a wall/ledge inside the radius).
- per-item dedupe: client-side 16-entry ring buffer of `(dwUnitGUID, GetTickCount)`
  stamps; skip if same GUID stamped < 1000 ms ago. **No writes to any retail unit
  field** (the A7 field is readable precedent, not a write target).
- action: `UnitMode::INTERACT_AnyObject(UNIT_ITEM /*4*/, pItem->nUnitGUID)` — the literal click
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

- **FIELD VERIFIED 2026-10-29** (owner, overlay build on Win11 — llvm-mingw
  i686 full build green): walk-over auto gold pickup works end-to-end
  ("Auto-Gold Pickup works!"). The session ran with `alt_toggle=1` enabled
  alongside — both features live at once, no interference reported (row-31
  coexistence sanity). V2/V3/V5/V6 below were not separately reported; the
  verified path exercises the same retail pick code those scenarios guard, so
  they ride as optional deep-QA on a later field round (non-blocking).
- **V1** ✅ DONE 2026-10-29: `ItemMode.cpp:948-986` (inside SERVER_PickToInventory)
  is the gold→stat core — STATSEX_GetCurrentStat(UNITSTAT_gold) on player+pile,
  cap clamp (remainder behavior read), STATLIST_AddUnitStat, then
  `UNIT::Party::ChangeGoldForPlayer` broadcast. Bonus: the 9-byte 0x16 always
  carries bSetting=0 server-side (SCMD_0x16 at PlayerMsg.cpp:2208 calls
  SERVER_InteractOrPick(bSetting=0)), so our path structurally lands in
  PickToInventory — the cursor branch is unreachable for auto-gold. No writes
  needed anywhere.
- **V2** Field: drop a pile larger than remaining gold cap → behavior must match a
  click on the same pile, frame-for-frame observable.
- **V3** Field: walk over gold with inventory open → gold to stat, nothing on cursor.
- **V4** ✅ DONE 2026-10-29: `eD2PlayerAnimMode_ns` constants pinned
  (d2_enums.h:5339-5360: Death=0, Neutral=1, Walk=2, Run=3, GetHit=4,
  TownNeutral=5, …, Dead=17, Sequence=18, KnockBack=19). Code uses qualified
  names (D2Client.cpp:950 precedent).
- **V5** Field: flip `auto_gold` 1→0 mid-game → picks stop ≤ 2 s; flip 0→1 → start.

## 5. TU list, QA, delivery

- **Touched TUs (3)**: `D2Client/D2Client.cpp` (gate + tick + ring buffer + template
  block), `D2Client/D2Client.h` (2 decls), `D2Client/UI/Draw.cpp` (1 call line in
  `DRAW_UI`). Nothing else compiles differently.
- **QA**: sandbox syntax-verified 2026-10-29 via signature-mirror harness
  (`g++ -fsyntax-only -Wall -Wextra` → PASS; scripts/auto_gold_syntax_check.cpp;
  stubs mirror Units.h:60/148, Collision.h:40, Dungeon.h:77, Items.h:178,
  UnitMode.h:21, Player.h:172, D2UnitStrc.h fields). Real gate PASSED
  2026-10-29: llvm-mingw i686 full build green (user Win11). Field loop
  PASSED (core): owner verified walk-over pickup end-to-end with
  `alt_toggle=1` enabled alongside (row-31 coexistence). Optional deep-QA on
  a later round, non-blocking: V2/V3/V5/V6 scenarios (incl. clicking gold
  with labels up) and the A/B `auto_gold=0` = vanilla check.
- **V6** (added v2) Field: gold behind a wall/ledge within distance 5 → NOT
  picked and NO walk started (collision parity suppresses the fire).
- **Land**: patch-first per §7 (unique anchors per site: A1/A6/A8 sites are all
  single-grep-gate), ledger row 33 + worklog entry, release note at next tag.

## 6. Owner decisions (2026-10-29) — both recommended options

1. **Radius**: **strict** — `< 5` instant-pick only, plus the mirrored 0x804
   collision test (D2 v2 amendment). Zero movement interference; the character
   is never moved by this feature.
2. **Combat guard**: **on** — fires only from Neutral/TownNeutral/Walk/Run;
   attacking/casting/gethit/block/sequence/knockback/dead frames suppress.
