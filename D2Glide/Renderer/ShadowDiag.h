// [glide fix 11] D2Glide shadow-draw field diagnostic — see ShadowDiag.cpp.
//
// One-shot capture of the first in-world shadow-quad draws into
// d2shadow-diag.txt (game directory), fix-8 (d2light-diag.txt) playbook:
// evidence-first, zero behavior change once the budget is exhausted.
//
// Why (field state at fix-10):
//   - Character/unit shadows are DrawShadow -> RENDERER_GLIDE_DrawShadowQuad
//     (mode-5 count-4 fan, uniform argb=0, blend (SRC_ALPHA,ONE_MINUS_SRC_ALPHA)
//     + constant alpha 0xc0, chroma-punched pal8 mask).
//   - Logging-wrapper vc0# ground truth (retail 1.14d vs rebuild): shadow-quad
//     screen rects are byte-identical between retail and the rebuild for every
//     perspective-neutral capture (64x256 / 32x128 / 64x64 classes), and the
//     blend/chroma state lines are identical. Geometry, S/T mapping and blend
//     are therefore exonerated.
//   - Static audit exonerated: dispatch table, fan-slot reconstruction
//     (fix-4/5), CalculateTextureSize spans, the DC6 RLE decode chain
//     (dispatcher + plain DecompressRLE + SkipRLERows) and the texture-cache
//     slot algebra (mode 4 -> 1).
//   - Remaining candidates are RUNTIME DATA, invisible to every capture so
//     far: (a) which DC6 frame the client hands DrawShadow (frame index /
//     direction), (b) the decoded mask content (frame bytes -> pow2 buffer
//     placement). This dump pins both, plus the submitted quad.
//
// Record layout per draw (see ShadowDiag.cpp): unit/frame identity, DC6 block
// header, GlideTexInfoStrc fields, mask crc32 + nonzero count, the four fan
// vertices (28-byte stride decode) and a downsampled ASCII bitmap of the
// decoded pow2 mask (buffer row 0 printed first; glide textures are
// bottom-up, so the visual top edge is the last row printed).

#ifndef D2GLIDE_RENDERER_SHADOWDIAG_H
#define D2GLIDE_RENDERER_SHADOWDIAG_H

struct GlideTexInfoStrc;

namespace D2Glide::Renderer::ShadowDiag {

// Called from DrawGroundTile: ground tiles only draw once the world is up,
// so the first call arms the capture (loading screens draw no ground tiles).
void NoteWorldFrame();

// Called from RENDERER_GLIDE_DrawShadowQuad right before the fan submit.
// pTexInfo is the texSizeBuf the caller uploaded from (pSprite = the
// D2GfxDataStrc carrying the frame identity); pFanVerts is the contiguous
// 4-vertex fan block (28-byte stride, wrapper-verified layout).
void OnShadowQuad(GlideTexInfoStrc * pTexInfo, const void * pFanVerts);

} // namespace D2Glide::Renderer::ShadowDiag

#endif // D2GLIDE_RENDERER_SHADOWDIAG_H
