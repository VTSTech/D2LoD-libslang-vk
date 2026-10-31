#!/usr/bin/env bash
# Vanilla+ merge gates: grep-audit of every edit the fork injects into upstream
# files. After any upstream merge, run this BEFORE building - it answers "did
# the merge lose a Vanilla+ edit?" in one second, without a toolchain.
#
#   tools/check-vanilla-plus.sh [repo-root]
#
# Each gate is (name, file, ERE, expected count). Expected counts describe the
# fork's own additions (upstream-only occurrences are noted). When a gate
# fails after a merge, the fix is in docs/merge-upstream.md; when a failure is
# INTENDED (a conscious refactor changed the surface), update the expected
# count here in the same commit.
#
# Companions: the VP-BEGIN/VP-END markers (// VP: for one-liners) this script
# partly counts.
set -u

ROOT=${1:-$(cd "$(dirname "$0")/.." && pwd)}
cd "$ROOT" || { echo "no such dir: $ROOT" >&2; exit 2; }

FAIL=0
PASS=0

gate() { # gate <name> <file> <ERE> <expected>
    local name="$1" file="$2" pat="$3" want="$4" got
    got=$(grep -cE -- "$pat" "$file" 2>/dev/null) || got=0
    if [ "$got" = "$want" ]; then
        printf 'ok   %-46s %s x%s\n' "$name" "$file" "$got"
        PASS=$((PASS + 1))
    else
        printf 'FAIL %-46s %s — /%s/ expected %s, found %s\n' "$name" "$file" "$pat" "$want" "$got"
        FAIL=$((FAIL + 1))
    fi
}

# ---- boot & self-identification -------------------------------------------
gate "banner (d2boot.log self-ID)"        D2Client/Engine/Application.cpp 'D2BOOT_Mark\("Vanilla\+0\.0\.6-R51' 1
gate "boot: ini template hook"            D2Client/Engine/Application.cpp 'VP-BEGIN \[alt-toggle\]'    1

# ---- [d2boot] rev 33: SP loading-screen freeze trace (overlay round) -------
# Marks along the single-player enter-world handshake, so d2boot.log alone
# names the stage that never completes. Reading guide: HISTORY.md H25
# (archived 720p-era handoff §8).
gate "d2boot: loop frame telemetry"       D2Client/GAME/Game/Game.cpp     'game-loop: '                2
gate "d2boot: s2c per-opcode mark"        D2Client/GAME/SCmd.cpp          's2c recv 0x'                1
gate "d2boot: 0x02/0x04 handler marks"    D2Client/GAME/SCmd.cpp          's2c 0x0(2|4)'               2
gate "d2boot: c2s send marks"             D2Client/GAME/Msg.cpp           'c2s send 0x6'               2
gate "d2boot: setup dispatcher marks"     D2Game/GAME/CCmd.cpp            'c2s 0x6(7|B)'               3
gate "d2boot: dequeue trace"              D2Game/GAME/Server.cpp          'srv c2s'                    3
gate "d2boot: SrvJoinAct stage marks"     D2Game/GAME/Server.cpp          'SrvJoinAct enter|char load REFUSED|character loaded ok|player placed in room' 4
gate "d2boot: load-act marks"             D2Game/GAME/Server.cpp          'join: (load-act|DRLG act ready|0x03 NEWACT)' 3
gate "d2boot: 0x04 send mark"             D2Game/GAME/Server.cpp          'join: all rooms active'     1
gate "d2boot: state-3 wait telemetry"     D2Game/GAME/Server.cpp          'waiting for rooms'          2
gate "d2boot: server frame marks"         D2Game/GAME/Server.cpp          'server: (first game tick|game frame %u)' 2
gate "d2boot: SP create-game marks"       D2Game/GAME/Server.cpp          'SP create-game'             2
gate "d2boot: state-cmd choke mark"       D2Game/GAME/SCmd.cpp            'srv state-cmd 0x'           1
gate "d2boot: state transition mark"      D2Game/GAME/Clients.cpp         'client state %d -> %d'      1
gate "d2boot: join stage marks (Clients)" D2Game/GAME/Clients.cpp         'join: (PlacePlayerInRoom|LoadCharacter|TellClientToLoadAct)' 3
gate "d2boot: DRLG worldgen marks"        D2Game/GAME/Level.cpp           'DRLG: '                     2
# ---- [d2boot-fix] rev 34: SP packet-queue race fix + dispatcher spin guards --
gate "d2boot-fix: enqueue lock (queue race)" D2Net/SRC/Client.cpp          'Serialize with the pop path' 1
gate "d2boot-fix: unknown-size returns 0"    D2Net/Game/Server.cpp         'Unhandled variable-length opcode' 1
gate "d2boot-fix: dispatch zero-consume guard" D2Client/GAME/SCmd.cpp      's2c dispatch guard' 1
gate "d2boot-fix: pump drain watchdog"       D2Client/GAME/Game/Game.cpp   's2c pump: drained' 2

# ---- [d2boot-fix] rev 35: roster chain sanitizer + bounded 0x5B guild parse
# hangwatch on the expanded d2boot.log pinned the freeze: main thread spinning inside
# ROSTER_GroupPlayersIntoParties (inlined collect walk) under the 0x5B player-join
# handler -> ROSTER_AllocOrUpdatePlayer -> ROSTER_RebuildPlayerParty. The gpRosterPlayers
# chain was cyclic (heap corruption upstream); every roster walk spins on it. Fix:
# sanitize+heal at the walk entry points, bound every unbounded roster string copy.
gate "d2boot-fix: roster chain sanitizer"    D2Client/GAME/Roster.cpp      'roster heal: gpRosterPlayers cycle cut' 1
gate "d2boot-fix: roster sanitizer calls"    D2Client/GAME/Roster.cpp      'ROSTER_SanitizePlayerChain\(\); // \[d2boot-fix rev35\]' 4
gate "d2boot-fix: bounded roster name copy"  D2Client/GAME/Roster.cpp      'nNameIdx < 15' 1
gate "d2boot-fix: bounded roster tag copy"   D2Client/GAME/Roster.cpp      'nTagIdx < 3' 1
gate "d2boot-fix: bounded guildname copy"    D2Client/GAME/Roster.cpp      'nGuildIdx < 27' 1
gate "d2boot-fix: bounded display name"      D2Client/GAME/Roster.cpp      'nOut < 25' 5
gate "d2boot-fix: bounded display copy"      D2Client/GAME/Roster.cpp      'nDispIdx < 25' 1
gate "d2boot-fix: 0x5B bounded guild scan"   D2Client/GAME/SCmd.cpp        'szGuildAreaEnd' 5
gate "d2boot-fix: 0x5B playerjoin mark"      D2Client/GAME/SCmd.cpp        's2c 0x5B playerjoin' 1
gate "d2boot-fix: guild name getter cap"     D2Game/GAME/Clients.cpp       'nCopied < 32' 1
gate "d2boot-fix: guild name setter cap"     D2Game/GAME/Clients.cpp       'nCopied >= 27' 1

