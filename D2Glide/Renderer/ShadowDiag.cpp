// [glide fix 11] D2Glide shadow-draw field diagnostic — see ShadowDiag.h.
//
// Captures the first kMaxRecords shadow quads drawn after the first ground
// tile (in-world marker) into d2shadow-diag.txt in the game directory. Every
// record carries the complete DrawShadowQuad input state so the rebuild's
// shadow pipeline output can be compared against Blizzard 1.14d retail
// (same-spot A/B) without any further instrumentation round trip:
//
//   unit/frame : which D2GfxDataStrc the client handed DrawShadow — frame
//                index, component, unit type/class, mode, direction, frame
//                count, sprite + frame-block pointers, DC6 filename. A
//                frame-selection divergence (fix-11's candidate (a)) shows
//                as a stuck/mismatched frame index or direction here.
//   block      : the DC6 frame block header the RLE decoder walks (flags,
//                width, height).
//   tex        : the GlideTexInfoStrc after CalculateTextureSize (pow2 dims,
//                crop offsets, area, lod, aspect, S/T factors).
//   mask       : crc32 + nonzero-texel count of the decoded pow2 buffer
//                (gpGlideSpriteBuffer) and a downsampled ASCII bitmap. A
//                decode/placement divergence (candidate (b)) shows as a
//                different crc or a displaced/eroded blob in the bitmap.
//   quad       : the four submitted fan vertices (28-byte stride: x, y,
//                argb @8, oow @12, s @16, t @20 — wrapper-verified layout).
//
// Zero behavior change: capture is append-only file I/O, guarded, budgeted,
// and never touches game state.

#include <stdio.h>
#include <string.h>
#include "D2Glide/Renderer/Glide.h"
#include "D2Glide/Renderer/ShadowDiag.h"
#include "D2Client/D2GfxDataStrc.h" // full D2GfxDataStrc for the pSprite derefs below; upstream moved the struct out of the old globals include chain
#include "globals/globals.h"

namespace D2Glide::Renderer::ShadowDiag {

static const int kMaxRecords = 24;
static const char kPath[] = "d2shadow-diag.txt";

static bool s_bWorldSeen;
static bool s_bFileStarted;
static bool s_bDisabled;
static int s_nCaptured;
static int s_nArmedDraws;

// ---- crc32 (IEEE, poly 0xEDB88320) --------------------------------------

static uint32_t s_crcTable[256];
static bool s_bCrcTableReady;

static void BuildCrcTable() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        s_crcTable[i] = c;
    }
    s_bCrcTableReady = true;
}

