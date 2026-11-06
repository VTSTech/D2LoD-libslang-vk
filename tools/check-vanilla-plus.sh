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

# ---- boot & d2boot instrumentation ----------------------------------------
# The build self-ID (D2BOOT_Mark "Vanilla+0.0.6-RXX") is deliberately NOT
# gate-pinned (rev 58, owner: "remove the version pin from the gate, gate is
# for features and fixes") — the owner bumps it every code round, so a pinned
# banner would red the suite by design. Check it manually: d2boot.log line 1
# (SMOKE-CHECKLIST item 0).
gate "boot: log overwrites per run"       Redivivus/d2_boot.cpp             '0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL' 2
# refiled R106: D2Client/Engine/Application.cpp -> Game/Main.cpp (upstream 316d537a)
gate "boot: ini template hook"            Source/Game/Main.cpp                    'VP-BEGIN \[alt-toggle\]'    1

# ---- [d2boot] rev 33: SP loading-screen freeze trace (overlay round) -------
# Marks along the single-player enter-world handshake, so d2boot.log alone
# names the stage that never completes. Reading guide: HISTORY.md H25
# (archived 720p-era handoff §8).
gate "d2boot: loop frame telemetry"       Source/D2Client/GAME/Game/Game.cpp     'game-loop: '                2
gate "d2boot: s2c per-opcode mark"        Source/D2Client/GAME/SCmd.cpp          's2c recv 0x'                1
gate "d2boot: 0x02/0x04 handler marks"    Source/D2Client/GAME/SCmd.cpp          's2c 0x0(2|4)'               2
gate "d2boot: c2s send marks"             Source/D2Client/GAME/Msg.cpp           'c2s send 0x6'               2
gate "d2boot: setup dispatcher marks"     Source/D2Game/GAME/CCmd.cpp            'c2s 0x6(7|B)'               3
gate "d2boot: dequeue trace"              Source/D2Game/GAME/Server.cpp          'srv c2s'                    3
gate "d2boot: SrvJoinAct stage marks"     Source/D2Game/GAME/Server.cpp          'SrvJoinAct enter|char load REFUSED|character loaded ok|player placed in room' 4
gate "d2boot: load-act marks"             Source/D2Game/GAME/Server.cpp          'join: (load-act|DRLG act ready|0x03 NEWACT)' 3
gate "d2boot: 0x04 send mark"             Source/D2Game/GAME/Server.cpp          'join: all rooms active'     1
gate "d2boot: state-3 wait telemetry"     Source/D2Game/GAME/Server.cpp          'waiting for rooms'          2
gate "d2boot: server frame marks"         Source/D2Game/GAME/Server.cpp          'server: (first game tick|game frame %u)' 2
gate "d2boot: SP create-game marks"       Source/D2Game/GAME/Server.cpp          'SP create-game'             2
gate "d2boot: state-cmd choke mark"       Source/D2Game/GAME/SCmd.cpp            'srv state-cmd 0x'           1
gate "d2boot: state transition mark"      Source/D2Game/GAME/Clients.cpp         'client state %d -> %d'      1
gate "d2boot: join stage marks (Clients)" Source/D2Game/GAME/Clients.cpp         'join: (PlacePlayerInRoom|LoadCharacter)' 2
# upstream 088c4ee3..94ec239d moved TellClientToLoadAct (and its join mark) from
# Clients.cpp into Level.cpp; the mark itself is re-applied there (R99+ merge)
gate "d2boot: join stage mark (Level)"      Source/D2Game/GAME/Level.cpp           'join: TellClientToLoadAct' 1
gate "d2boot: DRLG worldgen marks"        Source/D2Game/GAME/Level.cpp           'DRLG: '                     2
# ---- [d2boot-fix] rev 34: SP packet-queue race fix + dispatcher spin guards --
gate "d2boot-fix: enqueue lock (queue race)" Source/D2Net/SRC/Client.cpp          'Serialize with the pop path' 1
gate "d2boot-fix: unknown-size returns 0"    Source/D2Net/Game/Server.cpp         'Unhandled variable-length opcode' 1
gate "d2boot-fix: dispatch zero-consume guard" Source/D2Client/GAME/SCmd.cpp      's2c dispatch guard' 1
gate "d2boot-fix: pump drain watchdog"       Source/D2Client/GAME/Game/Game.cpp   's2c pump: drained' 2

# ---- [d2boot-fix] rev 35: roster chain sanitizer + bounded 0x5B guild parse
# hangwatch on the expanded d2boot.log pinned the freeze: main thread spinning inside
# ROSTER_GroupPlayersIntoParties (inlined collect walk) under the 0x5B player-join
# handler -> ROSTER_AllocOrUpdatePlayer -> ROSTER_RebuildPlayerParty. The gpRosterPlayers
# chain was cyclic (heap corruption upstream); every roster walk spins on it. Fix:
# sanitize+heal at the walk entry points, bound every unbounded roster string copy.
gate "d2boot-fix: roster chain sanitizer"    Source/D2Client/GAME/Roster.cpp      'roster heal: gpRosterPlayers cycle cut' 1
gate "d2boot-fix: roster sanitizer calls"    Source/D2Client/GAME/Roster.cpp      'ROSTER_SanitizePlayerChain\(\); // \[d2boot-fix rev35\]' 4
gate "d2boot-fix: bounded roster name copy"  Source/D2Client/GAME/Roster.cpp      'nNameIdx < 15' 1
gate "d2boot-fix: bounded roster tag copy"   Source/D2Client/GAME/Roster.cpp      'nTagIdx < 3' 1
gate "d2boot-fix: bounded guildname copy"    Source/D2Client/GAME/Roster.cpp      'nGuildIdx < 27' 1
gate "d2boot-fix: bounded display name"      Source/D2Client/GAME/Roster.cpp      'nOut < 25' 5
gate "d2boot-fix: bounded display copy"      Source/D2Client/GAME/Roster.cpp      'nDispIdx < 25' 1
gate "d2boot-fix: 0x5B bounded guild scan"   Source/D2Client/GAME/SCmd.cpp        'szGuildAreaEnd' 5
gate "d2boot-fix: 0x5B playerjoin mark"      Source/D2Client/GAME/SCmd.cpp        's2c 0x5B playerjoin' 1
gate "d2boot-fix: guild name getter cap"     Source/D2Game/GAME/Clients.cpp       'nCopied < 32' 1
gate "d2boot-fix: guild name setter cap"     Source/D2Game/GAME/Clients.cpp       'nCopied >= 27' 1

# ---- [d2boot-fix] rev 36: regroup-cluster walk caps (second dump round) ----
# The second hangwatch dump (base 004c0000) parked the main thread at rva 0x5f1fe =
# ROSTER_FlattenPartyListsIntoSingleRosterChain+0xb4 (pOwner tail walk); the first dump
# was ROSTER_GroupPlayersIntoParties+0x36. rev35's sanitize only heals gpRosterPlayers at
# walk entry points; the pOwner chains, the party list, gpRosterOther and the roster-link
# lists stayed unguarded. Every unbounded walk in the regroup cluster now carries a
# 64-node cap + "roster heal:" breadcrumb (SP rosters max out at 8 nodes -> vanilla data
# never reaches the cap). hangwatch offsets are map-relative now and a relocated run logs
# a base note; the build string is bumped to 0.0.6 (rev41) so the next d2boot.log self-identifies.
gate "d2boot-fix: rev36 walk-cap helper"     Source/D2Client/GAME/Roster.cpp      'static void ROSTER_WalkCapped' 1
gate "d2boot-fix: rev36 walk-cap call sites" Source/D2Client/GAME/Roster.cpp      'ROSTER_WalkCapped\(' 31
gate "d2boot-fix: rev36 flatten tail cap"    Source/D2Client/GAME/Roster.cpp      'flatten tail walk' 1
gate "d2boot-fix: rev36 flatten party cap"   Source/D2Client/GAME/Roster.cpp      'flatten party list' 1
gate "d2boot-fix: rev36 regroup iter cap"    Source/D2Client/GAME/Roster.cpp      'regroup iterations' 1
gate "d2boot-fix: rev36 regroup scan cap"    Source/D2Client/GAME/Roster.cpp      'regroup party scan' 1
gate "d2boot-fix: rev36 collect cap"         Source/D2Client/GAME/Roster.cpp      'regroup collect' 1
gate "d2boot-fix: rev36 sort caps"           Source/D2Client/GAME/Roster.cpp      'ROSTER_WalkCapped\("sort' 7
gate "d2boot-fix: rev36 move caps"           Source/D2Client/GAME/Roster.cpp      'ROSTER_WalkCapped\("move' 3
gate "d2boot-fix: rev36 roster-other cap"    Source/D2Client/GAME/Roster.cpp      'roster-other recycle' 1
gate "d2boot-fix: rev36 roster-link scan caps" Source/D2Client/GAME/Roster.cpp    'roster-link walk' 4
gate "d2boot-fix: rev36 roster-link free caps" Source/D2Client/GAME/Roster.cpp    'roster-link free' 2
gate "d2boot-fix: rev36 party-list free cap" Source/D2Client/GAME/Roster.cpp      'party-list free' 1
gate "d2boot-fix: rev36 clear-roster caps"   Source/D2Client/GAME/Roster.cpp      'clear-roster' 4
gate "d2boot-fix: rev36 portal-owner cap"    Source/D2Client/GAME/Roster.cpp      'portal-owner walk' 1
gate "d2boot-fix: rev36 partied-roster cap"  Source/D2Client/GAME/Roster.cpp      'partied-roster walk' 1
gate "d2boot-fix: rev36 by-guid walk cap"    Source/D2Client/GAME/Roster.cpp      'roster-by-guid walk' 1
gate "d2boot-fix: rev36 hangwatch map-relative offsets" Redivivus/d2_hangwatch.cpp 'map-relative' 3
gate "d2boot-fix: rev36 hangwatch base-reloc note" Redivivus/d2_hangwatch.cpp 'hangwatch: note base' 1
gate "d2boot-fix: rev36 hangwatch map span"  Redivivus/d2_hangwatch.cpp      '%08lx\.\.%08lx' 1
# (the "build version" gate that lived here pinned the self-ID string and was
# removed at rev 58 together with the banner gate — see the boot section note)

# ---- enum & stores (flag 38 = UI_VANILLAPLUS) -----------------------------
gate "UI_VANILLAPLUS defined = 38"        Source/D2Headers/d2_enums.h                      'UI_VANILLAPLUS = 38'        1
gate "UI_VANILLAPLUS using-decl"          Source/D2Headers/d2_enums.h                      'using eD2UIvars_ns::UI_VANILLAPLUS' 1
gate "dismiss table decl [39] (ui.data.h)" Source/D2Client/UI/ui.data.h           'gUIFlagDismissModeTable\[39\]' 1
gate "options table static [6] (EscMenu.cpp)" Source/D2Client/UI/EscMenu.cpp      'static D2EscMenuItemStrc gaEscMenuOptionsTable\[6\]' 1
gate "gnUIFlagCount bumped to 0x27"       Source/D2Client/UI/ui.cpp              'gnUIFlagCount = 0x27'       1
gate "dismiss table sized [39]"           Source/D2Client/UI/ui.cpp              'gUIFlagDismissModeTable\[39\] = \{' 1
gate "flag-store member VanillaPlus@0x98" Source/globals/globals.h               '/\* 0x98 \*/ BOOL VanillaPlus' 1
gate "anDismissMode grown to 39"          Source/globals/globals.h               'anDismissMode\[39\]'          1
gate "dismiss alias row (1 upstream + 1 fork)" Source/D2Client/UI/ui.cpp          'gaUIFlagDescs\[9\]\.anDismissMode\[0\]' 2
gate "options header nItemCount 0x6"      Source/D2Client/UI/EscMenu.cpp         '0x6, // VP:'                1
gate "options table sized [6]"            Source/D2Client/UI/EscMenu.cpp         'gaEscMenuOptionsTable\[6\] = \{ // VP:' 1
gate "row-10 click callback wired"        Source/D2Client/UI/EscMenu.cpp         'ESCMENU_ShowVanillaPlusOptions' 1
gate "row-10 init callback wired (row + comment)" Source/D2Client/UI/EscMenu.cpp     'ESCMENU_InitVanillaPlusLabel' 2
# [R102 merge] the row-10 table moved into EscMenu.cpp (wiring + VP comment) and the
# resolution-label code already in this TU references the row-label convention twice,
# so the marker count is 4.
gate "row-10 draw callback wired (row + comment)" Source/D2Client/UI/EscMenu.cpp     'VANILLAPLUS_DrawRowLabel'   4

# ---- Draw.cpp / ui.cpp dispatch + guards ----------------------------------
# Upstream 1775 filed SetUiFlagInit/SetUIFlagUnregister into ui.cpp; the row-10
# register/unregister blocks follow them there (the DRAW_UI render block stays in Draw.cpp).
# [R102 merge] the TU restructure moved the row-10 dismiss-table alias block into ui.cpp
# (beside the register/unregister blocks already pinned here), so the marker count is 3.
gate "SetUiFlagInit/unregister VP blocks"  Source/D2Client/UI/ui.cpp      'VP-BEGIN \[row-10 options page\]' 3
gate "render VP block"                     Source/D2Client/UI/Draw.cpp    'VP-BEGIN \[row-10 options page\]' 1
# rev47 track G: PRESENCE is not PLACEMENT. Row "auto-gold tick wired in
# DRAW_UI" stayed green through the d6affdd merge port even though the port
# re-inserted the VP block one brace early - INSIDE the HelpMenu block of
# DRAW_UI - so AUTO_GOLD_Tick only ran while the help overlay was open and
# auto gold was dead in normal play (issue #10). The awk order-check below
# pins the placement: after DrawBackgroundHelpMenu(); the NEXT code line must
# be the HelpMenu block's closing brace, and VP-BEGIN [auto-gold] must come
# AFTER that brace (never before it).
gate "auto-gold tick wired in DRAW_UI"    Source/D2Client/UI/Draw.cpp            'AUTO_GOLD_Tick\(\);'        1
gate "latch reset in InitializeUI"        Source/D2Client/UI/Draw.cpp            'ALTTOGGLE_Reset\(\);'       1
gate "case UI_VANILLAPLUS (register+unregister)" Source/D2Client/UI/ui.cpp       'case UI_VANILLAPLUS:'       2
gate "VANILLAPLUS_OnToggleUI calls"         Source/D2Client/UI/ui.cpp              'VANILLAPLUS_OnToggleUI\('   2
gate "SetUIFlag bound widened to 38"        Source/D2Client/UI/ui.cpp              'UI_VANILLAPLUS < eUI'       1
gate "GetUIFlag bound widened to 38"      Source/D2Client/UI/ui.cpp              'UI_VANILLAPLUS >= eUI'      1
gate "0x27 guards in Draw.cpp"            Source/D2Client/UI/Draw.cpp            'gnUIFlagCount (!=|==) 0x27' 5
gate "0x27 guards in ui.cpp"              Source/D2Client/UI/ui.cpp              'gnUIFlagCount (!=|==) 0x27' 1

# ---- alt-toggle consumers -------------------------------------------------
gate "label click-through in DRAW_AltItems" Source/D2Client/UI/ShowItems.cpp     'ALTTOGGLE_LabelClickThrough\(\)' 2
# R58 (issue #15): both stale-selection sprite-hit tests MUST pass the real-
# geometry marker (D2ObjectsTxt*)0x1 - HitTestScreen short-circuits a null
# 4th arg to validity-only (CUnit.cpp "if (!pObjectsTxt) return 1;"), which
# made the Rev-23-era nullptr version of both clauses dead code and re-pinned
# the walk-click-retarget pickup (issue #15 = row 37 reopened).
gate "alt-toggle: stale-clear real sprite hit" Source/D2Client/UI/ShowItems.cpp  '\(D2ObjectsTxt\*\)0x1' 2

# ---- fork-api includes at every external call site (missed-include guard) --
# Upstream TUs no longer get fork decls transitively (D2Client.h is pristine);
# every file that calls fork API must include VanillaPlusCore.h itself. A
# missing include here = "was not declared in this scope" at build time.
gate "fork-api include in Game.cpp"       Source/D2Client/GAME/Game/Game.cpp     'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in CUnit.cpp"      Source/D2Client/UNIT/CUnit.cpp         'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in Draw.cpp"       Source/D2Client/UI/Draw.cpp            'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in ShowItems.cpp"  Source/D2Client/UI/ShowItems.cpp       'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
# refiled R106: D2Client/Engine/Application.cpp -> Game/Main.cpp (upstream 316d537a)
gate "fork-api include in Game/Main.cpp"   Source/Game/Main.cpp                    'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "fork-api include in D2Client.cpp"   Source/D2Client/D2Client.cpp           'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1