# ---- [d2boot-fix] rev 36: regroup-cluster walk caps (second dump round) ----
# The second hangwatch dump (base 004c0000) parked the main thread at rva 0x5f1fe =
# ROSTER_FlattenPartyListsIntoSingleRosterChain+0xb4 (pOwner tail walk); the first dump
# was ROSTER_GroupPlayersIntoParties+0x36. rev35's sanitize only heals gpRosterPlayers at
# walk entry points; the pOwner chains, the party list, gpRosterOther and the roster-link
# lists stayed unguarded. Every unbounded walk in the regroup cluster now carries a
# 64-node cap + "roster heal:" breadcrumb (SP rosters max out at 8 nodes -> vanilla data
# never reaches the cap). hangwatch offsets are map-relative now and a relocated run logs
# a base note; the build string is bumped to 0.0.6 (rev41) so the next d2boot.log self-identifies.
gate "d2boot-fix: rev36 walk-cap helper"     D2Client/GAME/Roster.cpp      'static void ROSTER_WalkCapped' 1
gate "d2boot-fix: rev36 walk-cap call sites" D2Client/GAME/Roster.cpp      'ROSTER_WalkCapped\(' 31
gate "d2boot-fix: rev36 flatten tail cap"    D2Client/GAME/Roster.cpp      'flatten tail walk' 1
gate "d2boot-fix: rev36 flatten party cap"   D2Client/GAME/Roster.cpp      'flatten party list' 1
gate "d2boot-fix: rev36 regroup iter cap"    D2Client/GAME/Roster.cpp      'regroup iterations' 1
gate "d2boot-fix: rev36 regroup scan cap"    D2Client/GAME/Roster.cpp      'regroup party scan' 1
gate "d2boot-fix: rev36 collect cap"         D2Client/GAME/Roster.cpp      'regroup collect' 1
gate "d2boot-fix: rev36 sort caps"           D2Client/GAME/Roster.cpp      'ROSTER_WalkCapped\("sort' 7
gate "d2boot-fix: rev36 move caps"           D2Client/GAME/Roster.cpp      'ROSTER_WalkCapped\("move' 3
gate "d2boot-fix: rev36 roster-other cap"    D2Client/GAME/Roster.cpp      'roster-other recycle' 1
gate "d2boot-fix: rev36 roster-link scan caps" D2Client/GAME/Roster.cpp    'roster-link walk' 4
gate "d2boot-fix: rev36 roster-link free caps" D2Client/GAME/Roster.cpp    'roster-link free' 2
gate "d2boot-fix: rev36 party-list free cap" D2Client/GAME/Roster.cpp      'party-list free' 1
gate "d2boot-fix: rev36 clear-roster caps"   D2Client/GAME/Roster.cpp      'clear-roster' 4
gate "d2boot-fix: rev36 portal-owner cap"    D2Client/GAME/Roster.cpp      'portal-owner walk' 1
gate "d2boot-fix: rev36 partied-roster cap"  D2Client/GAME/Roster.cpp      'partied-roster walk' 1
gate "d2boot-fix: rev36 by-guid walk cap"    D2Client/GAME/Roster.cpp      'roster-by-guid walk' 1
gate "d2boot-fix: rev36 hangwatch map-relative offsets" support/d2_hangwatch.cpp 'map-relative' 3
gate "d2boot-fix: rev36 hangwatch base-reloc note" support/d2_hangwatch.cpp 'hangwatch: note base' 1
gate "d2boot-fix: rev36 hangwatch map span"  support/d2_hangwatch.cpp      '%08lx\.\.%08lx' 1
gate "d2boot-fix: build version 0.0.6-R51" D2Client/Engine/Application.cpp 'Vanilla\+0\.0\.6-R51' 1

# ---- enum & stores (flag 38 = UI_VANILLAPLUS) -----------------------------
gate "UI_VANILLAPLUS defined = 38"        d2_enums.h                      'UI_VANILLAPLUS = 38'        1
gate "UI_VANILLAPLUS using-decl"          d2_enums.h                      'using eD2UIvars_ns::UI_VANILLAPLUS' 1
gate "dismiss table extern [39]"          globals/globals.h               'gUIFlagDismissModeTable\[39\]' 1
gate "options table extern [6]"           globals/globals.h               'gaEscMenuOptionsTable\[6\]' 1
gate "gnUIFlagCount bumped to 0x27"       globals/D2Client.cpp            'gnUIFlagCount = 0x27'       1
gate "dismiss table sized [39]"           globals/D2Client.cpp            'gUIFlagDismissModeTable\[39\] = \{' 1
gate "flag-store member VanillaPlus@0x98" globals/globals.h               '/\* 0x98 \*/ BOOL VanillaPlus' 1
gate "anDismissMode grown to 39"          globals/globals.h               'anDismissMode\[39\]'          1
gate "dismiss alias row (1 upstream + 1 fork)" globals/D2Client.cpp       'gaUIFlagDescs\[9\]\.anDismissMode\[0\]' 2
gate "options header nItemCount 0x6"      globals/D2Client.cpp            '0x6, // VP:'                1
gate "options table sized [6]"            globals/D2Client.cpp            'gaEscMenuOptionsTable\[6\] = \{ // VP:' 1
gate "row-10 click callback wired"        globals/D2Client.cpp            'ESCMENU_ShowVanillaPlusOptions' 1
gate "row-10 init callback wired (row + comment)" globals/D2Client.cpp        'ESCMENU_InitVanillaPlusLabel' 2
gate "row-10 draw callback wired (row + comment)" globals/D2Client.cpp        'VANILLAPLUS_DrawRowLabel'   2

