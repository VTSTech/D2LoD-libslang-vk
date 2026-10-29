// [dropdiag v1] Drop-pipeline diagnostic implementation — see DropDiag.h.
//
// Mirrors the LightDiag/ShadowDiag pattern: plain fopen-append logging to
// d2drop-diag.txt, fixed caps per event class, aggregate counters embedded in
// every line, one-shot parse dump. All hooks are no-ops (counter bump only)
// once their cap is reached, so steady-state cost is negligible.

#include <stdio.h>
#include <string.h>
#include "d2_platform.h"
#include "globals/globals.h"
#include "Fog/Src/Excel.h"
#include "D2Common/DATATBLS/DropDiag.h"
#include "D2Common/DATATBLS/MonsterTbls.h"
#include "D2Game/D2ParsedTreasureClassStrc.h"
// [dropdiag v2.1] full D2UnitStrc definition — the v2 broadcast hooks
// dereference unit fields (pPath.pStaticPath, eUnitFlags, eAnimMode);
// previously only a forward declaration leaked in, which CI rejected
// (21x "invalid use of incomplete type" at d14e0af).
#include "D2Common/Unit/D2UnitStrc.h"

namespace D2Common::DATATBLS::DropDiag {

static const int kCapVictim = 64;
static const int kCapLookup = 128;
static const int kCapItemGen = 128;
static const int kCapChest = 48;

// [dropdiag v2] broadcast-chain caps (per event class).
static const int kCapV2 = 96;

static uint32_t s_nItemGensAudited;
static uint32_t s_nUpLinks;
static uint32_t s_nUpLinkSkips;
static uint32_t s_nDispatchItems;
static uint32_t s_nInitPktItemOpen;
static uint32_t s_nInitPktItemClosed;
static uint32_t s_nClientRx;
static uint32_t s_nItemReuse;
static uint32_t s_nItemFree;
static bool s_bChestBuildStarted;
static int s_nChestBuildSlot;

static uint32_t s_nVictimDrops;
static uint32_t s_nRollLookups;
static uint32_t s_nNullLookups;
static uint32_t s_nItemGens;
static uint32_t s_nItemGenFails;
static uint32_t s_nChestRolls;
static uint32_t s_nChestNullTc;
static uint32_t s_nChestNoItems;

static bool s_bAutoLogged;
static bool s_bTcExLogged;
static bool s_bParseDumped;

static FILE * OpenLog() {
    return fopen("d2drop-diag.txt", "a");
}

static void WriteAggregates(FILE * pFile) {
    fprintf(pFile, " | agg victim=%u lookup=%u lookupNull=%u itemGen=%u itemGenFail=%u chest=%u chestNullTc=%u chestNoItems=%u",
            s_nVictimDrops, s_nRollLookups, s_nNullLookups, s_nItemGens, s_nItemGenFails,
            s_nChestRolls, s_nChestNullTc, s_nChestNoItems);
    // [dropdiag v2] broadcast-chain counters appended (v1 fields unchanged).
    fprintf(pFile, " | v2 igen2=%u uplink=%u uplinkSkip=%u disp=%u initOpen=%u initClosed=%u crx=%u reuse=%u free=%u",
            s_nItemGensAudited, s_nUpLinks, s_nUpLinkSkips, s_nDispatchItems,
            s_nInitPktItemOpen, s_nInitPktItemClosed, s_nClientRx, s_nItemReuse, s_nItemFree);
}

// [dropdiag v2] shared cap+log prologue: returns NULL past the cap (writing a
// periodic aggregate line every 32 overflow calls), else an open log file.
static FILE * V2LogOrAggregate(uint32_t * pnCounter) {
    (*pnCounter)++;
    if ((int)*pnCounter > kCapV2) {
        if (!((*pnCounter - kCapV2) % 32)) {
            FILE * pFile = OpenLog();
            if (pFile) {
                fprintf(pFile, "V2 AGG cap reached, tick=%u", GetTickCount());
                WriteAggregates(pFile);
                fprintf(pFile, "\n");
                fclose(pFile);
            }
        }

        return nullptr;
    }

    return OpenLog();
}

// One-line health summary of a parsed TC record (indices are table slots).
static void DumpTcHealth(FILE * pFile, const char * szTag, int nIdx, const D2ParsedTreasureClassStrc * pTC) {
    if (!pTC) {
        fprintf(pFile, "%s [%d] -> NULL\n", szTag, nIdx);
        return;
    }

    fprintf(pFile, "%s [%d] group=%d level=%d picks=%d nodrop=%d entries=%d wC=%d wE=%d pEntries=%p\n",
            szTag, nIdx, (int)pTC->nGroup, (int)pTC->nLevel, (int)pTC->nProb, (int)pTC->nNoDrop,
            (int)pTC->nEntriesCount, (int)pTC->nTotalWeightClassic, (int)pTC->nTotalWeightExpansion,
            (const void *)pTC->pEntries);
}

// Name probe against the treasure-class link registry. Indices are 1-based
// for real TC names (slot 0 is the empty-name base TC), so <= 0 is unresolved.
static void ProbeTcName(FILE * pFile, const char * szName) {
    int nIdx = Fog::Src::Excel::GetLinkNodeIndex(sgtDataTable.pTxtMonsterTblsLink, (char *)szName, 0);
    if (nIdx < 1 || nIdx >= sgtDataTable.nParsedTreasureClassSize) {
        fprintf(pFile, "  probe \"%s\" -> %d (unresolved)\n", szName, nIdx);
        return;
    }

    DumpTcHealth(pFile, "  probe", nIdx, sgtDataTable.pParsedTreasureClass + nIdx);
}

void __stdcall NoteAutoItemTypesGenerated() {
    s_bAutoLogged = true;
    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "==== d2drop-diag v1 tick=%u stage: auto-itemtype TCs done ====\n", GetTickCount());
    fprintf(pFile, "parsed TC size after auto-itemtype generation: %d\n", sgtDataTable.nParsedTreasureClassSize);
    fclose(pFile);
}