static uint32_t Crc32(const uint8_t * p, int n) {
    if (!s_bCrcTableReady) {
        BuildCrcTable();
    }
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; ++i) {
        c = s_crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

// ---- capture ------------------------------------------------------------

void NoteWorldFrame() {
    s_bWorldSeen = true;
}

void OnShadowQuad(GlideTexInfoStrc * pTexInfo, const void * pFanVerts) {
    if (s_bDisabled || !s_bWorldSeen) {
        return;
    }
    if (s_nCaptured >= kMaxRecords) {
        s_bDisabled = true; // budget exhausted; zero further cost
        return;
    }
    if (!pTexInfo || !pFanVerts) {
        return;
    }
    ++s_nArmedDraws;

    FILE * f = fopen(kPath, s_bFileStarted ? "a" : "w");
    if (!f) {
        s_bDisabled = true;
        return;
    }
    if (!s_bFileStarted) {
        fprintf(f, "d2shadow-diag — glide fix 11 (first %d shadow quads after the first ground tile)\n", kMaxRecords);
        s_bFileStarted = true;
    }

    D2GfxDataStrc * pGfx = pTexInfo->pSprite;
    fprintf(f, "== shadow #%d (armed draw #%d) ==\n", s_nCaptured + 1, s_nArmedDraws);
    if (pGfx) {
        const uint8_t * pBlock = (const uint8_t *)pGfx->pDC6Block;
        fprintf(f,
                "unit: type=%d class=%d mode=%d comp=%d dir=%d frame=%d/%u flags=%02x\n",
                (int)pGfx->eUnitType, pGfx->nClassId, pGfx->nMode,
                (int)pGfx->nComponentType, pGfx->nDirection,
                pGfx->nFrameNumber, (unsigned)pGfx->nFrameCount,
                (unsigned)pGfx->dwFlags);
        fprintf(f,
                "sprite: dc6=%p block=%p loaded=%d fname=%.64s\n",
                (const void *)pGfx->pDC6, (const void *)pGfx->pDC6Block,
                pGfx->bLoaded, pGfx->szFilename ? pGfx->szFilename : "(null)");
        if (pBlock) {
            fprintf(f,
                    "block: flags=%02x w=%u h=%u\n",
                    (unsigned)pBlock[0],
                    (unsigned)*(const uint32_t *)(pBlock + 4),
                    (unsigned)*(const uint32_t *)(pBlock + 8));
        }
    } else {
        fprintf(f, "unit: (null pSprite)\n");
    }

    fprintf(f,
            "tex: w=%d h=%d left=%d top=%d pow2=%dx%d area=%d lod=%d aspect=%d scale0=%.4g s0=%.4g t0=%.4g s1=%.4g\n",
            pTexInfo->nWidth, pTexInfo->nHeight, pTexInfo->nLeft, pTexInfo->nTop,
            pTexInfo->nTexWidth, pTexInfo->nTexHeight, pTexInfo->nTexArea,
            pTexInfo->nLodLevel, pTexInfo->nAspect,
            (double)pTexInfo->fScale0, (double)pTexInfo->fTexS0,
            (double)pTexInfo->fTexT0, (double)pTexInfo->fTexS1);

    // Decoded pow2 mask (pal8 indices; 0 = chroma-transparent).
    const int nArea = pTexInfo->nTexArea;
    const uint8_t * pMask = (const uint8_t *)gpGlideSpriteBuffer;
    if (pMask && nArea > 0 && nArea <= 0x10000) {
        int nNonZero = 0;
        for (int i = 0; i < nArea; ++i) {
            nNonZero += (pMask[i] != 0);
        }
        fprintf(f, "mask: crc=%08x nonzero=%d/%d\n", Crc32(pMask, nArea), nNonZero, nArea);

        // Downsampled bitmap: buffer row 0 printed first (glide textures are
        // bottom-up, so the visual top edge is the last printed row).
        const int nW = pTexInfo->nTexWidth;
        const int nH = pTexInfo->nTexHeight;
        int nRowStep = 1;
        while (nH / nRowStep > 48) {
            ++nRowStep;
        }
        int nColStep = 1;
        while (nW / nColStep > 80) {
            ++nColStep;
        }
        fprintf(f, "mask bitmap (row 0 = buffer row 0; visual top = last row):\n");
        for (int y = 0; y < nH; y += nRowStep) {
            char line[82];
            int n = 0;
            for (int x = 0; x < nW && n < 80; x += nColStep, ++n) {
                line[n] = pMask[y * nW + x] ? '#' : '.';
            }
            line[n] = 0;
            fprintf(f, "%s\n", line);
        }
    } else {
        fprintf(f, "mask: (buffer unavailable: pMask=%p area=%d)\n", (const void *)pMask, nArea);
    }

    // Submitted fan: 28-byte stride, x,y @0 (f32), argb @8, oow @12, s/t @16/20.
    static const int kStride = 0x1c;
    const uint8_t * pV = (const uint8_t *)pFanVerts;
    for (int i = 0; i < 4; ++i, pV += kStride) {
        float x, y, oow, s, t;
        uint32_t argb;
        memcpy(&x, pV + 0, 4);
        memcpy(&y, pV + 4, 4);
        memcpy(&argb, pV + 8, 4);
        memcpy(&oow, pV + 12, 4);
        memcpy(&s, pV + 16, 4);
        memcpy(&t, pV + 20, 4);
        fprintf(f,
                "v%d: x=%.2f y=%.2f argb=%08x oow=%.4g s=%.4g t=%.4g\n",
                i, (double)x, (double)y, argb, (double)oow, (double)s, (double)t);
    }
    fprintf(f, "\n");
    fclose(f);

    ++s_nCaptured;
    if (s_nCaptured >= kMaxRecords) {
        s_bDisabled = true;
    }
}

} // namespace D2Glide::Renderer::ShadowDiag
