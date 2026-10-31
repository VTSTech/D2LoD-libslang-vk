// vanilla-plus core gates - implementation. Fork-owned TU; see VanillaPlusCore.h
// for the charter and docs/merge-upstream.md for the merge playbook.
// Include set mirrors D2Client.cpp's (the code moved verbatim from there).

#include "D2Client/D2Client.h"
#include "D2Client/UI/VanillaPlusCore.h"
#include "D2Client/UI/VanillaPlusOptions.h" // self-check pins the row wiring
#include "Storm/_unnamespaced.h" // self-check pins the Resolution row init callback
#include "D2CMP/SRC/CelCmp.h"
#include "D2CMP/SRC/SpriteCache.h"
#include "D2Client/ClientModeInGame.h"
#include "D2Client/Core/Archive.h"
#include "D2Client/D2ClientStrc.h"
#include "D2Client/D2DialogStrc.h"
#include "D2Client/D2GfxDataStrc.h"
#include "D2Client/D2IniConfigStrc.h"
#include "D2Client/D2LightMapStrc.h"
#include "D2Client/Draw/LightMap.h"
#include "D2Client/Engine.h"
#include "D2Client/Engine/Gfx.h"
#include "D2Client/Engine/GfxUtil.h"
#include "D2Client/GAME/Game/Game.h"
#include "D2Client/GAME/Game/SGAMEDATA.h"
#include "D2Client/GAME/Msg.h"
#include "D2Client/GAME/SCmd.h"
#include "D2Client/GAME/Select.h"
#include "D2Client/GAME/Sound.h"
#include "D2Client/GAME/Wall2.h"
#include "D2Client/Message.h"
#include "D2Client/Renderer/Renderer.h"
#include "D2Client/SKILLS/SkillsPal.h"
#include "D2Client/Sound/Sound.h"
#include "D2Client/Sound/SoundHdr.h"
#include "D2Client/Source/ClientEngine.h"
#include "D2Client/UI/Automap.h"
#include "D2Client/UI/CmdTbl.h"
#include "D2Client/UI/Dialog.h"
#include "D2Client/UI/Draw.h"
#include "D2Client/UI/EscMenu.h"
#include "D2Client/UI/Inv.h"
#include "D2Client/UI/Panel.h"
#include "D2Client/UI/Path.h"
#include "D2Client/UI/QuestLog.h"
#include "D2Client/UI/SkillDesc.h"
#include "D2Client/UI/SpellSel.h"
#include "D2Client/UI/Text.h"
#include "D2Client/UI/UseItem.h"
#include "D2Client/UI/npcmenu.h"
#include "D2Client/UI/npcmenu/Merc.h"
#include "D2Client/UI/ui.h"
#include "D2Client/UNIT.h"
#include "D2Client/UNIT/CUnit.h"
#include "D2Client/UNIT/CUnitEvent.h"
#include "D2Client/UNIT/Item.h"
#include "D2Client/UNIT/MonUnique.h"
#include "D2Client/UNIT/Monster.h"
#include "D2Client/UNIT/Player.h"
#include "D2Client/UNIT/UnitMode.h"
#include "D2Client/Warden/WardenClient.h"
#include "D2Common/D2BitBufferStrc.h"
#include "D2Common/D2InventoryStrc.h"
#include "D2Common/D2MissilesTxt.h"
#include "D2Common/D2MonStatsTxt.h"
#include "D2Common/D2RoomExStrc.h"
#include "D2Common/D2RoomStrc.h"
#include "D2Common/D2SeedStrc.h"
#include "D2Common/D2SkillsTxt.h"
#include "D2Common/D2StatListExStrc.h"
#include "D2Common/D2WaypointStrc.h"
#include "D2Common/DATATBLS/LvlTbls.h"
#include "D2Common/DATATBLS/OverlayTbls.h"
#include "D2Common/DUNGEON/Dungeon.h"
#include "D2Common/Drlg/D2DrlgActStrc.h"
#include "D2Common/Drlg/D2DrlgDeleteStrc.h"
#include "D2Common/Drlg/D2DrlgStrc.h"
#include "D2Common/Drlg/Path.h"
#include "D2Common/Drlg/Preset.h"
#include "D2Common/Environment.h"
#include "D2Common/ITEMS/Items.h"
#include "D2Common/Skills.h"
#include "D2Common/Stats/StatsEx.h"
#include "D2Common/Unit/D2UnitStrc.h"
#include "D2Common/Unit.h"
#include "D2Common/Units.h"
#include "D2DDraw/Renderer/DirectDraw.h"
#include "D2Game/D2ParsedTreasureClassStrc.h"
#include "D2Game/D2UnitFindDataStrc.h"
#include "D2Game/GAME/Event.h"
#include "D2Game/GAME/Targets.h"
#include "D2Game/Objects/Objects.h"
#include "D2Game/PLAYER/Player.h"
#include "D2Game/PLAYER/PlayerTrade.h"
#include "D2Game/UNIT/PlayerList.h"
#include "D2Game/UNIT/SUnit.h"
#include "D2Game/UNIT/SUnitDmg.h"
#include "D2Game/UNIT/SUnitInactive.h"
#include "D2Game/UNIT/SUnitMsg.h"
#include "D2Lang/StrTable/strtable.h"
#include "D2Lang/Unicode/UNISYS.h"
#include "D2Launch/MainMenus.h"
#include "D2Sound/Src/D2SoundSmp.h"
#include "D2Win/D2GFX.h"
#include "D2Win/DC6.h"
#include "D2Win/DC6Block.h"
#include "D2Win/Src/D2WinArchive.h"
#include "D2Win/Src/D2WinFont.h"
#include "D2Win/Src/D2WinPalette.h"
#include "D2gfx/D2GFX.h"
#include "Fog/D2PoolManagerStrc.h"
#include "Fog/Debug.h"
#include "Fog/Fog.h"
#include "Fog/Memory.h"
#include "Fog/SMem.h"
#include "Fog/Source/SFile.h"
#include "Fog/Src/AsyncData.h"
#include "Fog/Src/ErrorManager.h"
#include "Fog/Src/LogManager.h"
#include "Storm/D2ArchiveStrc.h"
#include "Storm/D2MPQFileStrc.h"
#include "Storm/Source/SFile.h"
#include "Storm/Source/SSTR.h" // [item-ilvl] SStrPrintf for the level digits
#include "Storm/Source/SSystem.h"
#include "Storm/Storm.h"
#include "globals/globals.h"
#include "D2Client/UI/Chat.h"
#include "D2Client/UI/MiniPanel.h"
#include "D2Client/UNIT/UnitSnd.h"
#include "D2Common/DATATBLS/ItemStats.h"
#include "D2Common/Drlg/Drlg.h"
#include "D2Common/ITEMS/ItemMods.h"
#include "D2Game/GAME/Arena.h"
#include "D2Game/Monster/AI/AI.h"
#include "D2Launch/CharSel.h"
#include "D2Common/Collision.h" // [vanilla-plus auto-gold] GetDistanceBetweenUnits
#include "support/d2_boot.h" // [vanilla-plus] self-check failures
#include <stdlib.h> // exit (self-check halt)
#include <cstddef> // offsetof (flag-store tripwire)

