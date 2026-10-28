// glide3xvk — host exe version detection (0.4.5 addendum).
//
// Answers "which Game.exe/d2.exe is running?" for the overlay. The wrapper
// loads into whatever process opened the Glide window — stock Blizzard
// Game.exe (1.09-1.14d), a renamed build, or this repo's own CMake-built
// d2.exe — and the overlay's tag box now carries one more line naming it:
//
//     Game.exe Retail 1.14d     (CRC-32 matches the known-build table)
//     d2.exe 1.14d              (VERSIONINFO resource, patch mapped)
//     d2.exe v1.14.3.70         (VERSIONINFO resource, patch not in the table)
//     Game.exe [CRC 3A5D2B1F]   (unknown build, no resource — the exe's CRC-32)
//     d2.exe                    (nothing readable — the exe name alone)
//
// Detection runs ONCE per process, at the first grSstWinOpen (the same seam
// that reads [glide3xvk] overlay=), and pushes the composed line into the
// overlay via g3x_overlay_set_hostver. Fail-soft throughout: every failure
// just degrades the line, never the game.
//
// Two name sources, both field-extensible:
//   - VERSIONINFO: the VS_FIXEDFILEINFO values Blizzard stamps on Game.exe,
//     as field-tested by d2dx (GameHelper.cpp GetGameVersion — the same
//     table d2dx uses to accept 1.09d/1.10/1.12/1.13c/1.13d/1.14b/1.14d).
//   - CRC-32: a zlib-compatible CRC of the exe file's bytes — a content
//     fingerprint that identifies the exact binary regardless of when it
//     was linked (a link timestamp changes with every rebuild; the CRC only
//     changes when the bytes do). A known CRC outranks the resource tiers:
//     the bytes are ground truth even when metadata is stale or missing.
//     Unknown builds show the raw hex and log crc=/bytes= — quote the log
//     line and the table gets the entry.
// Both mappings live in the PURE section below so the host tests can pin
// them.
//
// Layout mirrors g3x_slang.cpp: pure logic first (host-testable, no
// windows.h), the Win32 detection pass at the bottom under _WIN32 (the
// fakecheck harness type-checks that section against scripts/fakewin).
#pragma once

#include <stddef.h>
#include <stdint.h>

// Pure: community patch name for the running exe's VS_FIXEDFILEINFO
// dwFileVersionMS/LS pair ("1.09d", "1.10", "1.12", "1.13c", "1.13d",
// "1.14b", "1.14d"), or NULL when the pair is not a known classic-D2 patch.
const char* g3x_hostver_patch_name(unsigned ver_ms, unsigned ver_ls);

// Pure: friendly build name for a known exe CRC-32 ("Retail 1.14d", ...),
// or NULL when the CRC is not in the known-build table. Entries come from
// the field: the hostver log line's crc=/bytes= values quoted back to us.
const char* g3x_hostver_crc_name(uint32_t crc);

// Pure: zlib-compatible CRC-32 (reflected, poly 0xEDB88320 — the same value
// zip/unzip, 7-Zip and python's zlib.crc32 report). Streaming form: seed
// with G3X_HOSTVER_CRC32_INIT, feed chunks in order with ..._update(),
// finish with ..._final(); the one-shot helper wraps both. The lazy
// 256-entry table is built on first use (detection is once-per-process and
// single-threaded at grSstWinOpen).
#define G3X_HOSTVER_CRC32_INIT 0xFFFFFFFFu
uint32_t g3x_hostver_crc32_update(uint32_t state, const uint8_t* data, size_t len);
uint32_t g3x_hostver_crc32_final(uint32_t state);
uint32_t g3x_hostver_crc32(const uint8_t* data, size_t len);

// Pure: compose the overlay line (NUL-terminated, clamped to cap, always
// safe). exe_basename (e.g. "d2.exe", "Game.exe") drives everything: NULL or
// empty -> no line (returns 0). A known CRC (has_crc != 0 and crc_name hits)
// wins: "<exe> <build name>". Without that, a VERSIONINFO resource
// (has_version != 0) gives "<exe> <patch>" for a mapped patch or
// "<exe> v<m.m.b.r>" for an unmapped one; a computed-but-unknown CRC gives
// "<exe> [CRC <8-hex>]"; nothing readable leaves just "<exe>". Returns the
// composed length.
unsigned g3x_hostver_format(char* out, unsigned cap,
                            const char* exe_basename,
                            uint32_t crc, int has_crc,
                            int has_version, unsigned ver_ms, unsigned ver_ls);

// Real-DLL detection pass (Win32 only; defined in the _WIN32 section of
// g3x_hostver.cpp). Runs at most once per process: GetModuleFileNameA for
// the exe name, GetFileVersionInfoA/VerQueryValueA for the resource, a
// chunked read + CRC-32 of the exe file on disk as the fingerprint, then
// g3x_overlay_set_hostver + one g3x_log line with the raw values (so a
// field log always carries the evidence even when the box is hidden).
void g3x_hostver_detect_push();
