// glide3xvk — host exe version detection (see g3x_hostver.h for the design).
#include "g3x_hostver.h"

#include "g3x_log.h"
#include "g3x_overlay.h"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Pure logic (host-tested; identical in real and fakecheck builds)
// ---------------------------------------------------------------------------

// VS_FIXEDFILEINFO dwFileVersionMS/LS pairs Blizzard stamps on the game exe,
// mapped to the community patch names — the exact field-tested table d2dx
// uses in GameHelper.cpp GetGameVersion() (1.09d/1.10/1.12/1.13c/1.13d/
// 1.14b/1.14d are the versions d2dx accepts). MS = (major<<16)|minor,
// LS = (build<<16)|revision.
const char* g3x_hostver_patch_name(unsigned ver_ms, unsigned ver_ls) {
    static const struct { unsigned ms, ls; const char* name; } kTable[] = {
        { 0x00010000u, 0x00090016u, "1.09d" },  // 1.0.9.22
        { 0x00010000u, 0x000A0009u, "1.10"  },  // 1.0.10.9
        { 0x00010000u, 0x000C0031u, "1.12"  },  // 1.0.12.49
        { 0x00010000u, 0x000D003Cu, "1.13c" },  // 1.0.13.60
        { 0x00010000u, 0x000D0040u, "1.13d" },  // 1.0.13.64
        { 0x0001000Eu, 0x00030044u, "1.14b" },  // 1.14.3.68
        { 0x0001000Eu, 0x00030047u, "1.14d" },  // 1.14.3.71
    };
    for (const auto& e : kTable)
        if (e.ms == ver_ms && e.ls == ver_ls)
            return e.name;
    return nullptr;
}

// Known-build CRC-32 table — the content-fingerprint counterpart to the
// patch table above. Entries come from the field: the hostver log line's
// crc=/bytes= values quoted back to us become rows here, and the overlay
// then names that exact binary instead of showing raw hex. Size is kept as
// a comment (the CRC alone identifies the build; the log carries bytes=).
const char* g3x_hostver_crc_name(uint32_t crc) {
    static const struct { uint32_t crc; const char* name; } kTable[] = {
        { 0x4F393243u, "Retail 1.14d" },   // Game.exe, 3618792 bytes (3533 KiB)
        { 0x1706E55Au, "Vanilla+ 0.0.1" }, // d2s-4857c30.exe, 23550478 bytes (22 MiB)
        { 0xFE4CDB1Au, "Vanilla+ 0.0.2" }, // d2s-9cf9f69.exe, 23575054 bytes (22 MiB)
        { 0xA3F924DCu, "Vanilla+ 0.0.3" }, // d2s-569fc5c.exe, 23665152 bytes (22 MiB)
    };
    for (const auto& e : kTable)
        if (e.crc == crc)
            return e.name;
    return nullptr;
}

// CRC-32/ISO-HDLC, the reflected 0xEDB88320 variant every zip tool and
// python's zlib.crc32 report. The 256-entry table is derived on first use
// rather than shipped: eight shifts per byte once per process beats 1 KiB
// of static tables in a DLL that may never compute a CRC at all.
uint32_t g3x_hostver_crc32_update(uint32_t state, const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static int have_table = 0;
    if (!have_table) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        have_table = 1;
    }
    uint32_t c = state;
    for (size_t i = 0; i < len; ++i)
        c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c;
}

uint32_t g3x_hostver_crc32_final(uint32_t state) {
    return state ^ 0xFFFFFFFFu;
}

uint32_t g3x_hostver_crc32(const uint8_t* data, size_t len) {
    return g3x_hostver_crc32_final(
        g3x_hostver_crc32_update(G3X_HOSTVER_CRC32_INIT, data, len));
}