# ---- persistent orb text (FEATURES.md row 1, repurposed post-0.0.4) --------
# The 0.0.4 orb-face readout was removed after the field round - retail
# already prints cur/max above each orb on hover. The row now pins retail's
# own above-orb text on instead (a stale globe_values ini line is inert).
gate "persistent: gate fn defined"         Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall VANILLAPLUS_GlobeTextPersistent' 1
gate "ini template: globe_persistent key"  Source/D2Client/UI/VanillaPlusCore.cpp 'globe_persistent=0'                1
gate "persistent: decl + cache busted on write" Source/D2Client/UI/VanillaPlusCore.cpp 'dwGlobeTextPersistentLastCheck = 0' 2
gate "persistent wired in Panel.cpp (hp+mp)" Source/D2Client/UI/Panel.cpp          'VANILLAPLUS_GlobeTextPersistent\(\)' 2
gate "fork-api include in Panel.cpp"       Source/D2Client/UI/Panel.cpp           'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "options page: persistent row present" Source/D2Client/UI/VanillaPlusOptions.cpp '"globe_persistent"'          1

# ---- D2Client.cpp must stay hollow ----------------------------------------
gate "hooks marked in D2Client.cpp"       Source/D2Client/D2Client.cpp           'VP-BEGIN \[alt-toggle\]'    2
gate "gate impl NOT re-inlined in D2Client.cpp" Source/D2Client/D2Client.cpp     'gbAltToggleMode|gbAutoGoldMode|ALTTOGGLE_WriteIniTemplate' 0
gate "D2Client.h back to pristine upstream" Source/D2Client/D2Client.h           'ALTTOGGLE_|AUTO_GOLD_|D2VP_' 0

# ---- fork-owned core TU ----------------------------------------------------
gate "core: alt gate defined"             Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall ALTTOGGLE_Active' 1
gate "core: auto-gold tick defined"       Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall AUTO_GOLD_Tick'   1
gate "core: config flush defined"         Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall D2VP_ConfigReloaded' 1
gate "core: self-check defined"           Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall VANILLAPLUS_SelfCheck' 1
gate "core: tripwire static_asserts"      Source/D2Client/UI/VanillaPlusCore.cpp 'static_assert\(UI_'         2
gate "ini template: alt_toggle key"       Source/D2Client/UI/VanillaPlusCore.cpp 'alt_toggle=0'               1
gate "ini template: auto_gold key"        Source/D2Client/UI/VanillaPlusCore.cpp 'auto_gold=0'                1
gate "ini template: portable_save key"    Source/D2Client/UI/VanillaPlusCore.cpp 'portable_save=0'            1
gate "options page: read bridge used"     Source/D2Client/UI/VanillaPlusOptions.cpp 'D2VP_GetFeatureToggle'   2
gate "options page: write bridge used"    Source/D2Client/UI/VanillaPlusOptions.cpp 'D2VP_SetFeatureToggle'   1
gate "options page: shared ini path"      Source/D2Client/UI/VanillaPlusOptions.cpp 'ALTTOGGLE_BuildIniPath'  1

# ---- automap crash tolerance ----------------------------------------------
gate "automap tolerance VP blocks (4 sites + macro)" Source/D2Client/UI/Automap.cpp 'VP-BEGIN \[automap tolerance\]' 5
gate "automap tolerant frees"             Source/D2Client/UI/Automap.cpp         'D2_FREE\(nErrorAddress\);'  4

# ---- sound-defect: class sound tables + sounds.txt null guard ---------------
gate "sounds.txt null guard in SOUND_PlaySound" Source/D2Client/Sound/Sound.cpp 'VP: lookup returns null' 1
# [R102 merge] upstream 962f88d5 (define shared globals in their TU) moved the six
# definitions into D2Client/UNIT/UnitSnd.cpp (decls in UnitSnd.data.h); Amazon stays in
# globals.cpp (retail site) with its extern in globals.h.
gate "class sound externs right-sized [11]" Source/D2Client/UNIT/UnitSnd.data.h 'extern int gaUnitSoundIds\w+\[11\]' 6
gate "class sound tables initialized"      Source/D2Client/UNIT/UnitSnd.cpp     'int gaUnitSoundIds\w+\[11\] = \{' 6
# [1949-1969 re-pin] stale since the 1908->1948 sync's namespacing (the table's
# Amazon entry points at D2Client::UNIT::UnitSnd::gaUnitSoundIdsAmazon now); the
# row stayed red through the owner's R128 gate run. Pattern follows the code.
gate "pointer table targets Amazon table"  Source/D2Client/UNIT/UnitSnd.cpp     'gaUnitSoundTableByClassId\[7\] = \{ \(int\*\)&D2Client::UNIT::UnitSnd::gaUnitSoundIdsAmazon' 1
# restored retail values (1.14d Game.exe .data): full-initializer tripwire per class
# [R99 re-pin] upstream 0fe2eda8 (ghidra 1870 sound names) recovered the retail tables
# in multi-line hex form (values verified identical to the fork's decimal hand-build);
# each class now pins the upstream spelling's opening line plus its unique first value.
gate "sound table Amazon restored (retail .data)"      Source/globals/D2Client.cpp 'int gaUnitSoundIdsAmazon\[11\] = \{' 1
gate "sound table Amazon value [0] (retail .data)"     Source/globals/D2Client.cpp '^\s+0xb3e,\s*$' 1
gate "sound table Barbarian restored (retail .data)"   Source/D2Client/UNIT/UnitSnd.cpp 'int gaUnitSoundIdsBarbarian\[11\] = \{' 1
gate "sound table Barbarian value [0] (retail .data)"  Source/D2Client/UNIT/UnitSnd.cpp '^\s+0xb56,\s*$' 1
gate "sound table Necromancer restored (retail .data)" Source/D2Client/UNIT/UnitSnd.cpp 'int gaUnitSoundIdsNecromancer\[11\] = \{' 1
gate "sound table Necromancer value [0] (retail .data)" Source/D2Client/UNIT/UnitSnd.cpp '^\s+0xb4e,\s*$' 1
gate "sound table Paladin restored (retail .data)"     Source/D2Client/UNIT/UnitSnd.cpp 'int gaUnitSoundIdsPaladin\[11\] = \{' 1
gate "sound table Paladin value [0] (retail .data)"    Source/D2Client/UNIT/UnitSnd.cpp '^\s+0xb46,\s*$' 1
gate "sound table Sorceress restored (retail .data)"   Source/D2Client/UNIT/UnitSnd.cpp 'int gaUnitSoundIdsSorceress\[11\] = \{' 1
gate "sound table Sorceress value [0] (retail .data)"  Source/D2Client/UNIT/UnitSnd.cpp '^\s+0xb5e,\s*$' 1
gate "sound table Assassin restored (retail .data)"    Source/D2Client/UNIT/UnitSnd.cpp 'int gaUnitSoundIdsAssassin\[11\] = \{' 1
gate "sound table Assassin value [0] (retail .data)"   Source/D2Client/UNIT/UnitSnd.cpp '^\s+0xb66,\s*$' 1
gate "sound table Druid restored (retail .data)"       Source/D2Client/UNIT/UnitSnd.cpp 'int gaUnitSoundIdsDruid\[11\] = \{' 1
gate "sound table Druid value [0] (retail .data)"      Source/D2Client/UNIT/UnitSnd.cpp '^\s+0xb6e,\s*$' 1
# the decompile's phantom "init flag" (really Amazon[0]'s low byte) must stay gone
gate "bUnitSoundTableInit phantom gone (cpp)" Source/globals/globals.cpp 'uint8_t bUnitSoundTableInit' 0
gate "bUnitSoundTableInit phantom gone (h)"   Source/globals/globals.h   'uint8_t bUnitSoundTableInit' 0

# ---- rev41: sound FP-constant decode (upstream PR #10 commit 2 port) -------
# The merge port d6affdd re-took D2SoundSmp.cpp from the Ghidra-era tree and
# silently dropped the defect-12 fix (HISTORY.md H3): the IEEE-754 volume
# literals pasted as C integer casts evaluate to ~4.54e18 instead of 0.0001,
# so ClampVolume pinned EVERY engine volume to DSBVOLUME_MIN (-10000) — the
# "sound only on the intro video" field signature. These gates hold the
# decoded state so no future merge can lose it again.
# R87 merge: upstream spelled the same decoded doubles exactly (9.999999747378752e-05
# IS (long double)0.0001 as the compiler sees it — float32 0.0001 promoted; and the
# -2000.0 scale now rides an explicit (long double) cast). Same state, retail spelling.
gate "sound: ClampVolume/Attenuation epsilon decoded (R87 retail-exact)" Source/D2Sound/Src/D2SoundSmp.cpp '\(long double\)9\.999999747378752e-05'    4
gate "sound: -2000 centibel scale restored (R87 retail-exact)"           Source/D2Sound/Src/D2SoundSmp.cpp 'fVar1 \* \(long double\)-2000\.0'        1
gate "sound: ClampAttenuation bound decoded"          Source/D2Sound/Src/D2SoundSmp.cpp '\(long double\)-10000\.0' 1
gate "sound: ClampAttenuation divisor decoded"        Source/D2Sound/Src/D2SoundSmp.cpp '\(long double\)2000\.0'   1
gate "sound: no raw IEEE-754 volume literals left"    Source/D2Sound/Src/D2SoundSmp.cpp '\(double\)(0x3f1a36e2e0000000|-0x3f3c780000000000|0x409f400000000000|-0x3f60c00000000000)' 0

# ---- resolution-mode (1280x720 slider stop) --------------------------------
# The registry value is the slider POSITION (0/1/2/3/4); stops 2/3/4 are
# attached at runtime by the Resolution row's init callback because no MPQ
# label assets exist for them. EscMenu.cpp mapping: 0 -> mode 0, 1 -> mode 2,
# 2 -> mode 4, 3 -> mode 3 (R109), 4 -> mode 5 (R112; stops 3/4 tested
# BEFORE the >= 2 arm).
gate "resolution: draw callback wired (row)" Source/D2Client/UI/EscMenu.cpp    'fpDrawCallback = \*/ &D2Client::UI::EscMenu::UI_SLIDER_DrawResolutionValue' 1
gate "resolution: draw fn defined"           Source/D2Client/UI/EscMenu.cpp     '__fastcall UI_SLIDER_DrawResolutionValue' 1
# Both resolution callbacks (Apply + LoadAndApply) live in EscMenu.cpp on the
# 1775+ layout; main carried LoadAndApply in Storm/_unnamespaced.cpp until the
# 1839 merge, where upstream moved that file to root _unnamespaced.cpp and a
# new D2Hell module (the old UI include pin on it retired with the move).
gate "resolution: apply maps mode 4"         Source/D2Client/UI/EscMenu.cpp     'RequestedScreenModeMaybe = 4' 2
gate "resolution: decl in EscMenu.h"         Source/D2Client/UI/EscMenu.h       'UI_SLIDER_DrawResolutionValue' 1
gate "resolution: init attach"                  Source/D2Client/UI/EscMenu.cpp     'VPRES_BuildBlankPlaceholder|nSubImageCount = 3' 5
gate "resolution: minipanel clamp + modes 4/5/6" Source/D2Client/UI/MiniPanel.cpp   'nResolution > 5|nScreenMode_lcl = 6|nScreenMode_lcl = 5|nScreenMode_lcl = 4' 4
gate "resolution: core blank builder"        Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_BuildBlankPlaceholder' 1
gate "resolution: core font pick"            Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_PickFontMatchingDC6' 1
gate "resolution: self-check wiring"         Source/D2Client/UI/VanillaPlusCore.cpp 'UI_SLIDER_DrawResolutionValue' 1
# Issue #23: the visible 1280x720 label must sit where the retail value
# images sit - right-aligned to the value-column anchor (alignment 2) and on
# their ink line (glyph-cell descent compensated: DRAW_text anchors the cell
# bottom, the value DC6s bake ~4 px of padding under their ink).
gate "resolution: label right-align (#23)"   Source/D2Client/UI/EscMenu.cpp     'ScreenSizeX / 2 \+ 0xe6 - nLabelWidth' 1
gate "resolution: label descender comp (#23)" Source/D2Client/UI/EscMenu.cpp    'RESLABEL_DESCENDER_COMP' 2

# ---- resolution-mode 3 (1344x700 dormant retail stop, R109) -----------------
# The third stop activates retail's own dormant mode 3: D2GFX case 3 = 0x540 x
# 700 and the renderer-side mode-3 branches were always in the binaries (GDI
# DIB + scale, DDraw table slot, Glide wrapper enum 17) - only the UI stop and
# the two client-side mappings were missing. Plan: plans/1344x700-PLAN.md (Gap A =
# the mode-set case, Gap B = the slider stop + label, Gap C = the mini-panel
# re-apply that runs at every game-enter and cutscene-exit).
gate "res3: mode-set case 3"              Source/D2Client/ClientModeInGame.cpp  "Vanilla Plus: retail's dormant 1344x700" 1
gate "res3: d2gfx size-table case 3"      Source/D2gfx/D2GFX.cpp                '\*nWidth = 0x540' 1
gate "res3: init attach stop 4"           Source/D2Client/UI/EscMenu.cpp        'nSubImageCount = 4' 1
gate "res3: per-stop label 1344x700"      Source/D2Client/UI/EscMenu.cpp        'L"1344x700"' 1
gate "res3: draw covers all three stops"  Source/D2Client/UI/EscMenu.cpp        'nImageIndex != 2 && pItem->nImageIndex != 3 && pItem->nImageIndex != 4' 1
gate "res3: mappings to mode 3"           Source/D2Client/UI/EscMenu.cpp        'RequestedScreenModeMaybe = 3' 2
gate "res3: minipanel maps mode 3"        Source/D2Client/UI/MiniPanel.cpp      'nScreenMode_lcl = 3' 1
# Field round 1 (R109): "works in Glide, crashes upon switching in D3D". Root
# cause: Vista+ mode lists omit 1344x700 (non-standard timing), the exclusive
# DD7 SetDisplayMode rejects it, and D2Win::D2GFX_SetVideoMode exits on a
# failed resize. Two-layer fix that must move together (client probe and
# renderer agree on the candidate set: exact mode, else 1366x768 /
# 1360x768 letterbox hosts).
# Field round 2 (R110 -> R111): "doesn't crash, but D3D shows a borderless
# window instead of fullscreen". The R110 ladder only fired on a REJECTED
# SetDisplayMode; the Win10/11 DDraw emulation can also ACCEPT a mode it
# cannot set (fake success) and fall back to a borderless window on the
# unchanged desktop. R111 probes the mode list BEFORE the mode-set
# (unlisted stop -> straight to a listed host, no doomed call) and sizes
# the window to the HOST (R110 moved it to the logical 1344x700 inside the
# 1366x768 host mode, leaving the desktop peeking past the window edges).
gate "res3: d3d host-mode ladder table"   Source/D2Direct3D/Renderer/Direct3D.cpp 'gnVPHostDisplayModes' 4
gate "res3: d3d probe-first log"          Source/D2Direct3D/Renderer/Direct3D.cpp 'hosting fullscreen at' 1
gate "res3: d3d probe helper"             Source/D2Direct3D/Renderer/Direct3D.cpp 'VPRES3D_IsDisplayModeListed' 3
gate "res3: d3d window covers host"       Source/D2Direct3D/Renderer/Direct3D.cpp 'nHostWidth \? nHostWidth : RENDERER_nWindowWidth' 1
gate "res3: d3d clear uses locked rows"   Source/D2Direct3D/Renderer/Direct3D.cpp 'nRowsLeft = \(int\)ddSurfDescSize\.dwHeight' 1
gate "res3: client probe helper"          Source/D2Client/ClientModeInGame.cpp  'VPRES_ExclusiveRendererCanEnterStop' 2
gate "res3: probe exact+host listing"     Source/D2Client/ClientModeInGame.cpp  'VPRES_IsDisplayModeListed' 7
gate "res3: probe depth-aware helper"     Source/D2Client/ClientModeInGame.cpp  'VPRES_IsDisplayModeListedAtDepth' 3
gate "res3: ddraw exact-8bpp arm"         Source/D2Client/ClientModeInGame.cpp  'VPRES_IsDisplayModeListedAtDepth\(nWidth, nHeight, 8\)' 1
gate "res3: probe gates stops 3/4/5"      Source/D2Client/ClientModeInGame.cpp  'RequestedScreenModeMaybe == 3 \|\| RequestedScreenModeMaybe == 4 \|\| RequestedScreenModeMaybe == 5' 1
gate "res3: probe refusal log include"    Source/D2Client/ClientModeInGame.cpp  'include "Source/Fog/Src/LogManager\.h"' 1
gate "fork-api include in EscMenu.cpp"       Source/D2Client/UI/EscMenu.cpp     'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
# 1839 merge: D2FILE_CreateProcess moved to the new D2Hell module; the
# [d2boot] CreateProcess marks re-planted there (merge-tripwire against a
# future regen dropping them again).
gate "d2boot: D2Hell CreateProcess marks"    Source/D2Hell/D2Hell.cpp           'D2BOOT_MarkFmt\("CreateProcess' 2

