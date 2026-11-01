// vanilla-plus shared stash: server-side container (track N, round 1).
//
// Fork-owned TU (picked up by CMake's GLOB_RECURSE). Do not move this code
// into upstream files; upstream files only ever call the three entry points
// declared in VanillaPlusStash.h from thin VP-marked sites
// (PlayerTrade::ClickButton, PlayerSave::SaveGameAllGameTypes).
//
// ---------------------------------------------------------------------------
// How the shared page works (round 1 - one page, retail-shaped):
//
// While the shared page is ACTIVE, the player's INVPAGE_STASH (page 4)
// inventory holds the shared set - real item units materialized from the
// .sss file by the retail save-stream loader - and the personal set waits
// in a memory snapshot. Every retail stash flow works on it unchanged:
// grid draw, C->S 0x19 pick, C->S 0x18 place, swap 0x1F, gold buttons.
// The fork adds no packets, no item fields, no container code.
//
// Toggling the page (client tab -> C->S 0x4F button 0x7D):
//   1. serialize the current page-4 set with the retail per-item encoder
//      (D2Common::ITEMS::Items::SerializeItem) into a 'JM' block - the
//      exact bytes the .d2s item block uses;
//   2. dematerialize those units (ITEM_RemoveItemFromCurrentLocation,
//      which also pushes the client 0x9D remove packets);
//   3. materialize the other set (PlayerSave::PLRSAVE_DispatchItemLoadByFormat,
//      format 0x60 - the same call the trade-cancel restore uses);
//   4. push the client 0x9C put-in-container packets for the new set;
//   5. swap the goldbank borrow (see below); on shared->personal also
//      write the .sss file.
//
// Shared gold borrows the retail UNITSTAT_goldbank stat while the shared
// page is active: the retail stash-gold deposit (0x4F/0x14) and withdraw
// (0x4F/0x13) buttons and the stash gold display keep working verbatim,
// but move shared gold. The toggle swaps goldbank between the personal
// snapshot and the shared pool; the save flush absorbs whatever the retail
// buttons moved back into the .sss.
//
// .d2s safety (owner constraint 2, invariant I1): the shared set never
// survives into a character save. VPSHARED_FlushBeforeSave runs at the top
// of PlayerSave::SaveGameAllGameTypes - the single dispatcher every save
// path (SP file, LAN buffer, BNet) goes through - and parks the shared
// page (write .sss, restore the personal set and the personal goldbank)
// BEFORE MakeSaveFile serializes. When the page is inactive the flush is a
// zero-op: no file IO, no marks, vanilla byte-for-byte.
//
// .sss format (<save dir>\_d2vp_shared.sss, own file, owner constraint 1):
//   0   4   magic 'V','P','S','1'
//   4   2   version (u16, 1)
//   6   2   pageCount (u16, 1 - future 10/100 pages extend this, v2)
//   8   4   shared gold (u32)
//   12  4   blockSize (u32, bytes of the page-1 'JM' block that follows)
//   16  4   checksum (u32, additive byte-sum of the block bytes)
//   20  ..  page-1 item block: 'JM' + u16 count + retail item records
// Write is atomic (temp file + MoveFileExA replace) with a one-generation
// .bak kept behind it. Every failure degrades to a d2boot mark, never
// halts; a failed/degraded read never touches the file on disk.
//
// Scope guard: [features] shared_stash=1 in d2vp.ini AND
// pGame->nGameType == SGAMETYPE_SINGLEPLAYER. Other game types never
// engage: the intercept is false, the flush is a zero-op.
// ---------------------------------------------------------------------------

#include "D2Game/PLAYER/VanillaPlusStash.h"

#include "D2Client/D2ClientStrc.h"
#include "D2Client/GAME/Game/SGAMEDATA.h"
#include "D2Client/GAME/SCmd.h"
#include "D2Client/UI/VanillaPlusCore.h" // ALTTOGGLE_BuildIniPath (shared ini path helper)
#include "D2Common/D2InventoryStrc.h"
#include "D2Common/D2StatListExStrc.h"
#include "D2Common/D2UnitStrc.h"
#include "D2Common/INVENTORY/Inventory.h"
#include "D2Common/ITEMS/Items.h"
#include "D2Common/Stats/StatsEx.h"
#include "D2Common/Unit.h"
#include "D2Game/GAME/SCmd.h"
#include "D2Game/ITEMS/ItemMode.h"
#include "D2Game/PLAYER/PlayerSave.h"
#include "D2Game/PLAYER/PlayerSave2.h"
#include "D2Game/UNIT/Party.h"
#include "D2Game/UNIT/SUnit.h"
#include "Fog/D2PoolManagerStrc.h"
#include "Fog/Memory.h"
#include "Fog/Source/SFile.h"
#include "globals/globals.h"
#include "support/d2_boot.h" // [d2boot]

