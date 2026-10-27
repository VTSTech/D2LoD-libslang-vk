// glide3xvk — libslang-vk preset chain at the present pass (P3).
// Design contract: see g3x_slang.h.
#include "g3x_slang.h"

#include "g3x_log.h"

#include <cstdio>
#include <cstdlib>            // atof/atoi (controller_sens/deadzone)
#include <cstring>
#include <string>             // g3x_slang_conf_rewrite_text (0.4.5 panel edits)

// The portable logic below (parser + rect math) also compiles on the Linux
// host-test toolchain, where the MSVC CRT name does not exist.
#if !defined(_WIN32)
#include <strings.h>
#define _strnicmp strncasecmp
#ifndef _stricmp
#define _stricmp strcasecmp         // browser-page active-preset match
#endif
#endif

// ---------------------------------------------------------------------------
// Portable logic (host-tested; identical in real and stub builds)
// ---------------------------------------------------------------------------

bool g3x_slang_conf_parse(const char* text, G3xSlangConf* out) {
    if (!out) return false;
    std::memset(out, 0, sizeof(*out));
    out->scale = G3XSLANG_SCALE_FIT;
    out->vsync = -1;
    out->aspect = G3XSLANG_ASPECT_AUTO;
    out->base_res = 1;
    if (!text) return false;

    bool in_section = false;   // inside [slangvk]?
    const char* p = text;
    while (*p) {
        // one line at a time
        const char* eol = p;
        while (*eol && *eol != '\n' && *eol != '\r') ++eol;
        const size_t len = (size_t)(eol - p);

        // strip comment (';' or '#'), copy the raw token run
        char line[640];
        size_t n = 0;
        for (size_t i = 0; i < len && n + 1 < sizeof(line); ++i) {
            const char c = p[i];
            if (c == ';' || c == '#') break;      // comment to end of line
            line[n++] = c;
        }
        line[n] = '\0';

        // trim leading blanks
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;
        // trim trailing blanks
        size_t sl = std::strlen(s);
        while (sl && (s[sl - 1] == ' ' || s[sl - 1] == '\t')) s[--sl] = '\0';

        if (s[0] == '[') {
            const char* close = std::strchr(s, ']');
            in_section = close && _strnicmp(s, "[slangvk]",
                (size_t)(close - s)) == 0 && (size_t)(close - s) == 8;
        } else if (in_section && s[0]) {
            char* eq = std::strchr(s, '=');
            if (eq) {
                *eq = '\0';
                char* key = s;
                char* val = eq + 1;
                size_t kl = (size_t)(eq - s);
                while (kl && (key[kl - 1] == ' ' || key[kl - 1] == '\t')) key[--kl] = '\0';
                while (*val == ' ' || *val == '\t') ++val;

                if (_strnicmp(key, "preset", 6) == 0 && kl == 6) {
                    std::snprintf(out->preset, sizeof(out->preset), "%s", val);
                } else if (_strnicmp(key, "scale", 5) == 0 && kl == 5) {
                    if (_strnicmp(val, "fit", 3) == 0)           out->scale = G3XSLANG_SCALE_FIT;
                    else if (_strnicmp(val, "stretch", 7) == 0)  out->scale = G3XSLANG_SCALE_STRETCH;
                    else if (_strnicmp(val, "integer", 7) == 0)  out->scale = G3XSLANG_SCALE_INTEGER;
                    // anything else keeps the previous/default value
                } else if (_strnicmp(key, "vsync", 5) == 0 && kl == 5) {
                    if (val[0] == '0')      out->vsync = 0;
                    else if (val[0] == '1') out->vsync = 1;
                    // empty or anything else = default
                } else if (_strnicmp(key, "aspect", 6) == 0 && kl == 6) {
                    if (_strnicmp(val, "4:3", 3) == 0)        out->aspect = G3XSLANG_ASPECT_4_3;
                    else if (_strnicmp(val, "16:10", 5) == 0) out->aspect = G3XSLANG_ASPECT_16_10;
                    else if (_strnicmp(val, "16:9", 4) == 0)  out->aspect = G3XSLANG_ASPECT_16_9;
                    else if (_strnicmp(val, "auto", 4) == 0)  out->aspect = G3XSLANG_ASPECT_AUTO;
                    // anything else keeps the previous/default value
                } else if (_strnicmp(key, "base_res", 8) == 0 && kl == 8) {
                    // "1x".."4x" or the bare digit; anything else = default
                    if (val[0] == '1' || val[0] == '2' || val[0] == '3' || val[0] == '4')
                        out->base_res = val[0] - '0';
                } else if (_strnicmp(key, "borderless", 10) == 0 && kl == 10) {
                    if (val[0] == '0')      out->borderless = 0;
                    else if (val[0] == '1') out->borderless = 1;
                    // empty or anything else = default (off)
                } else if (_strnicmp(key, "window", 6) == 0 && kl == 6) {
                    // 0.4.5 present-window preset: name validation lives in
                    // g3x_window_preset_size; here it is an opaque token.
                    std::snprintf(out->window, sizeof(out->window), "%s", val);
                } else if (_strnicmp(key, "controller", 10) == 0 && kl == 10) {
                    // 0.4.5 gamepad-to-mouse/keys (g3x_controller); fixups and
                    // defaults live in g3x_ctrl_conf_fixup.
                    if (val[0] == '0')      out->controller = 0;
                    else if (val[0] == '1') out->controller = 1;
                } else if (_strnicmp(key, "controller_sens", 15) == 0 && kl == 15) {
                    // float; <=0 falls back to the module default (1.0)
                    out->controller_sens = (float)std::atof(val);
                } else if (_strnicmp(key, "controller_deadzone", 19) == 0 && kl == 19) {
                    // percent; <=0 falls back to the module default (15)
                    out->controller_deadzone = (int)std::atoi(val);
                } else if (_strnicmp(key, "controller_aimsens", 18) == 0 && kl == 18) {
                    // float; <=0 falls back to the module default (0.35)
                    out->controller_aimsens = (float)std::atof(val);
                } else if (_strnicmp(key, "controller_moveclick", 20) == 0 && kl == 20) {
                    // stick-walk: a present "0" means off (written as -1 so the
                    // key-absent zero-fill can keep meaning default-on); any
                    // other present value means on. Fixup normalizes.
                    out->controller_moveclick = (val[0] == '0') ? -1 : 1;
                } else if (_strnicmp(key, "controller_home", 15) == 0 && kl == 15) {
                    // cursor-home: same convention as moveclick (absent = on)
                    out->controller_home = (val[0] == '0') ? -1 : 1;
                }
                // unknown keys are deliberately ignored (forward compatibility)
            }
        }

        p = eol;
        while (*p == '\r' || *p == '\n') ++p;
    }
    return true;
}

G3xSlangRect g3x_slang_output_rect(int mode, unsigned in_w, unsigned in_h,
                                   unsigned out_w, unsigned out_h) {
    G3xSlangRect r = {0, 0, 0, 0};
    if (!in_w || !in_h || !out_w || !out_h) return r;   // degenerate -> library default
    if (mode == G3XSLANG_SCALE_STRETCH || mode == G3XSLANG_SCALE_FIT) {
        if (mode == G3XSLANG_SCALE_STRETCH) { r.w = out_w; r.h = out_h; }
        return r;                                        // fit: all-zero = fit + center
    }
    if (mode != G3XSLANG_SCALE_INTEGER) return r;

    // integer: k = floor(min(out/in)), at least 1; centered, clamped to target
    unsigned kx = out_w / in_w, ky = out_h / in_h;
    unsigned k = kx < ky ? kx : ky;
    if (k < 1) k = 1;
    uint64_t rw = (uint64_t)in_w * k, rh = (uint64_t)in_h * k;
    if (rw > out_w) rw = out_w;
    if (rh > out_h) rh = out_h;
    r.w = (uint32_t)rw;
    r.h = (uint32_t)rh;
    r.x = (int32_t)((out_w - r.w) / 2);
    r.y = (int32_t)((out_h - r.h) / 2);
    return r;
}