# ---- resolution mode 5 (1366x768 fifth stop) + stretch present (R112) ------
# Field round 3 (R111 -> R112): "ok, i see the black bars. can we center and
# stretch it to 1366x768 when 1344x700 is not valid. Might as well add
# 1366x768 as a resolution option as well." Two halves that must move
# together: (1) HOSTED STOPS STRETCH - the scene still renders into the
# top-left logical rect of the host-sized flip chain (R111), but the present
# is now a stretched Blt of that rect onto the primary
# (VPRES3D_PresentHostedFrame) instead of a Flip, and D2Win's cursor capture
# scales raw host-space coords back into the logical frame through the D2gfx
# stretch channel the renderer arms in InitializeDevice / clears in
# ReleaseDevice - clicks land where the stretched pixels are, the bottom
# 68px stay reachable; (2) 1366x768 = MODE 5, the slider's FIFTH stop
# (position 4 -> mode 5, before the >= 2 arm): CEA-standard timing, so D3D
# usually enters true-exclusive; the stretch ladder hosts it when the driver
# does not list it. The esc-menu slot array grew [4] -> [5] (EscMenu.h) -
# the only count-5 row is the VP Resolution row (blank 4 attaches at
# runtime; the loader walk still caps at the 4 real path fields).
gate "res5: d2gfx size-table case 5"       Source/D2gfx/D2GFX.cpp                  '\*nWidth = 0x556' 1
gate "res5: d3d mirror case 5"             Source/D2Direct3D/Renderer/Direct3D.cpp '\*nWidth = 0x556' 1
gate "res5: d3d perspective-class gate"    Source/D2Direct3D/Renderer/Direct3D.cpp 'nResolutionMode != 5' 1
gate "res5: glide entry 18"                Source/D2Glide/Renderer/Glide.cpp       'screen_resolution = 18' 1
gate "res5: glide mode-5 surface"          Source/D2Glide/Renderer/Glide.cpp       'RENDERER_nWindowWidth = 0x556' 1
gate "res5: glide perspective path 2/3/4/5" Source/D2Glide/Renderer/Glide.cpp      'nParam != 2 && nParam != 3 && nParam != 4 && nParam != 5' 1
gate "res5: ddraw table entries"           Source/D2DDraw/Renderer/DirectDraw.cpp  '0x540, 0x500, 0x556' 1
gate "res5: ddraw horizontal bound"        Source/D2DDraw/Renderer/DirectDraw.cpp  'gnDDrawRenderWidth = 0x556' 1
gate "res5: gdi dib 1366x768"              Source/D2Gdi/Renderer/Windowed.cpp      'nResolutionMode == 5' 1
gate "res5: gdi scale width 1366"          Source/D2Gdi/Renderer/Windowed.cpp      'nParam == 5' 1
gate "res5: mode-set case 5"               Source/D2Client/ClientModeInGame.cpp    'ScreenSizeX = 0x556' 1
gate "res5: probe stop-5 dims"             Source/D2Client/ClientModeInGame.cpp    'nWidth = 0x556' 1
gate "res5: wide classifier"               Source/D2Client/UI/VanillaPlusCore.cpp  'eResolutionMode == 5' 1
gate "res5: esc menu mapping mode 5"       Source/D2Client/UI/EscMenu.cpp          'RequestedScreenModeMaybe = 5' 2
gate "res5: init attach stop 5"            Source/D2Client/UI/EscMenu.cpp          'nSubImageCount = 5' 1
gate "res5: per-stop label 1366x768"       Source/D2Client/UI/EscMenu.cpp          'L"1366x768"' 1
gate "res6: draw clamp covers slot 5"      Source/D2Client/UI/EscMenu.cpp          'nDrawIdx > 5' 1
gate "res6: escmenu count 6 legal (load+free)" Source/D2Client/UI/EscMenu.cpp          'nSubImageCount > 6' 3
gate "res5: loader walk cap"               Source/D2Client/UI/EscMenu.cpp          'nSubImageWalkCount' 3
# R113 (the R112 struct-growth fallout round): D2EscMenuItemStrc grew to
# 0x554 bytes (pDC6SubImages[5]); the two raw 0x550-byte (0x154-int) table
# walks and the FreeLoadedImages count-4 halt were still retail-shaped. Pin
# the corrected strides so a future struct change cannot silently re-arm it.
# [R115] the struct grew AGAIN for the sixth stop ([5] -> [6], 0x554 ->
# 0x558 bytes): both strides re-point 0x155 -> 0x156 in the SAME commit as
# the growth (the R113 lesson). The pins carry the CURRENT stride.
gate "r115: mouse-Y walk stride 0x558"     Source/D2Client/UI/EscMenu.cpp          'pItemYPosition \+= 0x156' 1
gate "r115: loader item stride 0x558"      Source/D2Client/UI/EscMenu.cpp          'nSubImageOffset \+= 0x156' 1
gate "res5: minipanel maps mode 5"         Source/D2Client/UI/MiniPanel.cpp        'nScreenMode_lcl = 5' 1
gate "res6: subimage slots [6] (EscMenu.h)" Source/D2Client/UI/EscMenu.h           'pDC6SubImages\[6\]' 1

# ---- resolution mode 6 (1920x1080 sixth stop, R115) -------------------------
# Owner: "Great job. works. Published Glide3xVK 0.4.7. Now lets do 1080p".
# Same playbook as mode 5 (R112): D2GFX case 6 + D3D mirror (0x780 x 0x438),
# probe stop-6 dims + refusal snap-back position 5, slider SIXTH stop
# (position 5 -> mode 6, both mappings, ahead of >= 2) + L"1920x1080" label
# + FOURTH runtime blank (count 6), mini-panel == 5 -> mode 6 + clamp > 5,
# D3D perspective-class gate != 6, DDraw [7] tables + horizontal bound 0x780,
# GDI DIB + scale width, Glide grSstWinOpen enum 19 + perspective path, wide
# class (Glide dims pairs + D3D x2 + DDraw + GDI + VanillaPlusCore x2).
# Struct growth rides the R113 lesson: strides 0x155 -> 0x156 + free clamp
# > 6 + draw clamp > 5 land in the SAME commit (pinned above).
# Wrapper dependency: Glide3xVK needs g3x_resolution_size entry 19 =
# {0x780, 0x438} (wrapper 0.4.8 ships it; older wrappers fail soft to
# 640x480 + "unknown resolution 19" log, same contract as 16/17/18).
gate "res6: d2gfx size-table case 6"        Source/D2gfx/D2GFX.cpp                  '\*nWidth = 0x780' 1
gate "res6: d3d mirror case 6"              Source/D2Direct3D/Renderer/Direct3D.cpp '\*nWidth = 0x780' 1
gate "res6: d3d perspective-class gate"     Source/D2Direct3D/Renderer/Direct3D.cpp 'nResolutionMode != 6' 1
gate "res6: glide entry 19"                 Source/D2Glide/Renderer/Glide.cpp       'screen_resolution = 19' 1
gate "res6: glide mode-6 surface"           Source/D2Glide/Renderer/Glide.cpp       'RENDERER_nWindowWidth = 0x780' 1
gate "res6: glide perspective path incl 6"  Source/D2Glide/Renderer/Glide.cpp       'nParam != 5 && nParam != 6' 1
gate "res6: ddraw table entries"            Source/D2DDraw/Renderer/DirectDraw.cpp  '0x556, 0x780' 1
gate "res6: ddraw horizontal bound"         Source/D2DDraw/Renderer/DirectDraw.cpp  'gnDDrawRenderWidth = 0x780' 1
gate "res6: gdi dib 1920x1080"              Source/D2Gdi/Renderer/Windowed.cpp      'nResolutionMode == 6' 1
gate "res6: gdi scale width 1920"           Source/D2Gdi/Renderer/Windowed.cpp      'nParam == 6' 1
gate "res6: mode-set case 6"                Source/D2Client/ClientModeInGame.cpp    'ScreenSizeX = 0x780' 1
gate "res6: probe stop-6 dims"              Source/D2Client/ClientModeInGame.cpp    'nWidth = 0x780' 1
gate "res6: probe gates stop 6"             Source/D2Client/ClientModeInGame.cpp    'RequestedScreenModeMaybe == 6' 1
gate "res6: snap-back stop 6 -> position 5" Source/D2Client/ClientModeInGame.cpp    'dwPosition_lcl = 5' 1
gate "res6: wide classifier"                Source/D2Client/UI/VanillaPlusCore.cpp  'eResolutionMode == 6' 1
gate "res6: noshift wide-class arm"         Source/D2Client/UI/VanillaPlusCore.cpp 'ui::ScreenSizeX == 1920 && .*Inv::ScreenSizeY == 1080' 1
gate "res6: esc menu mapping mode 6"        Source/D2Client/UI/EscMenu.cpp          'RequestedScreenModeMaybe = 6' 2
gate "res6: init attach stop 6"             Source/D2Client/UI/EscMenu.cpp          'nSubImageCount = 6' 1
gate "res6: per-stop label 1920x1080"       Source/D2Client/UI/EscMenu.cpp          'L"1920x1080"' 1
gate "res6: minipanel maps mode 6"          Source/D2Client/UI/MiniPanel.cpp        'nScreenMode_lcl = 6' 1
# The stretch-present channel (five TUs must agree):
gate "stretch: d2gfx channel set/get"      Source/D2gfx/D2GFX.cpp                  'D2GFX_SetPresentationStretch|D2GFX_GetPresentationStretch' 2
gate "stretch: d3d present helper"         Source/D2Direct3D/Renderer/Direct3D.cpp 'VPRES3D_PresentHostedFrame' 7
gate "stretch: d3d Blt present"            Source/D2Direct3D/Renderer/Direct3D.cpp 'DDBLT_WAIT' 2
gate "stretch: d3d arm + release-clear"    Source/D2Direct3D/Renderer/Direct3D.cpp 'D2GFX_SetPresentationStretch' 2
# R123: upstream's file-symbols round refiled EndScene2 from D2Client/Renderer/
# Direct3D.cpp into this TU; the R112 hook rode along (count gate 7 = all five
# call sites + the helper's own 2). The row pins the transplant marker so a
# future regen of the refiled function cannot drop the hook silently.
gate "stretch: endscene2 guard"            Source/D2Direct3D/Renderer/Direct3D.cpp 'R123: function refiled here' 1
gate "stretch: cursor host->logical"       Source/D2Win/Src/D2WinMain.cpp          'D2GFX_GetPresentationStretch' 1

# ---- R116: GDI windowed 1366x768 diagonal shear ----------------------------
# Owner: "in D3D windowed mode 1366x768 everything is distorted and slanted".
# Root cause: -w ALWAYS lands in this GDI renderer (Game/Main.cpp maps
# bWINDOW -> RENDERER_Windowed regardless of bD3D), and the renderer's
# CreateDIBSection 8bpp DIB has a DWORD-aligned scanline stride of
# (width + 3) & ~3 - 1368 at the 1366-wide stop, == width everywhere else.
# Handing nfpFillYBufferTable the logical width (the DirectDraw windowed
# renderer passes its real lPitch) made the Y-buffer row table, the D2CMP
# nDstPitch slots and every raw memset/pixel address drift 2px per row ->
# the whole frame sheared diagonally. R112 shipped mode 5; windowed was
# only ever field-tested fullscreen, so this surfaced three rounds later.
# Pins: the stride computation, the FillY call riding it, the four CELCMP
# nDstPitch slots, and the global's total wiring count (26 incl. decl/comment).
gate "r116: gdi dib pitch computed"        Source/D2Gdi/Renderer/Windowed.cpp      'RENDERER_WINDOWED_nDIBPitch = \(uint32_t\)\(\(RENDERER_nWindowWidth \+ 3\) & ~3\)' 1
gate "r116: fillY rides real stride"       Source/D2Gdi/Renderer/Windowed.cpp      'nfpFillYBufferTable\(gpRenderBackBufferBits, RENDERER_WINDOWED_nDIBPitch' 1
gate "r116: cel nDstPitch slots"           Source/D2Gdi/Renderer/Windowed.cpp      'RENDERER_nWindowHeight, RENDERER_WINDOWED_nDIBPitch' 4
gate "r116: dib pitch global wired"        Source/D2Gdi/Renderer/Windowed.cpp      'RENDERER_WINDOWED_nDIBPitch' 26

# ---- r117: resolution-scaled edge-void fill (D2Client/GAME/Draw.cpp) ----
# Owner at the new 1920x1080 stop (PERSPECTIVE on): "some blackness at the
# very edge". Not a present/letterbox artifact - measured 0px solid black on
# every screen edge of the field screenshot. It is the D2 level-boundary
# void: DrawFloorByViewAndFloor draws only the current room + adjacent
# rooms, and retail's edge skirt (DrawSubTile -> DRAW_DrawEdgeFloorTile
# stamping the act edge tile past the drawn bounds) is a 3-tile strip with
# 30/33 + 25/23 subtile triggers - sized for 800x600 (~25-subtile reach).
# A 1080p view reaches ~127 subtiles ((W/16 + H/8)/2), so the void shows as
# black wedges at the screen edges near any level boundary. R117 replaces
# the strip with a band fill scaled from the live backbuffer (reach
# W/32+H/16+10 subtiles, depth reach/5+1 tiles, span reach*2/5+1 each way),
# off-screen stamps die on DrawFloorTile's rcExtendedView cull. Runs at
# every VPRES wide stop, ortho + perspective - [R121] ALSO at DrawEdges=0
# levels (field R120: A1 outdoor logs edge=0 in both modes, so the retail
# flag gated the whole fill off; the fill now keys on the wide classifier
# only). Retail modes keep the retail exit byte-identical.
# Pins: the reach/depth/span formulas and the 4 band call sites
# (a future edit that drops one open-side band fails the gate).
# [1949-1969 re-pin] the ninth sync's global namespace move respelled the fork
# line's globals (ScreenSizeX -> UI::ui::ScreenSizeX, ScreenSizeY -> UI::Inv::ScreenSizeY);
# the formula itself is unchanged.
gate "r117: fill reach formula"            Source/D2Client/GAME/Draw.cpp           'int nFillReach = UI::ui::ScreenSizeX / 32 \+ UI::Inv::ScreenSizeY / 16 \+ 10' 1
gate "r117: fill depth formula"            Source/D2Client/GAME/Draw.cpp           'int nFillDepth = nFillReach / 5 \+ 1' 1
gate "r117: fill span formula"             Source/D2Client/GAME/Draw.cpp           'int nFillSpan = nFillReach \* 2 / 5 \+ 1' 1
gate "r117: band fill call sites"          Source/D2Client/GAME/Draw.cpp           'nFillI \* 5' 4

# ---- r118: wide-view room walk (D2Client/GAME/Draw.cpp + Wall2.cpp) ----
# Owner after R117: "The blackness at the edges is still there. and is
# present in all video modes 720p and up. 1080p sees way beyond 720p's
# border and yet when i switch to 720p it has a blackness at the edges."
# Not the light grid (LIGHTMAP_InitFromPlayerAndRooms pre-fills the whole
# 48x48 ambient grid with the player room's ambient color) and not R117's
# fill (it only stamps DrawEdges level boundaries): the retail floor pass
# AND the wall2 scene pass walk exactly ONE hop of ppRoomList - the 3x3
# room block. 800x600's ~25-subtile screen reach always fit inside it; the
# wide stops reach 85 (720p) to ~127 (1080p) subtiles, so the world past
# the block never rendered and the cleared backbuffer showed as the black
# rim at the screen edges, in every mode, worse the wider the view. The
# rooms already exist: InitNearRooms materializes the RoomEx graph outward
# ([R121] depth 3->4: field R120 shows the 1080p bottom-corner room is the
# hop-4 diagonal - the depth-3 union stopped 30-70 coords short of the
# screen corner; unmaterialized hop-4 rooms simply yield nothing).
# R118 BFSes that
# neighborhood (WALL2_CollectRoomsBeyondAdjacency, depth 4, hop<=1 skipped,
# world-square culled at reach W/32+H/16+90 subtiles) and feeds hop-2/3
# rooms to both passes: floors through the retail two-pass shape
# (DRAW_DrawWideViewRoomFloors), walls/roofs/units/shadows through
# WALL2_CollectRoomTilesAndUnits' existing rcCull/grid/entry-cap culls.
# Bounds grown by the extra tiles also sharpen R117's open-bound tests to
# fire at real level boundaries. Wide stops only; 640x480/800x600 never
# reach the new code. Pins: collector symbol, both call sites, both wide
# gates, the two-pass helper, and the self-id roll.
gate "r118: room-walk collector defined"   Source/D2Client/GAME/Wall2.cpp          '__stdcall WALL2_CollectRoomsBeyondAdjacency' 1
gate "r118: scene pass wide walk"          Source/D2Client/GAME/Wall2.cpp          'WALL2_CollectRoomsBeyondAdjacency\(pGameViewLocal.pRoom, 4,' 1
gate "r118: scene walk wide gate"          Source/D2Client/GAME/Wall2.cpp          'VPRES_IsWideLayoutMode' 1
gate "r118: wall2 includes vpres header"   Source/D2Client/GAME/Wall2.cpp          'UI/VanillaPlusCore\.h' 1
gate "r118: floor pass wide walk"          Source/D2Client/GAME/Draw.cpp           'Wall2::WALL2_CollectRoomsBeyondAdjacency\(pRoom, 4,' 1
gate "r118: floor two-pass helper"         Source/D2Client/GAME/Draw.cpp           'DRAW_DrawWideViewRoomFloors' 3
gate "r118: floor pass wide gate"          Source/D2Client/GAME/Draw.cpp           'VPRES_IsWideLayoutMode' 4