#include <windows.h>
#include <string.h>
#include <stdio.h>

// Pool allocators - same #ifndef fallback shape as PlayerTrade.cpp (the
// macros live with their consumers, not in a common header).
#ifndef D2_ALLOC_POOL_AT
#define D2_ALLOC_POOL_AT(pPool, size, szFile, nLine) Fog::Memory::AllocServerMemory((pPool), (size), (szFile), (nLine), 0)
#endif

#ifndef D2_FREE_POOL_AT
#define D2_FREE_POOL_AT(pPool, p, szFile, nLine) Fog::Memory::FreeServerMemory((pPool), (p), (szFile), (nLine), 0)
#endif

// ---------------------------------------------------------------------------
// Tunables

// Per-page serialization cap. The retail stash grid cannot hold more than
// 64 items even in pathological layouts (v1 page = the retail stash grid);
// a load claiming more is treated as a corrupt count (clamped, marked).
constexpr int VPSTASH_PAGE_ITEM_CAP = 64;
// 'JM' block buffer cap. The retail trade copy uses 0x2000; 0x4000 gives
// 64 worst-case item records generous headroom.
constexpr uint32_t VPSTASH_BLOCK_CAP = 0x4000;
// .sss header size (see format sketch above).
constexpr uint32_t VPSTASH_HEADER_SIZE = 20;
constexpr uint16_t VPSTASH_FILE_VERSION = 1;
constexpr uint16_t VPSTASH_PAGE_COUNT_V1 = 1;

// ---------------------------------------------------------------------------
// Per-game state. v1 is single-player: one game per process. Ownership is
// pinned to the SGAMEDATA pointer - the first touch from a different game
// resets the state (the old game's memory pool is gone by then; the stale
// pointers are dropped, never dereferenced).

struct VPStashState {
    ::SGAMEDATA * pOwnerGame;   // owning game (ownership pin / lazy reset)
    int gbSharedActive;         // 1 while page 4 holds the shared set
    int gbSharedLoaded;         // 1 once the .sss was read this game (success or degrade)
    uint32_t dwSharedGold;      // shared gold pool (mirrored in the .sss header)
    uint32_t dwPersonalGoldSnapshot; // goldbank value at toggle-to-shared
    uint8_t * pPersonalBlock;   // 'JM' block of the personal set while shared is active
    uint32_t nPersonalSize;
    uint8_t * pSharedBlock;     // 'JM' block of the shared set (loaded / last serialized)
    uint32_t nSharedSize;
};

// Plain BSS static (zero-init by definition): no guard macro needed - the
// state holds no code pointers and the ownership pin keeps stale pools from
// ever being dereferenced.
static VPStashState s_VpStash = {};

// [vanilla-plus shared-stash] ini gate, server side. Same file, same key,
// same 2 s hot-read cadence as the client gate (the SP server runs in the
// same process, so the exe dir is identical). Missing/garbage = 0 = inert.
static int s_nVpStashGateCache = 0;
static DWORD s_dwVpStashGateTick = 0;

static BOOL VPSHARED_FeatureActive() {
    DWORD dwNow = GetTickCount();
    if (!s_dwVpStashGateTick || dwNow - s_dwVpStashGateTick >= 2000) {
        char szIniPath[260];
        s_dwVpStashGateTick = dwNow;
        if (ALTTOGGLE_BuildIniPath(szIniPath)) {
            // Missing file/section/key or a non-numeric value = 0 = inert.
            s_nVpStashGateCache = GetPrivateProfileIntA("features", "shared_stash", 0, szIniPath) ? 1 : 0;
        } else {
            s_nVpStashGateCache = 0;
        }
    }
    return s_nVpStashGateCache;
}

