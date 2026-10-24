// glide3xvk — P2.10 overlay implementation. See g3x_overlay.h for the design.
#include "g3x_overlay.h"

#include "g3x_font8x8.h"

#include <cstdio>
#include <cstring>

#ifndef G3XVK_BUILD_ID
#define G3XVK_BUILD_ID "unknown"
#endif
// Short form (7-char hash + optional -dirty). The game's own "Video:" line
// gets this exact text via g3x_drivertag, and the game sprintf()s it into a
// char[32] stack buffer — the long describe form would be truncated there.
#ifndef G3XVK_BUILD_SHORT
#define G3XVK_BUILD_SHORT G3XVK_BUILD_ID
#endif

namespace {

bool s_enabled = true;

// The tag line, composed once before main() runs: "glide3xvk 0.3.0 (build)".
struct TagLine {
    char     text[80];
    unsigned len;
    TagLine() {
        std::snprintf(text, sizeof(text), "glide3xvk %s (%s)",
                      G3XVK_VERSION, G3XVK_BUILD_SHORT);
        text[sizeof(text) - 1] = '\0';
        len = (unsigned)std::strlen(text);
    }
};
const TagLine s_tag;

// 0.4.2: second overlay line — the active slang preset's file name. Pushed
// by g3x_slang when a chain goes ready (first open, conf change, hot reload)
// and cleared when the chain dies; empty = line absent, box = tag alone.
char     s_shader[64];
unsigned s_shader_len = 0;
bool     s_shader_on  = false;

const uint8_t* glyph_of(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc < G3X_FONT8X8_FIRST || uc >= G3X_FONT8X8_FIRST + G3X_FONT8X8_COUNT)
        uc = '?';
    return g3x_font8x8[uc - G3X_FONT8X8_FIRST];
}

// One line of glyphs: pass 0 = 1px drop shadow (+1,+1, black), pass 1 = white
// text. Shared by the tag line and the shader line.
void draw_line(uint8_t* bgra, unsigned w, unsigned h,
               unsigned tx, unsigned ty, const char* text, unsigned len) {
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned cx = tx;
        for (unsigned i = 0; i < len; ++i, cx += 8) {
            const uint8_t* g = glyph_of(text[i]);
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

} // namespace

void g3x_overlay_set_enabled(bool on) { s_enabled = on; }
bool g3x_overlay_enabled()            { return s_enabled; }

const char* g3x_overlay_tagline() { return s_tag.text; }

void g3x_overlay_set_shader(const char* name) {
    if (!name || !name[0]) {
        s_shader[0] = '\0'; s_shader_len = 0; s_shader_on = false;
        return;
    }
    // Basename, defensively: g3x_slang passes one, but this seam takes any
    // path. Whatever fits displays; 63 chars covers every sane file name.
    const char* base = name;
    for (const char* p = name; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    s_shader_len = (unsigned)std::strlen(base);
    if (s_shader_len >= sizeof(s_shader)) s_shader_len = sizeof(s_shader) - 1;
    std::memcpy(s_shader, base, s_shader_len);
    s_shader[s_shader_len] = '\0';
    s_shader_on = s_shader_len != 0;
}

void g3x_overlay_render(uint8_t* bgra, unsigned w, unsigned h) {
    if (!s_enabled || !bgra || w < 8 || h < 8) return;

    // P2.11: box anchored 4px from the TOP-RIGHT edge — the game's `/fps`
    // readout occupies the top-left corner, so the tag moved out of its way.
    // 0.4.2: up to two lines — the tag, then the active shader's file name
    // one line-step below (10px: 8px cell + 2px gap, so line 1's descenders
    // stay clear of line 2). Box: (w-4-bw, 4)..(w-4, 4+bh), clamped to the
    // buffer edges so absurdly small present targets can't write OOB.
    const unsigned pad = 3, oy = 4, step = 10;
    const bool     two = s_shader_on;
    const unsigned len = (two && s_shader_len > s_tag.len) ? s_shader_len : s_tag.len;
    unsigned bw = len * 8 + pad * 2;
    unsigned bh = 8 + (two ? step : 0) + pad * 2;
    if (bw + 4 > w) bw = w - 4;                    // keep the 4px right margin
    const unsigned ox = w - bw - 4;                // unsigned-safe: bw+4 <= w
    if (oy + bh > h) bh = h - oy;                  // tiny present targets
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

    // 2. glyphs.
    const unsigned tx = ox + pad, ty = oy + pad;
    draw_line(bgra, w, h, tx, ty, s_tag.text, s_tag.len);
    if (two)
        draw_line(bgra, w, h, tx, ty + step, s_shader, s_shader_len);
}