# ---- Draw.cpp / ui.cpp dispatch + guards ----------------------------------
# Upstream 1775 filed SetUiFlagInit/SetUIFlagUnregister into ui.cpp; the row-10
# register/unregister blocks follow them there (the DRAW_UI render block stays in Draw.cpp).
gate "SetUiFlagInit/unregister VP blocks"  D2Client/UI/ui.cpp      'VP-BEGIN \[row-10 options page\]' 2
gate "render VP block"                     D2Client/UI/Draw.cpp    'VP-BEGIN \[row-10 options page\]' 1
# rev47 track G: PRESENCE is not PLACEMENT. Row "auto-gold tick wired in
# DRAW_UI" stayed green through the d6affdd merge port even though the port
# re-inserted the VP block one brace early - INSIDE the HelpMenu block of
# DRAW_UI - so AUTO_GOLD_Tick only ran while the help overlay was open and
# auto gold was dead in normal play (issue #10). The awk order-check below
# pins the placement: after DrawBackgroundHelpMenu(); the NEXT code line must
# be the HelpMenu block's closing brace, and VP-BEGIN [auto-gold] must come
# AFTER that brace (never before it).
gate "auto-gold tick wired in DRAW_UI"    D2Client/UI/Draw.cpp            'AUTO_GOLD_Tick\(\);'        1
gate "latch reset in InitializeUI"        D2Client/UI/Draw.cpp            'ALTTOGGLE_Reset\(\);'       1
gate "case UI_VANILLAPLUS (register+unregister)" D2Client/UI/ui.cpp       'case UI_VANILLAPLUS:'       2
gate "VANILLAPLUS_OnToggleUI calls"         D2Client/UI/ui.cpp              'VANILLAPLUS_OnToggleUI\('   2
gate "SetUIFlag bound widened to 38"        D2Client/UI/ui.cpp              'UI_VANILLAPLUS < eUI'       1
gate "GetUIFlag bound widened to 38"      D2Client/UI/ui.cpp              'UI_VANILLAPLUS >= eUI'      1
gate "0x27 guards in Draw.cpp"            D2Client/UI/Draw.cpp            'gnUIFlagCount (!=|==) 0x27' 5
gate "0x27 guards in ui.cpp"              D2Client/UI/ui.cpp              'gnUIFlagCount (!=|==) 0x27' 1

# ---- alt-toggle consumers -------------------------------------------------
gate "label click-through in DRAW_AltItems" D2Client/UI/ShowItems.cpp     'ALTTOGGLE_LabelClickThrough\(\)' 2

# ---- fork-api includes at every external call site (missed-include guard) --
# Upstream TUs no longer get fork decls transitively (D2Client.h is pristine);
# every file that calls fork API must include VanillaPlusCore.h itself. A
# missing include here = "was not declared in this scope" at build time.
gate "fork-api include in Game.cpp"       D2Client/GAME/Game/Game.cpp     'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in CUnit.cpp"      D2Client/UNIT/CUnit.cpp         'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in Draw.cpp"       D2Client/UI/Draw.cpp            'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in ShowItems.cpp"  D2Client/UI/ShowItems.cpp       'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in Application.cpp" D2Client/Engine/Application.cpp 'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in D2Client.cpp"   D2Client/D2Client.cpp           'include "D2Client/UI/VanillaPlusCore\.h"' 1

# ---- persistent orb text (FEATURES.md row 1, repurposed post-0.0.4) --------
# The 0.0.4 orb-face readout was removed after the field round - retail
# already prints cur/max above each orb on hover. The row now pins retail's
# own above-orb text on instead (a stale globe_values ini line is inert).
gate "persistent: gate fn defined"         D2Client/UI/VanillaPlusCore.cpp '__stdcall VANILLAPLUS_GlobeTextPersistent' 1
gate "ini template: globe_persistent key"  D2Client/UI/VanillaPlusCore.cpp 'globe_persistent=0'                1
gate "persistent: decl + cache busted on write" D2Client/UI/VanillaPlusCore.cpp 'dwGlobeTextPersistentLastCheck = 0' 2
gate "persistent wired in Panel.cpp (hp+mp)" D2Client/UI/Panel.cpp          'VANILLAPLUS_GlobeTextPersistent\(\)' 2
gate "fork-api include in Panel.cpp"       D2Client/UI/Panel.cpp           'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "options page: persistent row present" D2Client/UI/VanillaPlusOptions.cpp '"globe_persistent"'          1

# ---- D2Client.cpp must stay hollow ----------------------------------------
gate "hooks marked in D2Client.cpp"       D2Client/D2Client.cpp           'VP-BEGIN \[alt-toggle\]'    2
gate "gate impl NOT re-inlined in D2Client.cpp" D2Client/D2Client.cpp     'gbAltToggleMode|gbAutoGoldMode|ALTTOGGLE_WriteIniTemplate' 0
gate "D2Client.h back to pristine upstream" D2Client/D2Client.h           'ALTTOGGLE_|AUTO_GOLD_|D2VP_' 0

# ---- fork-owned core TU ----------------------------------------------------
gate "core: alt gate defined"             D2Client/UI/VanillaPlusCore.cpp '__stdcall ALTTOGGLE_Active' 1
gate "core: auto-gold tick defined"       D2Client/UI/VanillaPlusCore.cpp '__stdcall AUTO_GOLD_Tick'   1
gate "core: config flush defined"         D2Client/UI/VanillaPlusCore.cpp '__stdcall D2VP_ConfigReloaded' 1
gate "core: self-check defined"           D2Client/UI/VanillaPlusCore.cpp '__stdcall VANILLAPLUS_SelfCheck' 1
gate "core: tripwire static_asserts"      D2Client/UI/VanillaPlusCore.cpp 'static_assert\(UI_'         2
gate "ini template: alt_toggle key"       D2Client/UI/VanillaPlusCore.cpp 'alt_toggle=0'               1
gate "ini template: auto_gold key"        D2Client/UI/VanillaPlusCore.cpp 'auto_gold=0'                1
gate "ini template: portable_save key"    D2Client/UI/VanillaPlusCore.cpp 'portable_save=0'            1
gate "options page: read bridge used"     D2Client/UI/VanillaPlusOptions.cpp 'D2VP_GetFeatureToggle'   2
gate "options page: write bridge used"    D2Client/UI/VanillaPlusOptions.cpp 'D2VP_SetFeatureToggle'   1
gate "options page: shared ini path"      D2Client/UI/VanillaPlusOptions.cpp 'ALTTOGGLE_BuildIniPath'  1