// Drop the state if it belongs to a previous game (its memory pool was
// freed at teardown; the pointers are stale and must never be touched).
static void VPSHARED_EnsureOwner(::SGAMEDATA * pGame) {
    if (s_VpStash.pOwnerGame == pGame) {
        return;
    }
    s_VpStash = {};
    s_VpStash.pOwnerGame = pGame;
}

static BOOL VPSHARED_IsSinglePlayerGame(::SGAMEDATA * pGame) {
    return pGame && pGame->nGameType == SGAMETYPE_SINGLEPLAYER;
}

// ---------------------------------------------------------------------------
// File layer

static void VPSHARED_BuildFilePath(char * szOut, size_t nSize) {
    char szSaveDir[1024];
    szOut[0] = '\0';
    Fog::Source::SFile::GetAndCreateSavePath(szSaveDir, 0x400);
    sprintf(szOut, "%s_d2vp_shared.sss", szSaveDir);
}

// Additive byte-sum, the .d2s checksum convention.
static uint32_t VPSHADOW_BlockChecksum(const uint8_t * pBytes, uint32_t nSize) {
    uint32_t dwSum = 0;
    for (uint32_t i = 0; i < nSize; ++i) {
        dwSum += pBytes[i];
    }
    return dwSum;
}

// Validate + ingest one file (path given). Returns 1 with s_VpStash shared
// block/gold filled, 0 to start empty (file left untouched on disk).
static int VPSHARED_TryReadFile(const char * szPath) {
    HANDLE hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        D2BOOT_Mark("shared stash: no file - starting empty"); // [d2boot]
        return 0;
    }

    uint8_t aHeader[VPSTASH_HEADER_SIZE];
    DWORD dwRead = 0;
    int nOk = 0;
    if (ReadFile(hFile, aHeader, VPSTASH_HEADER_SIZE, &dwRead, nullptr) && dwRead == VPSTASH_HEADER_SIZE) {
        const uint8_t aMagic[4] = { 'V', 'P', 'S', '1' };
        uint16_t nVersion = (uint16_t)(aHeader[4] | (aHeader[5] << 8));
        uint16_t nPages = (uint16_t)(aHeader[6] | (aHeader[7] << 8));
        uint32_t dwGold = *(uint32_t *)&aHeader[8];
        uint32_t dwBlockSize = *(uint32_t *)&aHeader[12];
        uint32_t dwChecksum = *(uint32_t *)&aHeader[16];
        LARGE_INTEGER liSize;
        if (memcmp(aHeader, aMagic, 4) != 0 || nVersion != VPSTASH_FILE_VERSION || nPages != VPSTASH_PAGE_COUNT_V1) {
            D2BOOT_Mark("shared stash: bad header (magic/version) - starting empty"); // [d2boot]
        } else if (!GetFileSizeEx(hFile, &liSize) ||
                   VPSTASH_HEADER_SIZE + dwBlockSize > (uint32_t)liSize.QuadPart ||
                   dwBlockSize > VPSTASH_BLOCK_CAP) {
            D2BOOT_Mark("shared stash: bad header (size) - starting empty"); // [d2boot]
        } else if (dwBlockSize < 4) {
            // 'JM' + count alone: a legitimately empty page.
            s_VpStash.dwSharedGold = dwGold;
            s_VpStash.nSharedSize = dwBlockSize;
            D2BOOT_Mark("shared stash: loaded 0 items (empty page)"); // [d2boot]
            nOk = 1;
        } else {
            // Read the block straight into the pool buffer.
            uint8_t * pBlock = (uint8_t *)D2_ALLOC_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, VPSTASH_BLOCK_CAP,
                                                           ".\\PLAYER\\VanillaPlusStash.cpp", 0x101);
            LARGE_INTEGER liAt;
            liAt.QuadPart = VPSTASH_HEADER_SIZE;
            DWORD dwGot = 0; // DWORD, not uint32_t: ReadFile's out-param is LPDWORD (win32 'long'), mingw32 rejects unsigned int*
            if (pBlock && SetFilePointerEx(hFile, liAt, nullptr, FILE_BEGIN) &&
                ReadFile(hFile, pBlock, dwBlockSize, &dwGot, nullptr) && dwGot == dwBlockSize) {
                if (VPSHADOW_BlockChecksum(pBlock, dwBlockSize) != dwChecksum) {
                    D2_FREE_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, pBlock, ".\\PLAYER\\VanillaPlusStash.cpp", 0x10b);
                    D2BOOT_Mark("shared stash: checksum fail - file - starting empty"); // [d2boot]
                } else if (pBlock[0] != 'J' || pBlock[1] != 'M') {
                    D2_FREE_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, pBlock, ".\\PLAYER\\VanillaPlusStash.cpp", 0x10f);
                    D2BOOT_Mark("shared stash: bad item block header - starting empty"); // [d2boot]
                } else {
                    uint16_t nCount = (uint16_t)(pBlock[2] | (pBlock[3] << 8));
                    if (nCount > VPSTASH_PAGE_ITEM_CAP) {
                        // Clamp in memory only - the on-disk file stays verbatim.
                        D2BOOT_MarkFmt("shared stash: item count %d out of range - clamped", (int)nCount); // [d2boot]
                        pBlock[2] = (uint8_t)VPSTASH_PAGE_ITEM_CAP;
                        pBlock[3] = 0;
                    }
                    if (s_VpStash.pSharedBlock) {
                        D2_FREE_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, s_VpStash.pSharedBlock,
                                        ".\\PLAYER\\VanillaPlusStash.cpp", 0x118);
                    }
                    s_VpStash.pSharedBlock = pBlock;
                    s_VpStash.nSharedSize = dwBlockSize;
                    s_VpStash.dwSharedGold = dwGold;
                    D2BOOT_MarkFmt("shared stash: loaded %d items (gold %d)", (int)nCount, (int)dwGold); // [d2boot]
                    nOk = 1;
                }
            } else {
                if (pBlock) {
                    D2_FREE_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, pBlock, ".\\PLAYER\\VanillaPlusStash.cpp", 0x121);
                }
                D2BOOT_Mark("shared stash: read failed - starting empty"); // [d2boot]
            }
        }
    } else {
        D2BOOT_Mark("shared stash: header read failed - starting empty"); // [d2boot]
    }

    CloseHandle(hFile);
    return nOk;
}

