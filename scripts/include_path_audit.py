#!/usr/bin/env python3
"""vanilla-plus: include-path audit (added after the 1894->1908 sync).

    python3 scripts/include_path_audit.py [repo-root]

Why: merge 427551b7 (upstream 1894->1908, the seventh upstream sync) took
upstream's move of D2GfxHelperStrc.* from D2Client/ to D2gfx/ (old copies
deleted), while D2Client/GAME/Draw.cpp resolved byte-identical to parent 1
(it carries the [resolution site-5] VP block, so the fork side won the file
whole) and kept `#include "D2Client/D2GfxHelperStrc.h"`. The owner's Win11
build died at [49/485]: "D2Client/D2GfxHelperStrc.h: No such file or
directory".

Why every existing instrument is blind to this class:

- presence / placement / branch audits (R47/R48) are text-based: the include
  line is present, well-placed, in the right branch - the FILE it names is
  what is gone.
- d2boot_visibility_audit proves D2BOOT_* users reach a declaration; a plain
  struct include has no marker to trace.
- the merge-site syntax sweep (step 4e) compiles only TUs that differ from
  BOTH parents, on the theory that a file identical to one parent is as good
  as that parent. Draw.cpp is identical to parent 1 - but the merge changed
  the HEADER SET around it (took parent 2's deletion), so a parent-1-identical
  body can dangle against the merge tree. Negative-tested: the sweep runs
  green on the broken tree. The class is invisible to baseline diffing by
  construction, because the file is never compiled.

What it checks: every quoted #include in every tracked .cpp/.h outside
asm_matching/ and thirdparty/libbacktrace/ (own include sets, filtered from
the d2 target by vanilla-plus/cmake/sources.cmake) must resolve to a tracked
file the way the compiler does: the includer's directory, the repo root, or
any directory that holds a tracked header - exactly the set CMakeLists.txt
builds as D2_INCLUDE_DIRS (root + every dir containing a .h). Angle includes
are SDK/system surface and out of scope. No compiler, no shims, no parent
trees: the whole tree in seconds, including the fork-owned and single-sided
files the sweep's selection skips. A quoted include that resolves only by
letter-case is reported separately: mingw32 on Windows accepts it, a
case-sensitive host (Linux CI, the syntax-check path) does not - same class
as the exec-bit note in the merge procedure.

Exit: 0 every quoted include resolves; 1 dangling (or case-only) includes,
each printed as file:line.
"""

import os
import re
import subprocess
import sys

EXCLUDED_PREFIXES = ("asm_matching/", "thirdparty/libbacktrace/")
# Host-check shadow stubs (tools/syntax-check.sh): never tracked, always
# provided by the toolchain or the shadow dir on every platform we build on.
SHADOW_OK = set()

INC_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def main() -> int:
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                           os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    os.chdir(root)

    ls = subprocess.run(["git", "ls-files", "-z"], capture_output=True, check=True)
    tracked = {f for f in ls.stdout.decode("utf-8", "replace").split("\0") if f}
    tracked_lower = {f.lower(): f for f in tracked}

    scanned = [f for f in sorted(tracked)
               if f.endswith((".cpp", ".h")) and not f.startswith(EXCLUDED_PREFIXES)]
    header_dirs = {os.path.dirname(f) for f in tracked if f.endswith(".h")}

    dangling, case_only = [], []
    for f in scanned:
        base = os.path.dirname(f)
        with open(f, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
        for i, line in enumerate(lines, 1):
            m = INC_RE.match(line)
            if not m:
                continue
            inc = m.group(1).replace("\\", "/")
            cands = {os.path.normpath(os.path.join(d, inc))
                     for d in ({base, ""} | header_dirs)}
            if any(c in tracked for c in cands):
                continue
            # Case check must run against the TRACKED set, never against the
            # candidate set itself (a candidate always equals its own
            # lowercase - the first cut reported every dangling include as
            # case-only, which the negative test caught).
            hit = next((tracked_lower[c.lower()] for c in cands
                        if c.lower() in tracked_lower), None)
            if inc in SHADOW_OK:
                continue
            (case_only if hit else dangling).append((f, i, inc, hit))

    for f, i, inc, hit in case_only:
        print(f'{f}:{i}: include "{inc}" matches tracked "{hit}" by letter-case '
              f'only (mingw accepts, case-sensitive hosts do not)')
    for f, i, inc, _ in dangling:
        print(f'{f}:{i}: quoted include not in tree: "{inc}"')
    print(f"include-path audit: {len(scanned)} files scanned, "
          f"{len(dangling)} dangling, {len(case_only)} case-only")
    return 1 if (dangling or case_only) else 0


if __name__ == "__main__":
    sys.exit(main())