G3xSlangRect g3x_slang_aspect_rect(int aspect, unsigned out_w, unsigned out_h) {
    G3xSlangRect r = {0, 0, 0, 0};
    if (aspect <= G3XSLANG_ASPECT_AUTO || aspect >= G3XSLANG_ASPECT_COUNT) return r;
    if (!out_w || !out_h) return r;                      // degenerate -> library default

    // num : den, reduced to what each ratio needs
    const unsigned num = (aspect == G3XSLANG_ASPECT_4_3)   ? 4u
                       : (aspect == G3XSLANG_ASPECT_16_10) ? 16u : 16u;
    const unsigned den = (aspect == G3XSLANG_ASPECT_4_3)   ? 3u
                       : (aspect == G3XSLANG_ASPECT_16_10) ? 10u : 9u;

    // largest rect with that ratio that fits, centered (round half down on
    // the constrained axis so the rect never exceeds the target)
    uint64_t w_num = (uint64_t)out_h * num;
    if (w_num >= (uint64_t)out_w * den) {
        r.w = out_w;
        r.h = (uint32_t)((uint64_t)out_w * den / num);   // height-constrained
    } else {
        r.h = out_h;
        r.w = (uint32_t)(w_num / den);                   // width-constrained
    }
    if (!r.w || !r.h) return {};                         // 1px-tall targets etc.
    r.x = (int32_t)((out_w - r.w) / 2);
    r.y = (int32_t)((out_h - r.h) / 2);
    return r;
}

// ---- 0.4.5 conf-panel value editing -----------------------------------------
//
// The conf panel's Left/Right keys change the selected option by WRITING the
// new value back to d2slangvk.conf — a surgical single-key rewrite that
// keeps the file the user owns: comments, blank lines, spacing, key order
// and line endings all survive byte-for-byte; only the edited key's
// assignment changes. The pure line engine here takes the file text and
// returns the rewritten text; conf_write_key (real build, below) wraps it
// with file I/O. External linkage is deliberate — this is the host-test seam
// (scripts/test_conf_edit.cpp); it is not in the public header.
//
// Rules (mirroring the parser):
//   - assignments only count inside [slangvk] (case-insensitive, exact name)
//   - a key matches only when the WHOLE key equals the wanted name — the
//     prefix trap (controller= vs controller_sens=) must not fire
//   - commented-out assignments ('; key=...') are comments, not assignments;
//     they are left alone
//   - EVERY assignment of the key in the section becomes the new value (the
//     parser is last-one-wins, so leaving a later stray would undo the edit
//     on the next read)
//   - same-line trailing comments survive the rewrite
//   - a key not present is inserted at the end of the [slangvk] section
//     (after its last non-blank line, before trailing blanks / the next
//     header; right after the header itself when the section is empty); a
//     missing section — or missing text — appends/synthesizes one
//   - the original EOL style (\r\n, \n, lone \r) is preserved per line; new
//     lines use \r\n (the conf is a Windows file)
std::string g3x_slang_conf_rewrite_text(const char* text,
                                        const char* key, const char* value) {
    if (!key || !key[0] || !value) return text ? text : "";

    char pair[640];
    std::snprintf(pair, sizeof(pair), "%s=%s", key, value);
    const size_t klen = std::strlen(key);

    std::string out;
    const size_t tlen = text ? std::strlen(text) : 0;
    out.reserve(tlen + sizeof(pair) + 32);

    bool   in_sec = false, found = false, have_sec = false;
    size_t ins_at = 0;          // end-of-section insert point (a line boundary)

    const char* p = text ? text : "";
    while (*p) {
        const char* line = p;
        while (*p && *p != '\n' && *p != '\r') ++p;
        const size_t llen = (size_t)(p - line);

        // comment-stripped, trimmed working copy (the parser's rules)
        char work[640];
        size_t n = 0;
        for (size_t i = 0; i < llen && n + 1 < sizeof(work); ++i) {
            const char c = line[i];
            if (c == ';' || c == '#') break;
            work[n++] = c;
        }
        work[n] = '\0';
        char* s = work;
        while (*s == ' ' || *s == '\t') ++s;
        size_t sl = std::strlen(s);
        while (sl && (s[sl - 1] == ' ' || s[sl - 1] == '\t')) s[--sl] = '\0';

        bool hit    = false;
        bool is_hdr = s[0] == '[';
        if (is_hdr) {
            const char* close = std::strchr(s, ']');
            in_sec = close && _strnicmp(s, "[slangvk]", (size_t)(close - s)) == 0
                     && (size_t)(close - s) == 8;
            if (in_sec) have_sec = true;     // remember: a section exists
        } else if (in_sec && s[0]) {
            char* eq = std::strchr(s, '=');
            if (eq) {
                *eq = '\0';
                size_t kl = (size_t)(eq - s);
                while (kl && (s[kl - 1] == ' ' || s[kl - 1] == '\t')) s[--kl] = '\0';
                hit = kl == klen && _strnicmp(s, key, klen) == 0;
            }
        }

        if (hit) {
            out += pair;
            for (size_t i = 0; i < llen; ++i)    // keep a same-line comment
                if (line[i] == ';' || line[i] == '#') {
                    out += ' ';
                    out.append(line + i, llen - i);
                    break;
                }
            found = true;
        } else {
            out.append(line, llen);
        }
        // EOL run, verbatim (\r\n / \n / lone \r; runs of both count as one)
        const char* run = p;
        while (*p == '\r' || *p == '\n') out += *p++;

        // insert-point bookkeeping, after the line landed: a [slangvk] header
        // opens the section (insert right under it in an empty section), and
        // every non-blank in-section line moves the point past itself + its
        // FIRST line terminator — so a blank line carried by the same run (the
        // parser-style walk swallows "\r\n\r\n" into one run) never drags the
        // insert past it: the key lands where a hand edit would put it
        if (in_sec && (is_hdr || s[0])) {
            const size_t run_len = (size_t)(p - run);
            size_t t = 1;
            if (run_len >= 2 && ((run[0] == '\r' && run[1] == '\n') ||
                                 (run[0] == '\n' && run[1] == '\r'))) t = 2;
            ins_at = out.size() - run_len + t;
        }
    }

    if (!found) {
        std::string add;
        if (!have_sec) {                     // no [slangvk] anywhere: append one
            if (out.empty()) add += "[slangvk]\r\n";       // synthesize the file
            else             add += "\r\n[slangvk]\r\n";   // section at EOF
            add += pair;
            add += "\r\n";
            out += add;
        } else {
            if (ins_at > out.size()) ins_at = out.size();
            if (ins_at > 0 && out[ins_at - 1] != '\n' && out[ins_at - 1] != '\r')
                add += "\r\n";               // file ends mid-line (no newline)
            add += pair;
            add += "\r\n";
            out.insert(ins_at, add);         // end of the LAST [slangvk] section
        }
    }
    return out;
}

// ---- 0.4.5 preset browser page builder (portable; see g3x_slang.h) ---------
//
// Shape: "presets (N)[ - dir]" header, up to (max_lines - 2) rows of the
// entry window, "[Up/Down] select - [Enter] load - [Esc] back" footer. The
// window only ever scrolls as far as the cursor needs — the caller keeps
// *top between calls, so the view never jumps while the cursor stays inside
// it. Entry 0 is "(none)"; the conf's current preset basename is starred so
// the green cursor row and the actually-loaded preset stay distinguishable
// when they differ.
unsigned g3x_slang_browser_page(char (*lines)[G3XSLANG_BROWSER_LINE_CAP],
                                unsigned max_lines,
                                const char* const* names, unsigned count,
                                const char* active_basename,
                                const char* dir_display,
                                int* sel, unsigned* top, int* selected_out) {
    if (selected_out) *selected_out = -1;
    if (!lines || !sel || !top || max_lines < 3) return 0;
    if (!names) count = 0;

    const unsigned total = count + 1;              // entry 0 = "(none)"
    const unsigned body  = max_lines - 2;          // header + footer rows

    int s = *sel;
    if (s < 0) s = 0;
    if (s >= (int)total) s = (int)total - 1;
    unsigned t = *top;
    if (t > (unsigned)s) t = (unsigned)s;
    if ((unsigned)s - t >= body) t = (unsigned)s - body + 1;
    *sel = s;
    *top = t;

    unsigned n = 0;
    std::snprintf(lines[n++], G3XSLANG_BROWSER_LINE_CAP, "presets (%u)%s%.40s",
                  count, (dir_display && dir_display[0]) ? " - " : "",
                  dir_display ? dir_display : "");
    for (unsigned i = 0; i < body && t + i < total; ++i) {
        const unsigned e    = t + i;
        const char*    nm   = e ? names[e - 1] : "(none)";
        const bool     live = e ? (active_basename && active_basename[0] &&
                                   _stricmp(active_basename, nm) == 0)
                                : !(active_basename && active_basename[0]);
        std::snprintf(lines[n++], G3XSLANG_BROWSER_LINE_CAP, "%s%.61s",
                      live ? "* " : "  ", nm ? nm : "");
    }
    std::snprintf(lines[n++], G3XSLANG_BROWSER_LINE_CAP,
                  "[Up/Down] select - [Enter] load - [Esc] back");
    if (selected_out) *selected_out = 1 + (int)((unsigned)s - t);
    return n;
}

