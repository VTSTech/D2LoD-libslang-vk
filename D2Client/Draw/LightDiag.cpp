// [glide fix 8] D2Client lightmap field diagnostic — see LightDiag.h.
//
// Evidence this dump is designed to resolve (A/B logs, 535f2aa wrapper):
//   original D2Glide.dll ground-strip vertex argb : 0x4f7fbeba / 0x4f7fc9c4 /
//   0x4f7fb4b1 (varying, alpha byte 0x4f = night intensity);
//   rebuilt tree ground-strip vertex argb         : 0xff808080 (constant).
// With the tint formulas proven equivalent, the dump discriminates between:
//   (a) act-environment night values (DRLGENV),
//   (b) room ambient fill values (ROOM_GetAmbientLightColor),
//   (c) the cell field itself (gaLightMapCells),
//   (d) unit sample coordinates resolving to clamped/wrong cells,
//   (e) the ground grid sampling coordinates.

#include <stdio.h>
#include <string.h>
#include "d2_platform.h"
#include "globals/globals.h"
#include "D2Client/Draw/LightDiag.h"
#include "D2Client/Draw/LightMap.h"
#include "D2Client/GAME/Draw.h"
#include "D2Client/UNIT/Player.h"
#include "D2Common/D2RoomStrc.h"
#include "D2Common/DUNGEON/Dungeon.h"
#include "D2Common/Environment.h"
#include "D2Common/Units.h"

