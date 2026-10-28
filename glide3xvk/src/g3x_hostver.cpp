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

// Minimal bounds-checked walk over a mapped PE image: 'MZ' DOS header ->
// e_lfanew -> 'PE\0\0' -> COFF file header (TimeDateStamp) -> optional
// header (SizeOfImage, at offset 56 in both PE32 and PE32+). Only the first
// page of the image is touched when the caller passes cap = 0x1000 — the
// headers always live there.
int g3x_hostver_parse_pe(const uint8_t* image, size_t cap,
                         uint32_t* stamp_out, uint32_t* image_size_out) {
    if (!image || cap < 0x40) return 0;
    if (image[0] != 'M' || image[1] != 'Z') return 0;          // e_magic
    const uint32_t e_lfanew =
        (uint32_t)image[0x3C]       | ((uint32_t)image[0x3D] << 8) |
        ((uint32_t)image[0x3E] << 16) | ((uint32_t)image[0x3F] << 24);
    // NT signature (4) + COFF file header (20; TimeDateStamp at +4) +
    // optional header through SizeOfImage (+56..+59): 24 + 60 bytes.
    if (e_lfanew < 0x40 || (size_t)e_lfanew + 24 + 60 > cap) return 0;
    const uint8_t* nt = image + e_lfanew;
    if (nt[0] != 'P' || nt[1] != 'E' || nt[2] != '\0' || nt[3] != '\0')
        return 0;
    const uint32_t stamp =
        (uint32_t)nt[8]  | ((uint32_t)nt[9]  << 8) |
        ((uint32_t)nt[10] << 16) | ((uint32_t)nt[11] << 24);
    const uint8_t* opt = nt + 24;
    const uint32_t image_size =
        (uint32_t)opt[56] | ((uint32_t)opt[57] << 8) |
        ((uint32_t)opt[58] << 16) | ((uint32_t)opt[59] << 24);
    if (stamp_out)      *stamp_out      = stamp;
    if (image_size_out) *image_size_out = image_size;
    return 1;
}

unsigned g3x_hostver_format(char* out, unsigned cap,
                            const char* exe_basename,
                            int has_version, unsigned ver_ms, unsigned ver_ls,
                            uint32_t stamp, uint32_t image_size) {
    (void)image_size;   // logged by the caller; reserved for future tables
    if (!out || cap == 0) return 0;
    out[0] = '\0';
    if (!exe_basename || !exe_basename[0]) return 0;

    int n;
    const char* patch = has_version ? g3x_hostver_patch_name(ver_ms, ver_ls)
                                    : nullptr;
    if (patch) {
        n = std::snprintf(out, cap, "%s %s", exe_basename, patch);
    } else if (has_version) {
        n = std::snprintf(out, cap, "%s v%u.%u.%u.%u", exe_basename,
                          ver_ms >> 16, ver_ms & 0xFFFFu,
                          ver_ls >> 16, ver_ls & 0xFFFFu);
    } else if (stamp) {
        n = std::snprintf(out, cap, "%s [PE %08X]",
                          exe_basename, (unsigned)stamp);
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

    // Tier 2: the PE headers of the mapped image — the fallback for builds
    // with no version resource (this repo's own CMake-built d2.exe among
    // them). Only the always-mapped first page is read.
    uint32_t stamp = 0, image_size = 0;
    const HMODULE img = GetModuleHandleA(nullptr);
    if (img)
        g3x_hostver_parse_pe((const uint8_t*)img, 0x1000, &stamp, &image_size);

    char line[64];
    g3x_hostver_format(line, sizeof(line), base, has_version, ms, ls,
                       stamp, image_size);
    if (line[0]) {
        g3x_overlay_set_hostver(line);
        g3x_log("hostver: %s (ver=%d ms=0x%08X ls=0x%08X pe=0x%08X size=%u)",
                line, has_version, (unsigned)ms, (unsigned)ls,
                (unsigned)stamp, (unsigned)image_size);
    } else {
        g3x_log("hostver: exe name unreadable - overlay line skipped");
    }
}
#endif // _WIN32
