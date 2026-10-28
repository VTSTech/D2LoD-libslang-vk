// [dropdiag v1] D2Game/D2Common drop-pipeline diagnostic (jaenster/redivivus issue #7).
//
// Symptom: mobs AND chests drop nothing on the rebuilt Game.exe (confirmed
// without Glide on d2s-1a180d1), retail drops fine. Fixes 1-6 touch only
// D2Glide/*, so this is a pre-existing core-side defect, not a vanilla-plus
// regression. Static reading (issue #7 notes) clears the roll loop itself:
// the positive-picks path has proper NoDrop/RNG math (Drop.cpp 4076-4160)
// and the negative-picks no-RNG roll (3848) would mis-pick, never zero-drop.
// Zero drops from both mobs and chests points at the shared upstream:
// TreasureClassEx.txt parsing or the parsed-TC lookup.
//
// Silent kill points this dump discriminates between:
//   (F1) TreasureClassEx.txt not loaded/compiled -> nParsedTreasureClassSize
//        near zero -> TC_GetParsedTreasureClassByLevel returns NULL for every
//        id >= size (MonsterTbls.cpp 2448-2450) -> RollItemAndGetTreasureClass
//        returns without dropping; RollTreasureClass chest slots all NULL
//        (ObjectsMode.cpp 3660 silent return).
//   (F2) parser record-stride defect -> TCs registered but entries/weights
//        empty -> roll loop breaks on nTotalWeight == 0 (Drop.cpp 3844).
//   (F3) name-link registry defect -> nested TC / item-code entries dropped
//        by TC_ParseTreasureClassEntry early returns.
//   (F4) item creation failing downstream (ITEMGEN_CreateItemFromClassIdAndQuality
//        NULL: FindBestSpotToSpawnItem failure, items.txt gaps) -> rolls fine,
//        nothing appears.
//   (F5) chest-ABC table unresolved ("Act %d%s Chest %s" name mismatch) ->
//        chest path silently returns nullptr.
//   (F6) death dispatch never reaches the drop call -> zero LOOKUP events.
//
// Evidence plan: one-shot parse dump at the tail of TXT_AllocItemDropData
// (table sizes, per-TC health, known-name probes, chest-ABC resolution) plus
// capped per-event logs (victim dispatch, TC lookup results, item-gen
// results, chest rolls) with running aggregate counters. Output appends to
// d2drop-diag.txt in the game working directory.
//
// Cost: parse dump once per process; event hooks collapse to counter bumps
// after their caps (64 victim / 128 lookup / 128 itemgen / 48 chest), with
// one aggregate line per 32 capped calls.

#pragma once

#include <stdint.h>

struct D2ParsedTreasureClassStrc;

