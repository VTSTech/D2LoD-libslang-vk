// glide3xvk — 0.4.6 PrintScreen screenshots (see g3x_shot.h for the contract).
#include "g3x_shot.h"

#include "g3x_log.h"
#include "vk_backend.h"     // g_vk.hwnd — the foreground gate

#include <cstdio>
#include <cstring>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// ---- state -----------------------------------------------------------------

static bool s_enabled = true;         // [slangvk] screenshots= (1 default)
static bool s_pending = false;        // a focused PrintScreen press to serve
static bool s_prev_print = false;     // PrintScreen edge latch

void g3x_shot_set_enabled(bool on) { s_enabled = on; }
bool g3x_shot_enabled() { return s_enabled; }

void g3x_shot_poll() {
    if (GetForegroundWindow() != g_vk.hwnd) {
        s_prev_print = false;                  // don't queue presses made elsewhere
        return;
    }
    const bool print = (GetAsyncKeyState(VK_SNAPSHOT) & 0x8000) != 0;
    if (print && !s_prev_print) {
        if (s_enabled) {
            s_pending = true;                  // served at this frame's capture point
        } else {
            g3x_log_once("shot-off", "shot: PrintScreen ignored (screenshots=0 in d2slangvk.conf)");
        }
    }
    s_prev_print = print;
}

bool g3x_shot_take_pending() {
    const bool p = s_pending;
    s_pending = false;
    return p;
}

// ---- PNG pieces (dependency-free; see g3x_shot.h) ---------------------------
//
// Minimal standards-conformant encoder: 8-bit truecolor RGB (color type 2),
// filter type 0 per row, a zlib stream made of STORED deflate blocks (no
// compression — an 800x600 shot is ~1.4 MB; deterministic bytes and ~150
// lines of code beat ratio for an in-game hotkey), table-driven CRC-32 and
// the standard adler-32. Verified shapes: every decoder (Explorer, browser,
// image tools) accepts stored blocks — they are the deflate format's
// baseline, not an extension.

static uint32_t s_crc_table[256];
static bool     s_crc_ready = false;

static void crc_table_init() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        s_crc_table[i] = c;
    }
    s_crc_ready = true;
}

static uint32_t crc32_buf(const uint8_t* p, size_t n) {
    if (!s_crc_ready) crc_table_init();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i)
        c = s_crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static uint32_t adler32_buf(const uint8_t* p, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static void be32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

// One PNG chunk: BE32 length, type, data, CRC-32 over type+data.
static void png_chunk(std::vector<uint8_t>& out, const char* type,
                      const uint8_t* data, size_t len) {
    be32(out, (uint32_t)len);
    const size_t t0 = out.size();
    for (int i = 0; i < 4; ++i) out.push_back((uint8_t)type[i]);
    if (len) out.insert(out.end(), data, data + len);
    const uint32_t crc = crc32_buf(&out[t0], out.size() - t0);
    be32(out, crc);
}

static bool write_file(const char* path, const uint8_t* bytes, size_t len) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    // Chunked writes: a >2 GB-boundary can never happen here (a 32768x32768
    // RGB stream is ~3 GB worst case, but D2's 800x600 payloads are ~1.4 MB),
    // so a straight WriteFile of the whole span is fine; loop only on partial
    // writes.
    const size_t total = len;
    size_t done = 0;
    BOOL ok = TRUE;
    while (ok && done < total) {
        const DWORD take = (DWORD)((total - done > 0x40000000u)
                                       ? 0x40000000u : (total - done));
        DWORD wrote = 0;
        ok = WriteFile(h, bytes + done, take, &wrote, nullptr);
        if (!ok || wrote != take) { ok = FALSE; break; }
        done += wrote;
    }
    CloseHandle(h);
    return ok && done == total;
}