# ---- r121: the R120 field log verdicts -> fill un-gated + hop-4 walk -------
# Owner's R120 vpdiag capture (1080p, both modes) named the mechanisms:
# edge=0 in BOTH modes with persp=1 (A1 outdoor's DrawEdges=0) -> the R117
# fill + bounds block was dead code in the exact test area (all dumps phase
# 0, fire=0000 st=0 anch=(0,0) wb sentinels); gaps=0/52..86/0/0 -> the ortho
# bottom void is real and 52-86px; walk union stopped 30-70 coords short of
# the bottom screen corner -> the corner room is the hop-4 diagonal; lm
# misses are benign (LIGHTMAP_InitFromPlayerAndRooms pre-fills the grid
# with the player room's ambient - out-of-window samples get ambient, not
# black - same as retail). Fixes: bounds grow + fill runs on the wide
# classifier alone (DrawEdges only kept retail-shaped at retail modes);
# all four trigger conditions read bounds snapshotted BEFORE the first
# stamp (each band's span walks both axes - live reads let an earlier
# band's growth push a later bound past the reach and skip it); both walk
# call sites depth 3->4. Pins: the two wide-classifier arms, the snapshot
# locals, both depth-4 call sites.
gate "r121: bounds grow at wide stops"     Source/D2Client/GAME/Draw.cpp           'wide stops grow the same bounds' 1
gate "r121: fill runs despite DrawEdges0"  Source/D2Client/GAME/Draw.cpp           'band fill was dead code' 1
gate "r121: trigger bounds snapshot"       Source/D2Client/GAME/Draw.cpp           'int nTrigMinX = pDrawFloor->nMinTileX' 1
gate "r121: four snapshot locals"          Source/D2Client/GAME/Draw.cpp           'int nTrigM' 4
gate "r121: walk depth four (floor)"       Source/D2Client/GAME/Draw.cpp           'WALL2_CollectRoomsBeyondAdjacency\(pRoom, 4,' 1
gate "r121: walk depth four (scene)"       Source/D2Client/GAME/Wall2.cpp          'WALL2_CollectRoomsBeyondAdjacency\(pGameViewLocal\.pRoom, 4,' 1

# ---- r119: the R118 reach was subtile-scaled against room-coord (game-coord)
# bounds - 5x short (217 coords = 43 subtiles vs the 127-subtile 1080p half-
# reach), so hop-2/3 rooms only patched a band past the retail 3x3 block and
# R117's four fill open-bound triggers under-fired past 43 subtiles (the
# perspective top edge/corners + the rolling ortho bottom triangle). Reaches
# now computed in coords, fill triggers compare nFillReachCoords, BFS queue
# 128->512, out caps 96->256.
gate "r119: scene reach in coords"         Source/D2Client/GAME/Wall2.cpp          'UI::ui::ScreenSizeX \* 5 / 32 \+ UI::Inv::ScreenSizeY \* 5 / 16 \+ 450' 1
gate "r119: floor reach in coords"         Source/D2Client/GAME/Draw.cpp           'UI::ui::ScreenSizeX \* 5 / 32 \+ UI::Inv::ScreenSizeY \* 5 / 16 \+ 450' 1
gate "r119: fill triggers in coords"       Source/D2Client/GAME/Draw.cpp           'nFillReachCoords' 5
gate "r119: walk queue widened"            Source/D2Client/GAME/Wall2.cpp          'apQueue\[512\]' 1
gate "r119: scene out cap 256"             Source/D2Client/GAME/Wall2.cpp          'apWideRooms\[256\]' 1
gate "r119: floor out cap 256"             Source/D2Client/GAME/Draw.cpp           'apWideRooms\[256\]' 1

# ---- r127: the vpdiag retirement (R120 wide-view probes out) ----------------
# Owner, field-green on R126: "Great job, works. Let's retire the vpdiag
# probes". Same class as the R103 diag retirement (d2light-diag +
# d2drop-diag + hooks out): the R120 instrumentation leaves whole - the
# shared stats struct + externs (Draw.h), the per-frame scratch + reset +
# throttled dump (GAME Draw.cpp), the collector's per-call stats and the
# scene snapshot (Wall2.cpp), the lightmap clamp counters (View.cpp), the
# boot-log include, and Wall2's diag-only Draw.h include. Zero behavior
# change: every removed line was a counter increment, a dump call or a
# declaration - the wide-view fixes the probes shipped with (R117 fill,
# R118 walk, R121 un-gate + snapshot triggers, R122 cull re-fit) stay.
# Tombstones pin the absence.
gate "r127: vpdiag retired (Draw.h)"        Source/D2Client/GAME/Draw.h             'VPDIAG' 0
gate "r127: vpdiag retired (GAME Draw)"     Source/D2Client/GAME/Draw.cpp           'vpdiag|VPDIAG' 0
gate "r127: vpdiag retired (Wall2)"         Source/D2Client/GAME/Wall2.cpp          'vpdiag|VPDIAG|sWalk' 0
gate "r127: vpdiag retired (View)"          Source/D2Client/GAME/View.cpp           'vpdiag|VPDIAG' 0
gate "r127: vpdiag boot sink include gone"  Source/D2Client/GAME/Draw.cpp           'Redivivus/d2_boot\.h' 0
gate "r127: wall2 diag include gone"        Source/D2Client/GAME/Wall2.cpp          'include "D2Client/GAME/Draw\.h"' 0

# ---- r122: the R121-WIP field log verdict -> cull rect re-fit ---------------
# Owner's R121-build vpdiag capture (1080p, both modes, one flip) proved the
# R121 fixes landed AND named the last culprit: vpdiag[1] with edge=0 (the
# un-gate works), fire=1248 st=12432 (all four fill bands firing ~12k
# stamps/frame), f:n=22-29 qf=0 of=0 (hop-4 walk emitting, no drops), wb real
# in both modes - and STILL gaps=0/51..82/0/0. The drawn-floor px AABB hugs
# the rect on every edge (persp px=[-119 2024 -109 1024] vs
# rcv=[-125 -110 2030 1033]; ortho px=[-64 1981 -80 1000] vs
# rcv=[-80 -80 2000 1033]) - the retail rect, not the walk, is the binding
# cull: SetViewRectangleSize builds it as ScreenSizeY - 0x2f (bottom 47px
# above the backbuffer - the 800x600 panel-era wedge) and DrawFloorTile
# line-725 + the wall2 scene line-1258 culls kill every tile anchored past
# it, fill stamps included. Fix: at the wide classifier the rect is re-fit
# to the live backbuffer +-0xc0 (covers retail's widest margin, persp left
# -0x7d, with warp slack). Retail modes byte-identical. The 74% lm misses
# were already ruled benign (ambient pre-fill) in R121.
gate "r122: rect re-fit wide arm"          Source/D2Client/GAME/View.cpp           're-fit the fixed-pixel cull rect' 1
gate "r122: rect left/top base"            Source/D2Client/GAME/View.cpp           'lprc->(left|top) = -0xc0' 2
gate "r122: rect right covers backbuffer"  Source/D2Client/GAME/View.cpp           'lprc->right = UI::ui::ScreenSizeX \+ 0xc0' 1
gate "r122: rect bottom covers backbuffer" Source/D2Client/GAME/View.cpp           'lprc->bottom = UI::Inv::ScreenSizeY \+ 0xc0' 1

# ---- resolution site-5: wide-layout classification + centered 800-px UI ----
# Wide modes (3=1344x700 dormant, 4=1280x720, 5=1366x768 [R112]) join the
# retail 800x600 UI class via VPRES_IsWideLayoutMode and re-center the
# exactly-800-px-wide UI with VPRES_GetWideUiOffsetX() = (ScreenSizeX-800)/2.
# The offset is 0 at modes 0/1/2, so retail behavior stays byte-identical.
gate "site-5: classifier defined"          Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_IsWideLayoutMode' 1
gate "site-5: centering offset defined"    Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_GetWideUiOffsetX' 1
gate "site-5: DRAW_UI gnPanelOffset block" Source/D2Client/UI/Draw.cpp            'VP-BEGIN \[resolution site-5\]' 1
gate "site-5: panel offset ride"           Source/D2Client/UI/Draw.cpp            '80 \+ VPRES_GetWideUiOffsetX' 1
gate "site-5: bar branch + borders gate"   Source/D2Client/UI/Panel.cpp           'VPRES_IsWideLayoutMode' 2
gate "site-5: bar/orb/belt X shifts"       Source/D2Client/UI/Panel.cpp           'VPRES_GetWideUiOffsetX' 37
gate "site-5: ui.cpp 800-class gates"      Source/D2Client/UI/ui.cpp              'VPRES_IsWideLayoutMode' 20
gate "site-5: help art + text shifts"      Source/D2Client/UI/Help.cpp            'VPRES_GetWideUiOffsetX' 75
gate "site-5: questlog 800-class gates"    Source/D2Client/UI/QuestLog.cpp        'VPRES_IsWideLayoutMode' 6
gate "site-5: inventory table shifts"      Source/D2Client/UI/Inv.cpp             'VP_SHIFT_Inv' 43
gate "site-5: inv drop-click gate"         Source/D2Client/UI/Inv.cpp             'VPRES_IsWideLayoutMode' 1
gate "site-5: inv coord cache key"         Source/D2Client/UI/Inv.cpp             'nCoordPageKey' 3
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
  ' Source/D2Client/UI/Inv.cpp)"
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
gate "row-10: page draw call"              Source/D2Client/UI/Draw.cpp            'DRAW_VanillaPlusOptions\(\);' 1
gate "row-10: dispatch hoisted (rev44)"    Source/D2Client/UI/Draw.cpp            'render the page as a SIBLING of the' 1
# rev45 track F: the merge port had scrambled the two icon-pass anchors in
# SPELLSEL_RenderAllSkillGrids - the right-hand grid icons drew at the swap
# grid's anchor (0x50 + offset = left half of the screen at wide modes) and
# the swap grid icons drew at vanilla 0x50 (off the centered panel), while
# both tooltip passes kept the correct anchors. The icon passes also register
# the click regions, so the flyout opened on the wrong side. Gate the pairing:
# both icon passes must consume nStartX (right grid) or the shifted 0x50
# (swap grid) exactly as their tooltip passes do.
gate "site-5: spell grid shifts (R45)"      Source/D2Client/UI/SpellSel.cpp        'VPRES_GetWideUiOffsetX' 5
gate "site-5: spell right-grid icons bar-anchored (R45)" Source/D2Client/UI/SpellSel.cpp 'SPELLSEL_DrawSkillGrid\(UNIT::Player::GetPlayerUnit_UI\(\), nStartX,' 2
gate "site-5: spell swap-grid icons ride panel (R45)" Source/D2Client/UI/SpellSel.cpp 'SPELLSEL_DrawSkillGrid\(UNIT::Player::GetPlayerUnit_UI\(\), 0x50 \+ VPRES_GetWideUiOffsetX' 2
# R123 re-pin 0 -> 1: upstream's 693-unreferenced batch added never-called
# thunks (Unreferenced_004aa6e0/710) whose DrawSkillGrid anchors keep the
# retail spell. The fork's REACHABLE sites stay offset-anchored (rows above:
# nStartX x2, 0x50+offset x2); the thunks are upstream-dialect code by house
# rule and are dead until a future regen wires them.
gate "site-5: spell icons not vanilla-anchored (R45)" Source/D2Client/UI/SpellSel.cpp 'DrawSkillGrid\(UNIT::Player::GetPlayerUnit_UI\(\), 0x50,' 1
# R93: 1 -> 2 (the issue #26 border-3 scene-build guard adds a second
# VPRES_IsWideLayoutMode call in GAMEVIEW_PrepareScene)
gate "site-5: summit clouds gate"          Source/D2Client/GAME/View.cpp          'VPRES_IsWideLayoutMode' 3
# R118: 1 -> 2 (the wide-view room-walk floor gate adds a second classifier
# call in DrawFloorByViewAndFloor; the first is R117's fill gate)
gate "site-5: level edges gate (GAME)"     Source/D2Client/GAME/Draw.cpp          'VPRES_IsWideLayoutMode' 4
# R92: 1 -> 2 (the issue #26 warp-fade wide guard adds a second classifier call)
gate "site-5: level edges gate (client)"   Source/D2Client/D2Client.cpp           'VPRES_IsWideLayoutMode' 2
gate "site-5: listbox metrics gate"        Source/D2Client/UI/ListBox/ListBoxImplementation.cpp 'VPRES_IsWideLayoutMode' 1
gate "site-5: fork-api include in ui.cpp"  Source/D2Client/UI/ui.cpp              'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in Help.cpp" Source/D2Client/UI/Help.cpp           'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in QuestLog.cpp" Source/D2Client/UI/QuestLog.cpp   'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in Inv.cpp" Source/D2Client/UI/Inv.cpp             'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in SpellSel.cpp" Source/D2Client/UI/SpellSel.cpp   'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in View.cpp" Source/D2Client/GAME/View.cpp         'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in GAME Draw.cpp" Source/D2Client/GAME/Draw.cpp    'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-5: fork-api include in ListBox" Source/D2Client/UI/ListBox/ListBoxImplementation.cpp 'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1

# ---- resolution site-6: the vertical half of the shift ----------------------
# Fixed (top-anchored) 800-space Y coordinates - Belts.txt/inventory.txt rect
# Y bounds, "const - gnPanelOffsetY" texts and hit tests, raw border-frame Y -
# ride the bottom-anchored panel art by VPRES_GetWideUiOffsetY() =
# ScreenSizeY - 600 (0 at modes 0/1/2 -> byte-identical retail, 100 at mode 3,
# 120 at mode 4). Input paths use the inverse form "gnPanelOffsetY -
# VPRES_GetWideUiOffsetY". gnPanelOffsetY itself stays -60: bottom-anchored
# expressions already contain ScreenSizeY and would double-shift.
gate "site-6: Y offset defined"            Source/D2Client/UI/VanillaPlusCore.cpp '__stdcall VPRES_GetWideUiOffsetY' 1
gate "site-6: Y offset declared"           Source/D2Client/UI/VanillaPlusCore.h   'VPRES_GetWideUiOffsetY' 1
gate "site-6: belt + border Y shifts"      Source/D2Client/UI/Panel.cpp           'VPRES_GetWideUiOffsetY' 19
gate "site-6: belt hover Y virtualization (R52)" Source/D2Client/UI/Panel.cpp           'nVirtualMouseY = nMouseY - VPRES_GetWideUiOffsetY' 1
gate "site-6: belt tooltip Y anchor (R52)"        Source/D2Client/UI/Panel.cpp           'gnBeltTooltipY = aInvRect\[2\] \+ VPRES_GetWideUiOffsetY' 1
gate "site-6: inv table + text Y shifts"   Source/D2Client/UI/Inv.cpp             'VPRES_GetWideUiOffsetY' 15
# [1949-1969 re-pin 54 -> 70] upstream's switch-regen of the party button
# bodies spells the shared y coords as per-branch assignments, so the same
# behavioral sites now carry more VPRES_GetWideUiOffsetY occurrences; every
# site was diff-checked against HEAD's field-verified lines during the merge.
# [1949-1969 re-pin 54 -> 55] upstream's switch-regen spells shared y coords
# once per branch assignment (case bodies feed a shared draw_last_text_line
# label), where the fork's if-chain spelled the same draw per branch; the
# offset-constant multiset is byte-identical to HEAD's (0x18/0x1f/0x21/0x23/
# 0x32x10/0x37x4/0x41x10/0x185x3/0x1a5x4/0x182x1) - every behavioral site
# carries the offset.
gate "site-6: party panel Y shifts"        Source/D2Client/UI/Party.cpp           'VPRES_GetWideUiOffsetY' 55
gate "site-6: waypoint Y shifts (R81)"      Source/D2Client/UI/Waypoint.cpp        'VPRES_GetWideUiOffsetY' 12
gate "site-6: waypoint input inverse"      Source/D2Client/UI/Waypoint.cpp        'gnPanelOffsetY - VPRES_GetWideUiOffsetY' 3
gate "site-6: questlog Y shifts"           Source/D2Client/UI/QuestLog.cpp        'VPRES_GetWideUiOffsetY' 10
gate "site-6: questlog input inverse"      Source/D2Client/UI/QuestLog.cpp        'gnPanelOffsetY - VPRES_GetWideUiOffsetY' 3
gate "site-6: scrolling text Y shift"      Source/D2Client/UI/Text.cpp            'VPRES_GetWideUiOffsetY' 1
gate "site-6: tooltip + gold dialog Y shifts" Source/D2Client/UI/ui.cpp              'VPRES_GetWideUiOffsetY' 11
gate "site-6: gold dialog X shifts"         Source/D2Client/UI/ui.cpp              'VPRES_GetWideUiOffsetX' 5
# [1949-1969 re-pin 18 -> 19] upstream inlined the insert-disc string select
# into two draws; the site-6 offset rides both now (was one shared draw).
gate "site-6: modal popup Y shifts"        Source/D2Client/Engine.cpp             'VPRES_GetWideUiOffsetY' 19
gate "site-6: help screen Y shifts"        Source/D2Client/UI/Help.cpp            'VPRES_GetWideUiOffsetY' 74
gate "site-6: fork-api include in Party.cpp"  Source/D2Client/UI/Party.cpp        'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-6: fork-api include in Waypoint.cpp" Source/D2Client/UI/Waypoint.cpp   'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-6: fork-api include in Text.cpp"   Source/D2Client/UI/Text.cpp         'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "site-6: fork-api include in Engine.cpp" Source/D2Client/Engine.cpp          'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1

