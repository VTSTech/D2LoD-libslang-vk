#!/bin/sh
# R49 track C provenance: function-level diff of the component-appearance chain
# between the last field-verified pre-port tree (44e3518) and main.
# Usage: sh scripts/rev49_trackc_provenance.sh <repo-root>

REPO="${1:-.}"
cd "$REPO" || exit 1

TMP=$(mktemp -d)

extract() { # tree file start-pattern end-pattern outfile
  git show "$1:$2" 2>/dev/null | awk "/$3/{f=1} f{print} f&&/$4/{exit}" > "$5"
}

echo "=== 1. MONSTER_RollComponentAppearance (D2Game/Monster/Monster.cpp) ==="
extract main  D2Game/Monster/Monster.cpp 'MONSTER_RollComponentAppearance\(int32_t nParam' '^}' "$TMP/a1"
extract 44e3518 D2Game/Monster/Monster.cpp 'MONSTER_RollComponentAppearance\(int32_t nParam' '^}' "$TMP/b1"
if diff -u "$TMP/b1" "$TMP/a1" > "$TMP/d1"; then echo "IDENTICAL pre-port -> main"; else echo "DIFFERS:"; head -60 "$TMP/d1"; fi

echo ""
echo "=== 2. MONSTER_InitStats head incl. roll call (D2Game/Monster/Monster.cpp) ==="
extract main  D2Game/Monster/Monster.cpp 'static void __stdcall MONSTER_InitStats' 'MONSTER_RollComponentAppearance\(nParam, pUnit\);' "$TMP/a2"
extract 44e3518 D2Game/Monster/Monster.cpp 'static void __stdcall MONSTER_InitStats' 'MONSTER_RollComponentAppearance\(nParam, pUnit\);' "$TMP/b2"
if diff -u "$TMP/b2" "$TMP/a2" > "$TMP/d2"; then echo "IDENTICAL pre-port -> main"; else echo "DIFFERS:"; head -80 "$TMP/d2"; fi

echo ""
echo "=== 3. STRUCT_FillDataMonster head incl. InitStats call (D2Game/Monster/Monster.cpp) ==="
extract main  D2Game/Monster/Monster.cpp 'void __fastcall STRUCT_FillDataMonster' 'MONSTER_InitStats\(pGame, pRoom' "$TMP/a3"
extract 44e3518 D2Game/Monster/Monster.cpp 'void __fastcall STRUCT_FillDataMonster' 'MONSTER_InitStats\(pGame, pRoom' "$TMP/b3"
if diff -u "$TMP/b3" "$TMP/a3" > "$TMP/d3"; then echo "IDENTICAL pre-port -> main"; else echo "DIFFERS:"; head -80 "$TMP/d3"; fi

echo ""
echo "=== 4. InitializeSpawnCount (D2Game/Monster/MonsterRegion.cpp) ==="
for tree in main 44e3518; do
  if git cat-file -e "$tree:D2Game/Monster/MonsterRegion.cpp" 2>/dev/null; then
    extract "$tree" D2Game/Monster/MonsterRegion.cpp 'InitializeSpawnCount' '^}' "$TMP/$tree-init"
  else
    echo "  (no MonsterRegion.cpp at $tree)"
  fi
done
if [ -f "$TMP/main-init" ] && [ -f "$TMP/44e3518-init" ]; then
  if diff -u "$TMP/44e3518-init" "$TMP/main-init" > "$TMP/d4"; then echo "IDENTICAL pre-port -> main"; else echo "DIFFERS:"; head -80 "$TMP/d4"; fi
fi

echo ""
echo "=== 5. 0xAC component write block (D2Game/GAME/SCmd.cpp) ==="
extract main  D2Game/GAME/SCmd.cpp 'BVar1 = Monster::CheckComponents\(pUnit\)' 'nComponentIdx = Monsters::CheckMonUModList\(pUnit\);' "$TMP/a5"
extract 44e3518 D2Game/GAME/SCmd.cpp 'CheckComponents' 'CheckMonUModList' "$TMP/b5"
if [ -s "$TMP/b5" ]; then
  if diff -u "$TMP/b5" "$TMP/a5" > "$TMP/d5"; then echo "IDENTICAL pre-port -> main"; else echo "DIFFERS:"; head -80 "$TMP/d5"; fi
else
  echo "  (component block pattern not found at 44e3518 — checking file presence)"
  git cat-file -e "44e3518:D2Game/GAME/SCmd.cpp" 2>/dev/null && echo "  SCmd.cpp exists at 44e3518" || echo "  no D2Game/GAME/SCmd.cpp at 44e3518"
fi

echo ""
echo "=== 6. client 0xAC parser component loop (D2Client/GAME/SCmd.cpp) ==="
extract main  D2Client/GAME/SCmd.cpp 'NET_D2GS_CLIENT_Incoming0xAC' 'sCreateMonster.eMonsterType' "$TMP/a6"
extract 44e3518 D2Client/GAME/SCmd.cpp 'NET_D2GS_CLIENT_Incoming0xAC' 'sCreateMonster.eMonsterType' "$TMP/b6"
if [ -s "$TMP/b6" ]; then
  if diff -u "$TMP/b6" "$TMP/a6" > "$TMP/d6"; then echo "IDENTICAL pre-port -> main"; else echo "DIFFERS:"; head -80 "$TMP/d6"; fi
else
  echo "  (client 0xAC handler not found at 44e3518)"
fi

echo ""
echo "=== 7. UNIT_CreateMonster component copy (D2Client/UNIT/CUnit.cpp) ==="
extract main  D2Client/UNIT/CUnit.cpp 'if \(pItemSeed && pUnit->eUnitType == UNIT_MONSTER\)' 'Monster::MONSTER_InitMonsterUnit\(pUnit' "$TMP/a7"
extract 44e3518 D2Client/UNIT/CUnit.cpp 'pItemSeed && pUnit->eUnitType == UNIT_MONSTER' 'MONSTER_InitMonsterUnit' "$TMP/b7"
if [ -s "$TMP/b7" ]; then
  if diff -u "$TMP/b7" "$TMP/a7" > "$TMP/d7"; then echo "IDENTICAL pre-port -> main"; else echo "DIFFERS:"; head -80 "$TMP/d7"; fi
else
  echo "  (pattern not found at 44e3518)"
fi

echo ""
echo "--- artifact dir: $TMP ---"
