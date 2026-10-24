// glide3xvk — 0.4.1: the game's own debug line as the version stamp.
//
// The game's /fps debug block ends with (decompiled D2Client/GAME/Rendering.cpp,
// DRAW_UI_FPSStats):
//
//   char szVideoMode[32];
//   case RENDERER_Glide: szRendererName = "Glide"; break;
//   sprintf(szVideoMode, szRendererName);
//   D2GFX_DrawString(500, 2, "Video: %s", szVideoMode);
//
// That string never crosses the glide ABI, so claiming the line from inside
// glide3x.dll takes a runtime patch: locate the renderer-name string pool in
// the mapped Game.exe image, find the single 4-byte reference to the "Glide"
// string's VA, and repoint it at a static buffer of ours. The game then draws
// "Video: glide3xvk <ver> (<build>)" in its own font at its own spot — the
// P2.10 overlay box retires to fallback status (glide_exports/grSstWinOpen
// consults install()'s return; [glide3xvk] overlay=1 forces the box back).
//
// Fail-soft contract (PLAN.md section 4): every step is guarded; any surprise
// (PE header unreadable, pool missing or ambiguous, no unique reference,
// VirtualProtect failure) skips the patch, logs the reason to glide3x.log and
// returns false — the overlay box stays on and nothing else changes. The game
// is touched by exactly one 4-byte store on a page flipped writable for it.
//
// Notes:
//   - the tag is clamped to 31 chars: the game sprintf()s into char[32].
//   - scanning mapped memory (not the file) means relocations are already
//     applied, so the VA found as an immediate is the runtime VA, and our
//     replacement VA is a runtime VA too — apples to apples.
//   - the game LoadLibrary()s glide3x.dll and never FreeLibrary()s it, so the
//     static tag buffer is valid for the whole session.
#pragma once

namespace g3x_drivertag {

// Scan + patch. Idempotent: the first call decides, later calls are no-ops.
// Returns true when the game's "Video:" line now shows our tag.
bool install();

// The tag handed to the game — "glide3xvk <ver> (<build>)", always valid
// static storage, empty string before the first install() call.
const char* tag();

} // namespace g3x_drivertag