// ---------------------------------------------------------------------------
// Stub build (no libslang-vk checkout): same API, always inactive.
// ---------------------------------------------------------------------------

#ifndef G3XVK_HAVE_SLANGVK

static bool s_stub_active  = false;
static bool s_stub_present = false;
static unsigned s_stub_w = 0, s_stub_h = 0;
static uint8_t  s_stub_frame[1600 * 1200 * 4];

bool g3x_slang_init(void*, void*, void*, void*, unsigned) { return false; }
void g3x_slang_shutdown() {}
void g3x_slang_context_open() {}
void g3x_slang_swapchain_resized(unsigned, unsigned, int) {}
bool g3x_slang_active() { return s_stub_active; }
int  g3x_slang_vsync_pref() { return -1; }
void g3x_slang_hotkeys_poll() {}
void g3x_slang_conf_panel_poll() {}   // Win32 key polling is real-build only

bool g3x_slang_present(const uint8_t* bgra, unsigned w, unsigned h) {
    if (!s_stub_active || !s_stub_present || !bgra || !w || !h) return false;
    if ((size_t)w * h * 4 > sizeof(s_stub_frame)) return false;
    std::memcpy(s_stub_frame, bgra, (size_t)w * h * 4);
    s_stub_w = w; s_stub_h = h;
    return true;
}

void g3x_slang_test_set_active(bool on)  { s_stub_active = on; }
void g3x_slang_test_set_present(bool on) { s_stub_present = on; }
const uint8_t* g3x_slang_test_last_frame(unsigned* w, unsigned* h) {
    if (w) *w = s_stub_w;
    if (h) *h = s_stub_h;
    return s_stub_frame;
}

#else  // G3XVK_HAVE_SLANGVK — the real thing
// ---------------------------------------------------------------------------

#include "vk_backend.h"          // windows.h + the g3x_vk_slang_* services
#include "g3x_overlay.h"         // 0.4.2: the active-shader line on the overlay
#include "g3x_window.h"          // 0.4.5: the window= preset table (Left/Right cycling)

#include <volk.h>                // vulkan.h with VK_NO_PROTOTYPES (auto-defined)
#include <slangvk/c.h>           // the C API (POD only — fine from C++17)

#include <cstdio>

// 0.4.2: keep the overlay's second line truthful. Called with the resolved
// preset path when a chain goes ready and with nullptr whenever the chain
// dies (reload start, load/create failure, shutdown) — the overlay draws the
// file name only while that preset is actually filtering frames.
static void overlay_shader_line(const char* path_or_null) {
    if (!path_or_null || !path_or_null[0]) { g3x_overlay_set_shader(nullptr); return; }
    const char* base = path_or_null;
    for (const char* p = path_or_null; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    g3x_overlay_set_shader(base);
}

// 0.4.5 Shift+V conf panel (defined near the hotkeys below); reread_conf
// refreshes the panel when it is open so the file-is-truth rule shows up
// live on it too.
static void conf_panel_refresh();
static void conf_panel_refresh_if_visible();

static slangvk_device* s_dev      = nullptr;   // adopted context
static slangvk_preset* s_preset   = nullptr;
static slangvk_chain*  s_chain    = nullptr;
static slangvk_image*  s_img      = nullptr;
static unsigned        s_img_w = 0, s_img_h = 0;

static G3xSlangConf s_conf;                       // last parsed conf
static bool         s_conf_live  = false;         // conf read at least once
static bool         s_ready      = false;         // adopt succeeded
static bool         s_disabled   = true;          // no chain (not loaded / failed)
static unsigned     s_ext_w = 0, s_ext_h = 0;     // last swapchain extent
static int          s_swap_fmt   = 0;             // last swapchain format
static uint64_t     s_frame      = 0;
static unsigned     s_consec_fail = 0;
static unsigned     s_frames_filtered = 0;
static unsigned     s_frames_fallback = 0;
static uint64_t     s_conf_mtime = 0;             // d2slangvk.conf last-write (low dword enough to compare)
static uint64_t     s_preset_mtime = 0;

// 0.4.3: live hotkey state. Starts from the conf; F11/F12 cycle it for the
// session; a conf reload snaps both back to the file's values.
static int          s_aspect_live = G3XSLANG_ASPECT_AUTO;
static int          s_base_live   = 1;
static bool         s_prev_f11 = false, s_prev_f12 = false;

// 0.4.5: Shift+V edge latch for the conf panel (same scheme as F11/F12).
static bool         s_prev_shiftv = false;

// 0.4.5 nav: the panel's arrow-key selection. s_conf_sel indexes the OPTION
// lines of the pushed list (1..n-2 — the "d2slangvk.conf" header and the
// hint footer are not selectable), s_conf_opt_count is what the last
// conf_panel_refresh pushed, and s_conf_sel_key is the selected option's
// key ("aspect", "controller_sens", ...) captured at refresh time so the
// poll and the editor can name it without re-parsing. The Up/Down/Left/
// Right edge latches follow the same scheme as Shift+V/F11/F12.
static int          s_conf_sel       = 0;
static unsigned     s_conf_opt_count = 0;
static char         s_conf_sel_key[24];
static bool         s_prev_up = false, s_prev_down = false;
static bool         s_prev_left = false, s_prev_right = false;

// 0.4.5 preset browser (Enter on the preset row): the game dir's *.slangp
// list with the cursor (0 = "(none)", 1.. = the scanned names) and the
// scroll top. Scanned fresh at every open — the dir listing is read with
// the same file-is-truth rule as the conf.
static bool         s_browser_open = false;
static int          s_browser_sel  = 0;
static unsigned     s_browser_top  = 0;
static unsigned     s_browser_cnt  = 0;
static char         s_browser_names[32][64];
static bool         s_prev_enter = false, s_prev_esc = false;

// 0.4.3: the shader base resolution's upscaled input image (game res x
// s_base_live). The game frame is uploaded to s_img exactly as before, then
// one linear blit stretches it to s_img_up so the chain's source-relative
// pass FBOs all derive from the multiplied size. Null when 1x.
static slangvk_image* s_img_up = nullptr;
static unsigned       s_img_up_w = 0, s_img_up_h = 0;

static char s_exe_dir[MAX_PATH] = "";
static char s_conf_path[MAX_PATH] = "";

// ---- file helpers ----------------------------------------------------------

static bool exe_dir_of(char* out, size_t cap) {
    char exe[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return false;
    char* slash = std::strrchr(exe, '\\');
    if (!slash) return false;
    *slash = '\0';
    std::snprintf(out, cap, "%s", exe);
    return true;
}

static uint64_t file_mtime_u64(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) return 0;
    ULARGE_INTEGER u{};
    u.HighPart = fa.ftLastWriteTime.dwHighDateTime;
    u.LowPart  = fa.ftLastWriteTime.dwLowDateTime;
    return u.QuadPart;
}

// Reads a small file fully; returns malloc'd NUL-terminated text or nullptr.
static char* read_text_file(const char* path, size_t* out_len) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (1 << 20)) {
        CloseHandle(h); return nullptr;
    }
    const size_t bytes = (size_t)sz.QuadPart;
    char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, bytes + 1);
    if (!buf) { CloseHandle(h); return nullptr; }
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, (DWORD)bytes, &got, nullptr);
    CloseHandle(h);
    if (!ok) { HeapFree(GetProcessHeap(), 0, buf); return nullptr; }
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

static bool conf_path_of(char* out, size_t cap) {
    if (!s_exe_dir[0] && !exe_dir_of(s_exe_dir, sizeof(s_exe_dir))) return false;
    std::snprintf(out, cap, "%s\\d2slangvk.conf", s_exe_dir);
    return true;
}

// preset= paths resolve against the game exe dir unless they look absolute.
static void resolve_preset_path(const char* in, char* out, size_t cap) {
    if (!in[0]) { out[0] = '\0'; return; }
    const bool absolute = (in[1] == ':') || in[0] == '\\' || in[0] == '/';
    if (absolute || !s_exe_dir[0]) { std::snprintf(out, cap, "%s", in); return; }
    std::snprintf(out, cap, "%s\\%s", s_exe_dir, in);
}

static const char* scale_name(int m) {
    return m == G3XSLANG_SCALE_STRETCH ? "stretch"
         : m == G3XSLANG_SCALE_INTEGER ? "integer" : "fit";
}