// Lazy read of the shared file. One attempt per game: a degraded read keeps
// its verdict for the session (the on-disk file is never touched by a
// misread, so nothing is destroyed and the owner can inspect it).
static void VPSHARED_EnsureLoaded() {
    if (s_VpStash.gbSharedLoaded) {
        return;
    }
    s_VpStash.gbSharedLoaded = 1;

    char szPath[1100];
    VPSHARED_BuildFilePath(szPath, sizeof(szPath));
    if (VPSHARED_TryReadFile(szPath)) {
        return;
    }
    // Primary failed (or missing): retry once with the .bak generation.
    char szBak[1100];
    sprintf(szBak, "%s.bak", szPath);
    VPSHARED_TryReadFile(szBak);
}

// Atomic write: full temp file, one-generation .bak of the old file, then
// MoveFileExA replace. Any failure leaves the old file + .bak untouched and
// keeps the session memory copy (mark says which).
static void VPSHARED_WriteFileLocked() {
    char szPath[1100];
    VPSHARED_BuildFilePath(szPath, sizeof(szPath));

    uint32_t dwBlockSize = s_VpStash.nSharedSize;
    uint32_t dwTotal = VPSTASH_HEADER_SIZE + dwBlockSize;
    uint8_t * pOut = (uint8_t *)D2_ALLOC_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, dwTotal,
                                                 ".\\PLAYER\\VanillaPlusStash.cpp", 0x13b);
    if (!pOut) {
        D2BOOT_Mark("shared stash: write failed (alloc) - keeping memory copy"); // [d2boot]
        return;
    }

    pOut[0] = 'V'; pOut[1] = 'P'; pOut[2] = 'S'; pOut[3] = '1';
    pOut[4] = (uint8_t)(VPSTASH_FILE_VERSION & 0xff);
    pOut[5] = (uint8_t)(VPSTASH_FILE_VERSION >> 8);
    pOut[6] = (uint8_t)(VPSTASH_PAGE_COUNT_V1 & 0xff);
    pOut[7] = (uint8_t)(VPSTASH_PAGE_COUNT_V1 >> 8);
    *(uint32_t *)&pOut[8] = s_VpStash.dwSharedGold;
    *(uint32_t *)&pOut[12] = dwBlockSize;
    *(uint32_t *)&pOut[16] = dwBlockSize ? VPSHADOW_BlockChecksum(s_VpStash.pSharedBlock, dwBlockSize) : 0;
    if (dwBlockSize) {
        memcpy(pOut + VPSTASH_HEADER_SIZE, s_VpStash.pSharedBlock, dwBlockSize);
    }

    char szTmp[1100];
    char szBak[1100];
    sprintf(szTmp, "%s.tmp", szPath);
    sprintf(szBak, "%s.bak", szPath);

    HANDLE hFile = CreateFileA(szTmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        D2BOOT_MarkFmt("shared stash: write failed (%d) - keeping memory copy", (int)GetLastError()); // [d2boot]
        D2_FREE_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, pOut, ".\\PLAYER\\VanillaPlusStash.cpp", 0x155);
        return;
    }

    DWORD dwWritten = 0;
    int nFailed = (!WriteFile(hFile, pOut, dwTotal, &dwWritten, nullptr) || dwWritten != dwTotal);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    D2_FREE_POOL_AT(s_VpStash.pOwnerGame->pMemoryPool, pOut, ".\\PLAYER\\VanillaPlusStash.cpp", 0x161);
    if (nFailed) {
        D2BOOT_MarkFmt("shared stash: write failed (%d) - keeping memory copy", (int)GetLastError()); // [d2boot]
        return;
    }

    // One-generation .bak of the previous file (best effort - a missing
    // previous file is the normal first-save case).
    CopyFileA(szPath, szBak, FALSE);
    if (!MoveFileExA(szTmp, szPath, MOVEFILE_REPLACE_EXISTING)) {
        D2BOOT_MarkFmt("shared stash: write failed (rename %d) - keeping memory copy", (int)GetLastError()); // [d2boot]
        return;
    }

    D2BOOT_MarkFmt("shared stash: saved (%d bytes, gold %d)", (int)dwTotal, (int)s_VpStash.dwSharedGold); // [d2boot]
}