namespace D2Client::Draw::LightDiag {

static const int kMaxUnitSamples = 16;
static const int kCellDim = 0x30;                       // 48x48 cells
static const int kCellBytes = kCellDim * kCellDim * 8;  // 0x4800

struct UnitSample {
    int nX;
    int nY;
    uint32_t nPackedLightColor;
};

static uint32_t s_nFrameCount;
static bool s_bDumped;
static uint32_t s_nRetryFrames;
static uint8_t s_cellFieldSnapshot[kCellBytes];
static int s_nSnapWRF;
static int s_nSnapBS;
static int s_nSnapMaxX;
static int s_nSnapMaxY;
static UnitSample s_aUnitSamples[kMaxUnitSamples];
static uint32_t s_nUnitSampleCount;
static uint8_t s_groundGridSnapshot[64 * 12];
static int s_nGroundWorldX;
static int s_nGroundWorldY;
static bool s_bGroundCaptured;

static void WriteDump() {
    FILE * pFile = fopen("d2light-diag.txt", "a");
    if (!pFile) {
        return;
    }

    fprintf(pFile, "==== d2light-diag tick=%u frames=%u ====\n", GetTickCount(), s_nFrameCount);
    fprintf(pFile, "origins@snapshot: WRF=%d BS=%d worldMaxX=%d worldMaxY=%d (current: WRF=%d BS=%d)\n",
            s_nSnapWRF, s_nSnapBS, s_nSnapMaxX, s_nSnapMaxY, gnLightmapWorldRefreshFlag, gnLightmapBufferState);

    // (a) act environment night values
    int nEnvIntensity = D2Common::Environment::DRLGENV_GetIntensityFromAct(sgpDrlgAct);
    uint8_t nEnvRed = 0;
    uint8_t nEnvGreen = 0;
    uint8_t nEnvBlue = 0;
    D2Common::Environment::DRLGENV_GetLightColorFromAct(sgpDrlgAct, &nEnvRed, &nEnvGreen, &nEnvBlue);
    fprintf(pFile, "act env: intensity=%d rgb=(%u,%u,%u)\n", nEnvIntensity, nEnvRed, nEnvGreen, nEnvBlue);

    // (b) player-room + adjacent-room ambients
    D2UnitStrc * pPlayer = UNIT::Player::GetPlayerUnit_UI();
    D2RoomStrc * pPlayerRoom = pPlayer ? D2Common::Units::UNITS_GetRoom(pPlayer) : nullptr;
    if (pPlayerRoom) {
        uint32_t nAmbIntensity = 0;
        char nAmbRed = 0;
        char nAmbGreen = 0;
        char nAmbBlue = 0;
        LightMap::ROOM_GetAmbientLightColor(pPlayerRoom, &nAmbIntensity, &nAmbRed, &nAmbGreen, &nAmbBlue, 1);
        fprintf(pFile, "player room ambient: intensity=%u rgb=(%d,%d,%d)\n",
                nAmbIntensity, (int)(uint8_t)nAmbRed, (int)(uint8_t)nAmbGreen, (int)(uint8_t)nAmbBlue);

        D2RoomStrc ** ppRoomList = nullptr;
        int nNumRooms = 0;
        D2Common::DUNGEON::Dungeon::DRLGROOM_GetAdjacentRoomsList(pPlayerRoom, &ppRoomList, &nNumRooms);
        int nPrinted = 0;
        for (int nRoomIndex = 0; nRoomIndex < nNumRooms && nPrinted < 8; nRoomIndex++) {
            if (ppRoomList[nRoomIndex] == pPlayerRoom) {
                continue;
            }

            LightMap::ROOM_GetAmbientLightColor(ppRoomList[nRoomIndex], &nAmbIntensity, &nAmbRed, &nAmbGreen, &nAmbBlue, 1);
            fprintf(pFile, "adjacent[%d] ambient: intensity=%u rgb=(%d,%d,%d)\n",
                    nPrinted, nAmbIntensity, (int)(uint8_t)nAmbRed, (int)(uint8_t)nAmbGreen, (int)(uint8_t)nAmbBlue);
            nPrinted++;
        }
    } else {
        fprintf(pFile, "player room ambient: (no player room)\n");
    }

    // (c) the cell field — intensity plane from the frame-1 snapshot
    const D2LightMapCellStrc * pCells = (const D2LightMapCellStrc *)s_cellFieldSnapshot;
    fprintf(pFile, "cell intensity plane (y*48+x, hex):\n");
    for (int nY = 0; nY < kCellDim; nY++) {
        for (int nX = 0; nX < kCellDim; nX++) {
            fprintf(pFile, "%02x", pCells[nY * kCellDim + nX].nCellIntensity);
        }

        fprintf(pFile, "\n");
    }

    static const int kRgbRows[] = { 0, 1, 23, 24, 46, 47 };
    for (int nRow = 0; nRow < 6; nRow++) {
        int nY = kRgbRows[nRow];
        fprintf(pFile, "cell rgb row y=%d:", nY);
        for (int nX = 0; nX < kCellDim; nX++) {
            fprintf(pFile, " %02x%02x%02x", pCells[nY * kCellDim + nX].nRed,
                    pCells[nY * kCellDim + nX].nGreen, pCells[nY * kCellDim + nX].nBlue);
        }

        fprintf(pFile, "\n");
    }

    // (e) the first ground-tile light grid
    if (s_bGroundCaptured) {
        fprintf(pFile, "ground grid: world=(%d,%d) 64 entries (i r g b x y):\n", s_nGroundWorldX, s_nGroundWorldY);
        for (int nEntry = 0; nEntry < 64; nEntry++) {
            const uint8_t * pEntry = s_groundGridSnapshot + nEntry * 12;
            int nEntryX = *(const int *)(pEntry + 4);
            int nEntryY = *(const int *)(pEntry + 8);
            fprintf(pFile, "  [%d] %02x %02x %02x %02x %d %d\n",
                    nEntry, pEntry[0], pEntry[1], pEntry[2], pEntry[3], nEntryX, nEntryY);
        }
    } else {
        fprintf(pFile, "ground grid: (not captured)\n");
    }

    // (d) unit samples + the cell each resolves to under CoordIsInCoord rules
    fprintf(pFile, "unit samples (%u):\n", s_nUnitSampleCount);
    for (uint32_t nSample = 0; nSample < s_nUnitSampleCount; nSample++) {
        const UnitSample * pSample = &s_aUnitSamples[nSample];
        int nCellX = (pSample->nX >> 3) - s_nSnapWRF;
        int nCellY = (pSample->nY >> 3) - s_nSnapBS;
        const char * szClamp = "";
        if (nCellX < 0 || 0x2f < nCellX || nCellY < 0 || 0x2f < nCellY) {
            szClamp = " CLAMPED";
            if (nCellX < 0) {
                nCellX = 0;
            } else if (0x2f < nCellX) {
                nCellX = 0x2f;
            }

            if (nCellY < 0) {
                nCellY = 0;
            } else if (0x2f < nCellY) {
                nCellY = 0x2f;
            }
        }

        const D2LightMapCellStrc * pCell = &pCells[nCellY * kCellDim + nCellX];
        fprintf(pFile, "  [%u] entry=(%d,%d) packed=0x%08x cell=(%d,%d)%s cellI=%02x cellRGB=(%02x,%02x,%02x)\n",
                nSample, pSample->nX, pSample->nY, pSample->nPackedLightColor, nCellX, nCellY, szClamp,
                pCell->nCellIntensity, pCell->nRed, pCell->nGreen, pCell->nBlue);
    }

    fprintf(pFile, "==== end d2light-diag ====\n");
    fclose(pFile);
}

void __fastcall NoteFrameHook() {
    if (s_bDumped) {
        return;
    }

    // Give up if no world context materializes (title screens, menus).
    if (!UNIT::Player::GetPlayerUnit_UI()) {
        if (++s_nRetryFrames < 600) {
            return;
        }

        s_bDumped = true;
        return;
    }

    s_nFrameCount++;
    if (s_nFrameCount == 1) {
        memcpy(s_cellFieldSnapshot, gaLightMapCells, kCellBytes);
        s_nSnapWRF = gnLightmapWorldRefreshFlag;
        s_nSnapBS = gnLightmapBufferState;
        s_nSnapMaxX = gnLightmapWorldMaxX;
        s_nSnapMaxY = gnLightmapWorldMaxY;
        return;
    }

    s_bDumped = true;
    WriteDump();
}

void __stdcall NoteUnitSample(int nEntryX, int nEntryY, uint32_t nPackedLightColor) {
    if (s_bDumped || s_nUnitSampleCount >= kMaxUnitSamples) {
        return;
    }

    s_aUnitSamples[s_nUnitSampleCount].nX = nEntryX;
    s_aUnitSamples[s_nUnitSampleCount].nY = nEntryY;
    s_aUnitSamples[s_nUnitSampleCount].nPackedLightColor = nPackedLightColor;
    s_nUnitSampleCount++;
}

void __stdcall NoteGroundGrid(int nWorldX, int nWorldY, const void * pLightGrid) {
    if (s_bDumped || s_bGroundCaptured) {
        return;
    }

    s_bGroundCaptured = true;
    s_nGroundWorldX = nWorldX;
    s_nGroundWorldY = nWorldY;
    memcpy(s_groundGridSnapshot, pLightGrid, sizeof(s_groundGridSnapshot));
}

} // namespace D2Client::Draw::LightDiag
