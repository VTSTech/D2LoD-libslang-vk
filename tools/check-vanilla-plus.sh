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

# ---- resolution-mode (1280x720 slider stop) --------------------------------
# The registry value is the slider POSITION (0/1/2); stop 2 is attached at
# runtime by the Resolution row's init callback because no MPQ label asset
# exists for it. EscMenu.cpp mapping: 0 -> mode 0, 1 -> mode 2, 2 -> mode 4.
gate "resolution: draw callback wired (row)" globals/globals.cpp         'UI_SLIDER_DrawResolutionValue' 1
gate "resolution: draw fn defined"           D2Client/UI/EscMenu.cpp     '__fastcall UI_SLIDER_DrawResolutionValue' 1
gate "resolution: apply maps mode 4"         D2Client/UI/EscMenu.cpp     'RequestedScreenModeMaybe = 4' 1
gate "resolution: decl in EscMenu.h"         D2Client/UI/EscMenu.h       'UI_SLIDER_DrawResolutionValue' 1
gate "resolution: init attach + mode 4"      Storm/_unnamespaced.cpp     'VPRES_BuildBlankPlaceholder|RequestedScreenModeMaybe = 4' 2
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
gate "site-5: spell grid shifts"           D2Client/UI/SpellSel.cpp        'VPRES_GetWideUiOffsetX' 4
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

echo
if [ "$FAIL" -eq 0 ]; then
    echo "vanilla-plus gates: $PASS passed"
    exit 0
fi
echo "vanilla-plus gates: $FAIL FAILED, $PASS passed - a merge likely dropped an edit; see docs/merge-upstream.md"
exit 1
