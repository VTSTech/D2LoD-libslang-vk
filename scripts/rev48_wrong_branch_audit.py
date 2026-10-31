#!/usr/bin/env python3
"""rev48 wrong-branch audit — VP-marked stores whose result is immediately discarded.

Root cause of record (issue #12, track H): the d6affdd merge port resolved the
720p gold-dialog edit (cef3cf8) into the FAILURE arm of the widget-alloc
null-checks. Upstream's regeneration had inverted the branch shape
(if (!pAllocCtx) { gpX = 0; } else { gpX = Constructor(...); }  ->
 if (pAllocCtx)  { gpX = Constructor(...); } else { gpX = 0; }),
which is decompiler-equivalent and reads as churn; the port then matched the
VP-marked rider line onto the constructor call that LOOKED like the original -
now the failure arm, immediately followed by the same-LHS null-out. Marker,
call and even the x5 presence gate stayed green; in normal play the alloc
never fails, the raw 800-space constants ran, and the deposit-gold dialog was
misaligned at 720p. Bonus: the misplaced call passed a null context to the
constructor before the null-out - a latent OOM null-deref.

Signature checked here, tree-wide, on every VP-marked statement line:

    <LHS> = <expr ... // VP: ...>;
    <LHS> = 0;            <- same LHS on the next non-empty line = dead store

A VP-marked value that is instantly overwritten can never be the intended
placement: the edit is dead code wherever it landed. Presence gates cannot
see this class (presence is not placement, and placement is not BRANCH).

Run after any merge, alongside tools/check-vanilla-plus.sh and
scripts/rev47_vp_placement_audit.py (merge-procedure step 4).

Output: one line per hit (BAD); exit 1 on any hit, 0 on a clean tree.
"""
import re
import subprocess
import sys

ASSIGN = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_:>\[\]\.]*)\s*=[^=]")
NULL_OUT = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_:>\[\]\.]*)\s*=\s*(?:0|nullptr)\s*;")
VP_MARKER = re.compile(r"// VP[:\-]")


def main():
    r = subprocess.run(
        ["git", "grep", "-l", "-E", "// VP", "--", "*.cpp", "*.h"],
        capture_output=True, text=True)
    files = sorted(
        line.split(":", 1)[1] if ":" in line else line
        for line in r.stdout.splitlines()
        if "/VanillaPlus" not in line  # fork-owned TUs: no merge resolution risk
    )
    bad = 0
    checked = 0
    for path in files:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.read().split("\n")
        for i, line in enumerate(lines):
            if not VP_MARKER.search(line):
                continue
            m = ASSIGN.match(line)
            if not m or "VP-BEGIN" in line or "VP-END" in line:
                continue
            checked += 1
            lhs = m.group(1)
            # next non-empty line
            j = i + 1
            while j < len(lines) and not lines[j].strip():
                j += 1
            if j >= len(lines):
                continue
            n = NULL_OUT.match(lines[j])
            if n and n.group(1) == lhs:
                bad += 1
                print(f"BAD  {path}:{i + 1}  VP-marked store of '{lhs}' is nulled out "
                      f"on line {j + 1} - dead store (wrong-branch resolution)")
                print(f"     {line.strip()[:120]}")
                print(f"     {lines[j].strip()[:120]}")
    print(f"\nsummary: VP-marked stores checked={checked} dead-store hits={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