// ---------------------------------------------------------------------------
// Page-4 serialization / dematerialization / materialization

// Serialize the player's current INVPAGE_STASH set into a fresh pool buffer
// using the retail per-item encoder (the .d2s item-block bytes). Returns 1
// with (*ppOut, *pnSize) set (null/0 for an empty page), 0 on failure.
static int VPSHARED_SerializePage4(::SGAMEDATA * pGame, D2UnitStrc * pUnit, uint8_t ** ppOut, uint32_t * pnSize) {
    *ppOut = nullptr;
    *pnSize = 0;
    if (!pUnit || !pUnit->pInventory) {
        return 1;
    }

    uint8_t * pBlock = (uint8_t *)D2_ALLOC_POOL_AT(pGame->pMemoryPool, VPSTASH_BLOCK_CAP,
                                                   ".\\PLAYER\\VanillaPlusStash.cpp", 0x17c);
    if (!pBlock) {
        D2BOOT_Mark("shared stash: page serialize failed (alloc)"); // [d2boot]
        return 0;
    }

    pBlock[0] = 'J';
    pBlock[1] = 'M';
    uint16_t nCount = 0;
    uint8_t * pCursor = pBlock + 4;
    uint8_t * pEnd = pBlock + VPSTASH_BLOCK_CAP;

    for (D2UnitStrc * pNode = D2Common::INVENTORY::Inventory::INVENTORY_GetFirstItem(pUnit->pInventory); pNode;
         pNode = D2Common::INVENTORY::Inventory::INVENTORY_GetNextItem(pNode)) {
        D2UnitStrc * pCarried = D2Common::INVENTORY::Inventory::GetCarriedItem(pNode);
        if (!pCarried || pCarried->eUnitType != UNIT_ITEM) {
            continue;
        }
        if (D2Common::ITEMS::Items::ITEM_GetContainingInventoryPage(pCarried) != INVPAGE_STASH) {
            continue;
        }
        if (nCount >= VPSTASH_PAGE_ITEM_CAP) {
            D2BOOT_Mark("shared stash: page item cap reached - clamped"); // [d2boot]
            break;
        }
        // Retail per-item encoder, same call shape as the .d2s writer
        // (SAVE_SerializeInventoryItems): own statlist marker, all items.
        int nWritten = D2Common::ITEMS::Items::SerializeItem(pCarried, pCursor, (size_t)(pEnd - pCursor),
                                                             (D2StatListExStrc *)0x1, 1, 0);
        if (nWritten < 0) {
            D2BOOT_Mark("shared stash: page serialize failed (encoder)"); // [d2boot]
            D2_FREE_POOL_AT(pGame->pMemoryPool, pBlock, ".\\PLAYER\\VanillaPlusStash.cpp", 0x19c);
            return 0;
        }
        pCursor += nWritten;
        ++nCount;
    }

    pBlock[2] = (uint8_t)(nCount & 0xff);
    pBlock[3] = (uint8_t)(nCount >> 8);
    *ppOut = pBlock;
    *pnSize = (uint32_t)(pCursor - pBlock);
    return 1;
}