# ---- automap crash tolerance ----------------------------------------------
gate "automap tolerance VP blocks (4 sites + macro)" D2Client/UI/Automap.cpp 'VP-BEGIN \[automap tolerance\]' 5
gate "automap tolerant frees"             D2Client/UI/Automap.cpp         'D2_FREE\(nErrorAddress\);'  4

# ---- sound-defect: class sound tables + sounds.txt null guard ---------------
gate "sounds.txt null guard in SOUND_PlaySound" D2Client/Sound/Sound.cpp 'VP: lookup returns null' 1
gate "class sound tables right-sized [11]" globals/globals.h            'extern int gaUnitSoundIds\w+\[11\]' 7
gate "class sound tables initialized"      globals/globals.cpp          'int gaUnitSoundIds\w+\[11\] = \{' 7
gate "pointer table targets Amazon table"  globals/D2Client.cpp         'gaUnitSoundIdsAmazon' 1
# restored retail values (1.14d Game.exe .data): full-initializer tripwire per class
gate "sound table Amazon restored (retail .data)"      globals/globals.cpp 'int gaUnitSoundIdsAmazon\[11\] = \{ 2878, 2883, 2953, 2957, 2955, 2934, 2936, 2959, 2937, 2961, 2768 \}' 1
gate "sound table Barbarian restored (retail .data)"   globals/globals.cpp 'int gaUnitSoundIdsBarbarian\[11\] = \{ 2902, 2907, 3031, 3035, 3033, 3012, 3014, 3037, 3015, 3039, 2816 \}' 1
gate "sound table Necromancer restored (retail .data)" globals/globals.cpp 'int gaUnitSoundIdsNecromancer\[11\] = \{ 2894, 2899, 3109, 3113, 3111, 3090, 3092, 3115, 3093, 3117, 2720 \}' 1
gate "sound table Paladin restored (retail .data)"     globals/globals.cpp 'int gaUnitSoundIdsPaladin\[11\] = \{ 2886, 2891, 3187, 3191, 3189, 3168, 3170, 3193, 3171, 3195, 2768 \}' 1
gate "sound table Sorceress restored (retail .data)"   globals/globals.cpp 'int gaUnitSoundIdsSorceress\[11\] = \{ 2910, 2915, 3265, 3269, 3267, 3246, 3248, 3271, 3249, 3273, 2720 \}' 1
gate "sound table Assassin restored (retail .data)"    globals/globals.cpp 'int gaUnitSoundIdsAssassin\[11\] = \{ 2918, 2923, 3343, 3347, 3345, 3324, 3326, 3349, 3327, 3351, 2720 \}' 1
gate "sound table Druid restored (retail .data)"       globals/globals.cpp 'int gaUnitSoundIdsDruid\[11\] = \{ 2926, 2931, 3421, 3425, 3423, 3402, 3404, 3427, 3405, 3429, 2720 \}' 1
# the decompile's phantom "init flag" (really Amazon[0]'s low byte) must stay gone
gate "bUnitSoundTableInit phantom gone (cpp)" globals/globals.cpp 'uint8_t bUnitSoundTableInit' 0
gate "bUnitSoundTableInit phantom gone (h)"   globals/globals.h   'uint8_t bUnitSoundTableInit' 0

# ---- rev41: sound FP-constant decode (upstream PR #10 commit 2 port) -------
# The merge port d6affdd re-took D2SoundSmp.cpp from the Ghidra-era tree and
# silently dropped the defect-12 fix (HISTORY.md H3): the IEEE-754 volume
# literals pasted as C integer casts evaluate to ~4.54e18 instead of 0.0001,
# so ClampVolume pinned EVERY engine volume to DSBVOLUME_MIN (-10000) — the
# "sound only on the intro video" field signature. These gates hold the
# decoded state so no future merge can lose it again.
gate "sound: ClampVolume/Attenuation epsilon decoded" D2Sound/Src/D2SoundSmp.cpp '\(long double\)0\.0001'    4
gate "sound: -2000 centibel scale restored"           D2Sound/Src/D2SoundSmp.cpp 'fVar1 \* -2000\.0'        1
gate "sound: ClampAttenuation bound decoded"          D2Sound/Src/D2SoundSmp.cpp '\(long double\)-10000\.0' 1
gate "sound: ClampAttenuation divisor decoded"        D2Sound/Src/D2SoundSmp.cpp '\(long double\)2000\.0'   1
gate "sound: no raw IEEE-754 volume literals left"    D2Sound/Src/D2SoundSmp.cpp '\(double\)(0x3f1a36e2e0000000|-0x3f3c780000000000|0x409f400000000000|-0x3f60c00000000000)' 0

# ---- resolution-mode (1280x720 slider stop) --------------------------------
# The registry value is the slider POSITION (0/1/2); stop 2 is attached at
# runtime by the Resolution row's init callback because no MPQ label asset
# exists for it. EscMenu.cpp mapping: 0 -> mode 0, 1 -> mode 2, 2 -> mode 4.
gate "resolution: draw callback wired (row)" globals/globals.cpp         'UI_SLIDER_DrawResolutionValue' 1
gate "resolution: draw fn defined"           D2Client/UI/EscMenu.cpp     '__fastcall UI_SLIDER_DrawResolutionValue' 1
# Both resolution callbacks (Apply + LoadAndApply) live in EscMenu.cpp on the
# 1775+ layout; main carried LoadAndApply in Storm/_unnamespaced.cpp.
gate "resolution: apply maps mode 4"         D2Client/UI/EscMenu.cpp     'RequestedScreenModeMaybe = 4' 2
gate "resolution: decl in EscMenu.h"         D2Client/UI/EscMenu.h       'UI_SLIDER_DrawResolutionValue' 1
gate "resolution: init attach"                  D2Client/UI/EscMenu.cpp     'VPRES_BuildBlankPlaceholder|nSubImageCount = 3' 2
gate "resolution: minipanel clamp + mode 4"  D2Client/UI/MiniPanel.cpp   'nResolution > 2|nScreenMode_lcl = 4' 2
gate "resolution: core blank builder"        D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_BuildBlankPlaceholder' 1
gate "resolution: core font pick"            D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_PickFontMatchingDC6' 1
gate "resolution: self-check wiring"         D2Client/UI/VanillaPlusCore.cpp 'UI_SLIDER_DrawResolutionValue' 1
gate "fork-api include in EscMenu.cpp"       D2Client/UI/EscMenu.cpp     'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in _unnamespaced.cpp" Storm/_unnamespaced.cpp     'include "D2Client/UI/VanillaPlusCore\.h"' 1

