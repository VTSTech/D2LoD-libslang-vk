#!/usr/bin/env python3
"""fix_shiftbit.py - a flag read as a shifted bit, where Ghidra wrote a mask.

    fix_shiftbit.py [--apply] [-j N] [PATTERN ...]

Blizzard tests some flags as a bool made of the shifted bit - `mov eax,
dword ptr [esi + 0xc8]; shr eax, 3; test al, 1` (the unit's eUnitFlagsEx
tests in D2Client and D2Game). Ghidra writes `pUnit->eUnitFlagsEx >> 3 & 1`
or folds the shift into a mask, `pUnit->eUnitFlagsEx & 8`; VC8 compiles
either, and `if`-tests of `(f >> 3 & 1) == 1`, a bitfield, a local holding
it, to `test byte ptr [esi + 0xc8], 8`. Only the conversion to bool,
`(bool)(f >> 3 & 1)`, keeps the shift (measured with the oracle's
compiler).

The sites are taken in a function the original tests a bit that way at n
(a dword load, `shr R, n`, `test Rl, 1` or `and R, 1`) more often than ours
does: `E & K`, K = 1 << n a literal or an enumerator a header gives that
value (`UNITFLAGEX_HASINV`), or `E >> n & 1`, E a plain lvalue (a name with
member accesses and subscripts), where only the truth of the result counts
- the condition of an if or while, an operand of `!`, `&&`, `||`, `?`, or
a comparison with 0. It becomes `((bool)(E >> n & 1))`, true exactly when
`E & K` is.
Only as many sites per n as the original has beyond ours; a bit with more
sites than that is left alone. Kept where the function comes out more
similar.
"""
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audit            # noqa: E402
import compare          # noqa: E402
import fixlib           # noqa: E402
import ours as ourslib  # noqa: E402
from fix_bitmask import enumerators, occurrences  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

SITE = re.compile(r'(?<![\w.>\]])(?P<e>[A-Za-z_]\w*(?:(?:->|\.)[A-Za-z_]\w*|\[[^\[\]]*\])*) & '
                  r'(?P<k>0x[0-9a-fA-F]+[uUlL]*|\d+[uUlL]*|[A-Z_][A-Z0-9_]*)(?![\w(\[.])')
SHIFTED = re.compile(r'(?<![\w.>\]])(?P<e>[A-Za-z_]\w*(?:(?:->|\.)[A-Za-z_]\w*|\[[^\[\]]*\])*) >> '
                     r'(?P<n>0x[0-9a-fA-F]+|\d+) & 1(?![\w(\[.])')



def value(k, names):
    if k[0].isdigit():
        return int(k.rstrip('uUlL'), 0)
    return names.get(k)


def shifted_bits(view):
    """Counter of n over `mov R, dword ptr [..]; shr R, n; test Rl, 1 / and R, 1`."""
    out = collections.Counter()
    for k in range(len(view) - 2):
        m = re.match(r'^mov (e[a-d]x|e[sd]i), dword ptr \[', view[k].text)
        if not m:
            continue
        r = m.group(1)
        s = re.match(r'^shr %s, (0x[0-9a-f]+|\d+)$' % r, view[k + 1].text)
        low = {'eax': 'al', 'ebx': 'bl', 'ecx': 'cl', 'edx': 'dl'}.get(r)
        if s and re.match(r'^(?:and %s, 1|test %s, 1)$' % (r, low or r), view[k + 2].text):
            out[int(s.group(1), 0)] += 1
    return out


def truth_only(code, a, b):
    """Whether code[a:b] is read only for its truth."""
    before, after = code[:a].rstrip(), code[b:].lstrip()
    if re.match(r'^(?:&&|\|\||\?)', after) or before.endswith(('&&', '||')):
        return True
    if before.endswith('!') and not before.endswith('!='):
        return True
    if not (before.endswith('(') and after.startswith(')')):
        return False
    # In parentheses of its own: the group is what is tested.
    head, rest = before[:-1].rstrip(), after[1:].lstrip()
    if re.match(r'^(?:[!=]= 0[uUlL]*(?![\w.])|&&|\|\||\?)', rest) or head.endswith(('&&', '||')):
        return True
    if head.endswith('!') and not head.endswith('!='):
        return True
    return re.search(r'\b(?:if|while)$', head) is not None          # the whole condition


def candidates(rows, status, model, syms, pats):
    """tu -> [(original start, line index, old line, start, end, new text)]."""
    out = collections.defaultdict(list)
    names = enumerators()
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
            if s is None or s not in model or status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
                continue
            theirs = shifted_bits(compare.ref_view(s, model[s]))
            if not theirs:
                continue
            ours = shifted_bits(compare.our_view(f, syms))
            want = {n: c - ours[n] for n, c in theirs.items() if c > ours[n]}
            if not want:
                continue
            mine = [ln for _o, fn, ln in f.lines if fn.lower().replace('\\', '/').endswith(own)]
            if not mine:
                continue
            if lines is None:
                lines = fixlib.read(cpp).split('\n')
            found = collections.defaultdict(list)
            for k in range(min(mine) - 1, max(mine)):
                code = lines[k].split('//', 1)[0]
                for m in SITE.finditer(code):
                    v = value(m.group("k"), names)
                    if v is None or v <= 1 or v & (v - 1) or v.bit_length() - 1 not in want:
                        continue
                    if not truth_only(code, m.start(), m.end()):
                        continue
                    n = v.bit_length() - 1
                    found[n].append((k, m.start(), m.end(), '((bool)(%s >> %d & 1))' % (m.group('e'), n)))
                for m in SHIFTED.finditer(code):
                    n = int(m.group('n'), 0)
                    if n in want and truth_only(code, m.start(), m.end()):
                        found[n].append((k, m.start(), m.end(), '((bool)(%s))' % m.group(0)))
            for n, ss in found.items():
                if len(ss) <= want[n]:
                    out[tu].extend((s, k, lines[k], a, b, new) for k, a, b, new in ss)
    return out


def site_edit(cpp, ln, old, a, b, new):
    word = old[a:b]
    j = occurrences(old, word).index(a)

    def fn(texts):
        lines = texts[cpp].split('\n')
        at = occurrences(lines[ln], word)
        if len(at) <= j:
            return None
        lines[ln] = lines[ln][:at[j]] + new + lines[ln][at[j] + len(word):]
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
    cands = candidates(rows, status, model, syms, args.patterns)
    total = sum(len(v) for v in cands.values())
    print('%d sites in %d TUs' % (total, len(cands)))
    if not args.apply:
        for tu in sorted(cands):
            for s, k, old, a, b, new in cands[tu]:
                print('%s:%d  %08x  %s -> %s' % (tu, k + 1, s, old[a:b], new))
        return 0
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    groups = {}
    for tu in sorted(cands):
        cpp = os.path.join(REPO, tu + '.cpp')
        groups[tu] = ([fixlib.Edit('%s:%d:%d' % (tu, k + 1, a), [s], [s], site_edit(cpp, k, old, a, b, new),
                                   require_gain=True)
                       for s, k, old, a, b, new in cands[tu]], [cpp])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    print('kept %d of %d edits, MATCH %+d' % (kept, total, gained))
    fixlib.record('fix_shiftbit', total, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