// Snapshot the page-4 item units into a fixed array (max VPSTASH_PAGE_ITEM_CAP).
static int VPSHARED_CollectPage4(D2UnitStrc * pUnit, D2UnitStrc ** apItems, int nMax) {
    int nItems = 0;
    if (!pUnit || !pUnit->pInventory) {
        return 0;
    }
    for (D2UnitStrc * pNode = D2Common::INVENTORY::Inventory::INVENTORY_GetFirstItem(pUnit->pInventory); pNode;
         pNode = D2Common::INVENTORY::Inventory::INVENTORY_GetNextItem(pNode)) {
        D2UnitStrc * pCarried = D2Common::INVENTORY::Inventory::GetCarriedItem(pNode);
        if (!pCarried || pCarried->eUnitType != UNIT_ITEM) {
            continue;
        }
        if (D2Common::ITEMS::Items::ITEM_GetContainingInventoryPage(pCarried) != INVPAGE_STASH) {
            continue;
        }
        if (nItems >= nMax) {
            break;
        }
        apItems[nItems++] = pCarried;
    }
    return nItems;
}

// Dematerialize: detach + free every page-4 unit. The removal helper also
// sends the client 0x9D remove packets, so the grid empties on screen.
static void VPSHARED_DematerializePage4(::SGAMEDATA * pGame, D2UnitStrc * pUnit) {
    D2UnitStrc * apItems[VPSTASH_PAGE_ITEM_CAP];
    int nItems = VPSHARED_CollectPage4(pUnit, apItems, VPSTASH_PAGE_ITEM_CAP);
    for (int i = 0; i < nItems; ++i) {
        // Frees the unit (ITEM_DeleteItemAndFreeUnit) and notifies the client.
        D2Game::ITEMS::ItemMode::ITEM_RemoveItemFromCurrentLocation(pGame, pUnit, apItems[i]);
    }
}

// Materialize a 'JM' block into the (now empty) page 4 and push the client
// 0x9C put-in-container packets for everything that landed.
static void VPSHARED_MaterializePage4(::SGAMEDATA * pGame, D2UnitStrc * pUnit, uint8_t * pBlock, uint32_t nSize) {
    if (!pBlock || nSize < 4) {
        return;
    }

    short int * pAfter = nullptr;
    // Format 0x60 (the 1.10+ .d2s item-list version), no page handler - the
    // same restore call the trade-cancel backup uses. Placement comes from
    // each record's own location bits (stash + x/y), retail decode.
    int nResult = D2Game::PLAYER::PlayerSave::PLRSAVE_DispatchItemLoadByFormat(pGame, pUnit, pBlock, 0x60, (int32_t)nSize, 0, &pAfter);
    if (nResult) {
        D2BOOT_MarkFmt("shared stash: load error %d - partial page", nResult); // [d2boot]
        return;
    }

    D2ClientStrc * pClient = D2Game::UNIT::SUnit::SUNIT_GetClient(pUnit, ".\\PLAYER\\VanillaPlusStash.cpp", 0x1d5);
    if (!pClient) {
        return; // No client to resync (should not happen in SP) - state is still correct.
    }

    D2UnitStrc * apItems[VPSTASH_PAGE_ITEM_CAP];
    int nItems = VPSHARED_CollectPage4(pUnit, apItems, VPSTASH_PAGE_ITEM_CAP);
    // The standard full-flag complement every retail put-in-container send
    // uses (Player.cpp / UNIT/Item.cpp).
    for (int i = 0; i < nItems; ++i) {
        D2Game::GAME::SCmd::Send0x9CItemPutInContainer(pClient, pUnit, apItems[i],
            ~(ITEMFLAG_TAGGED | ITEMFLAG_DROW | ITEMFLAG_CURSED | ITEMFLAG_STAFFMODS | ITEMFLAG_MAGICAL |
              ITEMFLAG_RUNEWORD | ITEMFLAG_CRUDE | ITEMFLAG_PLAYERNAME | ITEMFLAG_SAVED | ITEMFLAG_ETHEREAL |
              ITEMFLAG_COMPACT | ITEMFLAG_100000 | ITEMFLAG_SERVER | ITEMFLAG_RESTRICT | ITEMFLAG_BEGINNER |
              ITEMFLAG_BODYPART | ITEMFLAG_HARDCORE | ITEMFLAG_DISABLED | ITEMFLAG_NEW | ITEMFLAG_MONSTER |
              ITEMFLAG_SOCKETED | ITEMFLAG_SORTED | ITEMFLAG_RESTORED | ITEMFLAG_BROKEN | ITEMFLAG_TAKEN |
              ITEMFLAG_ADDED | ITEMFLAG_REMOVED | ITEMFLAG_IDENTIFIED | ITEMFLAG_IGNORE | ITEMFLAG_CURSOR |
              ITEMFLAG_BOUGHT | ITEMFLAG_RELOAD));
    }
}

