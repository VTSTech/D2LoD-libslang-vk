// vanilla-plus combat fixes: the server-side drain-roll gate (mana burn).
// See VanillaPlusCombat.h for the bug write-up and the scope guard.
//
// Fork-owned TU (picked up by CMake's GLOB_RECURSE) - same shape as the
// shared-stash server gate (D2Game/PLAYER/VanillaPlusStash.cpp): own
// statics, a 2 s GetTickCount hot-read, and the shared ALTTOGGLE_BuildIniPath
// helper for the exe-anchored d2vp.ini path. Like the stash server gate,
// there is no options-page cache bust - a flip applies on the next <= 2 s
// poll.

#include "Source/D2Game/UNIT/VanillaPlusCombat.h"

#include "Source/D2Client/UI/VanillaPlusCore.h" // ALTTOGGLE_BuildIniPath (shared ini path helper)

#include <windows.h>

// [vanilla-plus mana-burn-fix] ini gate, server side. Same file, same key,
// same 2 s hot-read cadence as every gate (the SP server runs in the same
// process, so the exe dir is identical). Missing/garbage = 0 = retail.
static int s_nManaBurnFixCache = 0;
static DWORD s_dwManaBurnFixTick = 0;

BOOL __stdcall MANABURN_FixActive() {
    DWORD dwNow = GetTickCount();
    if (!s_dwManaBurnFixTick || dwNow - s_dwManaBurnFixTick >= 2000) {
        char szIniPath[260];
        s_dwManaBurnFixTick = dwNow;
        if (ALTTOGGLE_BuildIniPath(szIniPath)) {
            // Missing file/section/key or a non-numeric value = 0 = retail.
            s_nManaBurnFixCache = GetPrivateProfileIntA("features", "mana_burn_fix", 0, szIniPath) ? 1 : 0;
        } else {
            s_nManaBurnFixCache = 0;
        }
    }
    return s_nManaBurnFixCache;
}