static const char* aspect_name(int a) {
    switch (a) {
        case G3XSLANG_ASPECT_4_3:   return "4:3";
        case G3XSLANG_ASPECT_16_10: return "16:10";
        case G3XSLANG_ASPECT_16_9:  return "16:9";
        default:                    return "auto";
    }
}

static void drop_input_up() {
    if (s_img_up) { slangvk_image_destroy(s_img_up); s_img_up = nullptr; }
    s_img_up_w = s_img_up_h = 0;
}

// ---- chain lifecycle -------------------------------------------------------

static void destroy_chain() {
    if (s_chain) { slangvk_chain_destroy(s_chain); s_chain = nullptr; }
    s_disabled = true;
    overlay_shader_line(nullptr);   // nothing is filtering frames right now
}

static void destroy_all() {
    destroy_chain();
    drop_input_up();
    if (s_preset) { slangvk_preset_destroy(s_preset); s_preset = nullptr; }
    if (s_img)    { slangvk_image_destroy(s_img); s_img = nullptr; }
    s_img_w = s_img_h = 0;
    if (s_dev) { slangvk_device_destroy(s_dev); s_dev = nullptr; }
    s_ready = false;
}

static void reload_preset_and_chain(const char* why) {
    destroy_chain();

    char resolved[MAX_PATH * 2];
    resolve_preset_path(s_conf.preset, resolved, sizeof(resolved));

    if (!s_conf.preset[0]) {
        if (s_preset) { slangvk_preset_destroy(s_preset); s_preset = nullptr; }
        g3x_log_once("slang-off", "slang: no preset= in d2slangvk.conf — unfiltered present");
        return;
    }
    if (!s_ready || !s_dev) {
        g3x_log("slang: no device (vulkan dead?) — preset stays off");
        return;
    }

    // (Re)load the preset when it is missing or its file changed on disk.
    const uint64_t pmtime = file_mtime_u64(resolved);
    if (!s_preset || pmtime != s_preset_mtime) {
        if (s_preset) { slangvk_preset_destroy(s_preset); s_preset = nullptr; }
        const slangvk_result pr = slangvk_preset_load(s_dev, resolved, nullptr, &s_preset);
        if (pr != SLANGVK_OK) {
            g3x_log("slang: preset_load failed (%s): %s — %s",
                    slangvk_result_name(pr), slangvk_last_error(), resolved);
            s_preset_mtime = 0;
            return;                              // s_disabled stays: unfiltered present
        }
        s_preset_mtime = pmtime;
    }

    unsigned passes = 0;
    slangvk_preset_pass_count(s_preset, &passes);
    const slangvk_result cr = slangvk_chain_create(
        s_dev, s_preset, s_ext_w ? s_ext_w : 640, s_ext_h ? s_ext_h : 480,
        (VkFormat)(s_swap_fmt ? s_swap_fmt : VK_FORMAT_B8G8R8A8_UNORM), &s_chain);
    if (cr != SLANGVK_OK) {
        g3x_log("slang: chain_create failed (%s): %s", slangvk_result_name(cr),
                slangvk_last_error());
        return;
    }
    s_consec_fail = 0;
    s_disabled = false;
    overlay_shader_line(resolved);
    g3x_log("slang: chain ready '%s' (%s): %u passes, scale=%s, output %ux%u%s",
            s_conf.preset, why, passes, scale_name(s_conf.scale), s_ext_w, s_ext_h,
            s_img ? "" : " (input image deferred)");
}

static void reread_conf() {
    char path[MAX_PATH];
    if (!conf_path_of(path, sizeof(path))) return;
    s_conf_mtime = file_mtime_u64(path);         // 0 when absent
    char* text = read_text_file(path, nullptr);
    G3xSlangConf fresh;
    if (text) {
        g3x_slang_conf_parse(text, &fresh);
        HeapFree(GetProcessHeap(), 0, text);
        s_conf_live = true;
    } else {
        std::memset(&fresh, 0, sizeof(fresh));   // no file = disabled, defaults
        fresh.scale = G3XSLANG_SCALE_FIT;
        fresh.vsync = -1;
        fresh.aspect = G3XSLANG_ASPECT_AUTO;
        fresh.base_res = 1;
    }
    const bool target_changed = std::strcmp(fresh.preset, s_conf.preset) != 0;
    s_conf = fresh;
    // 0.4.3: the conf is the source of truth on every (re)read - session
    // hotkey overrides snap back to the file's values, and a base-res change
    // drops the upscaled input image (rebuilt lazily at the new multiplier).
    if (s_base_live != fresh.base_res) drop_input_up();
    s_aspect_live = fresh.aspect;
    s_base_live   = fresh.base_res;
    if (target_changed) {
        s_preset_mtime = 0;                      // force preset reload
        reload_preset_and_chain("conf");
    }
    conf_panel_refresh_if_visible();             // panel open = keep it current
}

// ---- hot reload ------------------------------------------------------------

static void hot_reload_tick() {
    if ((s_frame % 32) != 0) return;
    char path[MAX_PATH];
    if (!conf_path_of(path, sizeof(path))) return;
    const uint64_t cm = file_mtime_u64(path);
    if (cm && cm != s_conf_mtime) {
        g3x_log("slang: d2slangvk.conf changed on disk — reloading");
        reread_conf();
        return;
    }
    // preset file edited in place: recheck when a chain is live
    if (s_chain && s_conf.preset[0]) {
        char resolved[MAX_PATH * 2];
        resolve_preset_path(s_conf.preset, resolved, sizeof(resolved));
        const uint64_t pm = file_mtime_u64(resolved);
        if (pm && pm != s_preset_mtime) {
            g3x_log("slang: preset file changed on disk — rebuilding chain");
            reload_preset_and_chain("preset file");
        }
    }
}

// ---- 0.4.3 hotkeys (F11 aspect, F12 shader base resolution) ----------------

// Mirrors Chocolate-Doom-VK's in-game hotkeys. Only meaningful while a chain
// filters frames (g3x_slang_present gates this), and only when the game
// window is focused - GetAsyncKeyState is process-global otherwise. Both
// choices live for the session only; d2slangvk.conf keeps its say.
void g3x_slang_hotkeys_poll() {
    if (GetForegroundWindow() != g_vk.hwnd) {
        s_prev_f11 = s_prev_f12 = false;         // don't queue presses made elsewhere
        return;
    }
    const bool f11 = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    const bool f12 = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;

    if (f11 && !s_prev_f11) {
        // 4:3 -> 16:10 -> 16:9 -> auto -> 4:3 ...
        s_aspect_live = (s_aspect_live == G3XSLANG_ASPECT_AUTO)
                            ? G3XSLANG_ASPECT_4_3 : s_aspect_live + 1;
        if (s_aspect_live >= G3XSLANG_ASPECT_COUNT) s_aspect_live = G3XSLANG_ASPECT_AUTO;
        g3x_log("slang: F11 aspect -> %s", aspect_name(s_aspect_live));
        char msg[32];
        std::snprintf(msg, sizeof(msg), "aspect %s", aspect_name(s_aspect_live));
        g3x_overlay_toast(msg);
        conf_panel_refresh_if_visible();         // live marker moves with it
    }
    if (f12 && !s_prev_f12) {
        s_base_live = (s_base_live >= 4) ? 1 : s_base_live + 1;
        drop_input_up();                         // rebuilt lazily at the new size
        g3x_log("slang: F12 base res -> %dx", s_base_live);
        char msg[32];
        std::snprintf(msg, sizeof(msg), "shader base %dx", s_base_live);
        g3x_overlay_toast(msg);
        conf_panel_refresh_if_visible();
    }
    s_prev_f11 = f11;
    s_prev_f12 = f12;
}

// ---- 0.4.5 Shift+V conf panel ----------------------------------------------

// The panel's file-is-truth opening move, shared by the refresh, the
// Left/Right editor and the preset browser's cursor start: read + parse
// d2slangvk.conf fresh; a missing/unreadable file falls back to the
// reread_conf defaults (returns false, the call sites' old "have_file").
static bool conf_parse_fresh(G3xSlangConf* c) {
    char path[MAX_PATH];
    if (conf_path_of(path, sizeof(path))) {
        char* text = read_text_file(path, nullptr);
        if (text) {
            g3x_slang_conf_parse(text, c);
            HeapFree(GetProcessHeap(), 0, text);
            return true;
        }
    }
    std::memset(c, 0, sizeof(*c));
    c->scale    = G3XSLANG_SCALE_FIT;
    c->vsync    = -1;
    c->aspect   = G3XSLANG_ASPECT_AUTO;
    c->base_res = 1;
    return false;
}

