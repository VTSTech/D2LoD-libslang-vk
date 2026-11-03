#!/usr/bin/env bash
# vanilla-plus: post-restructure syntax sweep of the ENTIRE fork surface (R104).
#
#   scripts/fork_surface_syntax_sweep.sh [upstream-ref]
#
# Why: the fifth upstream sync (962f88d5, "define shared globals in their TU")
# re-homed globals into per-TU definitions + *.data.h decl headers. Text gates
# stayed green but SEMANTICS broke in fork-added lines (R103's first owner
# build: LightDiag.cpp named the now-static gnLightmapWorldMaxX/Y; then
# SCmd.cpp's roster mark named Roster::gpRosterOther / pRosterPartyList whose
# decls moved to Roster.data.h). tools/syntax-check.sh's default set only
# covers the four fork-owned TUs - it never saw the fork-edited upstream TUs.
#
# What it checks: every tracked .cpp that DIFFERS from upstream/main (the
# whole fork surface), fork body vs the same TU's upstream/main body.
#
# How it classifies: the merge_site_syntax_sweep.sh v2 baseline-diffing
# method, generalized - each TU body is compiled twice with the host shims
# against the SAME tree headers (fork working tree, upstream blob to a temp
# file), and an error is REAL only if its normalized message appears in the
# fork body but not in the upstream body. Pre-existing host noise (64-bit
# precision casts, undeclared Win32/CRT surface both bodies share) cancels
# automatically. "loses precision" is additionally dropped (never real on
# i686). Fork-owned TUs (no upstream counterpart) have an empty baseline:
# every error counts.
#
# Residual risk, accepted (as in the merge sweep): a fork-only error whose
# normalized text collides with an upstream-body error in the same TU
# cancels; this is a human-audit instrument, the owner's Win11 build stays
# the final gate.
#
# Exit: 0 = clean, 1 = real failures.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 2
UP=${1:-upstream/main}

mapfile -t TUS < <(git diff --name-only "$UP" HEAD -- '*.cpp' | sort)
echo "fork-surface TUs (differ from $UP): ${#TUS[@]}"
[ "${#TUS[@]}" -eq 0 ] && { echo "nothing to sweep"; exit 0; }

JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
[ "$JOBS" -gt 8 ] && JOBS=8
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

per_tu() {
    local tu="$1" UP="$2" ROOT="$3" TMP="$4"
    local safe; safe=$(printf '%s' "$tu" | tr '/' '_')
    cd "$ROOT" || return 0

    # fork body (in-tree path: quoted includes resolve relative to its dir)
    VP_SYNTAX_FULL=1 bash tools/syntax-check.sh . "$tu" > "$TMP/$safe.fork.raw" 2>&1
    grep 'error:' "$TMP/$safe.fork.raw" \
      | sed -e "s/^[^:]*:[0-9]*:[0-9]*: error: //" \
            -e "s/[^ ]*\.$//" \
      | sed -e "s/\xe2\x80\x98/'/g" -e "s/\xe2\x80\x99/'/g" \
      | sort -u > "$TMP/$safe.fork.n"

    # upstream body (temp file; -I dirs cover every header location)
    if git cat-file -e "$UP:$tu" 2>/dev/null; then
        git show "$UP:$tu" > "$TMP/$safe.up.cpp"
        VP_SYNTAX_FULL=1 bash tools/syntax-check.sh . "$TMP/$safe.up.cpp" > "$TMP/$safe.up.raw" 2>&1
        grep 'error:' "$TMP/$safe.up.raw" \
          | sed -e "s/^[^:]*:[0-9]*:[0-9]*: error: //" \
                -e "s/[^ ]*\.$//" \
          | sed -e "s/\xe2\x80\x98/'/g" -e "s/\xe2\x80\x99/'/g" \
          | sort -u > "$TMP/$safe.up.n"
    else
        : > "$TMP/$safe.up.n"   # fork-owned TU: empty baseline
    fi

    comm -23 "$TMP/$safe.fork.n" "$TMP/$safe.up.n" > "$TMP/$safe.real.n" || true
    grep -v "loses precision" "$TMP/$safe.real.n" > "$TMP/$safe.real2.n" || true
    mv "$TMP/$safe.real2.n" "$TMP/$safe.real.n"

    local n; n=$(wc -l < "$TMP/$safe.real.n")
    if [ "$n" -gt 0 ]; then
        printf 'REAL\t%s\t%s\n' "$tu" "$n"
        sed 's/^/    !! /' "$TMP/$safe.real.n"
    fi
}
export -f per_tu

printf '%s\n' "${TUS[@]}" \
  | xargs -P "$JOBS" -n 1 -I{} bash -c 'per_tu "$1" "$2" "$3" "$4"' _ {} "$UP" "$ROOT" "$TMP" \
  > "$TMP/report.txt"

REAL_TOTAL=$(grep -c '^REAL' "$TMP/report.txt" || true)
cat "$TMP/report.txt"
echo "sweep: ${#TUS[@]} fork-surface TUs, $REAL_TOTAL TU(s) with unique-to-fork error(s) after baseline diff"
[ "$REAL_TOTAL" -eq 0 ]
