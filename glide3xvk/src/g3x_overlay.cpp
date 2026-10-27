// glide3xvk — P2.10 overlay implementation. See g3x_overlay.h for the design.
#include "g3x_overlay.h"

#include "g3x_font8x8.h"

#include <cstdio>
#include <cstring>

#ifndef G3XVK_BUILD_ID
#define G3XVK_BUILD_ID "unknown"
#endif
// Short form (7-char hash + optional -dirty). Keeps the tag line compact for
// the 8x8-font box (and matches the CI artifact-name form) — the long
// describe form would blow the line budget on small preset windows.
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

// 0.4.3: transient hotkey-feedback line (F11 aspect / F12 base res). Decays
// per g3x_overlay_render call — one call == one presented frame.
char     s_toast[64];
unsigned s_toast_len   = 0;
bool     s_toast_on    = false;
unsigned s_toast_frames = 0;

// 0.4.5: the Shift+V conf panel. Lines are pushed pre-formatted by g3x_slang
// (it owns the conf parser and the live hotkey state); this module stores,
// toggles and draws. Hidden + empty until the first toggle-on.
char     s_conf_panel[G3XVK_CONF_PANEL_MAX_LINES][G3XVK_CONF_PANEL_LINE_CAP + 1];
unsigned s_conf_lens[G3XVK_CONF_PANEL_MAX_LINES] = {};
unsigned s_conf_count   = 0;
bool     s_conf_visible = false;

// 0.4.5: the arrow-key selection cursor — an index into s_conf_panel, -1 =
// none. The arrow handling (Up/Down selection, Left/Right value edits) lives
// in g3x_slang's conf-panel poll (it knows which lines are options); a
// refresh re-pushes the lines and re-points the cursor, so the two setters
// move together. Draw-time range checks make any stale index harmless.
int      s_conf_sel     = -1;

// The selection highlight color: glyphs in bright green (#20FF20-ish) over a
// dim-green band — the same hue D2's own UI uses for selected text, and the
// only non-white/non-black ink the overlay has ever used, so the cursor
// reads instantly as "the thing the arrows move".
const uint8_t k_sel_b = 0x20, k_sel_g = 0xFF, k_sel_r = 0x20;
// The band: the darkened backdrop gains this much green on the selected row
// (34 ≈ 13%), clamped — visible on the near-black box without washing out.
const unsigned k_sel_band = 34;

const uint8_t* glyph_of(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc < G3X_FONT8X8_FIRST || uc >= G3X_FONT8X8_FIRST + G3X_FONT8X8_COUNT)
        uc = '?';
    return g3x_font8x8[uc - G3X_FONT8X8_FIRST];
}

