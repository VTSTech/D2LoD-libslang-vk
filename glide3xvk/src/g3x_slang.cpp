// glide3xvk — libslang-vk preset chain at the present pass (P3).
// Design contract: see g3x_slang.h.
#include "g3x_slang.h"

#include "g3x_log.h"

#include <cstdio>
#include <cstdlib>            // atof/atoi (controller_sens/deadzone)
#include <cstring>

// The portable logic below (parser + rect math) also compiles on the Linux
// host-test toolchain, where the MSVC CRT name does not exist.
#if !defined(_WIN32)
#include <strings.h>
#define _strnicmp strncasecmp
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
                    // 0.4.4 present-window preset: name validation lives in
                    // g3x_window_preset_size; here it is an opaque token.
                    std::snprintf(out->window, sizeof(out->window), "%s", val);
                } else if (_strnicmp(key, "controller", 10) == 0 && kl == 10) {
                    // 0.4.4 gamepad-to-mouse/keys (g3x_controller); fixups and
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
    }
    if (f12 && !s_prev_f12) {
        s_base_live = (s_base_live >= 4) ? 1 : s_base_live + 1;
        drop_input_up();                         // rebuilt lazily at the new size
        g3x_log("slang: F12 base res -> %dx", s_base_live);
        char msg[32];
        std::snprintf(msg, sizeof(msg), "shader base %dx", s_base_live);
        g3x_overlay_toast(msg);
    }
    s_prev_f11 = f11;
    s_prev_f12 = f12;
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