// BGRA8 -> RGB PNG. The deflate input is h rows of [filter byte 0][R G B]*;
// the same byte stream is what the adler-32 covers.
static bool write_png(const char* path, const uint8_t* bgra,
                      unsigned w, unsigned h) {
    if (!path || !bgra || !w || !h || w > 32768 || h > 32768) return false;

    const size_t stride = (size_t)w * 3 + 1;      // filter byte + RGB
    const size_t raw_len = (size_t)h * stride;

    std::vector<uint8_t> png;
    png.reserve(raw_len * 2 + 4096);   // IDAT payload + its in-chunk copy

    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    png.insert(png.end(), sig, sig + 8);

    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)(w >> 24); ihdr[1] = (uint8_t)(w >> 16);
    ihdr[2] = (uint8_t)(w >> 8);  ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)(h >> 24); ihdr[5] = (uint8_t)(h >> 16);
    ihdr[6] = (uint8_t)(h >> 8);  ihdr[7] = (uint8_t)h;
    ihdr[8] = 8;                  // bit depth
    ihdr[9] = 2;                  // color type: truecolor RGB
    ihdr[10] = 0;                 // compression: deflate
    ihdr[11] = 0;                 // filter method: 0 (per-row type 0 used)
    ihdr[12] = 0;                 // interlace: none
    png_chunk(png, "IHDR", ihdr, sizeof(ihdr));

    // ---- IDAT: zlib header + STORED deflate blocks over the raw stream ----
    // Built in its own vector so png_chunk() copies a clean, contiguous
    // payload (no aliasing into the growing file image).
    std::vector<uint8_t> idat;
    idat.reserve(raw_len + 64);
    idat.push_back(0x78);         // CMF: deflate, 32K window
    idat.push_back(0x01);         // FLG: fastest; (0x78*256+1) % 31 == 0

    // Raw stream, walked row by row (BGRA -> RGB) and fed through the block
    // framing + the adler in lockstep. Blocks split on 65535-byte deflate
    // limits — a row boundary never has to align with a block boundary
    // because the framing carries byte counts, not row counts.
    std::vector<uint8_t> block;   // staging for the current stored block
    block.reserve(65535);
    uint32_t ad_a = 1, ad_b = 0;
    const auto block_byte = [&](uint8_t v) {
        block.push_back(v);
        ad_a = (ad_a + v) % 65521u;
        ad_b = (ad_b + ad_a) % 65521u;
        if (block.size() == 65535) {
            idat.push_back(0);                            // BFINAL=0, BTYPE=00
            const uint16_t len = 65535;
            idat.push_back((uint8_t)(len & 0xFF));
            idat.push_back((uint8_t)(len >> 8));
            idat.push_back((uint8_t)(~len & 0xFF));
            idat.push_back((uint8_t)((~len >> 8) & 0xFF));
            idat.insert(idat.end(), block.begin(), block.end());
            block.clear();
        }
    };
    for (unsigned y = 0; y < h; ++y) {
        block_byte(0);                                    // filter type 0
        const uint8_t* row = bgra + (size_t)y * w * 4;
        for (unsigned x = 0; x < w; ++x) {
            block_byte(row[x * 4 + 2]);                   // R
            block_byte(row[x * 4 + 1]);                   // G
            block_byte(row[x * 4 + 0]);                   // B
        }
    }
    if (!block.empty()) {                                 // final partial block
        idat.push_back(1);                                // BFINAL=1, BTYPE=00
        const uint16_t len = (uint16_t)block.size();
        idat.push_back((uint8_t)(len & 0xFF));
        idat.push_back((uint8_t)(len >> 8));
        idat.push_back((uint8_t)(~len & 0xFF));
        idat.push_back((uint8_t)((~len >> 8) & 0xFF));
        idat.insert(idat.end(), block.begin(), block.end());
    }
    be32(idat, (ad_b << 16) | ad_a);                      // zlib adler-32 (BE)
    png_chunk(png, "IDAT", idat.data(), idat.size());

    png_chunk(png, "IEND", nullptr, 0);

    return write_file(path, png.data(), png.size());
}

bool g3x_shot_submit(const uint8_t* bgra, unsigned w, unsigned h,
                     char* name_out, unsigned name_cap) {
    if (name_out && name_cap) name_out[0] = '\0';
    if (!bgra || !w || !h) return false;

    // screenshots\ beside the game exe (created on demand, idempotent).
    char exe[MAX_PATH];
    const DWORD en = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (!en || en >= MAX_PATH) { g3x_log("shot: no exe path — shot dropped"); return false; }
    char* slash = std::strrchr(exe, '\\');
    if (!slash) { g3x_log("shot: no exe dir — shot dropped"); return false; }
    *slash = '\0';

    char dir[MAX_PATH];
    std::snprintf(dir, sizeof(dir), "%s\\screenshots", exe);
    CreateDirectoryA(dir, nullptr);            // exists = ERROR_ALREADY_EXISTS, fine

    // D2_YYYYMMDD_HHMMSS.png, _1.._9 on a same-second collision.
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char base[64];
    std::snprintf(base, sizeof(base), "D2_%04u%02u%02u_%02u%02u%02u",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    char name[80];
    std::snprintf(name, sizeof(name), "%s.png", base);
    for (unsigned tries = 1; tries < 10; ++tries) {
        char probe[MAX_PATH];
        std::snprintf(probe, sizeof(probe), "%s\\%s", dir, name);
        if (GetFileAttributesA(probe) == INVALID_FILE_ATTRIBUTES) break;   // free
        std::snprintf(name, sizeof(name), "%s_%u.png", base, tries);
    }

    char path[MAX_PATH];
    std::snprintf(path, sizeof(path), "%s\\%s", dir, name);

    if (!write_png(path, bgra, w, h)) {
        g3x_log("shot: PNG write FAILED — screenshots\\%s", name);
        return false;
    }

    if (name_out && name_cap) std::snprintf(name_out, name_cap, "%s", name);
    g3x_log("shot: saved screenshots\\%s (%ux%u RGB PNG)", name, w, h);
    return true;
}
