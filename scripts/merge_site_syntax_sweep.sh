#!/usr/bin/env bash
# vanilla-plus: post-merge syntax sweep of every merge-site TU (v2, Rev 93).
#
#   scripts/merge_site_syntax_sweep.sh <merge-commit>
#
# Why: Rev 92 (R73, after the 1792 merge) - the merge resolution kept BOTH
# sides' local declarations in Inv.cpp ("redeclaration of D2UnitStrc* pNpc")
# and dropped a declaration SpellSel.cpp's fork-side body still used
# ("pPlayerUnit_lcl was not declared"). Rev 93: the same merge ALSO
# double-spliced two regions of Direct3D.cpp (RenderWallTileWithPerspective,
# Direct3DCallBack) - "a function-definition is not allowed here" x20 - and
# tools/syntax-check.sh's 30-line per-TU output cap hid the parse errors
# behind 64-bit-host noise.
#
# What it checks: every tracked file that DIFFERS FROM BOTH merge parents
# (the true merge sites) and ends in .cpp.
#
# How it classifies (v2 - baseline diffing, replaces the Rev 92 SDK allowlist):
# each TU is compiled three times with the host shims - the merge result AND
# both parents' bodies of the same TU against the same tree/headers - and an
# error is REAL only if its normalized message appears in the merge result
# but in NEITHER parent. Pre-existing host noise (undeclared Win32/CRT
# surface, shim overlaps, 64-bit precision casts) cancels out automatically:
# it exists in the parents too. Name-chasing an allowlist cannot enumerate
# the SDK; the parents' own compile results are the exact baseline. Parse
# classes (redeclaration, function-definition-not-allowed, expected '}')
# additionally ALWAYS surface, even if a parent somehow produced the same
# text, so the class Rev 92/93 shipped with can never hide again.
#
# Residual risk, accepted: a HEAD-only error whose normalized text collides
# with a parent error in the same TU (same message, different site) cancels;
# the sweep is a human-audit instrument (merge-procedure step 4e), the
# owner's Win11 build remains the final gate.
#
# Selection blind spot (accepted, covered elsewhere): a TU byte-identical to
# ONE parent is never compiled - if the merge took the OTHER parent's
# move/deletion of a header that TU includes, the dangle is invisible here
# (the 1894->1908 sync shipped exactly this in D2Client/GAME/Draw.cpp, and a
# sweep run on the broken tree stays 0-error). That class belongs to
# scripts/include_path_audit.py (whole-tree quoted-include resolution, no
# compiler); run both after every merge.
#
# Exit: 0 = clean, 1 = real failures, 2 = usage.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 2

BASE=${1:-}
if [ -z "$BASE" ]; then
    echo "usage: merge_site_syntax_sweep.sh <merge-commit>"
    echo "  (e.g. scripts/merge_site_syntax_sweep.sh \$(git log --merges -1 --format=%h))"
    exit 2
fi
P1=$(git rev-parse "$BASE^1" 2>/dev/null) || { echo "no such commit: $BASE" >&2; exit 2; }
P2=$(git rev-parse "$BASE^2" 2>/dev/null) || { echo "not a merge commit: $BASE" >&2; exit 2; }

mapfile -t TUS < <(comm -12 \
    <(git diff --name-only "$P1" "$BASE" | sort) \
    <(git diff --name-only "$P2" "$BASE" | sort) | grep -E '\.cpp$')

echo "merge-site TUs (differ from both parents of $(git rev-parse --short "$BASE")): ${#TUS[@]}"
[ "${#TUS[@]}" -eq 0 ] && { echo "nothing to sweep"; exit 0; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Compile one TU version via tools/syntax-check.sh (the shims + shadow
# d2_platform.h live there; do not duplicate the flag chain here).
# Emits normalized error messages (message text only, no path/line) to $2.
compile_norm() {
    local tu="$1" out="$2"
    VP_SYNTAX_FULL=1 bash tools/syntax-check.sh . "$tu" > "$out.raw" 2>&1
    # normalized keys: g++ message text, smart quotes -> ASCII, no positions
    grep 'error:' "$out.raw" \
      | sed -e "s/^[^:]*:[0-9]*:[0-9]*: error: //" \
            -e "s/[^ ]*\.$//" \
      | sed -e "s/\xe2\x80\x98/'/g" -e "s/\xe2\x80\x99/'/g" \
      | sort -u > "$out"
}

# Danger classes that surface REGARDLESS of the baseline (regex on the
# normalized message): a merge-splice that double-declares, double-splices
# or drops a brace must never cancel, even in theory.
DANGER_RE='redeclaration of|redefinition of|redefinition of|conflicting declaration|function-definition is not allowed|expected .\}.|expected unqualified-id|expected constructor|does not name a type|no member named|no matching function|cannot convert'

REAL_TOTAL=0
for tu in "${TUS[@]}"; do
    compile_norm "$tu" "$TMP/head.n"
    # parent bodies, compiled against the merge tree's headers
    if git cat-file -e "$P1:$tu" 2>/dev/null; then
        git show "$P1:$tu" > "$TMP/p1.cpp"
        compile_norm "$TMP/p1.cpp" "$TMP/p1.n"
    else
        : > "$TMP/p1.n"
    fi
    if git cat-file -e "$P2:$tu" 2>/dev/null; then
        git show "$P2:$tu" > "$TMP/p2.cpp"
        compile_norm "$TMP/p2.cpp" "$TMP/p2.n"
    else
        : > "$TMP/p2.n"
    fi

    # REAL = in head, not in either parent, matching a danger class
    # OR simply unique-to-head (both reported; danger ones flagged).
    comm -23 "$TMP/head.n" <(sort -u "$TMP/p1.n" "$TMP/p2.n") > "$TMP/unique.n" || true
    # drop 64-bit-host precision casts (never real on i686)
    grep -v "loses precision" "$TMP/unique.n" > "$TMP/unique2.n" || true
    mv "$TMP/unique2.n" "$TMP/unique.n"

    n=$(wc -l < "$TMP/unique.n")
    if [ "$n" -gt 0 ]; then
        echo "REAL  $tu ($n unique error(s)):"
        grep -E "$DANGER_RE" "$TMP/unique.n" | sed 's/^/      !! /'
        grep -Ev "$DANGER_RE" "$TMP/unique.n" | head -5 | sed 's/^/      ?  /'
        REAL_TOTAL=$((REAL_TOTAL + n))
    fi
done

echo "sweep: ${#TUS[@]} merge-site TUs, $REAL_TOTAL unique-to-merge error(s) after baseline diff"
[ "$REAL_TOTAL" -eq 0 ]
