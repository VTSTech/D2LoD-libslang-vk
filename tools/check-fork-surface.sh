#!/usr/bin/env bash
# Fork surface budget: compare the fork's whole diff against upstream with an
# allowlist, so merge-surface drift is a conscious decision instead of an
# accident. Run after any upstream merge (and in CI before the build).
#
#   tools/check-fork-surface.sh [--update] [repo-root] [upstream-ref]
#
#     --update   regenerate tools/fork-surface.allowlist from the current
#                diff (do this in the same commit as an intentional change)
#     repo-root  defaults to the checkout this script lives in
#     upstream-ref  defaults to upstream/main (must be fetched)
#
# Allowlist format: git diff --numstat lines (added<TAB>deleted<TAB>path),
# sorted by path; "-" counts are kept for binary files.
#
# Companions: tools/check-vanilla-plus.sh (per-feature grep gates) and
# docs/merge-upstream.md (the playbook this belongs to).
set -u

UPDATE=0
if [ "${1:-}" = "--update" ]; then UPDATE=1; shift; fi
ROOT=${1:-$(cd "$(dirname "$0")/.." && pwd)}
UP=${2:-upstream/main}

cd "$ROOT" || { echo "no such dir: $ROOT" >&2; exit 2; }
git rev-parse --verify -q "$UP" >/dev/null || {
    echo "upstream ref '$UP' not found - git remote add upstream <url> && git fetch $UP" >&2
    exit 2
}
git rev-parse --verify -q HEAD >/dev/null || { echo "no HEAD" >&2; exit 2; }

NOW=$(mktemp)
trap 'rm -f "$NOW"' EXIT
# The allowlist does not budget itself (self-referential); skip it here and in
# the staleness pass below.
git diff --numstat "$UP"...HEAD | grep -vP '\ttools/fork-surface\.allowlist$' | sort -t$'\t' -k3 > "$NOW"

ALLO="$(cd "$(dirname "$0")/.." && pwd)/tools/fork-surface.allowlist"

if [ "$UPDATE" -eq 1 ]; then
    cp "$NOW" "$ALLO"
    echo "fork-surface.allowlist regenerated from $UP...HEAD ($(wc -l < "$NOW") files)"
    exit 0
fi

if [ ! -f "$ALLO" ]; then
    echo "FAIL: $ALLO missing - run: tools/check-fork-surface.sh --update" >&2
    exit 1
fi

FAIL=0
NEW=0
GROWN=0
SHRUNK=0
STALE=0

# files present now
while IFS=$'\t' read -r na nd path; do
    allo_line=$(grep -F -m1 "$(printf '%s\t%s\t%s' "$na" "$nd" "$path")" "$ALLO" || true)
    if [ -n "$allo_line" ]; then
        :
    else
        # same path with different counts, or a brand-new file in the diff?
        allo_path=$(grep -P -m1 "\t${path//\//\\/}\$" "$ALLO" || true)
        if [ -n "$allo_path" ]; then
            echo "FAIL grown/shrunk: $path"
            echo "     now: +$na -$nd"
            echo "     budget: $(echo "$allo_path" | sed 's/\t/ +/; s/\t/ -/')"
            GROWN=$((GROWN + 1)); FAIL=1
        else
            echo "FAIL new fork surface: $path (+$na -$nd)"
            echo "     if intentional, update the budget: tools/check-fork-surface.sh --update"
            NEW=$((NEW + 1)); FAIL=1
        fi
    fi
done < "$NOW"

# allowlist entries that no longer appear (surface shrunk - fine, but stale)
while IFS=$'\t' read -r na nd path; do
    [ "$path" = "tools/fork-surface.allowlist" ] && continue
    if ! grep -qP "\t${path//\//\\/}\$" "$NOW"; then
        echo "note stale budget entry (diff shrunk, update with --update): $path"
        STALE=$((STALE + 1))
    fi
done < "$ALLO"

echo
if [ "$FAIL" -eq 0 ]; then
    echo "fork surface vs $UP: within budget ($(wc -l < "$NOW") files; $NEW new, $GROWN changed, $STALE stale)"
    [ "$STALE" -gt 0 ] && exit 0
    exit 0
fi
echo "fork surface vs $UP: $NEW new / $GROWN changed outside budget - see docs/merge-upstream.md"
exit 1