// Rebuilds the overlay panel's lines. ALWAYS re-parses d2slangvk.conf from
// disk (the file is the truth — this runs even when no chain is live and the
// present-path hot-reload probe is parked), fills in the module defaults the
// way the consumers do (g3x_ctrl_conf_fixup for the controller floats, the
// window= empty-string-means-native rule), then marks the two hotkey-cycled
// options with their session value and, when it differs, what the file says.
// 0.4.5: with controller= off the sub-options are inert, so their rows are
// skipped (10 lines); with it on the full 15 fit the overlay panel's 18-line
// budget with room to spare. While the preset browser is open this renders
// the browser page instead (the file's preset basename still stars the live
// entry) and the option-page cursor state freezes.
static void conf_panel_refresh() {
    G3xSlangConf c;
    const bool have_file = conf_parse_fresh(&c);

    char lines[G3XVK_CONF_PANEL_MAX_LINES][G3XVK_CONF_PANEL_LINE_CAP + 1];
    const char* ptrs[G3XVK_CONF_PANEL_MAX_LINES];
    unsigned n = 0;
    auto add = [&]() -> char* {
        if (n >= G3XVK_CONF_PANEL_MAX_LINES)       // fail-soft: page never overflows
            return lines[G3XVK_CONF_PANEL_MAX_LINES - 1];
        ptrs[n] = lines[n];
        return lines[n++];
    };

    // 0.4.5 preset browser open = this refresh renders the browser page (the
    // same panel widget, the game dir's preset list instead of the options).
    // Early out BEFORE the main page's cursor clamp/capture below — that
    // state is frozen while the browser is up and must never be re-derived
    // from browser lines. Enter/Esc/Up/Down while the browser is up are the
    // poll's business.
    if (s_browser_open) {
        const char* np[32];
        for (unsigned i = 0; i < s_browser_cnt; ++i) np[i] = s_browser_names[i];
        const char* ab = c.preset;                 // basename: panel shows no paths
        for (const char* p = c.preset; *p; ++p)
            if (*p == '/' || *p == '\\') ab = p + 1;
        int sel_line = -1;
        n = g3x_slang_browser_page(lines, G3XVK_CONF_PANEL_MAX_LINES, np,
                                   s_browser_cnt, ab, s_exe_dir,
                                   &s_browser_sel, &s_browser_top, &sel_line);
        for (unsigned i = 0; i < n; ++i) ptrs[i] = lines[i];
        g3x_overlay_conf_panel_set(ptrs, n);
        g3x_overlay_conf_panel_set_selected(sel_line);
        return;
    }

    std::snprintf(add(), sizeof(lines[0]), "d2slangvk.conf%s",
                  have_file ? " [slangvk]" : " (none)");

    const char* pb = c.preset;                   // basename: the 8px-column
    for (const char* p = c.preset; *p; ++p)      // panel has no room for paths
        if (*p == '/' || *p == '\\') pb = p + 1;
    std::snprintf(add(), sizeof(lines[0]), "preset               %.48s",
                  pb[0] ? pb : "(none)");
    std::snprintf(add(), sizeof(lines[0]), "scale                %s",
                  scale_name(c.scale));
    std::snprintf(add(), sizeof(lines[0]), "vsync                %s",
                  c.vsync == 0 ? "off" : c.vsync == 1 ? "on" : "auto");
    if (s_aspect_live != c.aspect)
        std::snprintf(add(), sizeof(lines[0]), "aspect               %s (file: %s)",
                      aspect_name(s_aspect_live), aspect_name(c.aspect));
    else
        std::snprintf(add(), sizeof(lines[0]), "aspect               %s",
                      aspect_name(s_aspect_live));
    if (s_base_live != c.base_res)
        std::snprintf(add(), sizeof(lines[0]), "base_res             %dx (file: %dx)",
                      s_base_live, c.base_res);
    else
        std::snprintf(add(), sizeof(lines[0]), "base_res             %dx", s_base_live);
    std::snprintf(add(), sizeof(lines[0]), "window               %.24s",
                  c.window[0] ? c.window : "native");
    std::snprintf(add(), sizeof(lines[0]), "borderless           %s",
                  c.borderless ? "on" : "off");
    std::snprintf(add(), sizeof(lines[0]), "controller           %s",
                  c.controller ? "on" : "off");
    if (c.controller) {                          // 0.4.5: master switch off = the
                                                 // sub-options are inert; hide
                                                 // them until it is back on
        float sens = c.controller_sens;          // g3x_ctrl_conf_fixup's table
        if (sens <= 0.0f) sens = 1.0f;
        if (sens > 16.0f) sens = 16.0f;
        std::snprintf(add(), sizeof(lines[0]), "controller_sens      %.2f", sens);
        float aimsens = c.controller_aimsens;
        if (aimsens <= 0.0f) aimsens = 0.35f;
        if (aimsens < 0.05f) aimsens = 0.05f;
        if (aimsens > 4.0f)  aimsens = 4.0f;
        std::snprintf(add(), sizeof(lines[0]), "controller_aimsens   %.2f", aimsens);
        int dz = c.controller_deadzone;
        if (dz <= 0) dz = 15;
        if (dz > 80) dz = 80;
        std::snprintf(add(), sizeof(lines[0]), "controller_deadzone  %d%%", dz);
        std::snprintf(add(), sizeof(lines[0]), "controller_moveclick %s",
                      c.controller_moveclick < 0 ? "off" : "on");
        std::snprintf(add(), sizeof(lines[0]), "controller_home      %s",
                      c.controller_home < 0 ? "off" : "on");
    }
    char* footer = add();                        // text set below, after the
                                                 // cursor's key is captured

    // 0.4.5 nav: the option lines sit between the header (line 0) and the
    // hint footer (the last line). Clamp the cursor to that range so a
    // refresh can never leave it pointing at the header, the footer or past
    // the end (the formatter's shape is fixed today, but this stays correct
    // if it ever isn't), then capture the selected option's key for the
    // poll's log line.
    s_conf_opt_count = (n >= 3) ? n - 2 : 0;
    if (s_conf_sel < 0) s_conf_sel = 0;
    if ((unsigned)s_conf_sel >= s_conf_opt_count)
        s_conf_sel = s_conf_opt_count ? (int)s_conf_opt_count - 1 : 0;
    s_conf_sel_key[0] = '\0';
    if (s_conf_opt_count) {
        const char* ln = lines[s_conf_sel + 1];
        unsigned k = 0;
        while (ln[k] && ln[k] != ' ' && k < sizeof(s_conf_sel_key) - 1) {
            s_conf_sel_key[k] = ln[k];
            ++k;
        }
        s_conf_sel_key[k] = '\0';
    }

    // 0.4.5: the footer hints the cursor row's extra action — on the preset
    // row Enter opens the preset browser (the game dir's *.slangp list).
    std::snprintf(footer, sizeof(lines[0]), "%s",
                  !_stricmp(s_conf_sel_key, "preset")
                      ? "[Up/Down] select - [Left/Right] cycle - [Enter] list - [Shift+V] hide"
                      : "[Up/Down] select - [Left/Right] change - [Shift+V] hide");

    g3x_overlay_conf_panel_set(ptrs, n);
    g3x_overlay_conf_panel_set_selected(
        s_conf_opt_count ? s_conf_sel + 1 : -1);   // +1: skip the header
}

static void conf_panel_refresh_if_visible() {
    if (g3x_overlay_conf_panel_visible()) conf_panel_refresh();
}

// ---- 0.4.5 value editing (Left/Right on the selected option) ----------------