void __stdcall NoteTreasureClassExLoaded(int nTxtRecords) {
    if (s_bTcExLogged) {
        return;
    }

    s_bTcExLogged = true;
    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "==== d2drop-diag v1 tick=%u stage: TreasureClassEx.txt compile ====\n", GetTickCount());
    fprintf(pFile, "compiled txt records: %d (0 = TreasureClassEx.txt missing or failed to compile = F1)\n", nTxtRecords);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteDropTablesInitialized() {
    if (s_bParseDumped) {
        return;
    }

    s_bParseDumped = true;
    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "==== d2drop-diag v1 tick=%u stage: TXT_AllocItemDropData complete ====\n", GetTickCount());
    fprintf(pFile, "parsed TC size=%d array=%p monTblsLink=%p monStats=%d superUniques=%d itemTypes=%d sgptAliasesSgt=%d\n",
            sgtDataTable.nParsedTreasureClassSize, (const void *)sgtDataTable.pParsedTreasureClass,
            (const void *)sgtDataTable.pTxtMonsterTblsLink, sgtDataTable.nTxtMonStatsSize,
            sgtDataTable.nTxtSuperUniquesSize, sgtDataTable.nTxtItemTypesSize,
            (int)(sgptDataTable == &sgtDataTable));

    // (F2 census) entries/weights health across the whole table.
    int nZeroEntries = 0;
    int nZeroWeights = 0;
    int nTotalEntries = 0;
    for (int nIdx = 0; nIdx < sgtDataTable.nParsedTreasureClassSize; nIdx++) {
        const D2ParsedTreasureClassStrc * pTC = sgtDataTable.pParsedTreasureClass + nIdx;
        nTotalEntries += pTC->nEntriesCount;
        if (!pTC->nEntriesCount) {
            nZeroEntries++;
        } else if (!pTC->nTotalWeightClassic && !pTC->nTotalWeightExpansion) {
            nZeroWeights++;
        }
    }

    fprintf(pFile, "census: totalEntries=%d zeroEntryTCs=%d zeroWeightTCs=%d (of %d)\n",
            nTotalEntries, nZeroEntries, nZeroWeights, sgtDataTable.nParsedTreasureClassSize);

    // First 16 records — see the parse shape directly.
    int nDumpCount = sgtDataTable.nParsedTreasureClassSize < 16 ? sgtDataTable.nParsedTreasureClassSize : 16;
    for (int nIdx = 0; nIdx < nDumpCount; nIdx++) {
        DumpTcHealth(pFile, "tc", nIdx, sgtDataTable.pParsedTreasureClass + nIdx);
    }

    // (F3) known-name probes — retail TreasureClassEx.txt names.
    ProbeTcName(pFile, "Act 1 Good");
    ProbeTcName(pFile, "Act 1A Gold");
    ProbeTcName(pFile, "Weapons 3");
    ProbeTcName(pFile, "Armor 3");

    // (F4 partial) first TC with entries: dump the first 4 parsed entries.
    for (int nIdx = 1; nIdx < sgtDataTable.nParsedTreasureClassSize; nIdx++) {
        const D2ParsedTreasureClassStrc * pTC = sgtDataTable.pParsedTreasureClass + nIdx;
        if (pTC->nEntriesCount > 0 && pTC->pEntries) {
            int nEntryCount = pTC->nEntriesCount < 4 ? pTC->nEntriesCount : 4;
            for (int nEntryIdx = 0; nEntryIdx < nEntryCount; nEntryIdx++) {
                const D2ParsedTreasureClassEntriesStrc * pEntry = pTC->pEntries + nEntryIdx;
                fprintf(pFile, "  tc[%d] entry[%d] classId=%d flags=0x%02x cumC=%d cumE=%d mods u=%d s=%d r=%d m=%d\n",
                        nIdx, nEntryIdx, (int)pEntry->nItemClassId, (unsigned)pEntry->nFlags,
                        (int)pEntry->nCumulativeWeightClassic, (int)pEntry->nCumulativeWeightExpansion,
                        (int)pEntry->nUniqueMod, (int)pEntry->nSetMod, (int)pEntry->nRareMod, (int)pEntry->nMagicMod);
            }

            break;
        }
    }

    // (F5) chest-ABC table: 45 flat slots (3 difficulties x 5 acts x 3 chests).
    D2ParsedTreasureClassStrc ** ppChestSlots = (D2ParsedTreasureClassStrc **)&sgtDataTable.pParsedTreasureClassDiffActChestABC;
    int nResolvedChests = 0;
    for (int nSlot = 0; nSlot < 45; nSlot++) {
        if (ppChestSlots[nSlot]) {
            nResolvedChests++;
        }
    }

    fprintf(pFile, "chest ABC table: resolved=%d/45 (0 = F5)\n", nResolvedChests);
    for (int nSlot = 0; nSlot < 9; nSlot++) {
        fprintf(pFile, "  chest slot[%d] (diff %d act %d chest %d) -> %p\n",
                nSlot, nSlot / 15, (nSlot / 3) % 5, nSlot % 3, (const void *)ppChestSlots[nSlot]);
    }

    // Chest name spelling probes — the in-game formatter is "Act %d%s Chest %s";
    // if the txt uses a different difficulty suffix spelling, probes show which.
    ProbeTcName(pFile, "Act 1 Chest A");
    ProbeTcName(pFile, "Act 1N Chest A");
    ProbeTcName(pFile, "Act 1 (N) Chest A");
    ProbeTcName(pFile, "Act 1H Chest A");
    ProbeTcName(pFile, "Act 1 (H) Chest A");

    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteVictimDrop(int nUnitClassId, int nSelectedTC, int nVictimLevel, int bIsItemFindDrop) {
    s_nVictimDrops++;
    if ((int)s_nVictimDrops > kCapVictim) {
        if (!((s_nVictimDrops - kCapVictim) % 32)) {
            FILE * pFile = OpenLog();
            if (pFile) {
                fprintf(pFile, "AGG victim cap reached, tick=%u", GetTickCount());
                WriteAggregates(pFile);
                fprintf(pFile, "\n");
                fclose(pFile);
            }
        }

        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "VICTIM cls=%d tc=%d lvl=%d ifd=%d tableSize=%d",
            nUnitClassId, nSelectedTC, nVictimLevel, bIsItemFindDrop, sgtDataTable.nParsedTreasureClassSize);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteRollLookup(int nSelectedTC, int nLevel, const D2ParsedTreasureClassStrc * pTreasure) {
    s_nRollLookups++;
    if (!pTreasure) {
        s_nNullLookups++;
    }

    if ((int)s_nRollLookups > kCapLookup) {
        if (!((s_nRollLookups - kCapLookup) % 32)) {
            FILE * pFile = OpenLog();
            if (pFile) {
                fprintf(pFile, "AGG lookup cap reached, tick=%u", GetTickCount());
                WriteAggregates(pFile);
                fprintf(pFile, "\n");
                fclose(pFile);
            }
        }

        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    if (!pTreasure) {
        // Smoking-gun line for F1: id requested vs table size.
        fprintf(pFile, "LOOKUP tc=%d lvl=%d -> NULL tableSize=%d (id >= size or id == 0 = F1)",
                (int)nSelectedTC, nLevel, sgtDataTable.nParsedTreasureClassSize);
    } else {
        fprintf(pFile, "LOOKUP tc=%d lvl=%d -> ok entries=%d wC=%d wE=%d picks=%d nodrop=%d",
                (int)nSelectedTC, nLevel, (int)pTreasure->nEntriesCount,
                (int)pTreasure->nTotalWeightClassic, (int)pTreasure->nTotalWeightExpansion,
                (int)pTreasure->nProb, (int)pTreasure->nNoDrop);
    }

    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteChestRoll(int nObjClassId, const D2ParsedTreasureClassStrc * pTreasure, int nVictimLevel) {
    s_nChestRolls++;
    if (!pTreasure) {
        s_nChestNullTc++;
    }

    if ((int)s_nChestRolls > kCapChest) {
        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    if (!pTreasure) {
        fprintf(pFile, "CHEST cls=%d lvl=%d -> TC NULL (F5)", nObjClassId, nVictimLevel);
    } else {
        fprintf(pFile, "CHEST cls=%d lvl=%d -> ok entries=%d wC=%d wE=%d picks=%d nodrop=%d",
                nObjClassId, nVictimLevel, (int)pTreasure->nEntriesCount,
                (int)pTreasure->nTotalWeightClassic, (int)pTreasure->nTotalWeightExpansion,
                (int)pTreasure->nProb, (int)pTreasure->nNoDrop);
    }

    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteChestRollResult(const D2ParsedTreasureClassStrc * pTreasure, void * const * ppResults, int nSlots) {
    if (!pTreasure) {
        return;
    }

    int nCreated = 0;
    for (int nSlot = 0; nSlot < nSlots; nSlot++) {
        if (ppResults[nSlot]) {
            nCreated++;
        }
    }

    if (!nCreated) {
        s_nChestNoItems++;
    }

    if ((int)s_nChestRolls > kCapChest) {
        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "CHESTRESULT created=%d/%d (0 with healthy TC = F2/F4)", nCreated, nSlots);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteItemGen(int nItemClassId, int eQuality, const void * pCreatedItem, int nQuantity, int nFlags) {
    s_nItemGens++;
    if (!pCreatedItem) {
        s_nItemGenFails++;
    }

    if ((int)s_nItemGens > kCapItemGen) {
        if (!((s_nItemGens - kCapItemGen) % 32)) {
            FILE * pFile = OpenLog();
            if (pFile) {
                fprintf(pFile, "AGG itemGen cap reached, tick=%u", GetTickCount());
                WriteAggregates(pFile);
                fprintf(pFile, "\n");
                fclose(pFile);
            }
        }

        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    if (!pCreatedItem) {
        fprintf(pFile, "IGEN cls=%d quality=%d qty=%d flags=0x%x -> NULL (F4)", nItemClassId, eQuality, nQuantity, nFlags);
    } else {
        fprintf(pFile, "IGEN cls=%d quality=%d qty=%d flags=0x%x -> ok", nItemClassId, eQuality, nQuantity, nFlags);
    }

    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

// ==== [dropdiag v2] broadcast-chain audit — see DropDiag.h stage table ====//

void __stdcall NoteItemSpawnAudit(const void * pVoidUnit) {
    const D2UnitStrc * pUnit = (const D2UnitStrc *)pVoidUnit;
    if (!pUnit) {
        return;
    }

    FILE * pFile = V2LogOrAggregate(&s_nItemGensAudited);
    if (!pFile) {
        return;
    }

    const D2StaticPathStrc * pPath = pUnit->pPath.pStaticPath;
    fprintf(pFile, "V2 IGEN2 guid=%08x mode=%d uflags=%08x path=%d bRNU=%d room=%p islink=%d initseed=%d",
            (unsigned)pUnit->nUnitGUID, (int)pUnit->eAnimMode.ePlayerMode, (unsigned)pUnit->eUnitFlags,
            pPath ? 1 : 0, (pPath && pPath->bRoomNeedsUpdate) ? 1 : 0,
            (const void *)(pPath ? pPath->pRoom : nullptr),
            (pUnit->eUnitFlags & UNITFLAG_ISLINKREFRESHMSG) ? 1 : 0,
            (pUnit->eUnitFlags & UNITFLAG_INITSEEDSET) ? 1 : 0);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteUpdateListLink(const void * pVoidUnit, int nReason) {
    const D2UnitStrc * pUnit = (const D2UnitStrc *)pVoidUnit;
    if (!pUnit || pUnit->eUnitType != UNIT_ITEM) {
        return;
    }

    // V2LogOrAggregate owns the class counter bump (s_nUpLinks = ok + skip).
    FILE * pFile = V2LogOrAggregate(&s_nUpLinks);
    if (nReason) {
        s_nUpLinkSkips++;
    }

    if (!pFile) {
        return;
    }

    const D2StaticPathStrc * pPath = pUnit->pPath.pStaticPath;
    fprintf(pFile, "V2 UPLINK guid=%08x res=%d room=%p bRNU=%d islink=%d",
            (unsigned)pUnit->nUnitGUID, nReason,
            (const void *)(pPath ? pPath->pRoom : nullptr),
            (pPath && pPath->bRoomNeedsUpdate) ? 1 : 0,
            (pUnit->eUnitFlags & UNITFLAG_ISLINKREFRESHMSG) ? 1 : 0);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteDispatchUnitUpdate(const void * pVoidUnit, const void * pVoidClient) {
    const D2UnitStrc * pUnit = (const D2UnitStrc *)pVoidUnit;
    if (!pUnit || pUnit->eUnitType != UNIT_ITEM) {
        return;
    }

    s_nDispatchItems++;
    if ((int)s_nDispatchItems > kCapV2) {
        if (!((s_nDispatchItems - kCapV2) % 32)) {
            FILE * pFile = OpenLog();
            if (pFile) {
                fprintf(pFile, "V2 AGG dispatch cap, tick=%u", GetTickCount());
                WriteAggregates(pFile);
                fprintf(pFile, "\n");
                fclose(pFile);
            }
        }

        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "V2 DISPATCH guid=%08x initseed=%d client=%p",
            (unsigned)pUnit->nUnitGUID,
            (pUnit->eUnitFlags & UNITFLAG_INITSEEDSET) ? 1 : 0, pVoidClient);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteInitPacketItemCase(const void * pVoidUnit) {
    const D2UnitStrc * pUnit = (const D2UnitStrc *)pVoidUnit;
    if (!pUnit) {
        return;
    }

    BOOL bGateOpen = pUnit->eUnitFlags >> 4 & 1;

    // V2LogOrAggregate owns the counter bump (open and closed cap together).
    FILE * pFile = V2LogOrAggregate(bGateOpen ? &s_nInitPktItemOpen : &s_nInitPktItemClosed);
    if (!pFile) {
        return;
    }

    fprintf(pFile, "V2 INITPKT guid=%08x gate=%d mode=%d refresh=%d",
            (unsigned)pUnit->nUnitGUID, (int)bGateOpen,
            (int)pUnit->eAnimMode.ePlayerMode,
            (pUnit->eUnitFlags & UNITFLAG_SENDREFRESHMSG) ? 1 : 0);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteItemRecycle(const void * pVoidUnit, int nStage) {
    const D2UnitStrc * pUnit = (const D2UnitStrc *)pVoidUnit;
    if (!pUnit || pUnit->eUnitType != UNIT_ITEM) {
        return;
    }

    uint32_t * pnCounter = nStage ? &s_nItemFree : &s_nItemReuse;
    (*pnCounter)++;
    if ((int)*pnCounter > kCapV2) {
        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "V2 %s guid=%08x mode=%d islink=%d",
            nStage ? "FREE" : "RECYCLE", (unsigned)pUnit->nUnitGUID,
            (int)pUnit->eAnimMode.ePlayerMode,
            (pUnit->eUnitFlags & UNITFLAG_ISLINKREFRESHMSG) ? 1 : 0);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteClient0x9C(const uint8_t * pBytes) {
    if (!pBytes) {
        return;
    }

    s_nClientRx++;
    if ((int)s_nClientRx > kCapV2) {
        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    // 0x9C layout: [0]=0x9C, [1]=action subcode, [2..5]=item GUID dword.
    fprintf(pFile, "V2 CRX9C sub=%d guid=%08x",
            (int)pBytes[1], (unsigned)*(const uint32_t *)&pBytes[2]);
    WriteAggregates(pFile);
    fprintf(pFile, "\n");
    fclose(pFile);
}

void __stdcall NoteChestTableBuildStart() {
    s_bChestBuildStarted = true;
    s_nChestBuildSlot = 0;
}

void __stdcall NoteChestTableLookup(const char * szTCName, int nTCIdx) {
    if (!s_bChestBuildStarted || s_nChestBuildSlot >= 45) {
        return;
    }

    FILE * pFile = OpenLog();
    if (!pFile) {
        return;
    }

    fprintf(pFile, "V2 CHESTBUILD [%d] \"%s\" -> %d\n",
            s_nChestBuildSlot, szTCName ? szTCName : "(null)", nTCIdx);
    fclose(pFile);
    s_nChestBuildSlot++;
}

} // namespace D2Common::DATATBLS::DropDiag