// [vanilla-plus alt-toggle] Config-gated Alt-key ground-item-label toggle
// (D2R "Show Items: Toggle" parity). d2vp.ini next to d2.exe:
//   [features]
//   alt_toggle=1
// Default/missing file/missing section/missing key/garbage value = 0 =
// vanilla hold-Alt behavior byte-for-byte (Principle 1: opt-in, config-gated).
// The ini is re-checked at most every 2000 ms via GetTickCount, mirroring the
// wrapper's ~2 s conf hot-reload cadence. A missing d2vp.ini is created with
// a documented all-defaults template: once per process at client boot
// (ApplicationMain) and lazily before the first config read, so the opt-in
// keys are discoverable without reading any docs. Auto-repeat never reaches
// the handlers (CmdTbl.cpp filters lParam bit 30 upstream), so the keydown
// handler fires exactly once per physical press.
static int gbAltToggleMode = 0;
static int gbAltLatched = 0;
static DWORD dwAltToggleLastCheck = 0;
static int gbAltIniAttempted = 0;

// Resolve "<exe dir>\d2vp.ini". GetModuleFileNameA always yields a fully
// qualified path; refuse the (unreachable) dir-less case rather than fall
// back to a CWD-relative file.
// non-static: shared with VanillaPlusOptions.cpp's [ui] label_font reader
BOOL __stdcall ALTTOGGLE_BuildIniPath(char* szIniPath) {
    szIniPath[0] = '\0';
    DWORD dwLen = GetModuleFileNameA(nullptr, szIniPath, 240);
    if (!dwLen || dwLen >= 240) {
        return FALSE;
    }
    char* pSlash = strrchr(szIniPath, '\\');
    if (!pSlash) {
        return FALSE;
    }
    pSlash[1] = '\0';
    lstrcatA(szIniPath, "d2vp.ini");
    return TRUE;
}