# ---- resolution renderer dispatch (post-field-test fixes, 2026-10-30) -------
# Mode 4 must take the fully-initialized perspective=1 D3D device path (the
# goto left floor/subtile/font caches uninitialized -> black world), and the
# software renderers must size their buffers to 1280x720 (GDI DIB stayed
# 800x600 in the resized -w window; DDraw indexed its size tables OOB).
gate "resfix: d3d mode-4 perspective path"  Source/D2Direct3D/Renderer/Direct3D.cpp 'nResolutionMode != 2 && nResolutionMode != 3 && nResolutionMode != 4' 1
gate "resfix: gdi dib mode-4 1280x720"      Source/D2Gdi/Renderer/Windowed.cpp   'nResolutionMode == 4' 1
gate "resfix: gdi scale width 1280"         Source/D2Gdi/Renderer/Windowed.cpp   'nParam == 4' 1
gate "resfix: gdi DrawSolidRectEx sig header (link tripwire)"     Source/D2Gdi/Renderer/Windowed.h   'void __fastcall DrawSolidRectEx\(int nXStart, int nYStart, int nXEnd, int nYEnd, BYTE dwColor' 1
gate "resfix: gdi DrawSolidRectEx sig definition (link tripwire)"  Source/D2Gdi/Renderer/Windowed.cpp 'void __fastcall DrawSolidRectEx\(int nXStart, int nYStart, int nXEnd, int nYEnd, BYTE dwColor' 1
gate "resfix: ddraw size tables extended"   Source/D2DDraw/Renderer/DirectDraw.cpp '0x540, 0x500' 1
gate "resfix: ddraw horizontal bound 1280"  Source/D2DDraw/Renderer/DirectDraw.cpp 'gnDDrawRenderWidth = 1280' 1

# ---- glide 720p (post-"Video: Software" round, 2026-10-30) ------------------
# Owner field-tested D3D at 720p as WORKING (renderer 6); Glide (renderer 4,
# -3dfx + the paired Glide3xVK glide3x.dll) still failed: InitWindow clamped
# every non-640 stop to an 800x600 surface AND left gbGlidePerspectiveMode 0
# at a non-800x600 size, where InitTextureMemory's non-perspective branch is
# gated on width==800 && height==600 and never runs -> uninitialized caches,
# black world. Game side passes grSstWinOpen enum 16 = 1280x720 (17 = dormant
# 1344x700); the paired wrapper learns both in g3x_resolution_size (Glide3xVK
# branch patch, delivered alongside this one).
gate "resfix: glide mode-4 surface 1280x720"    Source/D2Glide/Renderer/Glide.cpp 'RENDERER_nWindowWidth = 1280' 1
gate "resfix: glide mode-3 surface 1344x700"    Source/D2Glide/Renderer/Glide.cpp 'RENDERER_nWindowWidth = 1344' 1
gate "resfix: glide resolution enum 16"         Source/D2Glide/Renderer/Glide.cpp 'screen_resolution = 16' 1
gate "resfix: glide perspective path modes 2/3/4/5" Source/D2Glide/Renderer/Glide.cpp 'nParam != 2 && nParam != 3 && nParam != 4 && nParam != 5' 1

# ---- glide texmem init + fix-2/3 guards (R46: d6affdd merge-port casualties) --
# The d6affdd "merge port" replaced InitTextureMemory with the fork's pre-merge
# version: the subtile/floor cache size assignments (retail 0x400/0x8000) were
# lost, so GLFLOOR/GLSUBTILE_Initialize divided by uninitialized stack (the
# -3dfx first-frame 0xC0000094, track B); the 3.5 MB VRAM gate, the debug-font
# init and the TMU>=2 split went with it. The same commit dropped the glide-side
# fix-3 class-slot guard and fix-2 stale-nodeIdx validation (their D3D mirrors,
# fixes 21/22, stayed). These rows pin the restored surface.
gate "glide texmem: subtile size assigned (R46)" Source/D2Glide/Renderer/Glide.cpp 'nSubtileTexSize = 0x400' 1
gate "glide texmem: floor size assigned (R46)"   Source/D2Glide/Renderer/Glide.cpp 'nFloorTexSize = 0x8000' 1
gate "glide texmem: debug font wired (R46)"      Source/D2Glide/Renderer/Glide.cpp 'RENDERER_GLIDE_InitDebugFont\(gnGlideTexMemBaseAddress\[0\]' 1
gate "glide texmem: VRAM gate present (R46)"     Source/D2Glide/Renderer/Glide.cpp 'have enough video memory' 1
gate "glide fix-3: class-slot guard (R46)"       Source/D2Glide/Renderer/Glide.cpp '< 1 \|\| 3 <' 1
gate "glide fix-2: stale-nodeIdx guard (R46)"    Source/D2Glide/Renderer/Glide.cpp 'pSlotNode \+ 8\) != nTexStartAddress' 1

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
  ' Source/D2Client/UI/Draw.cpp)"
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
  ' Source/D2Client/UI/ui.cpp)"
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
  ' Source/D2Client/GAME/PalShift.cpp)"
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
# [sixth sync 2026-11-04] upstream 9addb0ec re-spelled the two sites in the VC8
# near-miss shape (per-call named locals). Re-pinned to the superseding form:
# raw = SetGfxSequenceData(..., bHostileSeq) + SetPalShiftIndex(..., bHostilePal),
# 4 sites; inverted = the old inline-cast spelling AND upstream's bNotHostile
# local spelling (both must stay 0).
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /SetPalShiftIndex\(pRosterUnit, \(uint32_t\)!bHostile\)/ { rp++ }
    /SetGfxSequenceData\(pRosterUnit, \(uint32_t\)!bHostile\)/ { rg++ }
    /SetPalShiftIndex\(pRosterUnit, bNotHostile\)/ { rp++ }
    /SetGfxSequenceData\(pRosterUnit, bNotHostile_\)/ { rg++ }
    /SetPalShiftIndex\(pRosterUnit, \(uint32_t\)bHostile\)/ { gp++ }
    /SetGfxSequenceData\(pRosterUnit, \(uint32_t\)bHostile\)/ { gg++ }
    /SetPalShiftIndex\(pRosterUnit, bHostilePal\)/ { gp++ }
    /SetGfxSequenceData\(pRosterUnit, bHostileSeq\)/ { gg++ }
    END { printf "vp_rp_bad=%d vp_rp_good=%d", rp+rg, gp+gg }
  ' Source/D2Client/GAME/RosterPets.cpp)"
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
# R89: a second good site was added on purpose — skeleton classes 0x16b/0x16c
# get ownership polarity forced regardless of byUtrans (UNIT_CreateMonster).
# R90-merge: expectation re-pinned raw=1 -> raw=2 (both sites good polarity).
if command -v awk >/dev/null 2>&1; then
  eval "$(awk '
    /nPalShiftIdx = \(uint32_t\)!uMercType;/ { sp_bad++ }
    /nPalShiftIdx = \(uint32_t\)uMercType;/ { sp_good++ }
    END { printf "vp_sp_bad=%d vp_sp_good=%d", sp_bad+0, sp_good+0 }
  ' Source/D2Client/UNIT/CUnit.cpp)"
  if [ "${vp_sp_bad:-0}" -eq 0 ] && [ "${vp_sp_good:-0}" -eq 2 ]; then
    printf 'ok   %-46s %s inverted=%s raw=%s\n' "spawn: ownership palshift polarity (R51)" "D2Client/UNIT/CUnit.cpp" "${vp_sp_bad:-0}" "${vp_sp_good:-0}"
    PASS=$((PASS + 1))
  else
    printf 'FAIL %-46s inverted=%s raw=%s (want inverted=0 raw=2) - !uMercType spawns own skeletons into the yellow hostile sub-table (issue #8)\n' "spawn: ownership palshift polarity (R51)" "${vp_sp_bad:-0}" "${vp_sp_good:-0}"
    FAIL=$((FAIL + 1))
  fi
fi

# ---- [R53] quest: den turn-in diagnostics (issue #13) ------------------------
# The Akara turn-in interaction was invisible in d2boot.log (mode-1 c2s
# logging caps at 4 packets and the quest path had no marks). These marks
# trace the full turn-in loop: server gossip pick (Q01_Callback00_NpcActivate)
# -> client c2s 0x31 echo -> gossip dispatch (QUEST_OnNpcGossip) -> turn-in
# gate (Q01_Callback11_ScrollMessage, msg 0x4c + server-record RewardAwaits)
# -> grant; plus the den counter per kill, the clear, the state-5 timer
# broadcast, the rejoin state restore (Q01_Callback13_PlayerStartedGame) and
# the Den 0x5D quest-state sync.
gate "quest: gossip dispatch mark (R53)"      Source/D2Game/Quests/QUESTS.cpp        'quest: c2s 0x31 gossip' 1
gate "quest: den 0x5D sync mark (R53)"        Source/D2Game/Quests/QUESTS.cpp        'quest: s2c 0x5D DenOfEvil sync' 1
gate "quest: akara activate marks (R53)"      Source/D2Game/Quests/QUESTS/A1Q1.cpp   'quest: akara activate' 3
gate "quest: akara gossip entry mark (R53)"   Source/D2Game/Quests/QUESTS/A1Q1.cpp   'quest: akara gossip msg=' 1
gate "quest: turn-in gate mark (R53)"         Source/D2Game/Quests/QUESTS/A1Q1.cpp   'quest: turn-in gate' 1
gate "quest: turn-in grant mark (R53)"        Source/D2Game/Quests/QUESTS/A1Q1.cpp   'quest: den reward GRANTED' 1
gate "quest: rejoin restore marks (R53)"      Source/D2Game/Quests/QUESTS/A1Q1.cpp   'quest: rejoin Den' 6
gate "quest: den kill mark (R53)"             Source/D2Game/Quests/QUESTS/A1Q1.cpp   'quest: den kill left=' 1
gate "quest: den cleared mark (R53)"          Source/D2Game/Quests/QUESTS/A1Q1.cpp   'quest: den CLEARED' 1
gate "quest: den state-5 broadcast mark (R53)" Source/D2Game/Quests/QUESTS/A1Q1.cpp  'quest: den final state 5' 1
gate "client: gossip send mark (R53)"         Source/D2Client/UI/ui.cpp              'c2s 0x31 gossip send' 1
gate "client: gossip send VP block (R53)"     Source/D2Client/UI/ui.cpp              'VP-BEGIN \[quest-gossip-mark\]' 8

# ---- [gossip] R54: issue #13 turn-in root cause — truncated gossip tables + chain marks
# Field log (R53) proved T2: activate rewardAwaits=1 + gossip line 3, then no c2s 0x31.
# Root cause: the regen emitted A1Q1/A2Q1 gossip tables SINGULAR (1 slot) while the
# gossip handlers index slots 0..4 — slot 3 (the reward line) read past the object.
# Fixed to 5-slot arrays (slot 0 accept + slot 3 turn-in, ids pinned by each quest's
# own turn-in gate; flavor slots left empty). Chain marks make the whole
# table->0x27->overhead->menu-branch->echo path visible in d2boot.log.
gate "quest: gossip add marks (R54)"          Source/D2Game/Quests/QUESTS.cpp        'quest: gossip add' 4
gate "quest: npc interact 0x27 mark (R54)"    Source/D2Game/Monster/MonsterAI.cpp    'quest: npc interact 0x27 send' 1
gate "s2c 0x27 overhead marks (R54)"          Source/D2Client/UI/Text.cpp            's2c 0x27 overhead' 2
gate "npc menu branch marks (R54)"            Source/D2Client/UI/ui.cpp              'npc menu:' 6
# R87 merge: upstream bc47c6bc recovered the retail gossip tables verbatim (sizes and
# flavor string ids the fork could not invent); the R54/R85 hand-built 5-slot tables are
# superseded. Gates now pin the retail-exact sizes via the VP history notes.
gate "gossip: A1Q1 retail table 8 slots (R87)"  Source/globals/D2Game.cpp              'A1Q1 table: retail-exact 8 slots' 1
gate "gossip: A2Q1 retail table 9 slots (R87)"  Source/globals/D2Game.cpp              'A2Q1 table: retail-exact 9 slots' 1
gate "gossip: Den turn-in entry present (R54)" Source/globals/D2Game.cpp             '/\* .nStringIndex = \*/ 0x4c' 1
gate "gossip: A1Q1 extern 8-slot (R87)"       Source/globals/D2Game.cpp               'A1Q1: retail-exact 8-slot table' 1
gate "gossip: A2Q1 extern 9-slot (R87)"       Source/globals/D2Game.cpp               'A2Q1: retail-exact 9-slot table' 1
gate "gossip: A1Q1 decay assignment (R54)"    Source/D2Game/Quests/QUESTS/A1Q1.cpp   'pNpcMessages = A1Q1::aNpcGossipData' 1
gate "gossip: A2Q1 decay assignment (R54)"    Source/D2Game/Quests/QUESTS/A2Q1.cpp   'pNpcMessages = A2Q1::aNpcGossipData' 1

# ---- [gossip] R85: issue #22 (track P) A1Q2 Bloodraven turn-in — the track-J root-cause family ----
# Same regen truncation as issue #13 (R54): A1Q2's gossip table was SINGULAR
# (accept entry only) while Q02_Callback00_NpcActivate picks slot 3 on
# RewardAwaits — the Blood Raven reward line never existed, so Kashya's
# turn-in never echoed c2s 0x31 with msg 0x5c and
# Q02_Callback11_ScrollMessage's HireMercFromNpc gate was never asked.
# Fixed to a 5-slot array (slot 0 accept 0x51 preserved + slot 3 turn-in 0x5c
# pinned by A1Q2's own gate; flavor slots 1/2/4 left empty), plus the Kashya
# chain marks mirroring the R53 A1Q1 set.
gate "gossip: A1Q2 retail table 6 slots (R87)" Source/globals/D2Game.cpp              'A1Q2 table: retail-exact 6 slots' 1
# R87 merge: retail has TWO 0x5c entries in A1Q2's table (bMenu 0 and bMenu 2 variants
# of the Bloodraven turn-in line, monstats 0x96 both times) — the fork's hand-built
# table only had the one, so the old expectation of 1 was an artifact, not a regression.
gate "gossip: Bloodraven turn-in entries (retail x2)" Source/globals/D2Game.cpp        '/\* .nStringIndex = \*/ 0x5c' 2
gate "gossip: A1Q2 extern 6-slot (R87)"       Source/globals/D2Game.cpp               'A1Q2: retail-exact 6-slot table' 1
gate "gossip: A1Q2 decay assignment (R85)"    Source/D2Game/Quests/QUESTS/A1Q2.cpp   'pNpcMessages = A1Q2::aNpcGossipData' 1
gate "quest: kashya activate marks (R85)"     Source/D2Game/Quests/QUESTS/A1Q2.cpp   'quest: kashya activate' 4
gate "quest: kashya gossip echo mark (R85)"   Source/D2Game/Quests/QUESTS/A1Q2.cpp   'quest: kashya gossip msg=' 1
gate "quest: turn-in gate mark A1Q2 (R85)"    Source/D2Game/Quests/QUESTS/A1Q2.cpp   'quest: turn-in gate REJECTED' 1
gate "quest: bloodraven grant mark (R85)"     Source/D2Game/Quests/QUESTS/A1Q2.cpp   'quest: bloodraven reward GRANTED' 1
gate "d2boot include in A1Q2 (R85)"           Source/D2Game/Quests/QUESTS/A1Q2.cpp   'Redivivus/d2_boot\.h' 1

