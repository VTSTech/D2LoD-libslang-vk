#!/usr/bin/env python3
"""Post-merge audit: every TU using D2BOOT_* must see the declaration.

The 1781 upstream merge re-planted D2Game/GAME/SCmd.cpp's [d2boot] mark but
dropped the fork's `#include "Redivivus/d2_boot.h"` (last include-block line) -
invisible to grep presence gates, fatal at compile ("D2BOOT_MarkFmt was not
declared in this scope"). This audit checks the whole class: any TU that
references a D2BOOT_* symbol must reach Redivivus/d2_boot.h, either by direct
include or through a header that (transitively) includes it.

Run after any upstream merge, alongside the rev47/rev48 audits -
see docs/merge-upstream.md step 4. Exit 0 = all visible, 1 = broken TU(s).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DECL = "Redivivus/d2_boot.h"
SKIP = ("Redivivus/d2_boot.cpp", DECL)  # impl + declaration itself

inc_re = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def includes_of(path: Path):
    try:
        return inc_re.findall(path.read_text(errors="replace"))
    except OSError:
        return []


def resolve(rel: str) -> Path | None:
    p = ROOT / rel
    return p if p.is_file() else None


# 1) headers that include d2_boot.h, expanded transitively (providers)
providers: set[str] = set()
for h in ROOT.rglob("*.h"):
    rel = h.relative_to(ROOT).as_posix()
    if rel.startswith(("asm_matching/", ".git/")) or rel in SKIP:
        continue
    if any(i.endswith(DECL) for i in includes_of(h)):
        providers.add(rel)

provider_closure = set(providers)
changed = True
while changed:
    changed = False
    for rel in list(provider_closure):
        p = resolve(rel)
        if not p:
            continue
        for inc in includes_of(p):
            tgt = resolve(inc)
            if not tgt:
                continue
            tgt_rel = tgt.relative_to(ROOT).as_posix()
            if tgt_rel.endswith(".h") and tgt_rel not in provider_closure \
                    and not tgt_rel.startswith("asm_matching/"):
                provider_closure.add(tgt_rel)
                changed = True

# 2) audit every TU that references a D2BOOT_ symbol
fail = 0
for cpp in sorted(ROOT.rglob("*.cpp")):
    rel = cpp.relative_to(ROOT).as_posix()
    if rel.startswith(("asm_matching/", ".git/")) or rel in SKIP:
        continue
    text = cpp.read_text(errors="replace")
    if "D2BOOT_" not in text:
        continue
    incs = inc_re.findall(text)
    direct = any(i.endswith(DECL) for i in incs)
    via = [i for i in incs
           if (r := resolve(i)) and r.relative_to(ROOT).as_posix() in provider_closure]
    ok = direct or via
    fail += 0 if ok else 1
    print(f"{'ok  ' if ok else 'FAIL'} {rel} | direct={direct} via={via}")

print(f"\nresult: {'ALL VISIBLE' if fail == 0 else str(fail) + ' BROKEN TU(s)'}")
sys.exit(1 if fail else 0)