// ---------------------------------------------------------------------------
// Page toggle (the whole feature)

static void VPSHARED_ToggleToShared(::SGAMEDATA * pGame, D2UnitStrc * pUnit) {
    if (s_VpStash.gbSharedActive) {
        return; // Idempotent: the client may repeat the request.
    }

    VPSHARED_EnsureLoaded();

    // 1. Snapshot the personal set FIRST - it is the only copy while the
    //    shared page is active (it is NOT in the .d2s until the next save).
    uint8_t * pPersonal = nullptr;
    uint32_t nPersonal = 0;
    if (!VPSHARED_SerializePage4(pGame, pUnit, &pPersonal, &nPersonal)) {
        return; // Degrade: keep the personal page live and untouched.
    }

    // 2. Park the personal set, then materialize the shared set.
    VPSHARED_DematerializePage4(pGame, pUnit);
    if (s_VpStash.pPersonalBlock) {
        D2_FREE_POOL_AT(pGame->pMemoryPool, s_VpStash.pPersonalBlock, ".\\PLAYER\\VanillaPlusStash.cpp", 0x1fe);
    }
    s_VpStash.pPersonalBlock = pPersonal;
    s_VpStash.nPersonalSize = nPersonal;
    VPSHARED_MaterializePage4(pGame, pUnit, s_VpStash.pSharedBlock, s_VpStash.nSharedSize);

    // 3. Goldbank borrow: park the personal bank, show the shared pool.
    uint32_t dwBank = D2Common::Stats::StatsEx::STATSEX_GetCurrentStat(pUnit, UNITSTAT_goldbank, 0);
    s_VpStash.dwPersonalGoldSnapshot = dwBank;
    int32_t nDelta = (int32_t)s_VpStash.dwSharedGold - (int32_t)dwBank;
    if (nDelta) {
        D2Game::UNIT::Party::ChangeGoldForPlayer(pUnit, UNITSTAT_goldbank, nDelta);
    }

    s_VpStash.gbSharedActive = 1;
    D2BOOT_MarkFmt("shared stash: shared page active (%d items, gold %d)",
                   (int)(s_VpStash.pSharedBlock && s_VpStash.nSharedSize >= 4 ? (uint16_t)(s_VpStash.pSharedBlock[2] | (s_VpStash.pSharedBlock[3] << 8)) : 0),
                   (int)s_VpStash.dwSharedGold); // [d2boot]
}