namespace D2Common::DATATBLS::DropDiag {

// Called at the tail of TC_GenerateAutoItemTypeTreasureClasses (MonsterTbls.cpp).
// Logs the parsed-TC count produced by auto-itemtype generation alone.
void __stdcall NoteAutoItemTypesGenerated();

// Called at the tail of TXT_AllocTxt_TreasureClassEx (MonsterTbls.cpp), before
// UnloadBin. Logs the raw compiled-record count for TreasureClassEx.txt.
void __stdcall NoteTreasureClassExLoaded(int nTxtRecords);

// Called at the tail of TXT_AllocItemDropData (MonsterTbls.cpp), after the
// chest-ABC table is populated. Writes the one-shot parse dump: global table
// sizes, first 16 parsed TC records, zero-entry/zero-weight census, known-name
// link probes, chest-ABC slot resolution and dual-spelling name probes.
void __stdcall NoteDropTablesInitialized();

// Called from the monster-death drop dispatch (GAME/Event/Dispatch/Monster.cpp)
// right before SERVER_ITEM_RollItemAndGetTreasureClass. Absence of these
// events while mobs die = F6 (dispatch broken); presence with NULL lookups = F1.
void __stdcall NoteVictimDrop(int nUnitClassId, int nSelectedTC, int nVictimLevel, int bIsItemFindDrop);

// Called from SERVER_ITEM_RollItemAndGetTreasureClass (Drop.cpp) immediately
// after TC_GetParsedTreasureClassByLevel, before the NULL early-out. Logs the
// requested TC id, the level selector, and the resolved record health
// (entries/weights/picks/nodrop) or NULL against the current table size.
void __stdcall NoteRollLookup(int nSelectedTC, int nLevel, const struct D2ParsedTreasureClassStrc * pTreasure);

// Called from the chest drop path (ObjectsMode.cpp) after RollTreasureClass
// and before SERVER_ITEM_RollItemToDrop. pTreasure NULL = F5 evidence.
void __stdcall NoteChestRoll(int nObjClassId, const struct D2ParsedTreasureClassStrc * pTreasure, int nVictimLevel);

// Called from the chest drop path (ObjectsMode.cpp) after SERVER_ITEM_RollItemToDrop.
// Counts non-null result slots (0/6 with a healthy TC = roll-loop or F4 issue).
void __stdcall NoteChestRollResult(const struct D2ParsedTreasureClassStrc * pTreasure, void * const * ppResults, int nSlots);

// Called from the roll loop (Drop.cpp) after ITEMGEN_CreateItemFromClassIdAndQuality.
// Covers gold, quivers and regular items. pCreatedItem NULL = F4 evidence.
void __stdcall NoteItemGen(int nItemClassId, int eQuality, const void * pCreatedItem, int nQuantity, int nFlags);

// ==== [dropdiag v2] broadcast-chain audit (server -> client visibility) ====
//
// v1 proved items are rolled and created (LOOKUP ok / IGEN ok, itemGenFail=0),
// yet nothing appears in-game. Static audit of the placement path (CreateUnit
// -> UNITS_PlaceUnitInRoom -> update list -> DispatchUnitUpdate ->
// SERVER_SendInitUnitPacket -> 0x9C) found no fault, so v2 instruments each
// broadcast stage and keys every line on the item GUID. For each created
// item, the FIRST missing stage in its G1-G6 chain is the defect location:
//   (G1) "V2 IGEN2"    - item unit exists with post-placement state (mode,
//                        unit flags, static path, bRoomNeedsUpdate, room,
//                        ISLINKREFRESHMSG/INITSEEDSET bits)
//   (G2) "V2 UPLINK"   - ROOM_LinkUnitToUpdateList outcome (0=linked,
//                        1=no room, 2=room update-locked, 3=already linked)
//   (G3) "V2 DISPATCH" - the per-frame SendRoomUnitUpdates walk reached the
//                        item for a client
//   (G4) "V2 INITPKT"  - the UNIT_ITEM case in SERVER_SendInitUnitPacket ran
//                        (gate=INITSEEDSET open/closed logged)
//   (G5) "V2 CRX9C"    - the client-side 0x9C handler saw the packet
//   (G6) "V2 RECYCLE"/"V2 FREE" - item was recycled/freed (before a later
//                        stage = destroyed before broadcast)
// Chest anomaly follow-up (F5-adjacent): v1 showed chest-ABC resolved=15/45
// (kind B only) while the same-name probes resolve after build - impossible
// for identical lookups in the same init call. The one-shot 45-line
// "V2 CHESTBUILD" dump logs the exact formatted name + link index per slot.
//
// QA grep gates (uniqueness assertions):
//   grep -c "V2 CHESTBUILD " == 45
//   grep -c "V2 IGEN2 "      == count of v1 "IGEN .* -> ok" lines (up to cap)
//   every guid in IGEN2 must have a UPLINK line; first stage missing from
//   a guid's chain = failing stage (document, don't enforce, in code).
void __stdcall NoteItemSpawnAudit(const void * pUnit);

// Called from ROOM_LinkUnitToUpdateList (D2Common/Unit/Unit.cpp) for every
// unit; DropDiag filters to UNIT_ITEM. nReason: 0=linked, 1=no room,
// 2=room update-locked, 3=ISLINKREFRESHMSG already set.
void __stdcall NoteUpdateListLink(const void * pUnit, int nReason);

// Called from DispatchUnitUpdate (D2Game/GAME/Clients.cpp) for every unit
// walked off a room update list; DropDiag filters to UNIT_ITEM.
void __stdcall NoteDispatchUnitUpdate(const void * pUnit, const void * pClient);

// Called at the top of the UNIT_ITEM case in SERVER_SendInitUnitPacket
// (D2Game/UNIT/SUnitMsg.cpp). Logs the 0x9C gate (INITSEEDSET) state.
void __stdcall NoteInitPacketItemCase(const void * pUnit);

// Called from UNITS_ResetUnitForReuse (nStage=0) and FreeServerUnit
// (nStage=1) for item units - catches items cleaned up before broadcast.
void __stdcall NoteItemRecycle(const void * pUnit, int nStage);

// Called from the client-side 0x9C item-action handler entry
// (D2Client/GAME/SCmd.cpp). Logs subcode and raw GUID dword from pBytes.
void __stdcall NoteClient0x9C(const uint8_t * pBytes);

// Called once before TXT_AllocParsedTreasureClassDiffActChestABC's loops.
void __stdcall NoteChestTableBuildStart();

// Called after each of the 45 chest-ABC lookups with the exact formatted
// name and GetLinkNodeIndex result (-1 = unresolved).
void __stdcall NoteChestTableLookup(const char * szTCName, int nTCIdx);

} // namespace D2Common::DATATBLS::DropDiag
