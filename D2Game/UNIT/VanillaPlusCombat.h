// vanilla-plus combat fixes: the server-side drain-roll gate (mana burn).
//
// Fork-owned TU (picked up by CMake's GLOB_RECURSE) - do not move this code
// into upstream files; upstream files only ever call the entry points
// declared here from thin VP-marked sites (see docs/merge-upstream.md).
//
// [vanilla-plus mana-burn-fix] Retail 1.14d bug: monsters with the Mana Burn
// affix drain 256x the intended mana with melee hits (512x in Hell). The
// affix stores UNITSTAT_manadrainmindam/manadrainmaxdam already in the
// mana 1/256 fixed point (D2Game/Monsters/Umod.cpp ManaBurn scales the
// MonUMod.txt 66/100 percentages by 0x100), but the melee drain roll routes
// them through DAMAGE_RollElementalDamageWithBonus (D2Game/UNIT/SUnitDmg.cpp),
// whose <<8/*0x100 inputs are right for the plain-point fire/cold/lightning
// stats it also serves and wrong for the drain family - a second x256 on
// already fixed-point values. The missile path (D2Game/MISSILES/
// MissilesMode.cpp) reads the same stats raw and is correct; the Amazon
// Basin wiki documents the bug ("it is multiplied by 256 when applied by a
// melee attack. Fixed in D2R") and the D2R 2.4 patch fixed it.
//
// Scope guard: [features] mana_burn_fix=1 in d2vp.ini next to d2.exe makes
// the melee drain roll consume the stats raw (the same values the umod
// stored), so a Mana Burn hit drains the designed amount - 66-100% of the
// monster's MonLvl DM for a unique, 33-50% for a Nightmare/Hell minion. The
// difficulty resist penalty the drain fields also receive (-40 Nightmare /
// -100 Hell, applied to a zero resist base by DAMAGE_CalculateResistance)
// is left alone on purpose: with the gate on, the drain still scales
// x1.4 / x2 by difficulty, matching D2R's post-2.4 behavior. Default/
// missing file/section/key/garbage value = 0 = retail byte-for-byte
// (charter rule 1). Same 2 s GetTickCount hot-read cadence as every gate;
// the SP server runs in the same process, so the exe-anchored ini is the
// same file the client gates read.

#pragma once

// TRUE when [features] mana_burn_fix is on: the drain-family roll in
// SUnitDmg.cpp must consume the lifedrain/manadrain/stamdrain stats RAW
// (they are already mana fixed-point) instead of through the generic
// elemental roll that shifts them another 8 bits. Beyond the cached
// 2 s poll window this never touches the ini.
BOOL __stdcall MANABURN_FixActive();