// One line of glyphs: pass 0 = 1px drop shadow (+1,+1, black), pass 1 = text
// in the given color (white everywhere except the panel's selection cursor).
// Shared by the tag line, the shader line, the toast and the conf panel.
void draw_line(uint8_t* bgra, unsigned w, unsigned h,
               unsigned tx, unsigned ty, const char* text, unsigned len,
               uint8_t cb, uint8_t cg, uint8_t cr) {
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
                    if (pass) { p[0] = cb;    p[1] = cg;    p[2] = cr; }
                    else      { p[0] = 0x00;  p[1] = 0x00;  p[2] = 0x00; }
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

void g3x_overlay_toast(const char* text) {
    if (!text || !text[0]) {
        s_toast[0] = '\0'; s_toast_len = 0; s_toast_on = false;
        return;
    }
    s_toast_len = (unsigned)std::strlen(text);
    if (s_toast_len >= sizeof(s_toast)) s_toast_len = sizeof(s_toast) - 1;
    std::memcpy(s_toast, text, s_toast_len);
    s_toast[s_toast_len] = '\0';
    s_toast_on = true;
    s_toast_frames = 120;                        // ~2s at 60fps, ~4s at D2's 30
}

void g3x_overlay_conf_panel_set(const char* const* lines, unsigned count) {
    s_conf_count = 0;
    if (!lines) return;
    for (unsigned i = 0; i < count && i < G3XVK_CONF_PANEL_MAX_LINES; ++i) {
        const char* src = lines[i];
        if (!src) continue;
        unsigned n = 0;
        while (src[n] && n < G3XVK_CONF_PANEL_LINE_CAP) {
            s_conf_panel[s_conf_count][n] = src[n];
            ++n;
        }
        s_conf_panel[s_conf_count][n] = '\0';
        s_conf_lens[s_conf_count] = n;
        ++s_conf_count;
    }
}

void g3x_overlay_conf_panel_toggle()    { s_conf_visible = !s_conf_visible; }
bool g3x_overlay_conf_panel_visible()   { return s_conf_visible; }

void g3x_overlay_conf_panel_set_selected(int line) { s_conf_sel = line; }
int  g3x_overlay_conf_panel_selected()             { return s_conf_sel; }

void g3x_overlay_render(uint8_t* bgra, unsigned w, unsigned h) {
    if (!s_enabled || !bgra || w < 8 || h < 8) return;

    // P2.11: box anchored 4px from the TOP-RIGHT edge — the game's `/fps`
    // readout occupies the top-left corner, so the tag moved out of its way.
    // 0.4.2: up to two lines — the tag, then the active shader's file name
    // one line-step below (10px: 8px cell + 2px gap, so line 1's descenders
    // stay clear of line 2). 0.4.3: optional third line — the hotkey toast,
    // same step, decaying per presented frame; the box grows for it and
    // shrinks back when it expires. Box: (w-4-bw, 4)..(w-4, 4+bh), clamped
    // to the buffer edges so absurdly small present targets can't write OOB.
    if (s_toast_on && s_toast_frames > 0) --s_toast_frames;
    else s_toast_on = false;

    const unsigned pad = 3, oy = 4, step = 10;
    const bool     two   = s_shader_on;
    const bool     three = two && s_toast_on;
    unsigned max_len = s_tag.len;
    if (two)   max_len = (s_shader_len > max_len) ? s_shader_len : max_len;
    if (three) max_len = (s_toast_len  > max_len) ? s_toast_len  : max_len;
    unsigned lines = 1 + (two ? 1u : 0u) + (three ? 1u : 0u);
    unsigned bw = max_len * 8 + pad * 2;
    unsigned bh = 8 + (lines - 1) * step + pad * 2;
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
    draw_line(bgra, w, h, tx, ty, s_tag.text, s_tag.len, 0xFF, 0xFF, 0xFF);
    if (two)
        draw_line(bgra, w, h, tx, ty + step, s_shader, s_shader_len, 0xFF, 0xFF, 0xFF);
    if (three)
        draw_line(bgra, w, h, tx, ty + step * 2, s_toast, s_toast_len, 0xFF, 0xFF, 0xFF);

    // 3. 0.4.5: the Shift+V conf panel — a LEFT-edge box (the tag owns the
    // top-right; the game's /fps readout the very top-left corner, so the
    // panel starts one line below it at y=20). Same backdrop + font + shadow
    // as the tag box; independent of the toast (both boxes can be up at
    // once). Drawn only while visible, with content, and with the overlay
    // enabled at all (overlay=0 in the ini = no overlay boxes).
    // 0.4.5 nav: the line at s_conf_sel (if any) draws as the arrow-key
    // cursor — a dim-green band across the box on that row, glyphs in bright
    // green. g3x_slang moves the cursor; this module only paints it, and a
    // stale index just paints nothing.
    if (!s_conf_visible || !s_conf_count) return;

    const unsigned ppad = 3, pox = 4, poy = 20, pstep = 10;
    unsigned pmax = 0;
    for (unsigned i = 0; i < s_conf_count; ++i)
        if (s_conf_lens[i] > pmax) pmax = s_conf_lens[i];
    unsigned pbw = pmax * 8 + ppad * 2;
    unsigned pbh = 8 + (s_conf_count - 1) * pstep + ppad * 2;
    if (pox + pbw + 4 > w)                       // keep a 4px right margin
        pbw = (w > pox + 4) ? w - pox - 4 : 0;
    if (poy + pbh > h)                           // tiny present targets
        pbh = (h > poy) ? h - poy : 0;
    if (!pbw || !pbh) return;

    const bool     sel_on = s_conf_sel >= 0 && (unsigned)s_conf_sel < s_conf_count;
    const unsigned sel_y0 = sel_on ? poy + ppad + (unsigned)s_conf_sel * pstep : 0;
    const unsigned sel_y1 = sel_y0 + 8;          // one glyph cell tall

    for (unsigned y = poy; y < poy + pbh; ++y) {
        uint8_t* row = bgra + (size_t)y * w * 4;
        for (unsigned x = pox; x < pox + pbw; ++x) {
            uint8_t* p = row + (size_t)x * 4;
            p[0] = (uint8_t)((p[0] * 89) >> 8);   // B
            p[1] = (uint8_t)((p[1] * 89) >> 8);   // G
            p[2] = (uint8_t)((p[2] * 89) >> 8);   // R
            if (sel_on && y >= sel_y0 && y < sel_y1 &&
                x > pox && x < pox + pbw - 1) {   // the cursor band (1px inset)
                const unsigned g = p[1] + k_sel_band;
                p[1] = (uint8_t)(g > 255 ? 255 : g);
            }
            p[3] = 0xFF;
        }
    }

    const unsigned ptx = pox + ppad, pty = poy + ppad;
    for (unsigned i = 0; i < s_conf_count; ++i) {
        const bool sel = sel_on && (int)i == s_conf_sel;
        draw_line(bgra, w, h, ptx, pty + i * pstep,
                  s_conf_panel[i], s_conf_lens[i],
                  sel ? k_sel_b : 0xFF, sel ? k_sel_g : 0xFF,
                  sel ? k_sel_r : 0xFF);
    }
}
