#!/usr/bin/env python3
"""fix_localtype.py - a 32-bit local where the original keeps a byte or a word in a whole register.

    fix_localtype.py [--apply] [PATTERN ...]

Ghidra types a local by what the function does with it: a value only ever
tested in its low byte gets `uint8_t`. VC8 then loads it into a byte
register and stores it as a byte, where the original - with an int local -
zero-extends the load and stores a dword: `mov al, byte ptr [edi + 3]`
against `movzx eax, byte ptr [edi + 3]`, `mov byte ptr [ebp - 0xcd], 0`
against `mov dword ptr [ebp - 0xd0], 0` (SMEM_DestroyHeap, 00411f00:
`uint8_t byFlags = pBlock->byFlags`, `uint8_t byLeakFlags = 0`).

A site is such a pair of instructions, aligned with the original's; its
source line comes from the /Z7 line table. A local the function declares
uint8_t, int8_t, char, uint16_t, int16_t, short, BYTE or WORD on a line of
its own (not bool, not an array) and names on a site's line, or two lines
around it (VC8 schedules a load among a neighbour's code), is declared
uint32_t (int32_t if the narrow type is signed), every such local of the
function in one edit. A wider local holds what the narrow one truncated
off, so the edit is kept only where the function comes out more similar
(fixlib's require_gain): VC8 must then emit the original's code.
"""
import argparse
import collections
import difflib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audit            # noqa: E402
import compare          # noqa: E402
import fixlib           # noqa: E402
import ours as ourslib  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

DECL = re.compile(r'^(?P<ind>\s*)(?P<t>uint8_t|int8_t|unsigned char|signed char|char|uint16_t|int16_t|'
                  r'unsigned short|short|BYTE|WORD|UCHAR|USHORT)\s+(?P<n>[A-Za-z_]\w*)(?P<rest>\s*(?:=[^;]*)?;.*)$')
# VC8 schedules a load into the code of a line nearby: `byFlags = pBlock->byFlags`
# is line 482 of Storm/SMem.cpp, its load comes under 483's.
NEAR = (-2, -1, 0, 1, 2)
SIGNED = {'int8_t', 'signed char', 'char', 'int16_t', 'short'}
NARROW_LOAD = re.compile(r'^mov\s+(?:[abcd][lh]|[abcd]x|si|di),\s*(?:byte|word) ptr ')
WIDE_LOAD = re.compile(r'^movzx\s+e\w\w,\s*(?:byte|word) ptr |^movsx\s+e\w\w,\s*(?:byte|word) ptr ')
NARROW_STORE = re.compile(r'^mov\s+(?:byte|word) ptr \[ebp - ')
WIDE_STORE = re.compile(r'^mov\s+dword ptr \[ebp - ')


def site(a, b):
    return (NARROW_LOAD.match(a) and WIDE_LOAD.match(b)) or (NARROW_STORE.match(a) and WIDE_STORE.match(b))


def site_lines(f, ours, ref):
    """The source lines of the function's sites."""
    sm = difflib.SequenceMatcher(None, [x.mnemonic for x in ours], [y.mnemonic for y in ref], autojunk=False)
    out = set()
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == 'equal' or (op == 'replace' and i2 - i1 == j2 - j1):
            for i, j in zip(range(i1, i2), range(j1, j2)):
                if site(ours[i].text, ref[j].text):
                    at = f.line_at(ours[i].addr)
                    if at:
                        out.add(at)
    return out


def candidates(rows, status, syms, model, pats):
    """tu -> [(original start, {line: (old, new)})]."""
    out = collections.defaultdict(list)
    for tu in sorted({r[3] for r in rows}):
        if pats and not any(p in tu for p in pats):
            continue
        cpp = os.path.join(REPO, tu + '.cpp')
        obj = os.path.join(BUILD, 'obj', tu + '.obj')
        if not os.path.exists(cpp) or not os.path.exists(obj):
            continue
        lines = None
        own = tu.lower() + '.cpp'
        by_sym = {r[4]: r[0] for r in rows if r[3] == tu}
        for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj'):
            s = by_sym.get(f.symbol)
            if s is None or status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
                continue
            mine = [ln for _o, fn, ln in f.lines if fn.lower().replace('\\', '/').endswith(own)]
            if not mine:
                continue
            at = {ln for fn, ln in site_lines(f, compare.our_view(f, syms), compare.ref_view(s, model[s]))
                  if fn.lower().replace('\\', '/').endswith(own)}
            if not at:
                continue
            if lines is None:
                lines = fixlib.read(cpp).split('\n')
            lo, hi = min(mine), max(mine)
            changes = {}
            for ln in range(lo, hi + 1):
                m = DECL.match(lines[ln - 1])
                if not m:
                    continue
                name = re.compile(r'(?<![\w.>])%s\b' % re.escape(m.group('n')))
                if not any(0 < k + d <= len(lines) and name.search(lines[k + d - 1]) for k in at for d in NEAR):
                    continue
                t = 'int32_t' if m.group('t') in SIGNED else 'uint32_t'
                changes[ln] = (lines[ln - 1], '%s%s %s%s' % (m.group('ind'), t, m.group('n'), m.group('rest')))
            if changes:
                out[tu].append((s, changes))
    return out


def line_edits(cpp, changes):
    def fn(texts):
        lines = texts[cpp].split('\n')
        for ln, (old, new) in changes.items():
            if ln > len(lines) or lines[ln - 1] != old:
                return None
            lines[ln - 1] = new
        return {cpp: '\n'.join(lines)}
    return fn


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('-j', '--jobs', type=int, default=4)
    ap.add_argument('patterns', nargs='*')
    args = ap.parse_args()
    rows = audit.load_funcmap(os.path.join(ORACLE, 'evidence', 'funcmap.tsv'))
    status = {}
    for line in open(os.path.join(ORACLE, 'evidence', 'audit.tsv')):
        p = line.rstrip('\n').split('\t')
        if p[0] != 'start' and len(p) > 9:
            status[int(p[0], 16)] = p[9]
    _ref, model = audit.load_context()
    syms = audit.sym_map(rows)
    cands = candidates(rows, status, syms, model, args.patterns)
    nlines = sum(len(c) for v in cands.values() for _s, c in v)
    print('%d locals in %d functions, %d TUs' % (nlines, sum(len(v) for v in cands.values()), len(cands)))
    if not args.apply:
        for tu in sorted(cands):
            for s, changes in cands[tu]:
                for ln, (old, new) in sorted(changes.items()):
                    print('%s:%d  %08x\n    - %s\n    + %s' % (tu, ln, s, old.strip()[:160], new.strip()[:160]))
        return 0
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    groups = {}
    for tu in sorted(cands):
        cpp = os.path.join(REPO, tu + '.cpp')
        groups[tu] = ([fixlib.Edit('%s:%d' % (tu, min(c)), [s], [s], line_edits(cpp, c), require_gain=True)
                       for s, c in cands[tu]], [cpp])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    n = sum(len(v) for v in cands.values())
    print('kept %d of %d edits, MATCH %+d' % (kept, n, gained))
    fixlib.record('fix_localtype', n, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