# ---- [item-ilvl] R57+R59: optional " (N)" suffix on the tooltip name (d2vp.ini item_ilvl)
# D2R "Advanced Tooltips" parity (the ilvl readout half), R56 presentation
# (owner: "append the item name with (ilvl) in the tooltip instead" of the R55
# standalone line) + R57 polish (owner: "Instead of saying (ilvl #) we can
# just say (#) they know it's the ilvl" - the label word is dropped) + R59
# spacing (owner: "Sometimes there is more than one space between Item Name
# and (#)" - the suffix fn trims string-table trailing spaces off the name
# before the append). OFF =
# the suffix fn (fork-owned TU, VanillaPlusCore.cpp) returns without touching
# the name buffer, so the decomp tooltip builders stay byte-identical to
# retail (charter rule 1). ON = the name line reads
# "Long Sword (12)" - the suffix joins the GetItemName buffer before the
# color wrap, in all THREE name sites: LoadItemDesc identified + unidentified
# paths (all normal/uniques/rares/whites/mercenary) and ITEM_BuildSetItemTooltip
# (set pieces). Data source = D2Common's parsed item data
# (ITEM_GetItemData_nItemLevel), same struct the retail req-level line already
# reads client-side. Shop-narrow / book / transmogrify tooltips stay vanilla
# by design.
gate "ilvl: gate fn defined"                  Source/D2Client/UI/VanillaPlusCore.cpp   '__stdcall ITEMILVL_Active' 1
gate "ilvl: suffix fn defined"                Source/D2Client/UI/VanillaPlusCore.cpp   '__stdcall ITEMILVL_AppendNameSuffix' 1
gate "ilvl: suffix format string pinned"      Source/D2Client/UI/VanillaPlusCore.cpp   '" \(%i\)"' 1
gate "ilvl: trailing-space trim (R59)"        Source/D2Client/UI/VanillaPlusCore.cpp   'nNameLen--' 1
gate "ilvl: ini key read by the gate"         Source/D2Client/UI/VanillaPlusCore.cpp   '"item_ilvl"' 1
gate "ilvl: ini template key"                 Source/D2Client/UI/VanillaPlusCore.cpp   'item_ilvl=0' 1
gate "ilvl: decl + cache busted on write"     Source/D2Client/UI/VanillaPlusCore.cpp   'dwItemIlvlLastCheck = 0' 2
gate "ilvl: header declares gate + suffix"    Source/D2Client/UI/VanillaPlusCore.h     'ITEMILVL_' 2
gate "ilvl: suffix call sites (id+unid+set)"  Source/D2Client/UI/Inv.cpp               'ITEMILVL_AppendNameSuffix' 3
gate "options page: ilvl row present"         Source/D2Client/UI/VanillaPlusOptions.cpp '"item_ilvl"' 1

# ---- [shared-stash options row] R67: track N's in-game Options toggle -------
# The "Shared" tab is no longer ini-only: the Vanilla+ Options page gains a
# sixth row ("Shared Stash") wired through the same D2VP_* bridge as the
# other five. D2VP_ConfigReloaded also busts the client stash gate cache so
# the tab follows the toggle the same frame; the server half keeps its own
# 2 s poll (D2Game cannot import D2Client helpers - module boundary).
gate "options page: stash row present"        Source/D2Client/UI/VanillaPlusOptions.cpp '"shared_stash"' 1
gate "stash: options gate flush (R67)"        Source/D2Client/UI/VanillaPlusCore.cpp   'VPSHAREDUI_FlushGateCache' 3

# ---- [save-reenter] R60: issue #19 (save a game, re-enter crashes) — instrumentation + hardening
# Theory of record: the upstream heap corruptor (proven alive by the R54 field log's
# two roster-heal 64-node cap marks during load, on regroup code that is provably
# correct on clean data) lands fatally on the second session. No save/load block
# asymmetry exists (R60 audit: all ten .d2s blocks byte-symmetric), so the round
# ships (a) join/exit roster-state probes that discriminate cross-session stale
# state in one log, (b) the merc petdata null guard (RollMercStats FALSE leaves a
# petdata-less merc whose next SaveMercData was an unguarded NULL deref), (c) the
# pRosterPartyList exit free (the only roster-adjacent global ClearBothRosterLists
# never cleaned), (d) the corpse-flags determinism fix. One owner field run names
# the corruptor's landing site via the walk names + probes.
gate "roster: exit-state probe (R60)"         Source/D2Client/GAME/Roster.cpp          'roster: clear lists players=' 1
gate "roster: party-list freed at exit (R60)" Source/D2Client/GAME/Roster.cpp          'FreeRosterPartyList\(\);' 2
gate "client: 0x5B roster-state probe (R60)"  Source/D2Client/GAME/SCmd.cpp            'roster state: players=' 1
gate "save: merc petdata null guard (R60)"    Source/D2Game/PLAYER/PlayerSave2.cpp     'merc petdata missing - merc fields skipped' 1
gate "save: corpse flags initialized (R60)"   Source/D2Game/PLAYER/PlayerSave2.cpp     'dwCorpseFlags = 1' 1
gate "merc: roll-fail mark (R60)"             Source/D2Game/Monster/MonsterAI.cpp      'merc: RollMercStats FALSE - petdata not registered' 1

# ---- [save-reenter] R61: issue #19 ROOT-CAUGHT — the VP Resolution row's runtime
# blank-attach mutates the STATIC esc-menu table (nSubImageCount 2->3) while
# szSubImage3Path stays empty; ESCMENU_Initialize re-runs on every game entry
# (0x01 -> InitializeUI), so init #2 walked the empty third path and hit the
# retail fatal HaltTail(0x501) = the field crash ("HALT line=1281"). The save is
# irrelevant: ANY second game entry in one process died. Fix: the loader's two
# data guards degrade to marks (empty stop -> keep prior DC6; count out of range
# -> clamp) and the type-1 draw site skips NULL slots + clamps the index.
gate "escmenu: empty-stop degrade (R61)"      Source/D2Client/UI/EscMenu.cpp           'path empty - prior dc6 kept' 1
gate "escmenu: sub-count clamp (R61)"         Source/D2Client/UI/EscMenu.cpp           'out of range - clamped' 1
gate "escmenu: halt tail retired at 0x501"    Source/D2Client/UI/EscMenu.cpp           'ESCMENU_LoadLocalizedImages_HaltTail\(0x501\)' 0
gate "escmenu: halt tail retired at 0x4fe"    Source/D2Client/UI/EscMenu.cpp           'ESCMENU_LoadLocalizedImages_HaltTail\(0x4fe\)' 0
gate "escmenu: null sub-image draw guard"     Source/D2Client/UI/EscMenu.cpp           'nDrawIdx' 5
# R95: string bumped R94 -> R95 (issue #26 round 4: the no-panel-shift feature)
# R96: string bumped R95 -> R96 (issue #26 D3D perspective-toggle triage probes;
# zero behavior change, see the videoopt/world/d3d probe rows below)
# R97: string bumped R96 -> R97 (issue #26: the R96 field log pinned the death
# to halt line=200 - the D3D floor/subtile texture-cache promote range checks -
# on the first frame after a runtime 1 -> 0 flip; R97 guards both promote
# callers and re-arms the probes, see the R97 rows below)
# refiled R106: D2Client/Engine/Application.cpp -> Game/Main.cpp (upstream 316d537a)
# R106: string bumped R105 -> R106 (the 1894->1908 sync + its compile fix: the
# seventh sync's broken round never produced an exe, so R106 in d2boot.log
# unambiguously means the fixed tree)
# R108: string bumped R107 -> R108 (the options-page row round; R107 was the
# owner-corrected landing of the R106 fix and re-pinned nothing - the gate ran
# stale until this round)
# R109: string bumped R108 -> R109 (the 1344x700 dormant-mode round: slider
# stop 3 + the client-side mode-set/mini-panel mappings)
# R111: string bumped R109 -> R111 (the D3D probe-first fullscreen round;
# R110 shipped the first D3D crash fix without a mark bump - mark skipped
# straight to R111 per the owner's every-round-is-a-new-Rxx rule)
# R112: string bumped R111 -> R112 (the stretch-to-fill host present + the
# 1366x768 fifth stop round; owner: "i see the black bars ... add 1366x768")
# R113: string bumped R112 -> R113 (the esc-menu regression round: R112's
# pDC6SubImages[5] growth left the two raw 0x550-byte walks stale - mouse
# hit-test dead + sub-image slot spill - and the FreeLoadedImages retail
# halt armed on the new count-5 row)
# R124: string bumped R123 -> R124 (the D3D fullscreen boot-crash fix: the
# eighth-sync merge left UploadTextureToCache's four node-base sites split
# across upstream's refiled static and the fork's retail-relative form - the
# rebuilt exe then indexed the texture cache through two different regions;
# all four sites back on the relative form, refile-aliasing audit added)
# R125: string bumped R124 -> R125 (the shift-compare round, FEATURES.md 0b:
# hold-Shift equipped-item compare tooltip, D2R hover-compare parity; owner
# decisions 2026-11-06: side-by-side second box, hold trigger, row label
# "Shift Item Compare", shop stock included)
# R126: string bumped 0.0.7-R125 -> 0.0.8-R126 (the shield-compare slot fix:
# shield/quiver hovers pick the off-hand when both arm cells are occupied -
# same-kind slot preference, retail's own choose_body_loc convention)
# R127: string bumped 0.0.8-R126 -> 0.0.8-R127 (the vpdiag retirement: the
# R120 wide-view probes out whole, zero behavior change; owner field-green
# on R126: "Great job, works. Let's retire the vpdiag probes")
# R128: string bumped 0.0.8-R127 -> 0.0.8-R128 (the gamble-cache fix, issue #32:
# click Gamble at Gheed = own-code AV READ on every rev since the regen. The
# vanilla consumer read the per-level gamble cache via data adjacency -
# (&pGambleTable[1].nSize)[level] = nTxtGambleSize+8+4*level, valid only where
# the 100-int cache sat adjacent in the original .data; the regen's separate
# statics made it an OOB read of unrelated globals (pointer read as roll bound
# -> seed%ptr -> pTable+idx*4 fault). Cache moved to file scope + accessor.)
gate "res: self-id 0.0.8-R130"              Source/Game/Main.cpp                    'Vanilla\+0\.0\.8-R130' 1
gate "res: self-id R127 gone"               Source/Game/Main.cpp                    'Vanilla\+0\.0\.8-R127' 0
# ---- [vanilla-plus issue #32] rev 136: track T - the gamble-cache fix. Root
# cause: D2Game/UNIT/SUNITNPC.cpp GAMBLE_GenerateGambleInventory read the
# per-level upper-bound cache through vanilla .data adjacency
# ((&pGambleTable[1].nSize)[level] = nTxtGambleSize+8+4*level); the regen
# split the cache into a function-local static, so the read hit unrelated
# .bss neighbors (nTxtItemRatioSize / pTxtItemRatio heap pointer / the
# quality+books table structs) - garbage bound -> wild pGambleTable->pTable
# index -> the 0x31A37000 AV READ. Fix: cache to file scope next to
# nTxtGambleSize (ItemTbls.cpp), bounds-checked accessor GetGambleLevelCache
# (ItemTbls.h/.cpp), consumer reads the accessor (SUNITNPC.cpp); gamble:
# field marks added (gen start / roll / gen done).
gate "gamble: cache at file scope (R128)"   Source/D2Common/DATATBLS/ItemTbls.cpp   'static int gnItemTblsCacheSize\[100\]' 1
gate "gamble: cache accessor def (R128)"    Source/D2Common/DATATBLS/ItemTbls.cpp   'int __stdcall GetGambleLevelCache\(int32_t nLevel\)' 1
gate "gamble: cache accessor decl (R128)"   Source/D2Common/DATATBLS/ItemTbls.h     'int __stdcall GetGambleLevelCache\(int32_t nLevel\);' 1
gate "gamble: consumer uses accessor (R128)" Source/D2Game/UNIT/SUNITNPC.cpp        'GetGambleLevelCache\(\(int\)pPlayerUnit\)' 1
gate "gamble: adjacency artifact gone (R128)" Source/D2Game/UNIT/SUNITNPC.cpp      'pGambleTable\[1\]\.nSize' 0
gate "gamble: gen start mark (R128)"        Source/D2Game/UNIT/SUNITNPC.cpp         'gamble: gen start npc=' 1
gate "gamble: roll mark (R128)"             Source/D2Game/UNIT/SUNITNPC.cpp         'gamble: roll level=' 1
gate "gamble: gen done mark (R128)"         Source/D2Game/UNIT/SUNITNPC.cpp         'gamble: gen done items=' 1
# R115: string bumped R113 -> R115 (the 1080p sixth-stop round; owner:
# "Great job. works. Published Glide3xVK 0.4.7. Now lets do 1080p")
# R116: string bumped R115 -> R116 (the GDI windowed 1366x768 shear fix;
# owner: "in D3D windowed mode 1366x768 everything is distorted and slanted")
# R118: string bumped R117 -> R118 (the wide-view room-walk round; owner:
# "The blackness at the edges is still there. and is present in all video
# modes 720p and up")
# R120: string bumped R119 -> R120 (the wide-view diagnostics round; owner
# after R119: "same behavior, blackness on top with perspective, blackness
# on bottom without. Is there anything we could add to d2boot.log to help
# diagnose the issue?")

# ---- [vanilla-plus issue #26] rev 122: track S - 720p single-panel black
# triangles. Root cause: every panel-open world cull/pan is a fraction of the
# WINDOW width (half/quarter), tuned for 640x480/800x600 where the open panel
# reaches the screen edge; the 800-class panel floats 240 px inboard at
# 1280x720, so the margin-side cut left the outer strip unrendered (black).
# R92 keeps the pan (the mouse mapping rides it) and opens the margin side of
# every cull in wide layouts only: the tile walk (Path.cpp), the renderer
# perspective rect (D3D + Glide), the unit-click reject (CUnit.cpp), the warp
# fade (D2Client.cpp) and the weather clamps (Weather.cpp).
gate "issue26: wide walk span (R92)"          Source/D2Client/UI/Path.cpp              'issue #26 / R92' 3
gate "issue26: wide d3d vp rect (R92)"        Source/D2Direct3D/Renderer/Direct3D.cpp  'issue #26 / R92' 1
gate "issue26: wide glide bounds (R92)"       Source/D2Glide/Renderer/Glide.cpp        'issue #26 / R92' 1
gate "issue26: wide click reject (R92)"       Source/D2Client/UNIT/CUnit.cpp           'issue #26 / R92' 1
gate "issue26: wide warp fade (R92)"          Source/D2Client/D2Client.cpp             'issue #26 / R92' 1
gate "issue26: wide weather clamps (R92)"     Source/D2Client/Draw/Weather.cpp         'issue #26 / R92' 4
# R93 (rev 123): the R92 field pass (owner: "left side ... no more blackness
# ... right side still does ... both panels ... black on all sides") showed
# three more members of the cull family: (1) the NON-perspective half-screen
# tile clips in every renderer (D3D defaults perspective OFF, so
# BuildWallTileVerticesNoPerspective / BuildShadowTileVertices + the Glide
# Simple drawers + the DDraw/GDI wall/shadow/trans clips are the live path);
# (2) GAMEVIEW_PrepareScene skipped WALL2_BuildSceneDrawList entirely for
# border 3 (both panels open = nothing rendered = all black); (3) R92's
# case-2 walk left flip broke the non-perspective wall pan (pView.left IS
# the pan origin in the software passes) - R93 keeps the retail +quarter
# anchor and rides the extra span on the right edge (w+3q).
gate "issue26: d3d nonpersp clips (R93)"      Source/D2Direct3D/Renderer/Direct3D.cpp  'issue #26 / R93' 5
gate "issue26: glide nonpersp clips (R93)"    Source/D2Glide/Renderer/Glide.cpp        'issue #26 / R93' 7
gate "issue26: ddraw clips (R93)"             Source/D2DDraw/Renderer/DirectDraw.cpp   'issue #26 / R93' 7
gate "issue26: gdi window clips (R93)"        Source/D2Gdi/Renderer/Windowed.cpp       'issue #26 / R93' 7
gate "issue26: border3 scene build (R93)"     Source/D2Client/GAME/View.cpp            'issue #26 / R93' 1
gate "issue26: case2 pan anchor (R93)"        Source/D2Client/UI/Path.cpp              'issue #26 / R92\+R93' 1
# R94 (rev 124): the R93 field pass (owner: "with it off. both right and left
# individually are good. both panels, still black. there is a little bit of
# black on the bottom of the screen with perspective mode off ... with
# perspective mode on, left side is ok, right side still has black triangles
# at edge. same black outside panels with both panels") exposed two more
# family members: (1) Game.cpp skips the WHOLE ViewDraw2 world pass for
# border 3 (retail fill-rate save; the 800-class UI floats 240 px inboard at
# 720p so the outer strips still show world -> pure black backbuffer on
# every renderer, both perspective modes - R93's scene-list build fed a pass
# that never ran); (2) the bottom-band vertical cuts - the non-perspective
# wall/shadow builders anchor-cut at h-0xf (a thin black strip at the screen
# bottom with perspective OFF) and the perspective cull rects reserve the
# bottom 0x2f px for the retail full-width HUD (bottom corners show world at
# 720p). All opened on wide layouts only.
gate "issue26: border3 world draw (R94)"      Source/D2Client/GAME/Game/Game.cpp       'issue #26 / R94' 1
gate "issue26: d3d bottom band (R94)"         Source/D2Direct3D/Renderer/Direct3D.cpp  'issue #26 / R94' 3
gate "issue26: glide bottom band (R94)"       Source/D2Glide/Renderer/Glide.cpp        'issue #26 / R94' 3

