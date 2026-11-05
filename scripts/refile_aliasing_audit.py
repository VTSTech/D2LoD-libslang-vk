#!/usr/bin/env python3
"""
refile_aliasing_audit.py -- sweep for the "refile-aliasing" defect class.

Background (R124, eighth-sync D3D fullscreen regression): upstream's
file-symbols refile rounds turn retail globals into function-local statics
("Static local variables (originally global data)"). A refiled static only
aliases its real storage when the code runs at RETAIL data addresses. In a
rebuilt exe the static lives wherever the linker puts it, so:

  defect A: OOB indexing  (&SCALAR)[i * N]  -- reads/writes past the static
            into unrelated .bss (the INT_00970ec4 case).
  defect B: mixed spelling -- the same TU reaches the same retail storage via
            the refiled static AND via old global-relative arithmetic
            (&SomeGlobal - k). Two different regions in a rebuilt exe.
  defect C: "also labelled: X" -- the static IS retail global X; if X is
            still referenced by name anywhere else in the repo, the two names
            must not point at different storages.

This audit flags all three shapes. Exit 1 if any hard finding.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPP_EXTS = (".cpp", ".h", ".data.h")

RE_BLOCK = re.compile(r"//\s*Static local variables \(originally global data\)")
RE_STATIC = re.compile(
    r"^\s*static\s+(?:const\s+)?[\w:]+(?:\s*\*)*\s+(\w+)\s*(\[[^\]]*\])?\s*;"
)
RE_LABELLED = re.compile(r"//\s*also labelled:\s*([\w, ]+)")
RE_INDEX_USE = re.compile(r"\(&?(\w+)\)\s*\[")
RE_NAME_INDEX = re.compile(r"\b(\w+)\s*\[")
RE_ADDR_IN_NAME = re.compile(r"(?:DAT|INT|BOOL|FLOAT|CHAR|STR|PTR)_([0-9a-fA-F]{8})\b")

findings = []


def audit_file(path):
    rel = os.path.relpath(path, ROOT)
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except OSError:
        return

    text = "".join(lines)

    # Collect refile blocks: for each block marker, the statics declared in
    # the following lines (up to 40 lines or a blank-line gap of 3).
    refile_statics = {}  # name -> dict(file, line, is_scalar, labelled)
    labelled_names = set()
    for i, ln in enumerate(lines):
        if RE_BLOCK.search(ln):
            j = i + 1
            gap = 0
            while j < len(lines) and gap < 3 and j - i < 40:
                m = RE_STATIC.match(lines[j])
                if m:
                    name, dims = m.group(1), m.group(2)
                    is_scalar = dims is None
                    refile_statics[name] = {
                        "line": j + 1,
                        "scalar": is_scalar,
                        "dims": dims or "",
                    }
                    gap = 0
                elif lines[j].strip() == "":
                    gap += 1
                j += 1
        m = RE_LABELLED.search(ln)
        if m:
            for nm in m.group(1).split(","):
                nm = nm.strip()
                if nm:
                    labelled_names.add(nm)
                    # register labelled name as refiled too (aliases the static)
                    if nm not in refile_statics:
                        refile_statics[nm] = {
                            "line": i + 1,
                            "scalar": None,  # unknown shape
                            "dims": "(labelled alias)",
                        }

    if not refile_statics:
        return

    # Defect A: OOB indexing (&NAME)[expr] where NAME is a refiled scalar
    for i, ln in enumerate(lines):
        for m in RE_INDEX_USE.finditer(ln):
            name = m.group(1)
            if name in refile_statics and refile_statics[name]["scalar"]:
                findings.append(
                    f"A|{rel}:{i+1}| scalar refiled static '{name}' indexed as array: {ln.strip()[:110]}"
                )

    # Defect B: mixed spelling -- for statics whose name encodes a retail
    # address, look for that same address-derived storage reached via old
    # global-relative arithmetic in the same TU. Heuristic: any OTHER refiled
    # static of the SAME function that is scalar and used with raw +- offsets
    # is covered by A; the B shape we detect is: a refiled static name X and a
    # DIFFERENT identifier Y with a _ADDRESS-style name where Y's address is
    # within 0x100 of X's and Y is used in address arithmetic. Simplified:
    # flag any reference to a retail-address literal 0x00NNNNNN within 0x1000
    # of a refiled static's encoded address, used in address arithmetic.
    addrs = {}
    for name in refile_statics:
        m = re.match(r"(?:DAT|INT|BOOL|FLOAT|CHAR|STR|PTR)_([0-9a-fA-F]{8})", name)
        if m:
            addrs.setdefault(name, int(m.group(1), 16))

    for i, ln in enumerate(lines):
        for m in re.finditer(r"0x0*([0-9a-fA-F]{7,8})\b", ln):
            try:
                v = int(m.group(1), 16)
            except ValueError:
                continue
            for name, av in addrs.items():
                if abs(v - av) <= 0x100 and re.search(
                    r"&\s*\w+\s*[-+]\s*0x" + m.group(1).lstrip("0"), ln
                ):
                    findings.append(
                        f"B?|{rel}:{i+1}| retail-address literal 0x{v:x} near static '{name}' (0x{av:x}) in address arithmetic: {ln.strip()[:110]}"
                    )

    # Defect C: "also labelled" names referenced elsewhere in the repo
    for nm in labelled_names:
        if nm in refile_statics and refile_statics[nm]["scalar"] is None:
            continue  # alias of a real array, checked via the static itself
        # skip the declaring file itself (already using the static)
        hits = []
        for dirpath, _dirs, files in os.walk(ROOT):
            if ".git" in dirpath:
                continue
            for fn in files:
                if not fn.endswith(CPP_EXTS):
                    continue
                p = os.path.join(dirpath, fn)
                if os.path.samefile(p, path) if os.path.exists(p) else False:
                    continue
                try:
                    with open(p, "r", encoding="utf-8", errors="replace") as f:
                        t = f.read()
                except OSError:
                    continue
                if re.search(r"\b" + re.escape(nm) + r"\b", t):
                    hits.append(os.path.relpath(p, ROOT))
        if hits:
            findings.append(
                f"C?|{rel}| labelled retail name '{nm}' also referenced in: {', '.join(hits[:6])}"
            )


def main():
    count = 0
    for dirpath, dirs, files in os.walk(ROOT):
        if ".git" in dirpath:
            dirs[:] = []
            continue
        for fn in files:
            if fn.endswith(".cpp") or fn.endswith(".h"):
                audit_file(os.path.join(dirpath, fn))
                count += 1
    print(f"audited {count} files")
    if not findings:
        print("REFILE-ALIASING AUDIT: clean")
        return 0
    hard = 0
    for f in sorted(set(findings)):
        kind = f.split("|")[0]
        if kind in ("A",):
            hard += 1
        print(f)
    print(f"REFILE-ALIASING AUDIT: {len(set(findings))} finding(s), {hard} hard")
    return 1 if hard else 0


if __name__ == "__main__":
    sys.exit(main())
