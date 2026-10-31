#!/usr/bin/env python3
"""rev47 VP-marker placement audit — 44e3518 (auto-gold field-verified tree) vs HEAD.

For every VP marker (VP-BEGIN blocks keyed by their [tag], and single-line
"// VP:" comments) in an upstream file at HEAD, find the same marker in the
same file at the pre-merge-port fork tree (44e3518) and compare the immediate
brace context: 2 lines before the marker and 2 lines after the VP-END (or the
line for single-line markers). CR-normalized (44e3518 is CRLF, main is LF).

Purpose: catch d6affdd-class RE-PARENTING casualties — a marked edit present
at the wrong brace depth (the auto-gold trigger sat inside the HelpMenu block;
a presence-only marker audit passes that).

Output classification per marker:
  OK        context identical (CR-normalized)
  MOVED     marker found at both refs, context differs  -> manual review
  NEW       no marker at 44e3518 (post-44e3518 work: R44/R45/R46 fixes)
  GONE      marker at 44e3518 but not at HEAD           -> casualty candidate
"""
import re
import subprocess
import sys

REF = "44e3518"
CTX = 2  # context lines each side


def git_show(ref: str, path: str):
    r = subprocess.run(["git", "show", f"{ref}:{path}"], capture_output=True)
    if r.returncode != 0:
        return None
    return r.stdout.decode("utf-8", errors="replace").replace("\r\n", "\n").split("\n")


def markers_in(lines):
    """Yield (key, kind, start_idx, end_idx) for each marker."""
    out = []
    i = 0
    n = len(lines)
    while i < n:
        m = re.search(r"VP-BEGIN \[([^\]]+)\]", lines[i])
        if m:
            tag = m.group(1)
            j = i
            end = None
            while j < n:
                if f"// VP-END [{tag}]" in lines[j]:
                    end = j
                    break
                j += 1
            out.append((f"BLOCK[{tag}]", i, end if end is not None else i))
            i = (end + 1) if end is not None else i + 1
            continue
        m = re.search(r"// VP: (.+)$", lines[i])
        if m and "VP-BEGIN" not in lines[i] and "VP-END" not in lines[i]:
            out.append((f"LINE:{m.group(1).strip()}", i, i))
        i += 1
    return out


def context(lines, start, end):
    before = [l.strip() for l in lines[max(0, start - CTX):start]]
    after = [l.strip() for l in lines[end + 1:end + 1 + CTX]]
    return before, after


def main():
    r = subprocess.run(
        ["git", "grep", "-l", "-E", "VP-BEGIN|// VP:", "HEAD", "--", "*.cpp", "*.h"],
        capture_output=True, text=True)
    files = sorted(
        line.split(":", 1)[1]
        for line in r.stdout.splitlines()
        if "/VanillaPlus" not in line  # fork-owned TUs: no placement risk
    )
    moved = gone = ok = new = 0
    for path in files:
        head_lines = git_show("HEAD", path)
        ref_lines = git_show(REF, path)
        if head_lines is None:
            continue
        head_m = markers_in(head_lines)
        ref_m = dict()
        if ref_lines is not None:
            for key, s, e in markers_in(ref_lines):
                ref_m.setdefault(key, (s, e))
        for key, s, e in head_m:
            if key not in ref_m:
                if ref_lines is None:
                    print(f"NEW-FILE  {path}  {key}  (file absent at {REF})")
                else:
                    print(f"NEW       {path}  {key}")
                new += 1
                continue
            rs, re_ = ref_m[key]
            hb, ha = context(head_lines, s, e)
            rb, ra = context(ref_lines, rs, re_)
            if hb == rb and ha == ra:
                ok += 1
            else:
                moved += 1
                print(f"MOVED     {path}  {key}")
                print(f"    HEAD @{s+1} before={hb}")
                print(f"           after ={ha}")
                print(f"    {REF} @{rs+1} before={rb}")
                print(f"           after ={ra}")
        for key in ref_m:
            if not any(k == key for k, _, _ in head_m):
                gone += 1
                print(f"GONE      {path}  {key}  (present at {REF}, absent at HEAD)")
    print(f"\nsummary: OK={ok} MOVED={moved} NEW={new} GONE={gone}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