static void VPSHARED_ToggleToPersonal(::SGAMEDATA * pGame, D2UnitStrc * pUnit) {
    if (!s_VpStash.gbSharedActive) {
        return;
    }

    // 1. Serialize the shared set AS IT STANDS NOW (session changes included)
    //    into a scratch block; swap it in only on success so a failed write
    //    can never poison the in-memory shared copy.
    uint8_t * pShared = nullptr;
    uint32_t nShared = 0;
    if (!VPSHARED_SerializePage4(pGame, pUnit, &pShared, &nShared)) {
        return; // Degrade: keep the shared page live and untouched.
    }

    // 2. Park the shared set, restore the personal set.
    VPSHARED_DematerializePage4(pGame, pUnit);
    if (s_VpStash.pSharedBlock) {
        D2_FREE_POOL_AT(pGame->pMemoryPool, s_VpStash.pSharedBlock, ".\\PLAYER\\VanillaPlusStash.cpp", 0x21d);
    }
    s_VpStash.pSharedBlock = pShared;
    s_VpStash.nSharedSize = nShared;
    VPSHARED_MaterializePage4(pGame, pUnit, s_VpStash.pPersonalBlock, s_VpStash.nPersonalSize);

    // 3. Un-borrow the goldbank: absorb what the retail gold buttons moved.
    uint32_t dwBank = D2Common::Stats::StatsEx::STATSEX_GetCurrentStat(pUnit, UNITSTAT_goldbank, 0);
    s_VpStash.dwSharedGold = dwBank;
    int32_t nDelta = (int32_t)s_VpStash.dwPersonalGoldSnapshot - (int32_t)dwBank;
    if (nDelta) {
        D2Game::UNIT::Party::ChangeGoldForPlayer(pUnit, UNITSTAT_goldbank, nDelta);
    }

    s_VpStash.gbSharedActive = 0;
    VPSHARED_WriteFileLocked();
    D2BOOT_MarkFmt("shared stash: personal page restored (%d items)",
                   (int)(s_VpStash.pPersonalBlock && s_VpStash.nPersonalSize >= 4 ? (uint16_t)(s_VpStash.pPersonalBlock[2] | (s_VpStash.pPersonalBlock[3] << 8)) : 0)); // [d2boot]
}

// ---------------------------------------------------------------------------
// Entry points (see VanillaPlusStash.h)

BOOL __stdcall VPSHARED_ShouldInterceptClickButton(::SGAMEDATA * pGame, D2UnitStrc * pUnit, int16_t nButtonId) {
    if (nButtonId != (int16_t)VPSHARED_BUTTON_TOGGLE) {
        return FALSE;
    }
    if (!pUnit || !pGame) {
        return FALSE;
    }
    if (!VPSHARED_FeatureActive()) {
        return FALSE;
    }
    if (!VPSHARED_IsSinglePlayerGame(pGame)) {
        return FALSE;
    }
    // (Game-type gate here AND in OnClickButton: the flush also refuses
    // non-SP games, so a toggle can never create state the flush would
    // not park.)
    return TRUE;
}

void __stdcall VPSHARED_OnClickButton(::SGAMEDATA * pGame, D2UnitStrc * pUnit, int16_t nButtonId, uint32_t dwParam) {
    if (!pGame || !pUnit || nButtonId != (int16_t)VPSHARED_BUTTON_TOGGLE) {
        return;
    }
    if (!VPSHARED_IsSinglePlayerGame(pGame)) {
        D2BOOT_Mark("shared stash: toggle refused (not single-player)"); // [d2boot]
        return;
    }
    VPSHARED_EnsureOwner(pGame);
    if (dwParam) {
        VPSHARED_ToggleToShared(pGame, pUnit);
    } else {
        VPSHARED_ToggleToPersonal(pGame, pUnit);
    }
}

void __stdcall VPSHARED_FlushBeforeSave(::SGAMEDATA * pGame, D2UnitStrc * pUnit) {
    // Zero-op unless state exists for this exact game AND the page is
    // active. The ini gate is deliberately NOT consulted here: once the
    // shared page is live, the personal set exists only in memory, so the
    // park must happen even if the owner flipped the key off mid-session.
    if (!pGame || !pUnit || s_VpStash.pOwnerGame != pGame || !s_VpStash.gbSharedActive) {
        return;
    }
    if (!VPSHARED_IsSinglePlayerGame(pGame)) {
        return; // Cannot happen (toggle refuses non-SP); belt and suspenders.
    }

    D2BOOT_Mark("shared stash: flush before save"); // [d2boot]
    VPSHARED_ToggleToPersonal(pGame, pUnit);
}