# ---- resolution site-5: wide-layout classification + centered 800-px UI ----
# Wide modes (3=1344x700 dormant, 4=1280x720) join the retail 800x600 UI
# class via VPRES_IsWideLayoutMode and re-center the exactly-800-px-wide UI
# with VPRES_GetWideUiOffsetX() = (ScreenSizeX-800)/2. The offset is 0 at
# modes 0/1/2, so retail behavior stays byte-identical.
gate "site-5: classifier defined"          D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_IsWideLayoutMode' 1
gate "site-5: centering offset defined"    D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_GetWideUiOffsetX' 1
gate "site-5: DRAW_UI gnPanelOffset block" D2Client/UI/Draw.cpp            'VP-BEGIN \[resolution site-5\]' 1
gate "site-5: panel offset ride"           D2Client/UI/Draw.cpp            '80 \+ VPRES_GetWideUiOffsetX' 1
gate "site-5: bar branch + borders gate"   D2Client/UI/Panel.cpp           'VPRES_IsWideLayoutMode' 2
gate "site-5: bar/orb/belt X shifts"       D2Client/UI/Panel.cpp           'VPRES_GetWideUiOffsetX' 37
gate "site-5: ui.cpp 800-class gates"      D2Client/UI/ui.cpp              'VPRES_IsWideLayoutMode' 20
gate "site-5: help art + text shifts"      D2Client/UI/Help.cpp            'VPRES_GetWideUiOffsetX' 75
gate "site-5: questlog 800-class gates"    D2Client/UI/QuestLog.cpp        'VPRES_IsWideLayoutMode' 6
gate "site-5: inventory table shifts"      D2Client/UI/Inv.cpp             'VP_SHIFT_Inv' 43
gate "site-5: inv drop-click gate"         D2Client/UI/Inv.cpp             'VPRES_IsWideLayoutMode' 1
gate "site-5: inv coord cache key"         D2Client/UI/Inv.cpp             'nCoordPageKey' 3
# rev44 track D: the fec99ff merge port had placed every VP_SHIFT_* call BEFORE
# its paired TokTbls fetch; the fetch overwrites the struct (TokTbls.cpp:806-894
# are pure overwrites), so all 40 table-rect sets ran vanilla-unshifted at wide
# modes - grid items drew 240/120 px off the shifted panel art. Gate the ORDER,
# not just the counts: every fetch line must be IMMEDIATELY followed by its own
# shift call, and every shift call must carry its paired fetch's target.
vp_pair_ok=0; vp_pair_bad=0
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /D2Common::DATATBLS::TokTbls::Get(Inv|Grid|Body)Dimensions\(/ { pend=1; next }
    /VP_SHIFT_Inv(Page|Grid|Body)Coords\((&|ga)/ { if (pend) { ok++; pend=0 } else { bad++ }; next }
    { if (pend) { bad++; pend=0 } }
    END { print "vp_pair_ok=" ok+0; print "vp_pair_bad=" bad+0 }
  ' D2Client/UI/Inv.cpp)"
fi
if [ "${vp_pair_bad:-1}" = 0 ] && [ "${vp_pair_ok:-0}" = 40 ]; then
    printf 'ok   %-46s %s fetch->shift pairs x%s\n' "site-5: fetch->shift pair order (rev44)" "D2Client/UI/Inv.cpp" "$vp_pair_ok"
    PASS=$((PASS + 1))
else
    printf 'FAIL %-46s ok=%s bad=%s (want ok=40 bad=0)\n' "site-5: fetch->shift pair order (rev44)" "${vp_pair_ok:-0}" "${vp_pair_bad:-0}"
    FAIL=$((FAIL + 1))
fi
# rev44 track E: the row-10 page draw sat INSIDE the ConfigControls guard -
# the page opens with ConfigControls clear, so the nested check never ran and
# the page was invisible. The dispatch must be a SIBLING of that guard.
gate "row-10: page draw call"              D2Client/UI/Draw.cpp            'DRAW_VanillaPlusOptions\(\);' 1
gate "row-10: dispatch hoisted (rev44)"    D2Client/UI/Draw.cpp            'render the page as a SIBLING of the' 1
# rev45 track F: the merge port had scrambled the two icon-pass anchors in
# SPELLSEL_RenderAllSkillGrids - the right-hand grid icons drew at the swap
# grid's anchor (0x50 + offset = left half of the screen at wide modes) and
# the swap grid icons drew at vanilla 0x50 (off the centered panel), while
# both tooltip passes kept the correct anchors. The icon passes also register
# the click regions, so the flyout opened on the wrong side. Gate the pairing:
# both icon passes must consume nStartX (right grid) or the shifted 0x50
# (swap grid) exactly as their tooltip passes do.
gate "site-5: spell grid shifts (R45)"      D2Client/UI/SpellSel.cpp        'VPRES_GetWideUiOffsetX' 5
gate "site-5: spell right-grid icons bar-anchored (R45)" D2Client/UI/SpellSel.cpp 'SPELLSEL_DrawSkillGrid\(pPlayerUnit_lcl, nStartX,' 2
gate "site-5: spell swap-grid icons ride panel (R45)" D2Client/UI/SpellSel.cpp 'SPELLSEL_DrawSkillGrid\(pPlayerUnit_lcl, 0x50 \+ VPRES_GetWideUiOffsetX' 2
gate "site-5: spell icons not vanilla-anchored (R45)" D2Client/UI/SpellSel.cpp 'DrawSkillGrid\(pPlayerUnit_lcl, 0x50,' 0
gate "site-5: summit clouds gate"          D2Client/GAME/View.cpp          'VPRES_IsWideLayoutMode' 1
gate "site-5: level edges gate (GAME)"     D2Client/GAME/Draw.cpp          'VPRES_IsWideLayoutMode' 1
gate "site-5: level edges gate (client)"   D2Client/D2Client.cpp           'VPRES_IsWideLayoutMode' 1
gate "site-5: listbox metrics gate"        D2Client/UI/ListBox/ListBoxImplementation.cpp 'VPRES_IsWideLayoutMode' 1
gate "site-5: fork-api include in ui.cpp"  D2Client/UI/ui.cpp              'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in Help.cpp" D2Client/UI/Help.cpp           'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in QuestLog.cpp" D2Client/UI/QuestLog.cpp   'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in Inv.cpp" D2Client/UI/Inv.cpp             'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in SpellSel.cpp" D2Client/UI/SpellSel.cpp   'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in View.cpp" D2Client/GAME/View.cpp         'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in GAME Draw.cpp" D2Client/GAME/Draw.cpp    'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in ListBox" D2Client/UI/ListBox/ListBoxImplementation.cpp 'include "D2Client/UI/VanillaPlusCore\.h"' 1

# ---- resolution site-6: the vertical half of the shift ----------------------
# Fixed (top-anchored) 800-space Y coordinates - Belts.txt/inventory.txt rect
# Y bounds, "const - gnPanelOffsetY" texts and hit tests, raw border-frame Y -
# ride the bottom-anchored panel art by VPRES_GetWideUiOffsetY() =
# ScreenSizeY - 600 (0 at modes 0/1/2 -> byte-identical retail, 100 at mode 3,
# 120 at mode 4). Input paths use the inverse form "gnPanelOffsetY -
# VPRES_GetWideUiOffsetY". gnPanelOffsetY itself stays -60: bottom-anchored
# expressions already contain ScreenSizeY and would double-shift.
gate "site-6: Y offset defined"            D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_GetWideUiOffsetY' 1
gate "site-6: Y offset declared"           D2Client/UI/VanillaPlusCore.h   'VPRES_GetWideUiOffsetY' 1
gate "site-6: belt + border Y shifts"      D2Client/UI/Panel.cpp           'VPRES_GetWideUiOffsetY' 17
gate "site-6: inv table + text Y shifts"   D2Client/UI/Inv.cpp             'VPRES_GetWideUiOffsetY' 15
gate "site-6: party panel Y shifts"        D2Client/UI/Party.cpp           'VPRES_GetWideUiOffsetY' 54
gate "site-6: waypoint Y shifts"           D2Client/UI/Waypoint.cpp        'VPRES_GetWideUiOffsetY' 10
gate "site-6: waypoint input inverse"      D2Client/UI/Waypoint.cpp        'gnPanelOffsetY - VPRES_GetWideUiOffsetY' 3
gate "site-6: questlog Y shifts"           D2Client/UI/QuestLog.cpp        'VPRES_GetWideUiOffsetY' 10
gate "site-6: questlog input inverse"      D2Client/UI/QuestLog.cpp        'gnPanelOffsetY - VPRES_GetWideUiOffsetY' 3
gate "site-6: scrolling text Y shift"      D2Client/UI/Text.cpp            'VPRES_GetWideUiOffsetY' 1
gate "site-6: tooltip + gold dialog Y shifts" D2Client/UI/ui.cpp              'VPRES_GetWideUiOffsetY' 11
gate "site-6: gold dialog X shifts"         D2Client/UI/ui.cpp              'VPRES_GetWideUiOffsetX' 5
gate "site-6: modal popup Y shifts"        D2Client/Engine.cpp             'VPRES_GetWideUiOffsetY' 18
gate "site-6: help screen Y shifts"        D2Client/UI/Help.cpp            'VPRES_GetWideUiOffsetY' 74
gate "site-6: fork-api include in Party.cpp"  D2Client/UI/Party.cpp        'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-6: fork-api include in Waypoint.cpp" D2Client/UI/Waypoint.cpp   'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-6: fork-api include in Text.cpp"   D2Client/UI/Text.cpp         'include "D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-6: fork-api include in Engine.cpp" D2Client/Engine.cpp          'include "D2Client/UI/VanillaPlusCore\.h"' 1

# ---- resolution renderer dispatch (post-field-test fixes, 2026-10-30) -------
# Mode 4 must take the fully-initialized perspective=1 D3D device path (the
# goto left floor/subtile/font caches uninitialized -> black world), and the
# software renderers must size their buffers to 1280x720 (GDI DIB stayed
# 800x600 in the resized -w window; DDraw indexed its size tables OOB).
gate "resfix: d3d mode-4 perspective path"  D2Direct3D/Renderer/Direct3D.cpp 'nResolutionMode != 2 && nResolutionMode != 3 && nResolutionMode != 4' 1
gate "resfix: gdi dib mode-4 1280x720"      D2Gdi/Renderer/Windowed.cpp   'nResolutionMode == 4' 1
gate "resfix: gdi scale width 1280"         D2Gdi/Renderer/Windowed.cpp   'nParam == 4' 1
gate "resfix: ddraw size tables extended"   D2DDraw/Renderer/DirectDraw.cpp '0x540, 0x500' 1
gate "resfix: ddraw horizontal bound 1280"  D2DDraw/Renderer/DirectDraw.cpp 'gnDDrawRenderWidth = 1280' 1

# ---- glide 720p (post-"Video: Software" round, 2026-10-30) ------------------
# Owner field-tested D3D at 720p as WORKING (renderer 6); Glide (renderer 4,
# -3dfx + the paired Glide3xVK glide3x.dll) still failed: InitWindow clamped
# every non-640 stop to an 800x600 surface AND left gbGlidePerspectiveMode 0
# at a non-800x600 size, where InitTextureMemory's non-perspective branch is
# gated on width==800 && height==600 and never runs -> uninitialized caches,
# black world. Game side passes grSstWinOpen enum 16 = 1280x720 (17 = dormant
# 1344x700); the paired wrapper learns both in g3x_resolution_size (Glide3xVK
# branch patch, delivered alongside this one).
gate "resfix: glide mode-4 surface 1280x720"    D2Glide/Renderer/Glide.cpp 'RENDERER_nWindowWidth = 1280' 1
gate "resfix: glide mode-3 surface 1344x700"    D2Glide/Renderer/Glide.cpp 'RENDERER_nWindowWidth = 1344' 1
gate "resfix: glide resolution enum 16"         D2Glide/Renderer/Glide.cpp 'screen_resolution = 16' 1
gate "resfix: glide perspective path modes 2/3/4" D2Glide/Renderer/Glide.cpp 'nParam != 2 && nParam != 3 && nParam != 4' 1

# ---- glide texmem init + fix-2/3 guards (R46: d6affdd merge-port casualties) --
# The d6affdd "merge port" replaced InitTextureMemory with the fork's pre-merge
# version: the subtile/floor cache size assignments (retail 0x400/0x8000) were
# lost, so GLFLOOR/GLSUBTILE_Initialize divided by uninitialized stack (the
# -3dfx first-frame 0xC0000094, track B); the 3.5 MB VRAM gate, the debug-font
# init and the TMU>=2 split went with it. The same commit dropped the glide-side
# fix-3 class-slot guard and fix-2 stale-nodeIdx validation (their D3D mirrors,
# fixes 21/22, stayed). These rows pin the restored surface.
gate "glide texmem: subtile size assigned (R46)" D2Glide/Renderer/Glide.cpp 'nSubtileTexSize = 0x400' 1
gate "glide texmem: floor size assigned (R46)"   D2Glide/Renderer/Glide.cpp 'nFloorTexSize = 0x8000' 1
gate "glide texmem: debug font wired (R46)"      D2Glide/Renderer/Glide.cpp 'RENDERER_GLIDE_InitDebugFont\(gnGlideTexMemBaseAddress\[0\]' 1
gate "glide texmem: VRAM gate present (R46)"     D2Glide/Renderer/Glide.cpp 'have enough video memory' 1
gate "glide fix-3: class-slot guard (R46)"       D2Glide/Renderer/Glide.cpp '< 1 \|\| 3 <' 1
gate "glide fix-2: stale-nodeIdx guard (R46)"    D2Glide/Renderer/Glide.cpp 'pSlotNode \+ 8\) != nTexStartAddress' 1

# ---- rev47 track G: auto-gold trigger placement (issue #10) ----------------
# The d6affdd merge port re-parented the VP [auto-gold] block into the
# HelpMenu block of DRAW_UI (marker + call both present - the presence gate
# above could not see it). Tick body, send path and server pick were all
# intact; only the reachability died. The order-check mirrors the rev44
# site-5 pair-order gate: DrawBackgroundHelpMenu(); -> closing brace ->
# VP-BEGIN, with VP-BEGIN-before-brace counted as the regression signature.
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /Help::DrawBackgroundHelpMenu\(\);/ { if (stage==0) stage=1; next }
    stage==1 && /VP-BEGIN \[auto-gold\]/ { bad++; stage=9; next }
    stage==1 && /^[[:space:]]*}[[:space:]]*$/ { stage=2; next }
    stage==1 && NF { stage=9 }
    stage==2 && /VP-BEGIN \[auto-gold\]/ { ok++; stage=0; next }
    stage==2 && NF { stage=9 }
    END { printf "vp_ag_ok=%d vp_ag_bad=%d", ok+0, bad+0 }
  ' D2Client/UI/Draw.cpp)"
  if [ "${vp_ag_ok:-0}" -eq 1 ] && [ "${vp_ag_bad:-0}" -eq 0 ]; then
    printf 'ok   %-46s %s brace-after-call x%s\n' "auto-gold: trigger outside help block (R47)" "D2Client/UI/Draw.cpp" "$vp_ag_ok"
    PASS=$((PASS + 1))
  else
    printf 'FAIL %-46s ok=%s bad=%s (want ok=1 bad=0) - VP block re-parented into the HelpMenu block\n' "auto-gold: trigger outside help block (R47)" "${vp_ag_ok:-0}" "${vp_ag_bad:-0}"
    FAIL=$((FAIL + 1))
  fi
fi

# ---- rev48 track H: gold-dialog rider BRANCH placement (issue #12) ---------
# The d6affdd merge port resolved the 720p gold-dialog edit (cef3cf8) into
# the FAILURE arm of the widget-alloc null-checks: upstream's regen had
# inverted the if/else shape (decompiler-equivalent churn), and the port
# matched the VP-marked line onto the constructor call that now sat next to
# the same-LHS null-out. Presence (x5) stayed green; in play the raw
# 800-space constants ran and the dialog was misaligned at 720p. Order-check:
# each VP-marked constructor must sit directly inside an `if (pAllocCtx) {`
# arm (ok=4), and a VP-marked constructor may NEVER be followed by a
# same-LHS "= 0;" line (bad=0 - the dead-store regression signature).
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /if \(pAllocCtx\) \{/ { stage=1; next }
    stage==1 && /\/\// && !/site-6/ { next }
    stage==1 && NF {
      if (/Constructor\(/ && /VPRES_GetWideUiOffset/ && /site-6/) ok++;
      stage=0
    }
    /Constructor\(/ && /VPRES_GetWideUiOffset/ && /site-6/ {
      vp=NR; vplhs=$0; sub(/^[ \t]*/,"",vplhs); sub(/[ \t]*=.*/,"",vplhs); next
    }
    vp && NR==vp+1 {
      lhs=$0; sub(/^[ \t]*/,"",lhs); sub(/[ \t]*=.*/,"",lhs);
      if (lhs==vplhs && /= 0;/) bad++; vp=0
    }
    END { printf "vp_gd_ok=%d vp_gd_bad=%d", ok+0, bad+0 }
  ' D2Client/UI/ui.cpp)"
  if [ "${vp_gd_ok:-0}" -eq 4 ] && [ "${vp_gd_bad:-0}" -eq 0 ]; then
    printf 'ok   %-46s %s success-branch-rider x%s\n' "gold-dialog: riders in success branch (R48)" "D2Client/UI/ui.cpp" "$vp_gd_ok"
    PASS=$((PASS + 1))
  else
    printf 'FAIL %-46s ok=%s bad=%s (want ok=4 bad=0) - VP rider resolved into the alloc-FAILURE branch (dead store)\n' "gold-dialog: riders in success branch (R48)" "${vp_gd_ok:-0}" "${vp_gd_bad:-0}"
    FAIL=$((FAIL + 1))
  fi
fi

# ---- [R49] palshift: rolled unique tint reachable (issue #9) ----------------
# GetMonsterTransform must NOT early-out on a missing palshift.dat entry:
# RandTransforms.dat is a GLOBAL table (indices 8-0x25, the rolled unique/
# champion tints) and resolves without any per-monster entry. The entry
# pointer is consumed only by the index 0-7 sub-table paths, so entry
# guards must live inside the switch arms (after `switch (nPalShiftIndex) {`),
# never before it. Regression signature of the R49 fix: any entry-guard
# line preceding the switch = the unconditional early-out is back.
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /switch \(nPalShiftIndex\) \{/ { if (swl==0) swl=NR }
    /if \(!pPalShiftEntryPtr\) \{/ { if (swl==0) pse++; else psg++ }
    /PalShift_RandTransform \+ \(nPalShiftIndex - 8U\)/ { if (psr==0) psr=NR }
    END { printf "vp_ps_swl=%d vp_ps_guard=%d vp_ps_early=%d vp_ps_rand=%d", swl+0, psg+0, pse+0, psr+0 }
  ' D2Client/GAME/PalShift.cpp)"
  if [ "${vp_ps_swl:-0}" -ge 1 ] && [ "${vp_ps_guard:-0}" -eq 2 ] && [ "${vp_ps_early:-0}" -eq 0 ] && [ "${vp_ps_rand:-0}" -ge 1 ]; then
    printf 'ok   %-46s %s guards=%s early=%s\n' "palshift: rand-transform reachable (R49)" "D2Client/GAME/PalShift.cpp" "$vp_ps_guard" "$vp_ps_early"
    PASS=$((PASS + 1))
  else
    printf 'FAIL %-46s switch=%s guards=%s early=%s rand=%s (want switch>=1 guards=2 early=0 rand>=1) - unconditional entry-out before the switch kills the unique tint (issue #9)\n' "palshift: rand-transform reachable (R49)" "${vp_ps_swl:-0}" "${vp_ps_guard:-0}" "${vp_ps_early:-0}" "${vp_ps_rand:-0}"
    FAIL=$((FAIL + 1))
  fi
fi

# ---- [R50] roster: pet recolor polarity (issue #8) ---------------------------
# The skeleton recolor must write the RAW hostility flag: bHostile=0 (own/ally
# pet) -> palshift.dat sub-table 0 (bone-white), bHostile=1 (hostile owner) ->
# sub-table 1 (yellow tell). The transcribed `!bHostile` inverted the pair and
# fed every own raised skeleton the yellow hostile table (issue #8 "Minion
# Skeletons are Yellow"). Regression signature: any `!bHostile` inside a
# SetPalShiftIndex/SetGfxSequenceData recolor call in RosterPets.cpp, and both
# fields must stay equal-valued (dwCofDataFlags is the re-sync latch shadow).
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /SetPalShiftIndex\(pRosterUnit, \(uint32_t\)!bHostile\)/ { rp++ }
    /SetGfxSequenceData\(pRosterUnit, \(uint32_t\)!bHostile\)/ { rg++ }
    /SetPalShiftIndex\(pRosterUnit, \(uint32_t\)bHostile\)/ { gp++ }
    /SetGfxSequenceData\(pRosterUnit, \(uint32_t\)bHostile\)/ { gg++ }
    END { printf "vp_rp_bad=%d vp_rp_good=%d", rp+rg, gp+gg }
  ' D2Client/GAME/RosterPets.cpp)"
  if [ "${vp_rp_bad:-0}" -eq 0 ] && [ "${vp_rp_good:-0}" -eq 4 ]; then
    printf 'ok   %-46s %s inverted=%s raw=%s\n' "roster: pet recolor polarity (R50)" "D2Client/GAME/RosterPets.cpp" "${vp_rp_bad:-0}" "${vp_rp_good:-0}"
    PASS=$((PASS + 1))
  else
    printf 'FAIL %-46s inverted=%s raw=%s (want inverted=0 raw=4) - !bHostile feeds own skeletons the yellow hostile sub-table (issue #8)\n' "roster: pet recolor polarity (R50)" "${vp_rp_bad:-0}" "${vp_rp_good:-0}"
    FAIL=$((FAIL + 1))
  fi
fi

# ---- [R51] spawn: ownership palshift polarity (issue #8) ---------------------
# CreateObject's MonStats2 Utrans==0xFF fallback picks the palshift.dat
# ownership sub-table pair from the RAW hostility flag: 0 (own/ally unit) ->
# sub-table 0 (bone-white), 1 (hostile owner) -> sub-table 1 (yellow tell).
# The transcribed `!uMercType` spawned every own raised skeleton straight into
# the yellow hostile table (issue #8) and the event-driven roster recolors
# (R50) only run from roster-update packets, so in SP the wrong spawn tint
# stuck for the unit's whole life. Regression signature: any `!uMercType`
# feeding nPalShiftIdx in CUnit.cpp.
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /nPalShiftIdx = \(uint32_t\)!uMercType;/ { sp_bad++ }
    /nPalShiftIdx = \(uint32_t\)uMercType;/ { sp_good++ }
    END { printf "vp_sp_bad=%d vp_sp_good=%d", sp_bad+0, sp_good+0 }
  ' D2Client/UNIT/CUnit.cpp)"
  if [ "${vp_sp_bad:-0}" -eq 0 ] && [ "${vp_sp_good:-0}" -eq 1 ]; then
    printf 'ok   %-46s %s inverted=%s raw=%s\n' "spawn: ownership palshift polarity (R51)" "D2Client/UNIT/CUnit.cpp" "${vp_sp_bad:-0}" "${vp_sp_good:-0}"
    PASS=$((PASS + 1))
  else
    printf 'FAIL %-46s inverted=%s raw=%s (want inverted=0 raw=1) - !uMercType spawns own skeletons into the yellow hostile sub-table (issue #8)\n' "spawn: ownership palshift polarity (R51)" "${vp_sp_bad:-0}" "${vp_sp_good:-0}"
    FAIL=$((FAIL + 1))
  fi
fi

echo
if [ "$FAIL" -eq 0 ]; then
    echo "vanilla-plus gates: $PASS passed"
    exit 0
fi
echo "vanilla-plus gates: $FAIL FAILED, $PASS passed - a merge likely dropped an edit; see docs/merge-upstream.md"
exit 1