// Best-effort template write. CREATE_NEW so a file that appears between the
// existence check and the create is never clobbered; any failure (read-only
// dir, AV lock, invalid path) stays silent - GetPrivateProfileIntA then keeps
// returning the vanilla defaults, exactly like a missing file. Even a partial
// write is safe: whatever GetPrivateProfileIntA can parse is a default.
static void ALTTOGGLE_WriteIniTemplate(const char* pszPath) {
    static const char szTemplate[] =
        "; d2vp.ini - vanilla-plus client options (auto-created by d2.exe)\r\n"
        ";\r\n"
        "; Missing file, section or key, or a garbage value, always falls back\r\n"
        "; to vanilla behavior. The file is re-read at most every 2 seconds\r\n"
        "; while the game runs. Delete this file and a fresh copy is written\r\n"
        "; on the next game start.\r\n"
        "\r\n"
        "[features]\r\n"
        "; Ground-item labels on the Alt key:\r\n"
        ";   0 = hold Alt to show labels         (vanilla behavior)\r\n"
        ";   1 = press Alt once to toggle labels (D2R \"Show Items: Toggle\")\r\n"
        "; While labels are toggled on, clicking still attacks, moves and\r\n"
        "; interacts normally; clicking an item's label picks the item up.\r\n"
        "alt_toggle=0\r\n"
        "\r\n"
        "; Save file location (single-player):\r\n"
        ";   0 = retail resolution: the registry save path if one is set,\r\n"
        ";       else the system Saved Games location\r\n"
        ";   1 = keep saves in the game folder, .\\save next to this file\r\n"
        ";       (the folder is created on demand, the registry is not\r\n"
        ";       touched, and existing saves are not migrated - copy your\r\n"
        ";       .d2s/.d2x/.key/.map files over to keep them)\r\n"
        "portable_save=0\r\n"
        "\r\n"
        "; Auto gold pickup (single-player focused):\r\n"
        ";   0 = off                                 (vanilla behavior)\r\n"
        ";   1 = walk over gold piles to pick them up. Uses the exact\r\n"
        ";       same server path as clicking the pile; never moves\r\n"
        ";       your character; disabled while attacking/casting;\r\n"
        ";       gold never sits on the cursor\r\n"
        "auto_gold=0\r\n"
        "\r\n"
        "; Persistent globe text:\r\n"
        ";   0 = the Life/Mana text above the orbs shows on hover only (vanilla)\r\n"
        ";   1 = keep the Life/Mana text above the orbs always visible\r\n"
        "globe_persistent=0\r\n"
        "\r\n"
        "; Item level tooltips:\r\n"
        ";   0 = item tooltips exactly as retail               (vanilla behavior)\r\n"
        ";   1 = add an \"Item Level: <ilvl>\" line to every item\r\n"
        ";       tooltip, right below the Required Level line\r\n"
        "item_ilvl=0\r\n";
    HANDLE hFile = CreateFileA(pszPath, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD dwWritten = 0;
    WriteFile(hFile, szTemplate, (DWORD)(sizeof(szTemplate) - 1), &dwWritten, nullptr);
    CloseHandle(hFile);
}

// Create d2vp.ini with documented defaults when none exists. At most one
// attempt per process: a file deleted while the game runs is recreated on
// the next game start, and a failed create (read-only dir) is not retried
// every 2 s.
void __stdcall ALTTOGGLE_EnsureIni() {
    VANILLAPLUS_SelfCheck();
    if (gbAltIniAttempted) {
        return;
    }
    gbAltIniAttempted = 1;
    char szIniPath[260];
    if (!ALTTOGGLE_BuildIniPath(szIniPath)) {
        return;
    }
    if (GetFileAttributesA(szIniPath) != INVALID_FILE_ATTRIBUTES) {
        return; // exists (or exists-but-inaccessible: never overwrite)
    }
    ALTTOGGLE_WriteIniTemplate(szIniPath);
}

BOOL __stdcall ALTTOGGLE_Active() {
    DWORD dwNow = GetTickCount();
    if (!dwAltToggleLastCheck || dwNow - dwAltToggleLastCheck >= 2000) {
        char szIniPath[260];
        dwAltToggleLastCheck = dwNow;
        if (ALTTOGGLE_BuildIniPath(szIniPath)) {
            // The first config read can precede the boot hook in odd flows
            // (e.g. non-interactive modes): create the template here too,
            // before the read, so the read always sees a parseable file.
            ALTTOGGLE_EnsureIni();
            // GetPrivateProfileIntA: missing file/section/key or a non-numeric
            // value all return the 0 default = vanilla hold mode.
            gbAltToggleMode = GetPrivateProfileIntA("features", "alt_toggle", 0, szIniPath) ? 1 : 0;
        } else {
            gbAltToggleMode = 0;
        }
    }
    return gbAltToggleMode;
}

BOOL __stdcall ALTTOGGLE_Latched() {
    return gbAltLatched;
}

void __stdcall ALTTOGGLE_Flip() {
    if (ALTTOGGLE_Active()) {
        gbAltLatched = !gbAltLatched;
    }
}

void __stdcall ALTTOGGLE_Reset() {
    gbAltLatched = 0;
}

// [vanilla-plus alt-toggle] True while labels are shown by the toggle latch
// (never true in hold mode). Vanilla's ground-label renderer (DRAW_AltItems)
// clears the selected unit every frame the cursor is not over a label, which
// turns every click into a walk while labels are on screen - survivable for a
// momentary Alt hold, fatal for a latched toggle. Toggle-latch mode hands
// selection back to the normal hover machinery instead (D2R "Show Items:
// Toggle" parity: labels stay up, clicks still attack/interact, and clicking
// an item's label or sprite picks the item up).
BOOL __stdcall ALTTOGGLE_LabelClickThrough() {
    return ALTTOGGLE_Active() && ALTTOGGLE_Latched();
}

// ===== [vanilla-plus auto-gold] =============================================
// D2R "Gold Pickup" parity. While [features] auto_gold=1 in d2vp.ini, gold
// piles near the player are picked up through the exact click path -
// INTERACT_AnyObject(UNIT_ITEM) sends the same 0x16 packet a click sends, and
// unmodified D2Game code answers with its own immediate SERVER_PickToInventory
// (gold-to-stat with retail cap semantics). Strict radius + collision parity:
// a pile is fired only when GetDistanceBetweenUnits < 5 AND
// TestCollisionBetweenInteractingUnits(0x804) is clear - the exact condition
// under which the server ITEM branch (PlayerMsg.cpp) picks instantly, so a
// fire can NEVER start the retail walk-to-item mode (the character is never
// moved by this feature). Combat guard: nothing fires outside
// Neutral/TownNeutral/Walk/Run, so attack/cast/gethit/block/sequence/
// knockback/dead frames never touch it. Missing/garbage/0 ini = 0 sends =
// vanilla byte-for-byte (charter rule 1). Single-player focused; the 0x16
// flow itself is untouched retail.

static int gbAutoGoldMode = 0;
static DWORD dwAutoGoldLastCheck = 0;   // 2 s ini re-read gate (row-28 pattern)
static DWORD dwAutoGoldLastScan = 0;    // 200 ms scan floor (UnitMode throttle style)

// Per-pile fire dedupe: 16-entry ring of (GUID, last fired tick). Keeps a
// pile that cannot be removed (gold cap full, removal in flight) from being
// re-fired on every 200 ms scan while the player stands inside the radius.
// Client-side only - no retail unit field is ever written.
static DWORD gdwAutoGoldRingGuid[16];
static DWORD gdwAutoGoldRingTick[16];
static int gnAutoGoldRingNext = 0;

BOOL __stdcall AUTO_GOLD_Active() {
    DWORD dwNow = GetTickCount();
    if (!dwAutoGoldLastCheck || dwNow - dwAutoGoldLastCheck >= 2000) {
        char szIniPath[260];
        dwAutoGoldLastCheck = dwNow;
        if (ALTTOGGLE_BuildIniPath(szIniPath)) {
            ALTTOGGLE_EnsureIni();
            // Missing file/section/key or a non-numeric value = 0 = vanilla.
            gbAutoGoldMode = GetPrivateProfileIntA("features", "auto_gold", 0, szIniPath) ? 1 : 0;
        } else {
            gbAutoGoldMode = 0;
        }
    }
    return gbAutoGoldMode;
}

// Per-frame trigger, called from DRAW_UI (UI/Draw.cpp). Guards in order; any
// fail = return with zero sends.
void __stdcall AUTO_GOLD_Tick() {
    D2RoomStrc** ppRooms;
    D2RoomStrc* pScanRoom;
    D2RoomStrc* pPlayerRoom;
    D2UnitStrc* pItem;
    D2UnitStrc* pPlayer;
    DWORD dwNow;
    DWORD dwGUID;
    int nRoomCount;
    int nRing;
    int i;
    BOOL bFired;

    if (!AUTO_GOLD_Active()) {
        return;
    }
    pPlayer = D2Client::UNIT::Player::GetPlayerUnit_UI(); // fully qualified: this TU is outside namespace D2Client (D2Client.cpp was inside)
    if (!pPlayer) {
        return;
    }
    // Combat guard: fire only from idle/walk modes.
    switch (pPlayer->eAnimMode.ePlayerMode) {
      case eD2PlayerAnimMode_ns::Neutral:
      case eD2PlayerAnimMode_ns::TownNeutral:
      case eD2PlayerAnimMode_ns::Walk:
      case eD2PlayerAnimMode_ns::Run:
        break;
      default:
        return;
    }
    dwNow = GetTickCount();
    if (dwAutoGoldLastScan && dwNow - dwAutoGoldLastScan < 200) {
        return;   // scan throttle
    }
    dwAutoGoldLastScan = dwNow;
    pPlayerRoom = D2Common::Units::UNITS_GetRoom(pPlayer);
    if (!pPlayerRoom) {
        return;
    }
    // Ground-item scan model of the label renderer (ShowItems): the player's
    // room plus its adjacent rooms, each room's pUnitList chain.
    ppRooms = nullptr;
    nRoomCount = 0;
    D2Common::DUNGEON::Dungeon::DRLGROOM_GetAdjacentRoomsList(pPlayerRoom, &ppRooms, &nRoomCount);
    if (!ppRooms || !nRoomCount) {
        return;
    }
    for (i = 0; i < nRoomCount; ++i) {
      pScanRoom = ppRooms[i];
      if (!pScanRoom) {
        continue;
      }
      for (pItem = pScanRoom->pUnitList; pItem; pItem = pItem->pRoomNext) {
        if (pItem->eUnitType != UNIT_ITEM) {
          continue;
        }
        if (!(pItem->eUnitFlagsEx >> 7 & 1)) {
          continue;   // not lying on the ground
        }
        if (pItem->eAnimMode.ePlayerMode != eD2PlayerAnimMode_ns::Run) {
          continue;   // ground-mode check (CUnit.cpp precedent)
        }
        if (D2Common::ITEMS::Items::ITEM_GetItemsTxt_szCode(pItem) != 0x20646c67) {
          continue;   // items.txt 4cc 'gld ' - gold piles only
        }
        // Strict radius: 5 is the server's own instant-pick distance.
        if (D2Common::Collision::GetDistanceBetweenUnits(pPlayer, pItem) >= 5) {
          continue;
        }
        // The server's own collision test in the same branch: with < 5 and a
        // clear 0x804 test the server always answers with an immediate pick,
        // so firing can never start its walk-to-item mode.
        if (D2Common::Units::TestCollisionBetweenInteractingUnits(pPlayer, pItem, 0x804)) {
          continue;
        }
        dwGUID = (DWORD)pItem->nUnitGUID;
        bFired = FALSE;
        for (nRing = 0; nRing < 16; ++nRing) {
          if (gdwAutoGoldRingGuid[nRing] == dwGUID && dwNow - gdwAutoGoldRingTick[nRing] < 1000) {
            bFired = TRUE;
            break;
          }
        }
        if (bFired) {
          continue;
        }
        gdwAutoGoldRingGuid[gnAutoGoldRingNext] = dwGUID;
        gdwAutoGoldRingTick[gnAutoGoldRingNext] = dwNow;
        gnAutoGoldRingNext = (gnAutoGoldRingNext + 1) & 15;
        D2Client::UNIT::UnitMode::INTERACT_AnyObject(UNIT_ITEM, dwGUID);   // the click path (0x16)
      }
    }
}

// ===== [globe-persistent] always-on orb text (FEATURES.md row 1) ============
// History: the 0.0.4 orb-face readout (patches 0013+0014) was removed after
// the first field round - retail's own PANEL_DrawPanelLayout already prints
// the same cur/max ABOVE each orb on hover (locale 0x1045/0x1046, baseline
// ScreenSizeY-0x5f, fed by its own "Show HP/MP Text" registry toggle), so
// the orb-face copy was a duplicate display. The slate row is repurposed:
// with globe_persistent=1, Panel.cpp's two orb-text draw conditions also
// fire without hover, pinning retail's above-orb Life/Mana text on (the
// consumer reads this gate directly; there is no draw code in this TU and
// nothing reads the retired 0.0.4 key a stale d2vp.ini may still carry).
static int gbGlobeTextPersistentMode = 0;
static DWORD dwGlobeTextPersistentLastCheck = 0;

BOOL __stdcall VANILLAPLUS_GlobeTextPersistent() {
    // 2 s hot-read cache (the shared gate pattern; this runs per frame).
    DWORD dwNow = GetTickCount();
    if (dwNow - dwGlobeTextPersistentLastCheck >= 2000) {
        dwGlobeTextPersistentLastCheck = dwNow;
        gbGlobeTextPersistentMode = D2VP_GetFeatureToggle("globe_persistent");
    }
    return gbGlobeTextPersistentMode;
}

// ===== [vanilla-plus item-ilvl] optional "Item Level" tooltip line ==========
// D2R "Advanced Tooltips" parity (the ilvl readout half). See VanillaPlusCore.h
// for the feature contract. Gate = the shared 2 s hot-read cache pattern
// (globe_persistent shape): tooltip rebuilds happen every frame the cursor
// dwells, so no rebuild trigger is needed - the next hover after the flip
// carries the new state.
static int gbItemIlvlMode = 0;
static DWORD dwItemIlvlLastCheck = 0;

BOOL __stdcall ITEMILVL_Active() {
    DWORD dwNow = GetTickCount();
    if (!dwItemIlvlLastCheck || dwNow - dwItemIlvlLastCheck >= 2000) {
        dwItemIlvlLastCheck = dwNow;
        gbItemIlvlMode = D2VP_GetFeatureToggle("item_ilvl");
    }
    return gbItemIlvlMode;
}

// Same line idiom as Inv.cpp's INV_FormatReqStrText: label, white code
// (locale 0xf9b), "%i" digits, line separator (locale 0xf9e). The caller
// applies the line's base color via AppendAsWideChar. OFF = empty buffer =
// the upstream appends are no-ops (AppendToBuffer copies just the terminator,
// AppendAsWideChar returns empty buffers untouched) - vanilla byte-for-byte.
void __stdcall ITEMILVL_FormatLine(D2UnitStrc * pItem, uint16_t * wszText) {
    uint16_t wszNumBuffer[10];
    char szNumBuffer[12];
    if (!wszText) {
        return;
    }
    wszText[0] = L'\0';
    if (!ITEMILVL_Active() || !pItem) {
        return;
    }
    int nItemLevel = D2Common::ITEMS::Items::ITEM_GetItemData_nItemLevel(pItem);
    if (nItemLevel < 1) {
        return;   // not an item unit / no parsed level (accessor returns 0)
    }
    // Hardcoded ASCII label: the vanilla string table has no "Item Level"
    // entry and the fork never edits MPQ data. 12 chars + terminator = 13.
    D2Lang::Unicode::UNISYS::STRING_CopyCharToWCharWithSetMaxSize(wszText, (char*)"Item Level: ", 13);
    memset(wszNumBuffer, 0, sizeof(wszNumBuffer));
    Storm::Source::SSTR::SStrPrintf(szNumBuffer, 10, "%i", nItemLevel);
    D2Lang::Unicode::UNISYS::STRING_CopyCharToWCharWithSetMaxSize(wszNumBuffer, szNumBuffer, 9);
    D2Lang::Unicode::UNISYS::AppendToBuffer(wszText, D2Lang::StrTable::strtable::GetLocaleString(0xf9b));
    D2Lang::Unicode::UNISYS::AppendToBuffer(wszText, wszNumBuffer);
    D2Lang::Unicode::UNISYS::AppendToBuffer(wszText, D2Lang::StrTable::strtable::GetLocaleString(0xf9e));
}

// ===== [vanilla-plus options page] d2vp.ini feature toggle bridge ==========
// The "Vanilla+ Options" page (D2Client/UI/VanillaPlusOptions.cpp) reads and
// flips the same [features] keys the gates consume. Reads go through the
// gate's own accessor path (path builder + template ensure + 0 default);
// writes go through WritePrivateProfileStringA - the first ini writer in the
// tree - and end with D2VP_ConfigReloaded() so every 2 s hot-read gate sees
// the change on its very next poll (same frame for the draw, <= 1 frame for
// auto_gold, next save-path resolution for portable_save). A failed write is
// silently ignored: the gates keep reading whatever the file still says,
// exactly like an external editor that lost a race.

int __stdcall D2VP_GetFeatureToggle(const char* szKey) {
    char szIniPath[260];
    ALTTOGGLE_EnsureIni();
    if (!szKey || !ALTTOGGLE_BuildIniPath(szIniPath)) {
        return 0;
    }
    // Missing file/section/key or a non-numeric value = 0 = vanilla.
    return GetPrivateProfileIntA("features", szKey, 0, szIniPath) ? 1 : 0;
}

void __stdcall D2VP_SetFeatureToggle(const char* szKey, int nValue) {
    char szIniPath[260];
    char szValue[2];
    if (!szKey || !ALTTOGGLE_BuildIniPath(szIniPath)) {
        return;
    }
    ALTTOGGLE_EnsureIni();
    szValue[0] = nValue ? '1' : '0';
    szValue[1] = '\0';
    WritePrivateProfileStringA("features", szKey, szValue, szIniPath);
    D2VP_ConfigReloaded();
}

// Bust every gate's 2 s hot-read cache so the next poll re-reads d2vp.ini.
// Called by D2VP_SetFeatureToggle after each write; exported for future
// tooling (console commands, external reload hooks).
void __stdcall D2VP_ConfigReloaded() {
    dwAltToggleLastCheck = 0;     // next ALTTOGGLE_Active() re-reads alt_toggle
    dwAutoGoldLastCheck = 0;      // next AUTO_GOLD_Active() re-reads auto_gold
    dwGlobeTextPersistentLastCheck = 0; // next VANILLAPLUS_GlobeTextPersistent() re-reads globe_persistent
    dwItemIlvlLastCheck = 0;      // next ITEMILVL_Active() re-reads item_ilvl
    Fog::Source::SFile::PORTABLESAVE_FlushConfig(); // next save-path poll re-reads portable_save
}

// ===== [resolution-mode] Route-B label primitives ===========================
// Shared with the video-options Resolution row's 1280x720 stop (see
// VanillaPlusCore.h for the wiring and the why). The builder is the same
// retail-shape 8x8 transparent DC6 the Vanilla+ row label shipped
// (VPLABEL_BuildBlankPlaceholder in VanillaPlusOptions.cpp); deliberately
// duplicated here instead of shared so the field-proven options page never
// churns when this feature evolves. Both copies freeze the SAME image:
// 24-byte DC6 header (version 6, flags 1, 1 direction, 1 frame), file-
// absolute frame-offset slot 0x1C, 0x20-byte frame header (nFlip=1, 8x8,
// nAllocSize 64, nLength 16), 16 RLE bytes encoding an all-zero 8x8 raster
// (per row one 0x80|8 transparent-run token plus the 0x80 end-of-row token).
// Keep the copies in sync byte-for-byte.

#ifndef D2_ALLOC
#define D2_ALLOC(size) Fog::Memory::AllocClientMemory((size), __FILE__, __LINE__, 0)
#endif

#ifndef D2_FREE
#define D2_FREE(p) Fog::Memory::FreeClientMemory((p), __FILE__, __LINE__, 0)
#endif

BOOL __stdcall VPRES_BuildBlankPlaceholder(struct DC6** ppOut) {
    *ppOut = nullptr;
    static const uint8_t gaBlankRle[16] = {
        0x88, 0x80, 0x88, 0x80, 0x88, 0x80, 0x88, 0x80,
        0x88, 0x80, 0x88, 0x80, 0x88, 0x80, 0x88, 0x80,
    };
    const int nTotalSize = 0x3C + (int)sizeof(gaBlankRle);
    DC6* pRaw = (DC6*)D2_ALLOC(nTotalSize);
    if (!pRaw) {
        return FALSE;
    }
    memset(pRaw, 0, nTotalSize);
    pRaw->nVersion = 6;       // required by the fixup (nExpectedFormat == -1)
    pRaw->nFlags = 1;         // bit0 required, bit1 clear (pre-fixup state)
    pRaw->nFormat = 0;
    /* nTerm stays zeroed */
    pRaw->nDirections = 1;
    pRaw->nFrames = 1;
    *(uint32_t*)((char*)pRaw + 0x18) = 0x1C; // file-absolute frame offset

    DC6Block* pFrame = (DC6Block*)((char*)pRaw + 0x1C);
    pFrame->nFlip = 1;        // retail frame termination value
    pFrame->nWidth = 8;
    pFrame->nHeight = 8;
    pFrame->nOffsetX = 0;
    pFrame->nOffsetY = 0;
    pFrame->nAllocSize = 8 * 8;
    pFrame->nNextBlock = 0;   // fixup patch slot: overwritten with the row ptr
    pFrame->nLength = (int)sizeof(gaBlankRle);
    memcpy((char*)pRaw + 0x3C, gaBlankRle, sizeof(gaBlankRle));

    // Retail fixup: converts the file-relative offset to an absolute pointer,
    // allocates the 1x1 block row, patches the frame's +0x18 slot. Idempotent
    // (nFlags bit1), exactly what every file-loaded DC6 goes through.
    D2CMP::SRC::CelCmp::CELCMP_FixupPointersAndPrepare(pRaw, ppOut, ".\\UI\\VANILLAPLUSCORE.CPP", 0x0, -1, "resolution stop placeholder");
    if (!*ppOut) {
        D2_FREE(pRaw);
        return FALSE;
    }
    return TRUE;
}

// Frame-0 pixel height of pDC6 (0 = not measurable). Same accessors as
// VPLABEL_MeasureRowImageHeight (VanillaPlusOptions.cpp): fixup-prepared
// DC6, GfxData for frame 0, retail block lookup, GetDC6Height.
static int VPRES_MeasureDC6FrameHeight(struct DC6* pDC6) {
    if (!pDC6) {
        return 0;
    }
    D2GfxDataStrc sGfxData;
    memset(&sGfxData, 0, sizeof(sGfxData));
    sGfxData.pDC6 = pDC6;
    sGfxData.nDirection = 0;
    sGfxData.nFrameNumber = 0;
    DC6Block* pBlock = D2CMP::SRC::CelCmp::CELCMP_GetDC6BlockFromGfxData(&sGfxData);
    if (!pBlock) {
        return 0;
    }
    int nHeight = (int)D2CMP::SRC::CelCmp::GetDC6Height(pBlock);
    return nHeight > 0 ? nHeight : 0;
}

// Capital-glyph frame height in font nFontId (0 = not measurable). Mirrors
// VPLABEL_GlyphFrameHeight: forces the standard lazy font load through
// SetAndReturnLastFont (the exact path every text draw takes when a font is
// first used), then reads the glyph's frame through the retail accessors -
// the same glyph-entry -> frame-number lookup every drawn character takes.
// Leaves the current font switched; callers save/restore around batches.
static int VPRES_GlyphFrameHeight(int nFontId, uint16_t wCharCode) {
    if (nFontId < 0 || nFontId >= 14) {
        return 0;
    }
    D2Win::Src::D2WinFont::SetAndReturnLastFont(nFontId);
    // A failed load leaves the cache entry null (SetAndReturnLastFont assigns
    // the loader result unconditionally); FONT_GetCharWidthCallback would
    // then index pGlyphs + ch against a null/garbage table - never
    // dereference that.
    if (!gaFontCache[nFontId].pFontDC6 || !gaFontCache[nFontId].pGlyphs) {
        return 0;
    }
    D2FontGlyphStrc* pGlyph = (*gpfnFontGetCharWidth)(wCharCode);
    if (!pGlyph) {
        return 0;
    }
    DC6* pFontDc6 = (DC6*)gaFontCache[nFontId].pFontDC6;
    D2GfxDataStrc sGfxData;
    memset(&sGfxData, 0, sizeof(sGfxData));
    sGfxData.pDC6 = pFontDc6;
    sGfxData.nDirection = 0;
    sGfxData.nFrameNumber = (int)*(const uint16_t*)(pGlyph->abUnverified + 6);
    DC6Block* pBlock = D2CMP::SRC::CelCmp::CELCMP_GetDC6BlockFromGfxData(&sGfxData);
    if (!pBlock) {
        return 0;
    }
    int nHeight = (int)D2CMP::SRC::CelCmp::GetDC6Height(pBlock);
    return nHeight > 0 ? nHeight : 0;
}

int __stdcall VPRES_PickFontMatchingDC6(struct DC6* pTargetDC6) {
    // Same probe list + policy as the Vanilla+ row label (revE): ascending
    // probe, distance ties resolve to the larger font, Font42 fallback.
    static const int gaProbe[3] = { 7, 2, 3 };  // Font24, Font30, Font42
    static const int VPRES_FONT_FALLBACK = 3;   // Font42 - largest menu font

    // One decision per process per target: the retail label DC6 lives from
    // ESCMENU_Initialize to shutdown, so pointer identity is stable. A null
    // target (should not happen - slot 1 is retail-loaded) is never cached.
    static const DC6* pCachedTarget = nullptr;
    static int nCachedFontId = VPRES_FONT_FALLBACK;
    if (pTargetDC6 && pTargetDC6 == pCachedTarget) {
        return nCachedFontId;
    }

    int nResult = VPRES_FONT_FALLBACK;
    int nTargetHeight = VPRES_MeasureDC6FrameHeight(pTargetDC6);
    if (nTargetHeight > 0) {
        int32_t nSavedFont = D2Win::Src::D2WinFont::SetAndReturnLastFont(gaProbe[0]);
        int nBestFont = -1;
        int nBestDist = 0;
        for (int i = 0; i < 3; i++) {
            int nCapsHeight = VPRES_GlyphFrameHeight(gaProbe[i], (uint16_t)L'S');
            if (nCapsHeight <= 0) {
                continue;
            }
            int nDist = nCapsHeight > nTargetHeight ? nCapsHeight - nTargetHeight : nTargetHeight - nCapsHeight;
            // "<=" on purpose: the probe order is ascending, so a distance
            // tie resolves to the larger font (the retail value images are
            // the big menu family - when in doubt, go bigger).
            if (nBestFont == -1 || nDist <= nBestDist) {
                nBestFont = gaProbe[i];
                nBestDist = nDist;
            }
        }
        D2Win::Src::D2WinFont::SetAndReturnLastFont(nSavedFont);
        if (nBestFont != -1) {
            nResult = nBestFont;
        }
    }

    if (pTargetDC6) {
        pCachedTarget = pTargetDC6;
        nCachedFontId = nResult;
    }
    return nResult;
}

// ===== [vanilla-plus] merge tripwires ======================================
// The fork extends upstream-sized things in several places (UI flag table,
// esc-menu options row, dismiss-mode alias). A future upstream merge that
// drops or reverts one of those edits previously failed only at runtime, far
// from the cause. These checks fail LOUDLY instead:
//   - static_assert pins what the compiler can see (enum values, array bounds
//     of the extern declarations in globals/globals.h);
//   - VANILLAPLUS_SelfCheck pins what only the runtime can see (values and
//     wired callbacks), once per process from ALTTOGGLE_EnsureIni (boot).
// Any failure means a merge edit was lost or upstream changed shape - audit
// against docs/merge-upstream.md before changing these expectations.
static_assert(UI_VANILLAPLUS == 38,
    "vanilla-plus: UI_VANILLAPLUS moved - upstream added/changed a UI var; "
    "re-audit the flag-38 count bumps (docs/merge-upstream.md)");
static_assert(UI_RECIPESCROLL == 37,
    "vanilla-plus: upstream inserted a UI var before UI_VANILLAPLUS; "
    "re-audit the flag-38 count bumps (docs/merge-upstream.md)");
static_assert(sizeof(gaEscMenuOptionsTable) / sizeof(gaEscMenuOptionsTable[0]) == 6,
    "vanilla-plus: gaEscMenuOptionsTable bounds drifted from the extern "
    "declaration - the row-10 table edit or globals.h extern was lost in a merge");
static_assert(sizeof(gUIFlagDismissModeTable) / sizeof(gUIFlagDismissModeTable[0]) == 39,
    "vanilla-plus: gUIFlagDismissModeTable bounds drifted from the extern "
    "declaration - the 38 -> 39 edit or globals.h extern was lost in a merge");
static_assert(sizeof(D2UIFlagDescStrc) == 39 * sizeof(uint32_t),
    "vanilla-plus: D2UIFlagDescStrc::anDismissMode no longer 39 entries - "
    "the globals.h struct growth was lost or upstream changed the struct");
static_assert(offsetof(D2UIFlagStrc, VanillaPlus) == 0x98,
    "vanilla-plus: D2UIFlagStrc::VanillaPlus moved off 0x98 - the flag-store "
    "member addition was lost, or upstream added a flag before it");

void __stdcall VANILLAPLUS_SelfCheck() {
    static int nDone = 0;
    if (nDone) {
        return;
    }
    nDone = 1;
    if (gnUIFlagCount != 0x27) {
        D2BOOT_Fail("vanilla-plus self-check: gnUIFlagCount != 0x27 - a merge "
                    "likely dropped the count bumps (see docs/merge-upstream.md)");
        exit(-1);
    }
    if (gaEscMenuHeaderOptions.nItemCount != 0x6) {
        D2BOOT_Fail("vanilla-plus self-check: gaEscMenuHeaderOptions.nItemCount "
                    "!= 6 - the row-10 esc-menu edit was lost (see docs/merge-upstream.md)");
        exit(-1);
    }
    // The row-10 entry is INSERTED above "Previous Menu" (index 4 of 6, owner
    // decision 2026-10-29), not appended - and a future upstream merge may
    // reshuffle rows again. Scan for it instead of hardcoding an index: fail
    // only if the wired row is missing or duplicated.
    {
        constexpr int kRow10Count =
            sizeof(gaEscMenuOptionsTable) / sizeof(gaEscMenuOptionsTable[0]);
        int nWired = 0;
        for (int i = 0; i < kRow10Count; ++i) {
            if (gaEscMenuOptionsTable[i].fpClickCallback ==
                &D2Client::UI::VanillaPlus::ESCMENU_ShowVanillaPlusOptions) {
                ++nWired;
            }
        }
        if (nWired != 1) {
            D2BOOT_Fail("vanilla-plus self-check: gaEscMenuOptionsTable row-10 "
                        "click callback not wired (or duplicated) - the row-10 "
                        "table edit was lost or doubled in a merge "
                        "(see docs/merge-upstream.md)");
            exit(-1);
        }
    }
    // [resolution-mode] the video-options Resolution row (expansion table)
    // must carry the VP value-label draw callback; its fpInitCallback (the
    // stop-2 attach point) identifies the row. Scan instead of hardcoding an
    // index - a merge may reshuffle rows. Runtime state (nSubImageCount bump,
    // blank placeholder) is applied by that init callback; this pins only the
    // static globals.cpp wiring a merge could drop.
    {
        constexpr int kVidRowCount =
            sizeof(gaEscMenuDeleteCharTable) / sizeof(gaEscMenuDeleteCharTable[0]);
        int nWired = 0;
        for (int i = 0; i < kVidRowCount; ++i) {
            if (gaEscMenuDeleteCharTable[i].fpInitCallback !=
                &D2Client::UI::EscMenu::UI_SLIDER_LoadAndApplyResolution) {
                continue;
            }
            ++nWired;
            if (gaEscMenuDeleteCharTable[i].fpDrawCallback !=
                &D2Client::UI::EscMenu::UI_SLIDER_DrawResolutionValue) {
                D2BOOT_Fail("vanilla-plus self-check: Resolution row draw "
                            "callback not wired - the resolution-mode "
                            "globals.cpp edit was lost (see docs/merge-upstream.md)");
                exit(-1);
            }
        }
        if (nWired != 1) {
            D2BOOT_Fail("vanilla-plus self-check: Resolution row init callback "
                        "not found (or duplicated) - the resolution-mode table "
                        "wiring was lost or doubled in a merge "
                        "(see docs/merge-upstream.md)");
            exit(-1);
        }
    }
    if (gUIFlagDismissModeTable[38] !=
        (D2UIFlagDescStrc*)&gaUIFlagDescs[9].anDismissMode[0]) {
        D2BOOT_Fail("vanilla-plus self-check: gUIFlagDismissModeTable[38] alias "
                    "missing - the dismiss-mode edit was lost (see docs/merge-upstream.md)");
        exit(-1);
    }
}
// ---------------------------------------------------------------------------
// [resolution site-5] Wide-layout classification + 800-UI centering offset.
//
// Retail D2 UI code classifies layouts by D2GFX_GetResolutionMode(): the
// 640x480 class (mode 0) vs the 800x600 class (mode 2). Every gate that
// picks art ("Panel\\800CtrlPnl7" vs "Panel\\CtrlPnl7"), input handling
// (count-based vs geometric stat/skill button tests) or layout tables
// (nScreenMode 0/1 variants from D2Common) tests == 2 / != 2. The Vanilla
// Plus wide modes reuse the 800x600 art and tables (mode 4 sets
// nScreenMode = 1 in CLIENT_SetScreenResolution), so they must join the
// 800 class everywhere; this classifier is the single place that defines
// "renders like retail 800x600".
//
// The 800-class UI is exactly 800 px wide (interface bar pieces span
// 0..800, side panels and border frames tile 0..800), so on wider
// playfields it is re-centered as one rigid unit: every X anchor that is
// not already screen-center- or ScreenSizeX-relative gets
// VPRES_GetWideUiOffsetX() (= (ScreenSizeX - 800) / 2) added. The offset
// is 0 at 800x600 and clamped to 0 at 640x480, so retail modes keep
// byte-identical behavior; only modes 3 (1344x700) and 4 (1280x720) see a
// non-zero shift (240 px at 1280x720).
// ---------------------------------------------------------------------------
BOOL __stdcall VPRES_IsWideLayoutMode(DWORD eResolutionMode) {
    // 2 = retail 800x600; 3 = dormant 1344x700 (same 800-class UI, already
    // in the D2GFX mode table); 4 = Vanilla Plus 1280x720.
    return eResolutionMode == 2 || eResolutionMode == 3 || eResolutionMode == 4;
}

int __stdcall VPRES_GetWideUiOffsetX() {
    int nOffsetX = (ScreenSizeX - 800) / 2;
    return 0 < nOffsetX ? nOffsetX : 0;
}

// [resolution site-6] see VanillaPlusCore.h - the Y half of the rigid-unit
// rule: every fixed (top-anchored) 800-space Y coordinate rides the
// bottom-anchored panel art by the same height delta the art gains
// automatically ("gnPanelOffsetY - const + ScreenSizeY" forms). Clamped to
// 0 at 640x480/800x600 so retail modes stay byte-identical; 100 at mode 3
// (1344x700), 120 at mode 4 (1280x720). gnPanelOffsetY itself stays -60 -
// bottom-anchored expressions already contain ScreenSizeY and would
// double-shift.
int __stdcall VPRES_GetWideUiOffsetY() {
    int nOffsetY = ScreenSizeY - 600;
    return 0 < nOffsetY ? nOffsetY : 0;
}
