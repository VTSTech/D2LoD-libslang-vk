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
# Companions: tools/check-fork-surface.sh (whole-diff budget) and the
# VP-BEGIN/VP-END markers (// VP: for one-liners) this script partly counts.
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
gate "banner (d2boot.log self-ID)"        D2Client/Engine/Application.cpp 'D2BOOT_Mark\("vanilla-plus' 1
gate "boot: ini template hook"            D2Client/Engine/Application.cpp 'VP-BEGIN \[alt-toggle\]'    1

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
gate "SetUiFlagInit/unregister/render VP blocks" D2Client/UI/Draw.cpp     'VP-BEGIN \[row-10 options page\]' 3
gate "auto-gold tick wired in DRAW_UI"    D2Client/UI/Draw.cpp            'AUTO_GOLD_Tick\(\);'        1
gate "latch reset in InitializeUI"        D2Client/UI/Draw.cpp            'ALTTOGGLE_Reset\(\);'       1
gate "case UI_VANILLAPLUS (register+unregister)" D2Client/UI/Draw.cpp     'case UI_VANILLAPLUS:'       2
gate "VANILLAPLUS_OnToggleUI calls"       D2Client/UI/Draw.cpp            'VANILLAPLUS_OnToggleUI\('   2
gate "SetUIFlag bound widened to 38"      D2Client/UI/Draw.cpp            'UI_VANILLAPLUS < eUI'       1
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

echo
if [ "$FAIL" -eq 0 ]; then
    echo "vanilla-plus gates: $PASS passed"
    exit 0
fi
echo "vanilla-plus gates: $FAIL FAILED, $PASS passed - a merge likely dropped an edit; see docs/merge-upstream.md"
exit 1
