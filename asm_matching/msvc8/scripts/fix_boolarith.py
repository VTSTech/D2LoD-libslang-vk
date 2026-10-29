#!/usr/bin/env python3
"""fix_boolarith.py - a choice between two constants, where Ghidra did arithmetic on a comparison.

    fix_boolarith.py [--apply] [-j N] [PATTERN ...]

VC8 compiles `c ? 3 : 1` without a branch - `neg; sbb eax, eax; and eax, 2;
add eax, 1` or `setne al; lea eax, [eax*2 + 1]` by what c is - and Ghidra
reads that back as arithmetic on the comparison:

    (uint32_t)(bForWriting != 0) * 2 - 1

which VC8 compiles as written, another sequence (Lane D's quest and
DRLG sites). The choice goes back:

    (uint32_t)((bForWriting != 0) ? 1 : -1)

`(T)(C) * K [+|- B]`, C a comparison (==, !=, <, <=, >, >=) and K, B
integer constants, standing as a whole operand (after `(`, `[`, `,`, `=`,
`return`; before `)`, `]`, `,`, `;`), becomes `(T)((C) ? K+B : B)` with
the two values folded. C is 0 or 1, so the value is the same; the cast
keeps the type. Kept where the function comes out more similar.
"""
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audit            # noqa: E402
import fixlib           # noqa: E402
import ours as ourslib  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

NUM = r'(?:0x[0-9a-fA-F]+|\d+)[uUlL]*'
SITE = re.compile(r'(?:(?<=[(\[,=])|(?<=[(\[,=] )|(?<=return ))'
                  r'(?P<cast>\((?:uint32_t|int32_t|int|uint|unsigned int|DWORD)\))?'
                  r'\((?P<c>[^()]*(?:\([^()]*\)[^()]*)*\s(?:==|!=|<=|>=|<|>)\s[^()]*(?:\([^()]*\)[^()]*)*)\)'
                  r' \* (?P<k>%s)(?: (?P<s>[+-]) (?P<b>%s))?(?=\s*[)\],;])' % (NUM, NUM))


def val(t):
    return int(t.rstrip('uUlL'), 0)


def rewrite(m):
    k = val(m.group('k'))
    b = val(m.group('b')) if m.group('b') else 0
    if m.group('s') == '-':
        b = -b
    choice = '(%s) ? %d : %d' % (m.group('c'), k + b, b)
    return '%s(%s)' % (m.group('cast'), choice) if m.group('cast') else '(%s)' % choice


def candidates(rows, status, pats):
    """tu -> [(original start, line index, old line, new line)]."""
    out = collections.defaultdict(list)
    for tu in sorted({r[3] for r in rows}):
        if pats and not any(p in tu for p in pats):
            continue
        cpp = os.path.join(REPO, tu + '.cpp')
        obj = os.path.join(BUILD, 'obj', tu + '.obj')
        if not os.path.exists(cpp) or not os.path.exists(obj):
            continue
        lines = fixlib.read(cpp).split('\n')
        own = tu.lower() + '.cpp'
        by_sym = {r[4]: r[0] for r in rows if r[3] == tu}
        for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj'):
            s = by_sym.get(f.symbol)
            if s is None or status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
                continue
            mine = [ln for _o, fn, ln in f.lines if fn.lower().replace('\\', '/').endswith(own)]
            if not mine:
                continue
            for k in range(min(mine) - 1, max(mine)):
                new = SITE.sub(rewrite, lines[k])
                if new != lines[k]:
                    out[tu].append((s, k, lines[k], new))
    return out


def line_edit(cpp, k, old, new):
    def fn(texts):
        lines = texts[cpp].split('\n')
        if lines[k] != old:
            return None
        lines[k] = new
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
    cands = candidates(rows, status, args.patterns)
    total = sum(len(v) for v in cands.values())
    print('%d lines in %d TUs' % (total, len(cands)))
    if not args.apply:
        for tu in sorted(cands):
            for s, k, old, new in cands[tu]:
                print('%s:%d  %08x\n    - %s\n    + %s' % (tu, k + 1, s, old.strip()[:160], new.strip()[:160]))
        return 0
    _ref, model = audit.load_context()
    syms = audit.sym_map(rows)
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    groups = {}
    for tu in sorted(cands):
        cpp = os.path.join(REPO, tu + '.cpp')
        groups[tu] = ([fixlib.Edit('%s:%d' % (tu, k + 1), [s], [s], line_edit(cpp, k, old, new), require_gain=True)
                       for s, k, old, new in cands[tu]], [cpp])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    print('kept %d of %d edits, MATCH %+d' % (kept, total, gained))
    fixlib.record('fix_boolarith', total, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