unsigned g3x_hostver_format(char* out, unsigned cap,
                            const char* exe_basename,
                            uint32_t crc, int has_crc,
                            int has_version, unsigned ver_ms, unsigned ver_ls) {
    if (!out || cap == 0) return 0;
    out[0] = '\0';
    if (!exe_basename || !exe_basename[0]) return 0;

    int n;
    // A known CRC outranks the resource tiers: the file's bytes are ground
    // truth even when its metadata is stale or missing.
    const char* known = has_crc ? g3x_hostver_crc_name(crc) : nullptr;
    const char* patch = (!known && has_version)
                            ? g3x_hostver_patch_name(ver_ms, ver_ls) : nullptr;
    if (known) {
        n = std::snprintf(out, cap, "%s %s", exe_basename, known);
    } else if (patch) {
        n = std::snprintf(out, cap, "%s %s", exe_basename, patch);
    } else if (has_version) {
        n = std::snprintf(out, cap, "%s v%u.%u.%u.%u", exe_basename,
                          ver_ms >> 16, ver_ms & 0xFFFFu,
                          ver_ls >> 16, ver_ls & 0xFFFFu);
    } else if (has_crc) {
        n = std::snprintf(out, cap, "%s [CRC %08X]",
                          exe_basename, (unsigned)crc);
    } else {
        n = std::snprintf(out, cap, "%s", exe_basename);
    }
    if (n < 0) { out[0] = '\0'; return 0; }
    if ((unsigned)n >= cap) n = (int)cap - 1;   // clamped; snprintf NUL-ended
    return (unsigned)n;
}

// ---------------------------------------------------------------------------
// Real-DLL detection pass (Win32; fakecheck type-checks via scripts/fakewin)
// ---------------------------------------------------------------------------

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winver.h>

void g3x_hostver_detect_push() {
    static bool s_done = false;                 // the exe never changes mid-process
    if (s_done) return;
    s_done = true;

    // The exe this DLL was loaded into — stock Game.exe (1.09-1.14d) or the
    // repo's own d2.exe. Its basename names the overlay line.
    char path[MAX_PATH] = "";
    const char* base = "";
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) > 0) {
        path[MAX_PATH - 1] = '\0';
        base = path;
        for (const char* p = path; *p; ++p)
            if (*p == '/' || *p == '\\') base = p + 1;
    }

    // Tier 1: the exe's VERSIONINFO resource — Blizzard stamps one on every
    // Game.exe, and the dwFileVersion pair IS the patch (see the table above).
    unsigned ms = 0, ls = 0;
    int has_version = 0;
    static unsigned char vbuf[8192];            // the fixed block is far smaller
    DWORD h = 0;
    const DWORD vsz = base[0] ? GetFileVersionInfoSizeA(path, &h) : 0;
    if (vsz > 0 && vsz <= sizeof(vbuf) &&
        GetFileVersionInfoA(path, h, vsz, vbuf)) {
        VS_FIXEDFILEINFO* ffi = nullptr;
        UINT flen = 0;
        if (VerQueryValueA((LPCVOID)vbuf, "\\", (LPVOID*)&ffi, &flen) &&
            ffi && flen >= sizeof(VS_FIXEDFILEINFO) &&
            ffi->dwSignature == 0xFEEF04BDu) {
            ms = ffi->dwFileVersionMS;
            ls = ffi->dwFileVersionLS;
            has_version = 1;
        }
    }

    // Tier 2: CRC-32 of the exe file on disk — the content fingerprint that
    // feeds the known-build table (a hit names the binary outright) and the
    // raw-hex fallback for builds with no version resource. Unlike a link
    // timestamp it is stable across rebuilds of the same bytes, and the
    // loader's read lock is no obstacle for a plain GENERIC_READ open.
    // Chunked through a static buffer, so multi-MB exes never touch the heap.
    uint32_t crc = 0;
    int has_crc = 0;
    unsigned bytes = 0;
    if (base[0]) {
        const HANDLE f = CreateFileA(path, GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     nullptr, OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            static unsigned char buf[65536];
            uint32_t st = G3X_HOSTVER_CRC32_INIT;
            DWORD got = 0;
            int ok = 1;
            for (;;) {
                if (!ReadFile(f, buf, sizeof(buf), &got, nullptr)) { ok = 0; break; }
                if (got == 0) break;
                st = g3x_hostver_crc32_update(st, buf, got);
                bytes += (unsigned)got;
            }
            if (ok) {
                crc = g3x_hostver_crc32_final(st);
                has_crc = 1;
            }
            CloseHandle(f);
        }
    }

    char line[64];
    g3x_hostver_format(line, sizeof(line), base, crc, has_crc,
                       has_version, ms, ls);
    if (line[0]) {
        g3x_overlay_set_hostver(line);
        g3x_log("hostver: %s (ver=%d ms=0x%08X ls=0x%08X crc=0x%08X bytes=%u)",
                line, has_version, (unsigned)ms, (unsigned)ls,
                (unsigned)crc, bytes);
    } else {
        g3x_log("hostver: exe name unreadable - overlay line skipped");
    }
}
#endif // _WIN32