// Lists the game dir's *.slangp presets (sorted, case-insensitive) for the
// preset row's Left/Right cycling. The conf's preset= resolves relative
// paths against this dir, so a scanned basename round-trips through the
// parser exactly. Subdirectory presets stay file-edit-only — the panel
// shows basenames only anyway (an 8px-column list has no room for paths).
static bool scan_presets(char (*names)[64], unsigned max, unsigned* count) {
    *count = 0;
    if (!s_exe_dir[0] && !exe_dir_of(s_exe_dir, sizeof(s_exe_dir))) return false;
    char pat[MAX_PATH];
    std::snprintf(pat, sizeof(pat), "%s\\*.slangp", s_exe_dir);
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return true;    // none found is not an error
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (*count >= max) break;
        std::snprintf(names[*count], 64, "%s", fd.cFileName);
        unsigned i = (*count)++;                   // insertion sort, ci-order
        while (i && _stricmp(names[i], names[i - 1]) < 0) {
            char tmp[64];
            std::memcpy(tmp, names[i], sizeof(tmp));
            std::memcpy(names[i], names[i - 1], sizeof(tmp));
            std::memcpy(names[i - 1], tmp, sizeof(tmp));
            --i;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return true;
}

// File I/O around g3x_slang_conf_rewrite_text: read d2slangvk.conf, rewrite
// <key>=<value>, write the result back whole. CREATE_ALWAYS truncates in one
// call and FILE_SHARE_READ keeps the other conf readers (window/controller
// ticks, our own hot-reload probe) able to open the file meanwhile — they
// re-probe mtime and take the change as a normal on-disk edit, which is
// exactly the live-apply path this feature rides.
static bool conf_write_key(const char* key, const char* value) {
    if (!key || !key[0] || !value) return false;
    char path[MAX_PATH];
    if (!conf_path_of(path, sizeof(path))) return false;

    char* text = read_text_file(path, nullptr);    // no file yet = synthesized
    std::string out = g3x_slang_conf_rewrite_text(text, key, value);
    if (text) HeapFree(GetProcessHeap(), 0, text);
    if (out.empty()) return false;                 // rewrite never returns empty

    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const BOOL ok = WriteFile(h, out.data(), (DWORD)out.size(), &wrote, nullptr);
    CloseHandle(h);
    return ok && wrote == (DWORD)out.size();
}

// One Left/Right step on the selected option. Reads + parses the conf fresh
// (the file is the truth — the same opening move as conf_panel_refresh, and
// the panel's display lags at most one refresh behind the file), picks the
// neighbor value for the selected key, and writes it back with the surgical
// single-key rewrite. Returns false (nothing touched) at a list end, when
// there is nothing to cycle (no .slangp files in the game dir), or when the
// write fails. On success *desc carries the new value's display text for
// the toast/log; the caller then runs reread_conf(), which routes the edit
// through the exact machinery a hand edit takes — aspect/base_res snap
// immediately (the session values re-sync to the file), scale is read per
// frame, vsync at the next swapchain (re)creation, and window=/borderless=/
// controller= on their own mtime probes (32nd frame / ~2 s). Steps and
// clamps mirror the consumers' own fixups (g3x_ctrl_conf_fixup, the parser's
// token lists, the window preset table), so what the arrows write is always
// a value the parser accepts and the panel displays.
static bool conf_panel_edit(int dir, char* desc, size_t desc_cap) {
    if (desc && desc_cap) desc[0] = '\0';
    if (!dir || !s_conf_opt_count || !s_conf_sel_key[0]) return false;
    dir = dir < 0 ? -1 : 1;

    G3xSlangConf c;
    conf_parse_fresh(&c);                      // same opening move as the refresh

    const char* key = s_conf_sel_key;
    char val[560];                             // preset= caps at 48 in the panel,
    val[0] = '\0';                             // 512 in the struct; both fit

    if (!_stricmp(key, "preset")) {
        char names[32][64];
        unsigned cnt = 0;
        if (!scan_presets(names, 32, &cnt) || !cnt) return false;
        const char* cur = c.preset;            // basename: what the panel shows
        for (const char* q = c.preset; *q; ++q)
            if (*q == '/' || *q == '\\') cur = q + 1;
        int idx = -1;
        for (unsigned i = 0; i < cnt; ++i)
            if (!_stricmp(cur, names[i])) { idx = (int)i; break; }
        const int nx = idx + dir;              // -1 = "(none)", the list front
        if (nx < -1 || nx >= (int)cnt) return false;
        if (nx >= 0) std::snprintf(val, sizeof(val), "%s", names[nx]);
        std::snprintf(desc, desc_cap, "%.44s", nx < 0 ? "(none)" : names[nx]);
    } else if (!_stricmp(key, "scale")) {
        static const char* const kScale[3] = { "fit", "stretch", "integer" };
        int i = c.scale;
        if (i < 0 || i > 2) i = G3XSLANG_SCALE_FIT;
        const int nx = i + dir;
        if (nx < 0 || nx > 2) return false;
        std::snprintf(val, sizeof(val), "%s", kScale[nx]);
        std::snprintf(desc, desc_cap, "%s", kScale[nx]);
    } else if (!_stricmp(key, "vsync")) {
        int i = c.vsync;                       // -1 auto, 0 off, 1 on
        if (i < -1 || i > 1) i = -1;
        const int nx = i + dir;
        if (nx < -1 || nx > 1) return false;
        std::snprintf(val, sizeof(val), "%s", nx < 0 ? "" : nx ? "1" : "0");
        std::snprintf(desc, desc_cap, "%s", nx < 0 ? "auto" : nx ? "on" : "off");
    } else if (!_stricmp(key, "aspect")) {
        int i = c.aspect;
        if (i < 0 || i >= G3XSLANG_ASPECT_COUNT) i = G3XSLANG_ASPECT_AUTO;
        const int nx = i + dir;
        if (nx < 0 || nx >= G3XSLANG_ASPECT_COUNT) return false;
        std::snprintf(val, sizeof(val), "%s", aspect_name(nx));
        std::snprintf(desc, desc_cap, "%s", aspect_name(nx));
    } else if (!_stricmp(key, "base_res")) {
        int i = c.base_res;
        if (i < 1 || i > 4) i = 1;
        const int nx = i + dir;
        if (nx < 1 || nx > 4) return false;
        std::snprintf(val, sizeof(val), "%dx", nx);
        std::snprintf(desc, desc_cap, "%dx", nx);
    } else if (!_stricmp(key, "window")) {
        int i = g3x_window_preset_index(c.window[0] ? c.window : "native");
        if (i < 0) i = 0;                      // WxH/junk arrows from native
        unsigned cnt = 0;
        const G3xWindowPreset* t = g3x_window_preset_table(&cnt);
        const int nx = i + dir;
        if (nx < 0 || nx >= (int)cnt) return false;
        std::snprintf(val, sizeof(val), "%s", t[nx].name);
        std::snprintf(desc, desc_cap, "%s", t[nx].name);
    } else if (!_stricmp(key, "borderless")) {
        std::snprintf(val, sizeof(val), "%d", c.borderless ? 0 : 1);
        std::snprintf(desc, desc_cap, "%s", c.borderless ? "off" : "on");
    } else if (!_stricmp(key, "controller")) {
        std::snprintf(val, sizeof(val), "%d", c.controller ? 0 : 1);
        std::snprintf(desc, desc_cap, "%s", c.controller ? "off" : "on");
    } else if (!_stricmp(key, "controller_sens")) {
        float v = c.controller_sens;           // g3x_ctrl_conf_fixup's rules
        if (v <= 0.0f) v = 1.0f;
        if (v > 16.0f) v = 16.0f;
        float nv = v + 0.25f * dir;
        if (nv < 0.25f) nv = 0.25f;
        if (nv > 16.0f) nv = 16.0f;
        if (nv == v) return false;
        std::snprintf(val, sizeof(val), "%.2f", nv);
        std::snprintf(desc, desc_cap, "%.2f", nv);
    } else if (!_stricmp(key, "controller_aimsens")) {
        float v = c.controller_aimsens;
        if (v <= 0.0f) v = 0.35f;
        if (v < 0.05f) v = 0.05f;
        if (v > 4.0f)  v = 4.0f;
        float nv = v + 0.05f * dir;
        if (nv < 0.05f) nv = 0.05f;
        if (nv > 4.0f)  nv = 4.0f;
        if (nv == v) return false;
        std::snprintf(val, sizeof(val), "%.2f", nv);
        std::snprintf(desc, desc_cap, "%.2f", nv);
    } else if (!_stricmp(key, "controller_deadzone")) {
        int v = c.controller_deadzone;
        if (v <= 0) v = 15;
        if (v < 1)  v = 1;
        if (v > 80) v = 80;
        int nv = v + 5 * dir;
        if (nv < 1)  nv = 1;
        if (nv > 80) nv = 80;
        if (nv == v) return false;
        std::snprintf(val, sizeof(val), "%d", nv);
        std::snprintf(desc, desc_cap, "%d%%", nv);
    } else if (!_stricmp(key, "controller_moveclick")) {
        const int nx = c.controller_moveclick < 0 ? 1 : 0;   // toggles
        std::snprintf(val, sizeof(val), "%d", nx);
        std::snprintf(desc, desc_cap, "%s", nx ? "on" : "off");
    } else if (!_stricmp(key, "controller_home")) {
        const int nx = c.controller_home < 0 ? 1 : 0;
        std::snprintf(val, sizeof(val), "%d", nx);
        std::snprintf(desc, desc_cap, "%s", nx ? "on" : "off");
    } else {
        return false;                          // unknown option: nothing to edit
    }

    return conf_write_key(key, val);
}

// ---- 0.4.5 preset browser (Enter on the panel's preset row) ----------------

// Opens the browser page: scans the game dir fresh (the listing is read like
// the conf — at open), and the cursor starts on the conf's current preset, or
// on "(none)" when there is none / it is not in the dir. An empty dir is a
// toast, not an empty page — there is nothing useful to show or pick.
static void conf_panel_browser_open() {
    char names[32][64];
    unsigned cnt = 0;
    if (!scan_presets(names, 32, &cnt) || !cnt) {
        g3x_overlay_toast("no .slangp in game dir");
        g3x_log("overlay: preset browser empty (no .slangp in game dir)");
        return;
    }
    for (unsigned i = 0; i < cnt; ++i)
        std::snprintf(s_browser_names[i], sizeof(s_browser_names[0]), "%s",
                      names[i]);
    s_browser_cnt = cnt;

    G3xSlangConf c;                            // cursor starts on the live preset
    conf_parse_fresh(&c);
    const char* cur = c.preset;                // basename: what the panel shows
    for (const char* q = c.preset; *q; ++q)
        if (*q == '/' || *q == '\\') cur = q + 1;
    s_browser_sel = 0;
    if (cur[0]) {
        for (unsigned i = 0; i < cnt; ++i)
            if (!_stricmp(cur, s_browser_names[i])) {
                s_browser_sel = (int)i + 1;
                break;
            }
    }
    s_browser_top  = 0;
    s_browser_open = true;
    conf_panel_refresh();                      // renders the page, clamps the view
    char msg[40];
    std::snprintf(msg, sizeof(msg), "presets: %u", cnt);
    g3x_overlay_toast(msg);
    g3x_log("overlay: preset browser open (%u found)", cnt);
}

// Enter in the browser: write the highlighted entry into preset= (the same
// surgical single-key write the Left/Right cycling uses; "(none)" writes an
// empty value, i.e. the key present but blank) and hand the change to
// reread_conf — which reloads the chain on a real change and lands the panel
// back on the option page (the browser closes first). Selecting the
// already-active preset rewrites the same value: the reload is skipped by
// the mtime logic, the toast still confirms.
static void conf_panel_browser_load() {
    char val[64] = "";
    if (s_browser_sel > 0 && (unsigned)s_browser_sel <= s_browser_cnt)
        std::snprintf(val, sizeof(val), "%s", s_browser_names[s_browser_sel - 1]);
    char desc[64];
    std::snprintf(desc, sizeof(desc), "%.44s", val[0] ? val : "(none)");
    if (conf_write_key("preset", val)) {
        s_browser_open = false;
        reread_conf();                         // file-is-truth: reload + refresh
        char msg[80];
        std::snprintf(msg, sizeof(msg), "preset %s", desc);
        g3x_overlay_toast(msg);
        g3x_log("overlay: conf preset = %s", desc);
    } else {
        g3x_log("overlay: conf preset unchanged (write failed)");
    }
}

// Edge-polled from the present path (g3x_frame_present) on every frame, so
// the panel works with no chain active — the F11/F12 gate lives in
// g3x_slang_present, this one is its always-on sibling. Same focus rule:
// presses made while another window is foreground never toggle.
void g3x_slang_conf_panel_poll() {
    if (GetForegroundWindow() != g_vk.hwnd) {
        s_prev_shiftv = false;                   // don't queue presses made elsewhere
        s_prev_up     = false;
        s_prev_down   = false;
        s_prev_left   = false;
        s_prev_right  = false;
        s_prev_enter  = false;
        s_prev_esc    = false;
        return;
    }
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool v     = (GetAsyncKeyState(0x56 /* 'V' */) & 0x8000) != 0;
    const bool down  = shift && v;
    if (down && !s_prev_shiftv) {
        if (g3x_overlay_conf_panel_visible()) {
            s_browser_open = false;              // hide resets the browser too:
            g3x_overlay_conf_panel_toggle();     // reopen lands on the option page
            g3x_overlay_toast("conf overlay off");
            g3x_log("overlay: conf panel off (Shift+V)");
        } else {
            conf_panel_refresh();
            g3x_overlay_conf_panel_toggle();
            g3x_overlay_toast("conf overlay on");
            g3x_log("overlay: conf panel on (Shift+V)");
        }
    }
    s_prev_shiftv = down;

    // 0.4.5: Up/Down move the green selection cursor while the panel is up.
    // Same focus rule (unfocused presses are latched off above), edge-latch
    // like every other hotkey here, clamped at the ends — no wrap: the top
    // and bottom options sit one press away from the conf's first/last key,
    // and a wrap on a list this short reads as a glitch. Each actual move
    // re-runs conf_panel_refresh, so the cursor rides a fresh parse (the
    // file-is-truth rule holds even for selection redraws) and the panel
    // picks up any conf edit that landed since the last refresh. The arrows
    // are untouched while the panel is hidden — the game keeps them.
    const bool up   = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    const bool down2 = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
    if (g3x_overlay_conf_panel_visible()) {
        if (s_browser_open) {
            // Browser up: the arrows move the preset list cursor — clamped,
            // no wrap, same feel as the option list. Each move re-renders
            // (the starred live entry and the scroll view follow).
            const unsigned total = s_browser_cnt + 1;
            if (up && !s_prev_up && s_browser_sel > 0) {
                --s_browser_sel;
                conf_panel_refresh();
                g3x_log("overlay: preset browser sel %s",
                        s_browser_sel ? s_browser_names[s_browser_sel - 1]
                                      : "(none)");
            }
            if (down2 && !s_prev_down && (unsigned)s_browser_sel < total - 1) {
                ++s_browser_sel;
                conf_panel_refresh();
                g3x_log("overlay: preset browser sel %s",
                        s_browser_sel ? s_browser_names[s_browser_sel - 1]
                                      : "(none)");
            }
        } else {
            if (up && !s_prev_up && s_conf_opt_count && s_conf_sel > 0) {
                --s_conf_sel;
                conf_panel_refresh();
                g3x_log("overlay: conf panel sel %s", s_conf_sel_key);
            }
            if (down2 && !s_prev_down && s_conf_opt_count &&
                (unsigned)s_conf_sel < s_conf_opt_count - 1) {
                ++s_conf_sel;
                conf_panel_refresh();
                g3x_log("overlay: conf panel sel %s", s_conf_sel_key);
            }
        }
    }
    s_prev_up   = up;
    s_prev_down = down2;

    // 0.4.5: Enter on the preset row opens the preset browser; in the
    // browser Enter LOADS the highlighted preset and Esc goes back to the
    // option page. Same focus rule and edge latches as the other panel keys
    // (Enter and Esc also reach the game, like every polled key here — the
    // panel is a read/write view, not an input grab).
    const bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    const bool esc   = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    if (g3x_overlay_conf_panel_visible()) {
        if (enter && !s_prev_enter) {
            if (s_browser_open) {
                conf_panel_browser_load();
            } else if (s_conf_opt_count &&
                       !_stricmp(s_conf_sel_key, "preset")) {
                conf_panel_browser_open();
            }
        }
        if (esc && !s_prev_esc && s_browser_open) {
            s_browser_open = false;
            conf_panel_refresh();
            g3x_log("overlay: preset browser back");
        }
    }
    s_prev_enter = enter;
    s_prev_esc   = esc;

    // 0.4.5: Left/Right CHANGE the selected option. Each press is one step
    // (enum neighbor, +/- one increment, or a toggle — conf_panel_edit's
    // rules) written straight into d2slangvk.conf, then pushed through
    // reread_conf() so the change lands via the exact path a hand edit
    // takes: the panel re-reads the file it just wrote, aspect/base_res
    // snap live, and the window/controller/slang consumers pick the file up
    // on their own mtime probes. Same focus rule and edge latches as
    // Up/Down; the arrows stay the game's while the panel is hidden, and
    // they are inert while the browser is up (the option page is frozen).
    const bool lft   = (GetAsyncKeyState(VK_LEFT)  & 0x8000) != 0;
    const bool rgt   = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    if (g3x_overlay_conf_panel_visible() && !s_browser_open) {
        int dir = 0;
        if (rgt && !s_prev_right) dir = 1;
        else if (lft && !s_prev_left) dir = -1;
        if (dir) {
            char desc[64];
            if (conf_panel_edit(dir, desc, sizeof(desc))) {
                reread_conf();               // file-is-truth: reparse + refresh,
                                             // snap aspect/base, reload on preset=
                char msg[80];
                std::snprintf(msg, sizeof(msg), "%s %s", s_conf_sel_key, desc);
                g3x_overlay_toast(msg);
                g3x_log("overlay: conf %s = %s", s_conf_sel_key, desc);
            } else {
                g3x_log("overlay: conf %s unchanged (list end / nothing to cycle / write failed)",
                        s_conf_sel_key);
            }
        }
    }
    s_prev_left  = lft;
    s_prev_right = rgt;
}

// ---- public API ------------------------------------------------------------

bool g3x_slang_init(void* instance, void* physical_device, void* device,
                    void* queue, unsigned queue_family) {
    std::memset(&s_conf, 0, sizeof(s_conf));
    s_conf.scale = G3XSLANG_SCALE_FIT;
    s_conf.vsync = -1;
    s_conf.aspect = G3XSLANG_ASPECT_AUTO;
    s_conf.base_res = 1;
    s_aspect_live = G3XSLANG_ASPECT_AUTO;
    s_base_live = 1;

    if (slangvk_device_adopt((VkInstance)instance, (VkPhysicalDevice)physical_device,
                             (VkDevice)device, (VkQueue)queue, queue_family,
                             &s_dev) != SLANGVK_OK) {
        g3x_log("slang: device adopt failed (%s) — preset chain unavailable",
                slangvk_last_error());
        s_dev = nullptr;
        return false;
    }
    s_ready = true;
    slangvk_device_info info{};
    if (slangvk_device_get_info(s_dev, &info) == SLANGVK_OK) {
        g3x_log("slang: device adopted — %s", info.device_name);
    }

    g3x_vk_slang_swap_extent(&s_ext_w, &s_ext_h);
    s_swap_fmt = g3x_vk_slang_swap_format();
    reread_conf();                                // loads preset + chain when configured
    return true;
}

void g3x_slang_shutdown() {
    if (s_ready || s_dev) {
        if (s_dev) slangvk_device_wait_idle(s_dev);
        destroy_all();
        g3x_log("slang: shut down (frames filtered: %u, fell back: %u)",
                s_frames_filtered, s_frames_fallback);
    }
}

void g3x_slang_context_open() {
    // Same re-read cycle as hot reload, minus the mtime probe: conf may have
    // appeared/changed while the game was in a menu-less state.
    if (!s_ready) return;
    reread_conf();
}

void g3x_slang_swapchain_resized(unsigned out_w, unsigned out_h, int swap_format) {
    s_ext_w = out_w; s_ext_h = out_h; s_swap_fmt = swap_format;
    if (!s_chain) return;                         // reload_preset_and_chain() sizes later
    const slangvk_result r = slangvk_chain_resize(s_chain, out_w, out_h);
    if (r != SLANGVK_OK) {
        g3x_log("slang: chain_resize failed (%s) — falling back this session",
                slangvk_result_name(r));
        destroy_chain();
    } else {
        g3x_log("slang: chain resized to %ux%u", out_w, out_h);
    }
}

int g3x_slang_vsync_pref() { return s_conf_live ? s_conf.vsync : -1; }

bool g3x_slang_active() { return s_ready && !s_disabled && s_chain != nullptr; }

bool g3x_slang_present(const uint8_t* bgra, unsigned w, unsigned h) {
    if (!g3x_slang_active() || !bgra || !w || !h) return false;
    ++s_frame;
    g3x_slang_hotkeys_poll();
    hot_reload_tick();
    if (!g3x_slang_active()) return false;        // hot reload may have torn it down

    // (Re)create the input image when the game resolution changed.
    if (!s_img || s_img_w != w || s_img_h != h) {
        if (s_img) { slangvk_image_destroy(s_img); s_img = nullptr; }
        slangvk_image_options io{};
        io.width = w; io.height = h;
        io.format = VK_FORMAT_B8G8R8A8_UNORM;     // compose bytes are B8G8R8A8 in memory
        const slangvk_result ir = slangvk_image_create(s_dev, &io, &s_img);
        if (ir != SLANGVK_OK) {
            g3x_log("slang: image_create %ux%u failed (%s)", w, h,
                    slangvk_result_name(ir));
            destroy_chain();                      // fail this session, keep the game alive
            return false;
        }
        s_img_w = w; s_img_h = h;
        g3x_log("slang: input image %ux%u B8G8R8A8", w, h);
    }

    const slangvk_result ur = slangvk_image_update(s_img, bgra, (size_t)w * h * 4);
    if (ur != SLANGVK_OK) {
        g3x_log_once("slang-upd", "slang: image_update failed (%s)",
                     slangvk_result_name(ur));
        return false;
    }

    // 0.4.3: shader base resolution. 1x feeds s_img directly; N>1 lazily
    // creates the multiplied image and stretches the frame into it with one
    // linear blit (both slangvk_image_update and our blit are fenced-waited,
    // so the chain's own submission sees everything with no extra semaphores).
    slangvk_image* feed_img = s_img;
    unsigned feed_w = w, feed_h = h;
    if (s_base_live > 1) {
        const unsigned upw = w * s_base_live, uph = h * s_base_live;
        if (!s_img_up || s_img_up_w != upw || s_img_up_h != uph) {
            drop_input_up();
            slangvk_image_options uo{};
            uo.width = upw; uo.height = uph;
            uo.format = VK_FORMAT_B8G8R8A8_UNORM;
            if (slangvk_image_create(s_dev, &uo, &s_img_up) == SLANGVK_OK) {
                s_img_up_w = upw; s_img_up_h = uph;
                g3x_log("slang: base %dx input image %ux%u", s_base_live, upw, uph);
            } else {
                g3x_log_once("slang-upimg", "slang: base-res image_create %ux%u failed "
                           "- 1x until further notice", upw, uph);
                s_img_up = nullptr;
            }
        }
        if (s_img_up) {
            VkImageView sv = VK_NULL_HANDLE, dv = VK_NULL_HANDLE;
            VkImage si = VK_NULL_HANDLE, di = VK_NULL_HANDLE;
            VkFormat sf = VK_FORMAT_UNDEFINED, df = VK_FORMAT_UNDEFINED;
            unsigned suw = 0, suh = 0, duw = 0, duh = 0;
            slangvk_image_get_handles(s_img, &sv, &si, &sf, &suw, &suh);
            slangvk_image_get_handles(s_img_up, &dv, &di, &df, &duw, &duh);
            if (si && di && g3x_vk_slang_blit_scaled((void*)si, (void*)di,
                                                     w, h, upw, uph)) {
                feed_img = s_img_up; feed_w = upw; feed_h = uph;
            } else {
                g3x_log_once("slang-blit", "slang: base-res blit failed - 1x this frame");
            }
        }
    }

    uint32_t idx = 0;
    if (!g3x_vk_slang_acquire(&idx)) return false;

    slangvk_input_frame in{};
    slangvk_image_get_handles(feed_img, &in.view, &in.image, &in.format, &in.width, &in.height);
    in.rotation = SLANGVK_ROTATION_NONE;

    unsigned ew = 0, eh = 0;
    g3x_vk_slang_swap_extent(&ew, &eh);
    slangvk_swapchain_frame out{};
    out.swapchain_image_view = (VkImageView)g3x_vk_slang_swap_view(idx);
    out.format   = (VkFormat)g3x_vk_slang_swap_format();
    out.width    = ew;
    out.height   = eh;
    const G3xSlangRect rect = (s_aspect_live != G3XSLANG_ASPECT_AUTO)
        ? g3x_slang_aspect_rect(s_aspect_live, ew, eh)
        : g3x_slang_output_rect(s_conf.scale, feed_w, feed_h, ew, eh);
    out.output_rect.x = rect.x; out.output_rect.y = rect.y;
    out.output_rect.width = rect.w; out.output_rect.height = rect.h;
    out.acquire_semaphore = (VkSemaphore)g3x_vk_slang_acquire_semaphore();

    slangvk_frame_info fi{};
    fi.frame_count = s_frame;
    fi.frame_delta_us = 16666.0f;                 // ~60 Hz reference; presets only ease with it
    fi.frame_direction = +1;

    const slangvk_result rr = slangvk_chain_render_swapchain(s_chain, &in, &out, &fi);
    if (rr != SLANGVK_OK) {
        ++s_consec_fail;
        g3x_log("slang: render failed (%s)%s", slangvk_result_name(rr),
                s_consec_fail >= 3 ? " — chain disabled, unfiltered present" : "");
        if (s_consec_fail >= 3) destroy_chain();
        ++s_frames_fallback;
        return false;                             // legacy path presents this frame
    }
    s_consec_fail = 0;

    g3x_vk_slang_present_acquired(idx);
    ++s_frames_filtered;
    return true;
}

#endif // G3XVK_HAVE_SLANGVK