# ---- [vanilla-plus shared-stash] rev 82: track N round 1 - one-page shared
# stash. Server TU D2Game/PLAYER/VanillaPlusStash.cpp materializes the shared
# set into the retail page-4 stash grid; the .sss file (_d2vp_shared.sss) is
# the pool's own save; the client tab rides 0x4F/0x7D; the save flush keeps
# every .d2s retail-pure (I1). Owner constraints: own save file, retail-
# compatible character saves, stash-panel access, no hotkey, gold + items.
gate "stash: server TU header"                Source/D2Game/PLAYER/VanillaPlusStash.h   'VPSHARED_BUTTON_TOGGLE = 0x7D' 1
gate "stash: sss io DWORD outparam"           Source/D2Game/PLAYER/VanillaPlusStash.cpp 'DWORD dwGot' 1
gate "stash: tab right edge anchor (R83)"     Source/D2Client/UI/VanillaPlusCore.cpp    'VPSHAREDUI_TAB_RIGHT_EDGE = 0x130' 1
gate "stash: tab out of right-art cover"      Source/D2Client/UI/VanillaPlusCore.cpp    'VPSHAREDUI_TAB_RIGHT_EDGE = 0x1(4|[5-9a-f])[0-9a-f]' 0
gate "stash: tab row above the gold rows (R77)" Source/D2Client/UI/VanillaPlusCore.cpp  'bIsExpansion \? 0x1cc : 0x108' 1
gate "stash: old gold-max row retired (R77)"  Source/D2Client/UI/VanillaPlusCore.cpp    'bIsExpansion \? 0x19f : 0xdb' 0
gate "stash: click-button intercept site"     Source/D2Game/PLAYER/PlayerTrade.cpp      'VPSHARED_ShouldInterceptClickButton\(pGame, pUnit, nButtonId\)' 1
gate "stash: click-button include"            Source/D2Game/PLAYER/PlayerTrade.cpp      'PLAYER/VanillaPlusStash\.h' 1
gate "stash: save flush site"                 Source/D2Game/PLAYER/PlayerSave.cpp       'VPSHARED_FlushBeforeSave\(pGame, pUnit\)' 1
gate "stash: save flush include"              Source/D2Game/PLAYER/PlayerSave.cpp       'PLAYER/VanillaPlusStash\.h' 1
gate "stash: client tab draw site"            Source/D2Client/UI/Inv.cpp                'VPSHAREDUI_DrawTab\(\); // VP: site-N' 1
gate "stash: client tab click site"           Source/D2Client/UI/Inv.cpp                'VPSHAREDUI_HandleStashClick\(\(int\)uMouseX, \(int\)uMouseY\)' 1
gate "stash: client reset site"               Source/D2Client/UI/Draw.cpp               'VPSHAREDUI_ResetForNewGame\(\);' 1
gate "stash: ini template key"                Source/D2Client/UI/VanillaPlusCore.cpp    'shared_stash=0' 1
gate "stash: client gate key"                 Source/D2Client/UI/VanillaPlusCore.cpp    '"features", "shared_stash", 0' 1
gate "stash: server gate key"                 Source/D2Game/PLAYER/VanillaPlusStash.cpp '"features", "shared_stash", 0' 1
gate "stash: sss file name"                   Source/D2Game/PLAYER/VanillaPlusStash.cpp '_d2vp_shared\.sss' 2
gate "stash: sss magic"                       Source/D2Game/PLAYER/VanillaPlusStash.cpp "'V', 'P', 'S', '1'" 1
# R75: two shared pages + pager arrows + outlined label. bad-header(size)
# fires twice now (per-page ingest + the v2 page-1 extent pre-check).
gate "stash: load marks"                      Source/D2Game/PLAYER/VanillaPlusStash.cpp 'shared stash: (no file|bad header|checksum fail|item count)' 6
gate "stash: write marks"                     Source/D2Game/PLAYER/VanillaPlusStash.cpp 'shared stash: (write failed|saved)' 5
gate "stash: toggle marks"                    Source/D2Game/PLAYER/VanillaPlusStash.cpp 'shared stash: (shared page|switched to page|personal page restored)' 3
gate "stash: save flush mark"                 Source/D2Game/PLAYER/VanillaPlusStash.cpp 'shared stash: flush before save' 1
gate "stash: atomic write"                    Source/D2Game/PLAYER/VanillaPlusStash.cpp 'MoveFileExA' 3
gate "stash: .bak generation"                 Source/D2Game/PLAYER/VanillaPlusStash.cpp 'CopyFileA' 1
gate "stash: goldbank borrow"                 Source/D2Game/PLAYER/VanillaPlusStash.cpp 'UNITSTAT_goldbank' 5
gate "stash: retail item encoder"             Source/D2Game/PLAYER/VanillaPlusStash.cpp 'Items::SerializeItem' 2
gate "stash: retail item loader"              Source/D2Game/PLAYER/VanillaPlusStash.cpp 'PLRSAVE_DispatchItemLoadByFormat' 2
gate "stash: dematerialize via retail remove" Source/D2Game/PLAYER/VanillaPlusStash.cpp 'ITEM_RemoveItemFromCurrentLocation' 2
gate "stash: client resync 0x9C"              Source/D2Game/PLAYER/VanillaPlusStash.cpp 'Send0x9CItemPutInContainer' 1
gate "stash: scope guard SP-only"             Source/D2Game/PLAYER/VanillaPlusStash.cpp 'SGAMETYPE_SINGLEPLAYER' 2

# ---- [vanilla-plus shared-stash] R75: page 2 + pager arrows + outlined tab
# Owner ask: "give the current stash tab label an outline" + "add a 2nd page
# and buttons/arrows to navigate pages". Server: two shared pages as two 'JM'
# blocks, .sss v2 (magic VPS2, per-page size/checksum pairs) with the v1
# field files read back as page 1; the 0x4F/0x7D param word now carries the
# ABSOLUTE desired page (0/1/2). Client: outlined composite (word + < >),
# click bands from the same measured layout the draw uses.
gate "stash: two-page state"                  Source/D2Game/PLAYER/VanillaPlusStash.cpp 'apSharedBlock\[VPSTASH_PAGES\]' 1
gate "stash: v2 file magic"                   Source/D2Game/PLAYER/VanillaPlusStash.cpp "'V', 'P', 'S', '2'" 1
gate "stash: v1 file read-back"               Source/D2Game/PLAYER/VanillaPlusStash.cpp 'VPSTASH_FILE_VERSION_V1' 2
gate "stash: per-page ingest helper"          Source/D2Game/PLAYER/VanillaPlusStash.cpp 'VPSHARED_IngestPageBlock' 2  # R82: one call site inside the per-page ingest loop
gate "stash: page switch mark"                Source/D2Game/PLAYER/VanillaPlusStash.cpp 'shared stash: switched to page' 1
gate "stash: page param range mark"           Source/D2Game/PLAYER/VanillaPlusStash.cpp 'shared stash: page %d out of range' 1
gate "stash: pager page count"                Source/D2Client/UI/VanillaPlusCore.cpp    'VPSHAREDUI_PAGE_COUNT = 10' 1  # R82 (was 2 at R75)
gate "stash: outlined label helper"           Source/D2Client/UI/VanillaPlusCore.cpp    'VPSHAREDUI_DrawOutlinedText' 4
gate "stash: outline shadow palette"          Source/D2Client/UI/VanillaPlusCore.cpp    'aOffs\[4\]\[2\]' 1
gate "stash: pager arrow glyphs"              Source/D2Client/UI/VanillaPlusCore.cpp    'L"<"' 1
gate "stash: page word variants"              Source/D2Client/UI/VanillaPlusCore.cpp    'VPSHAREDUI_PageWord' 3
gate "stash: layout shared by draw+click"     Source/D2Client/UI/VanillaPlusCore.cpp    'VPSHAREDUI_TabLayout' 3

# ---- [vanilla-plus shared-stash] R82: ten shared pages ---------------------
# Owner ask: "increase the shared stash pages to 10" - the widening the v2
# header reserved room for (plans/SHARED-STASH-PLAN.md: "10 pages. Then 100.").
# Same VPS2 magic + version, pageCount 2 -> 10 (header 28 -> 92 bytes), the
# two fixed ingest calls became a per-page loop, and the reader accepts any
# pageCount this build can hold so pre-R82 two-page files read back with
# pages 3..10 empty (the next save rewrites ten). Client pager labels come
# from a literal word table (Shared 1 .. Shared 10); arrows still step ONE
# page; ONE shared gold pool across all ten.
gate "stash: ten-page server constant (R82)"  Source/D2Game/PLAYER/VanillaPlusStash.cpp 'VPSTASH_PAGE_COUNT = 10' 1
gate "stash: ten-page pager constant (R82)"   Source/D2Game/PLAYER/VanillaPlusStash.cpp 'constexpr int VPSTASH_PAGES = 10' 1
gate "stash: v2 header sized by page count"   Source/D2Game/PLAYER/VanillaPlusStash.cpp 'VPSTASH_HEADER_SIZE_V2 = 12 \+ 8 \* VPSTASH_PAGE_COUNT' 1
gate "stash: reader accepts pre-R82 widths"   Source/D2Game/PLAYER/VanillaPlusStash.cpp 'nPages >= 1 && nPages <= VPSTASH_PAGE_COUNT' 1
gate "stash: header tail sized by pageCount"  Source/D2Game/PLAYER/VanillaPlusStash.cpp 'uint32_t dwHeaderSize = 12 \+ 8 \* \(uint32_t\)nPages' 1
gate "stash: pre-R82 readback mark (R82)"     Source/D2Game/PLAYER/VanillaPlusStash.cpp 'start empty \(pre-R82 file\)' 1
gate "stash: page word table (R82)"           Source/D2Client/UI/VanillaPlusCore.cpp    'L"Shared 10"' 1

# ---- [sync] Rev 76: the 1781 merge re-planted SCmd.cpp's srv state-cmd
# [d2boot] mark but dropped the fork's include of support/d2_boot.h (last line
# of the include block - classic merge-drop, invisible to presence gates and
# fatal at compile: "D2BOOT_MarkFmt was not declared in this scope"). This row
# pins the regression site; scripts/d2boot_visibility_audit.py checks the
# WHOLE class (every D2BOOT_* user TU must see the declaration) - run it
# alongside rev47/rev48 after any merge (docs/merge-upstream.md step 4).
gate "d2boot: SCmd.cpp include (merge-drop tripwire)" Source/D2Game/GAME/SCmd.cpp 'Redivivus/d2_boot\.h' 1

# ---- [vanilla-plus item-tooltip] R84: single-value elemental/poison varargs -
# Issue #16: items dropped with "0 poison damage over 0 seconds" and 'of Frost'
# items with "+0 cold damage". Root cause: SKILLDESC_BuildStatDescription's
# single-value branches (fire 0xe1c, lightning 0xe20, magic 0xe22, cold 0xe1e,
# poison 0xe24) called UNICODE_FormatWideString with NO varargs, so any
# min==max roll printed the stack residue instead of the number. 'of Frost'
# (weap) rolls cold-max 1..2 with cold-min fixed 1 -> half the drops collapse;
# poison ranges collapse through the len_disp rounding (and length-0 content
# lands here too). Fix passes the same values the sibling range branches pass
# (extra varargs are harmless in cdecl). Five VP-marked sites, one per branch.
gate "itemdesc: zero-fix VP marks (R84)"      Source/D2Client/UI/SkillDesc.cpp          'item-tooltip-zero-fix R84' 5
gate "itemdesc: elemental single-value varargs (R84)" Source/D2Client/UI/SkillDesc.cpp  'wszFmtTemplate, pStatDescResult, (psVar1|pMaxDamage)\);' 8
gate "itemdesc: poison single-value varargs (R84)" Source/D2Client/UI/SkillDesc.cpp      'pStatDescResult, (psVar1|pMaxDamage), (iVar2|nPoisonSeconds)\);' 2

# ---- [vanilla-plus nointro] R86: the -nointro command-line switch ----------
# The two logo .biks (New_BLIZ/BlizNorth) play on EVERY launch, and the title
# screen auto-fires the d2intro .bik from its timer form
# (gaPredefinedForms[1] -> UIMENU_PlayIntroAndReturnToMainScreen). Both
# boot/title cutscene entries now consult VPNOINTRO_IsRequested (fork-owned
# one-shot parse of GetCommandLineA in VanillaPlusCore.cpp) and, with the
# switch present, never enter BeginCutSceneAtResolutionMode0 - each site's
# retail begin-failed fallthrough (no cutscene, straight to the next screen)
# becomes the boot path. In-game quest cutscenes ride a different call path
# and stay vanilla. Without the switch the flag reads 0: boot is untouched.
gate "nointro: parse + d2boot mark (R86)"  Source/D2Client/UI/VanillaPlusCore.cpp 'cmdline -nointro: boot intro videos disabled' 1
gate "nointro: token compare (R86)"        Source/D2Client/UI/VanillaPlusCore.cpp '_stricmp\(szCompare, "-nointro"\)' 1
gate "nointro: header entry point (R86)"   Source/D2Client/UI/VanillaPlusCore.h   'VPNOINTRO_IsRequested' 1
gate "nointro: MainMenus include (R86)"    Source/D2Launch/MainMenus.cpp          'include "Source/D2Client/UI/VanillaPlusCore\.h"' 1
gate "nointro: cutscene gates (R86)"       Source/D2Launch/MainMenus.cpp          'VPNOINTRO_IsRequested\(\) \? 0 : D2Win::D2GFX::BeginCutSceneAtResolutionMode0' 2
gate "nointro: host decl (R86)"            Source/D2Headers/d2_platform.h                   'LPSTR GetCommandLineA\(\);' 1

# ---- [vanilla-plus no-panel-shift] R95: static world with one panel open ---
# Retail slides the world a quarter screen when exactly one panel is open so
# the action re-centers in the uncovered strip - exact only when the panel
# reaches the screen edge (640x480/800x600). At the wide stops the 800-class
# UI floats 240 px inboard, the slide drags world out from under the visible
# outer margins, and the perspective projection (player-anchored, expanding
# from the screen center) makes every panned channel a separate artifact
# source - the whole R92-R94 tail of issue #26 lived on that panned path.
# no_panel_shift=1 renders the world exactly like the both-panels-open case
# (border 3: no slide, full unpanned view rect) and lets the panel float over
# it. The renderers receive 0 through PANELSHIFT_BorderForWorld and take
# their own border-0 full-screen path - zero renderer edits; the R92-R94
# renderer guards stay live for the vanilla slide. UI/interaction consumers
# keep reading the real border type. Retail dims: gate false, byte-identical.
gate "noshift: ini key + reader (R95)"     Source/D2Client/UI/VanillaPlusCore.cpp 'GetPrivateProfileIntA\("features", "no_panel_shift", 0, szIniPath\)' 1
gate "noshift: ini template entry (R95)"   Source/D2Client/UI/VanillaPlusCore.cpp 'no_panel_shift=0' 2
gate "noshift: wide dims gate (R95)"       Source/D2Client/UI/VanillaPlusCore.cpp 'PANELSHIFT_NoShiftWorld' 3
gate "noshift: header decls (R95)"         Source/D2Client/UI/VanillaPlusCore.h   'PANELSHIFT_BorderForWorld' 1
gate "noshift: camera pan gate (R95)"      Source/D2Client/UI/Path.cpp            'MouseYRelatedB = PANELSHIFT_NoShiftWorld\(\) \? 0' 2
gate "noshift: border-0 view rects (R95)"  Source/D2Client/UI/Path.cpp            'if \(PANELSHIFT_NoShiftWorld\(\)\) \{' 2
gate "noshift: world-draw origins (R95)"   Source/D2Client/GAME/Draw.cpp          'PANELSHIFT_BorderForWorld\(UI::Path::DRAW_GetTypeOfBorder\(\)\)' 6
gate "noshift: weather clips (R95)"        Source/D2Client/Draw/Weather.cpp       'PANELSHIFT_BorderForWorld\(UI::Path::DRAW_GetTypeOfBorder\(\)\)' 3
gate "noshift: automap pan (R95)"          Source/D2Client/UI/Automap.cpp         'PANELSHIFT_BorderForWorld\(Path::DRAW_GetTypeOfBorder\(\)\)' 1
gate "noshift: config cache bust (R95)"    Source/D2Client/UI/VanillaPlusCore.cpp 'dwNoPanelShiftLastCheck = 0' 2
# R100: the unit-sprite draw path (Ghidra-1864+ retail bodies hand-pan
# sprites/shadows/selection rings/hover text +/-quarter off the REAL border
# in the perspective branch) must ride the same border-0 camera as the
# terrain passes when no_panel_shift=1.
gate "noshift: unit sprite pan (R100)"      Source/D2Client/Engine/Gfx.cpp         'PANELSHIFT_BorderForWorld\(UI::Path::DRAW_GetTypeOfBorder\(\)\)' 7
gate "noshift: unit cof direction (R100)"   Source/D2Client/Engine/GfxUtil.cpp     'PANELSHIFT_BorderForWorld\(UI::Path::DRAW_GetTypeOfBorder\(\)\)' 1
gate "noshift: unit hover text (R100)"      Source/D2Client/UI/Text.cpp            'PANELSHIFT_BorderForWorld\(Path::DRAW_GetTypeOfBorder\(\)\)' 2
gate "noshift: item label anchor (R100)"    Source/D2Client/UI/ShowItems.cpp       'PANELSHIFT_BorderForWorld\(Path::DRAW_GetTypeOfBorder\(\)\)' 1
gate "noshift: unit overlays (R100)"        Source/D2Client/UI/ui.cpp              'PANELSHIFT_BorderForWorld\(Path::DRAW_GetTypeOfBorder\(\)\)' 3

