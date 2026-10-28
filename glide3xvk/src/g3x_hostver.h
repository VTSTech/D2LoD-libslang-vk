// glide3xvk — host exe version detection (0.4.5 addendum).
//
// Answers "which Game.exe/d2.exe is running?" for the overlay. The wrapper
// loads into whatever process opened the Glide window — stock Blizzard
// Game.exe (1.09-1.14d), a renamed build, or this repo's own CMake-built
// d2.exe — and the overlay's tag box now carries one more line naming it:
//
//     d2.exe 1.14d              (VERSIONINFO resource, patch mapped)
//     d2.exe v1.14.3.70         (VERSIONINFO resource, patch not in the table)
//     Game.exe [PE 3A5D2B1F]    (no resource — the exe's PE TimeDateStamp)
//     d2.exe                    (nothing readable — the exe name alone)
//
// Detection runs ONCE per process, at the first grSstWinOpen (the same seam
// that reads [glide3xvk] overlay=), and pushes the composed line into the
// overlay via g3x_overlay_set_hostver. Fail-soft throughout: every failure
// just degrades the line, never the game.
//
// Patch-name mapping source: the VS_FIXEDFILEINFO values Blizzard stamps on
// Game.exe, as field-tested by d2dx (GameHelper.cpp GetGameVersion — the
// same table d2dx uses to accept 1.09d/1.10/1.12/1.13c/1.13d/1.14b/1.14d).
// The mapping lives in the PURE section below so the host tests can pin it.
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

// Pure: parse the PE headers at the base of a MAPPED image (the DOS 'MZ'
// header sits at image[0]). Bounds-checked against cap — nothing is read
// past it, so the caller can pass the always-mapped first page (0x1000).
// On success returns 1 and writes *stamp_out (TimeDateStamp) and
// *image_size_out (SizeOfImage); returns 0 on any malformed/short input
// (outputs untouched then).
int g3x_hostver_parse_pe(const uint8_t* image, size_t cap,
                         uint32_t* stamp_out, uint32_t* image_size_out);

// Pure: compose the overlay line (NUL-terminated, clamped to cap, always
// safe). exe_basename (e.g. "d2.exe", "Game.exe") drives everything: NULL or
// empty -> no line (returns 0). With a VERSIONINFO resource (has_version != 0)
// the line is "<exe> <patch>" for a mapped patch or "<exe> v<m.m.b.r>" for an
// unmapped one; without one, "<exe> [PE <stamp 8-hex>]" when the PE headers
// were parsed (stamp != 0), else just "<exe>". Returns the composed length.
unsigned g3x_hostver_format(char* out, unsigned cap,
                            const char* exe_basename,
                            int has_version, unsigned ver_ms, unsigned ver_ls,
                            uint32_t stamp, uint32_t image_size);

// Real-DLL detection pass (Win32 only; defined in the _WIN32 section of
// g3x_hostver.cpp). Runs at most once per process: GetModuleFileNameA for
// the exe name, GetFileVersionInfoA/VerQueryValueA for the resource, PE
// headers of the mapped image as fallback, then g3x_overlay_set_hostver +
// one g3x_log line with the raw values (so a field log always carries the
// evidence even when the box is hidden).
void g3x_hostver_detect_push();
