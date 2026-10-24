// glide3xvk — P2.10 overlay implementation. See g3x_overlay.h for the design.
#include "g3x_overlay.h"

#include "g3x_font8x8.h"

#include <cstdio>
#include <cstring>

#ifndef G3XVK_BUILD_ID
#define G3XVK_BUILD_ID "unknown"
#endif

namespace {

bool s_enabled = true;

// The tag line, composed once before main() runs: "glide3xvk 0.3.0 (build)".
struct TagLine {
    char     text[80];
    unsigned len;
    TagLine() {
        std::snprintf(text, sizeof(text), "glide3xvk %s (%s)",
                      G3XVK_VERSION, G3XVK_BUILD_ID);
        text[sizeof(text) - 1] = '\0';
        len = (unsigned)std::strlen(text);
    }
};
const TagLine s_tag;

const uint8_t* glyph_of(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc < G3X_FONT8X8_FIRST || uc >= G3X_FONT8X8_FIRST + G3X_FONT8X8_COUNT)
        uc = '?';
    return g3x_font8x8[uc - G3X_FONT8X8_FIRST];
}

} // namespace

void g3x_overlay_set_enabled(bool on) { s_enabled = on; }
bool g3x_overlay_enabled()            { return s_enabled; }

void g3x_overlay_render(uint8_t* bgra, unsigned w, unsigned h) {
    if (!s_enabled || !bgra || w < 8 || h < 8) return;

    // Box: (4,4)..(4+pad*2+8*len, 4+pad*2+8), clamped to the buffer edges so
    // absurdly small present targets can't write out of bounds.
    const unsigned pad = 3, ox = 4, oy = 4;
    const unsigned text_w = s_tag.len * 8;
    unsigned bw = text_w + pad * 2;
    unsigned bh = 8 + pad * 2;
    if (ox + bw > w) bw = w - ox;
    if (oy + bh > h) bh = h - oy;
    if (!bw || !bh) return;

    // 1. 65%-black backdrop (dst *= 0.35 per channel — rounds toward black).
    for (unsigned y = oy; y < oy + bh; ++y) {
        uint8_t* row = bgra + (size_t)y * w * 4;
        for (unsigned x = ox; x < ox + bw; ++x) {
            uint8_t* p = row + (size_t)x * 4;
            p[0] = (uint8_t)((p[0] * 89) >> 8);   // B
            p[1] = (uint8_t)((p[1] * 89) >> 8);   // G
            p[2] = (uint8_t)((p[2] * 89) >> 8);   // R
            p[3] = 0xFF;
        }
    }

    // 2. glyphs: pass 0 = 1px drop shadow (+1,+1, black), pass 1 = white text.
    const unsigned tx = ox + pad, ty = oy + pad;
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned cx = tx;
        for (unsigned i = 0; i < s_tag.len; ++i, cx += 8) {
            const uint8_t* g = glyph_of(s_tag.text[i]);
            for (unsigned r = 0; r < 8; ++r) {
                const uint8_t bits = g[r];
                if (!bits) continue;
                const unsigned py = ty + r + (pass ? 0u : 1u);
                if (py >= h) continue;
                uint8_t* row = bgra + (size_t)py * w * 4;
                for (unsigned b = 0; b < 8; ++b) {
                    if (!((bits >> b) & 1)) continue;
                    const unsigned pxx = cx + b + (pass ? 0u : 1u);
                    if (pxx >= w) continue;
                    uint8_t* p = row + (size_t)pxx * 4;
                    if (pass) { p[0] = 0xFF; p[1] = 0xFF; p[2] = 0xFF; }
                    else      { p[0] = 0x00; p[1] = 0x00; p[2] = 0x00; }
                    p[3] = 0xFF;
                }
            }
        }
    }
}