# ---- [issue #26 / R101] the options-page row (toggle no longer ini-only) -----
# Field-proven R100 -> owner: "no_panel_shift is working as intended now. Let's
# add the toggle to the Vanilla+ Options panel". The row is the R67 stash-row
# shape: one gaVanillaPlusRows entry riding the existing D2VP_* bridge (the
# write bridge already busts dwNoPanelShiftLastCheck in D2VP_ConfigReloaded,
# so the very next PANELSHIFT_NoShiftWorld poll re-reads the key). Seventh row
# forced the Previous Menu row down one pitch (0x124 -> 0x144); R108's eighth
# row (mana_burn_fix) ran the same move again (0x144 -> 0x164, box 0x171 ->
# 0x191), and R125's ninth row (shift_compare) once more (0x164 -> 0x184, box
# 0x191 -> 0x1b1) - the layout check comment in VanillaPlusOptions.cpp
# carries the arithmetic; the two R108 shape pins retired with R125 (the
# shiftcmp: cluster below owns the CURRENT shape: VPP_ROW_COUNT = 9 /
# VPPAGE_BACK_ROW_Y = 0x184 / VPPAGE_BOX_HEIGHT = 0x1b1).
gate "options page: noshift row present (R101)"  Source/D2Client/UI/VanillaPlusOptions.cpp '"no_panel_shift"' 1

# ---- [issue #26 / R96] D3D perspective-toggle death triage probes -------------
# Owner field report (R95): toggling perspective in D3D kills the process;
# Glide toggles fine; with no_panel_shift=0 the R95 code is inert at the
# toggle moment (options menu = border 0/3 unchanged, every gate call site
# returns its retail value), so the R95 feature itself is not the trigger.
# R96 ships five one-shot d2boot.log probes that bracket the toggle (slider
# load + apply), the first perspective world frame (enter/drawn) and the D3D
# perspective floor path, plus the two-artifact ask (d2crash-*.txt per
# VANILLA-PLUS.md). Read-out matrix is documented in SMOKE-CHECKLIST §L.
gate "probe: perspective load mark (R96)"   Source/D2Client/UI/EscMenu.cpp            'videoopt: perspective load reg=%d capable=%d \[R96\]' 1
gate "probe: perspective apply mark (R96)"  Source/D2Client/UI/EscMenu.cpp            'videoopt: perspective apply renderer=%d active=%d -> %d \[R96\]' 1
gate "probe: world frame enter (R96)"       Source/D2Client/GAME/View.cpp             'world: first perspective frame enter \(renderer=%d\) \[R96\]' 1
gate "probe: world frame drawn (R96)"       Source/D2Client/GAME/View.cpp             'world: first perspective frame drawn \[R96\]' 1
gate "probe: d3d floor path mark (R96)"     Source/D2Direct3D/Renderer/Direct3D.cpp   'd3d: first perspective DrawGroundTile \(panels=%d w=%d h=%d\) \[R96\]' 1

# ---- [issue #26 / R97] D3D texture-cache promote guards + re-armed probes -----
# The R96 field log names the death: halt line=200 = D3DFLOOR_PromoteNodeToMRU
# / D3DSUBTILE_PromoteNodeToMRU range check (nIndex > nMaxNodes), same 15 ms
# tick as the slider apply (1 -> 0), D3D 800x600. Every static link audits
# retail-clean (cache init once per device init; Free(1) invalidates all tile
# indices; SetViewRectangleSize is rects only; alias offsets verified), so the
# out-of-range word is runtime state. R97: both promote callers validate before
# promoting - dead cache skips the draw, out-of-range index is logged and
# healed to uncached (fresh alloc + re-upload) - and the one-shot R96 probes
# are complemented by transition marks that re-arm on every flip.
gate "guard: floor promote heal (R97)"      Source/D2Direct3D/d3dFloorCache.cpp       'floor promote out-of-range idx=%d max=%d used=%d bufNull=%d tile=%p - healed to uncached \[R97\]' 1
gate "guard: floor dead-cache skip (R97)"   Source/D2Direct3D/d3dFloorCache.cpp       'floor texture cache dead \(max=%d used=%d bufNull=%d tile=%p\) - tile skipped \[R97\]' 1
gate "guard: subtile promote heal (R97)"    Source/D2Direct3D/d3dFloorCache.cpp       'subtile promote out-of-range idx=%d max=%d used=%d bufNull=%d subtile=%p - healed to uncached \[R97\]' 1
gate "guard: subtile dead-cache skip (R97)" Source/D2Direct3D/d3dFloorCache.cpp       'subtile texture cache dead \(max=%d used=%d bufNull=%d subtile=%p\) - subtile skipped \[R97\]' 1
gate "probe: d3d floor transition (R97)"    Source/D2Direct3D/Renderer/Direct3D.cpp   'd3d: perspective now %d, floor tile pass \(panels=%d w=%d h=%d\) \[R97\]' 1
gate "probe: world transition (R97)"        Source/D2Client/GAME/View.cpp             'world: perspective now %d, view frame \[R97\]' 1

# ---- [issue #26 / R98] flat wall byte-offset iterator widened -----------------
# The R97 field pass returned outcome (b): death WITH the guard mark. The mark
# plus the AV decode to one defect: BuildWallTileVerticesNoPerspective walked
# pSubTiles by BYTE OFFSET with a uint8_t iterator (+0x14 per 0x14-byte record,
# wraps at 256 after 12 records - a decompiler artifact merging the dead
# nParam4 arg slot; every sibling iterator is full-width int). From the 13th
# subtile the loop read +4-shifted pseudo-records: nTextureCacheIndex = the
# real record's DT1 wFormat word (idx=4097 = 0x1001, the word that halted
# line=200 in R96) and pPixelData = the next record's {nOffsetY<<16|nOffsetX}
# packed (AV READ at FF600060 = -160/+96 wall offsets), which the R97 heal
# dereferenced in the alloc path. Perspective-ON walls and both shadow builders
# index the array properly - only the flat builder walked raw bytes, so the
# death fires on the first post-flip frame and Glide toggles clean. R98 widens
# the iterator to int and adds a bounded wide-tile probe; the R97 guards stay
# as defense in depth.
gate "fix: flat wall int iterator (R98)"     Source/D2Direct3D/Renderer/Direct3D.cpp   'int nSubtileByteOffset;' 1
gate "probe: flat wall wide-tile (R98)"      Source/D2Direct3D/Renderer/Direct3D.cpp   'd3d: flat wall subtiles=%d tile=%p \[R98\]' 1

# ---- [manaburn-fix / R106] melee drain roll consumes the fixed-point stats raw --
# Retail 1.14d bug (D2R 2.4 parity): the Mana Burn umod stores
# manadrainmindam/maxdam already in the mana 1/256 fixed point (Umod.cpp
# ManaBurn scales by 0x100), but the melee drain roll in SUnitDmg.cpp routes
# them through DAMAGE_RollElementalDamageWithBonus, whose <<8/*0x100 inputs
# are right for the plain-point fire/cold/lightning stats and wrong here -
# a second x256 on top of the umod's. Melee hits drained 256x the intended
# mana (512x in Hell, where the difficulty resist penalty also lands on the
# drain fields' zero resist base); the missile path reads the stats raw and
# is correct. mana_burn_fix=1 rolls the raw fixed-point stats through the
# same statics (same RNG, same shape); 0 = retail byte-for-byte. Difficulty
# scaling (x1.4 NM / x2 Hell) is left alone - D2R's post-2.4 behavior.
gate "manaburn: fork TU gate poll"           Source/D2Game/UNIT/VanillaPlusCombat.cpp 'GetPrivateProfileIntA\("features", "mana_burn_fix", 0, szIniPath\)' 1
gate "manaburn: fork TU entry point"         Source/D2Game/UNIT/VanillaPlusCombat.h   'MANABURN_FixActive' 1
gate "manaburn: SUnitDmg gate include"       Source/D2Game/UNIT/SUnitDmg.cpp          'include "Source/D2Game/UNIT/VanillaPlusCombat.h"' 1
gate "manaburn: SUnitDmg drain shim"         Source/D2Game/UNIT/SUnitDmg.cpp          'DAMAGE_RollDrainStatFixedPoint' 4
gate "manaburn: VP markers on the shim"      Source/D2Game/UNIT/SUnitDmg.cpp          'VP-BEGIN \[manaburn-fix\]|// VP: \[manaburn-fix\]' 5
gate "manaburn: ini template entry"          Source/D2Client/UI/VanillaPlusCore.cpp   'mana_burn_fix=0' 1

# ---- [manaburn-fix / R108] the options-page row (toggle no longer ini-only) ---
# Owner: "I notice the mana_burn_fix toggle is not in the Vanilla+ Options
# panel, it does get generated in a fresh d2vp.ini thou. For R108 let's add it
# to the Options Panel." The row is the R101 noshift-row shape: one
# gaVanillaPlusRows entry riding the existing D2VP_* bridge - and nothing
# else, because the ini template block shipped with R106 and the server gate
# (VanillaPlusCombat.cpp) keeps its 2 s poll by design (the stash-server
# shape): the panel draw re-reads the key fresh every frame and shows the
# flip immediately, the drain roll picks it up on the next <= 2 s poll, the
# same latency as an external ini edit. The eighth row moved the Previous
# Menu row down one pitch (0x144 -> 0x164) and grew the box (0x171 -> 0x191);
# the layout check comment in VanillaPlusOptions.cpp carries the arithmetic
gate "options page: manaburn row present (R108)" Source/D2Client/UI/VanillaPlusOptions.cpp '"mana_burn_fix"' 1

# ---- [shift-compare / R125] hold-Shift equipped-item compare tooltip --------
# FEATURES.md row 0b, the code round of plans/SHIFT-COMPARE-PLAN.md. D2R
# hover-compare parity: with [features] shift_compare=1, holding the key
# bound to retail cmd 0x24 (default VK_SHIFT, remap honored - retail's own
# per-frame UpdateMouseModifierKeyState poll idiom, Game.cpp:1193-1221)
# while an equippable item is hovered in a panel draws a SECOND tooltip -
# the equipped item for the matching body slot - left of the primary with a
# 4 px gap (fallback right). Owner decisions 2026-11-06: side-by-side second
# box, hold trigger, row label "Shift Item Compare", SHOP STOCK INCLUDED
# (the capture fires after the single LoadItemDesc publish, so the
# shop-narrow name+price branch is covered too). The compose is the
# save/swap/capture/restore round trip in VanillaPlusCore.cpp - the swap set
# is exactly { pItemToDescribe, g_nCurrentInventoryPage } (page 0 forces the
# full builder at Inv.cpp:8532 and suppresses the vendor price line at
# :8783-8787); equipped set items ride LoadItemDesc's own quality-5 route
# into ITEM_BuildSetItemTooltip (:8540-8546), so set-bonus lines come
# through. The second channel (gVpCompareTooltip) lives in D2WinFont.cpp
# beside the file-local metric statics and self-clears on draw; the trigger
# adds no CmdTbl entry, no latch, no keyup hook.
gate "shiftcmp: gate fn defined"              Source/D2Client/UI/VanillaPlusCore.cpp   '__stdcall SHIFTCOMPARE_Active' 1
gate "shiftcmp: ini key read by the gate"     Source/D2Client/UI/VanillaPlusCore.cpp   '"shift_compare"' 1
gate "shiftcmp: ini template key"             Source/D2Client/UI/VanillaPlusCore.cpp   'shift_compare=0' 1
gate "shiftcmp: decl + cache busted on write" Source/D2Client/UI/VanillaPlusCore.cpp   'dwShiftCompareLastCheck = 0' 2
gate "shiftcmp: trigger poll (cmd 0x24)"      Source/D2Client/UI/VanillaPlusCore.cpp   'CMD_GetKeyForCommand\(0x24' 2
gate "shiftcmp: slot resolver defined"        Source/D2Client/UI/VanillaPlusCore.cpp   'SHIFTCOMPARE_ResolveSlot' 2
gate "shiftcmp: hook fn defined"              Source/D2Client/UI/VanillaPlusCore.cpp   '__stdcall SHIFTCOMPARE_CompareForHover' 1
gate "shiftcmp: page-0 compose guard"         Source/D2Client/UI/VanillaPlusCore.cpp   'g_nCurrentInventoryPage = 0;' 1
gate "shiftcmp: Inv.cpp call site"            Source/D2Client/UI/Inv.cpp               'SHIFTCOMPARE_CompareForHover' 1
gate "shiftcmp: second channel capture"       Source/D2Win/Src/D2WinFont.cpp           'TEXT_CompareTooltipSetAttributes' 1
gate "shiftcmp: second channel draw"          Source/D2Win/Src/D2WinFont.cpp           'void __stdcall DRAW_CompareTooltipUnderCursor' 1
gate "shiftcmp: channel struct"               Source/D2Win/Src/D2WinFont.cpp           'VPSHIFTCOMPARE_TooltipStrc' 2
gate "shiftcmp: font decls (set + draw)"      Source/D2Win/Src/D2WinFont.h             'TEXT_CompareTooltipSetAttributes|DRAW_CompareTooltipUnderCursor' 2
gate "shiftcmp: draw call in the pipeline"    Source/D2Client/UI/Draw.cpp              'DRAW_CompareTooltipUnderCursor' 1
gate "shiftcmp: paperdoll exclusion"          Source/D2Client/UI/VanillaPlusCore.cpp   'gbInvBodySlotUnderCursorExists' 2
gate "shiftcmp: options row present (R125)"   Source/D2Client/UI/VanillaPlusOptions.cpp '"shift_compare"' 1
gate "shiftcmp: row count pin (R125)"         Source/D2Client/UI/VanillaPlusOptions.cpp 'VPP_ROW_COUNT = 9' 1
gate "shiftcmp: back-row Y re-pin (R125)"     Source/D2Client/UI/VanillaPlusOptions.cpp 'VPPAGE_BACK_ROW_Y = 0x184' 1
gate "shiftcmp: box height re-pin (R125)"     Source/D2Client/UI/VanillaPlusOptions.cpp 'VPPAGE_BOX_HEIGHT = 0x1b1' 1
gate "shiftcmp: shield-kind slot pick (R126)"  Source/D2Client/UI/VanillaPlusCore.cpp   'ITEMTYPE_AnyShield' 1

# ---- docs budgets (owner, 2026-11-06: "The handoff.md should maintain a limit of 16kb") ----
# Two doc lanes: human (README/CHANGELOG/SMOKE - concise) and agent (handoff/HISTORY/plans -
# verbose OK). handoff.md is the working-state doc and grows back fast, so its 16 KB cap is
# gate-enforced (the standing trim-#16/#17 budget); when this fails, offload the
# oldest-again material verbatim into the next HISTORY.md H-record (handoff §8) and re-run.
# The other sizes are advisory only (printed in the summary) - HISTORY.md is unbounded by design.
HANDOFF_BYTES=$(wc -c < handoff.md)
if [ "$HANDOFF_BYTES" -le 16384 ]; then
    printf 'ok   %-46s handoff.md %s/16384 B\n' "docs: handoff.md 16 KB budget" "$HANDOFF_BYTES"
    PASS=$((PASS + 1))
else
    printf 'FAIL %-46s handoff.md %s B > 16384 - offload to HISTORY.md (trim pattern, handoff §8)\n' "docs: handoff.md 16 KB budget" "$HANDOFF_BYTES"
    FAIL=$((FAIL + 1))
fi

echo
echo "doc sizes (advisory): README $(wc -c < README.md) B, CHANGELOG $(wc -c < CHANGELOG.md) B, SMOKE $(wc -c < SMOKE-CHECKLIST.md) B, handoff ${HANDOFF_BYTES} B, HISTORY $(wc -c < HISTORY.md) B"

echo
if [ "$FAIL" -eq 0 ]; then
    echo "vanilla-plus gates: $PASS passed"
    exit 0
fi
echo "vanilla-plus gates: $FAIL FAILED, $PASS passed - a merge likely dropped an edit; see docs/merge-upstream.md"
exit 1
