#!/usr/bin/env python3
"""fix_inplace.py - update a field in place where Ghidra went through a pointer.

    fix_inplace.py [--apply] [-j N] [PATTERN ...]

A read-modify-write of memory, `inc dword ptr [esi + 0x10]` or `or dword
ptr [eax + 4], ecx`, comes back from Ghidra through a temporary pointer:

    piVar1 = &pUnit->nCount;
    *piVar1 = *piVar1 + 1;            (or (*piVar1)++, *piVar1 |= x)

which VC8 compiles as written - the address into a register, then the
update through it - where the original updates the field itself (Lane D's
in-place increments in D2Game). The temporary goes back into the
expression:

    pUnit->nCount++;                  (pUnit->nCount += x, |= x, ...)

where:

  - the temporary's line assigns it `&E` and nothing else, E naming memory
    without calling or changing anything (no call, `=`, `++`, `--`);
  - the next line updates `*p` once: `(*p)++`, `(*p)--`, `*p OP= v`, or
    `*p = *p OP v` with OP one of + - | & ^ << >>, and v does not mention p;
  - p is dead after that line (fix_stringops.dead_after); a declaration
    with the address as its initialiser keeps the bare declaration when p is
    mentioned elsewhere.

E is evaluated once either way, so the meaning is the same. Kept where the
function comes out more similar.
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
from fix_stringops import dead_after, mentions  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

TEMP = re.compile(r'^(?P<ind>[ \t]*)(?P<decl>(?:const\s+)?[A-Za-z_][\w:]*(?:\s*\*)+\s*)?(?P<p>[A-Za-z_]\w*) = '
                  r'&(?P<e>[^;]+);[ \t]*$')
IMPURE = re.compile(r'\+\+|--|(?<![=!<>])=(?!=)|\b(?!sizeof\b)[A-Za-z_]\w*\s*\(')
OPS = r'\+|-|\||&|\^|<<|>>'


def top_ops(v):
    """The binary operators of `v` outside any parentheses or brackets."""
    depth, out, prev = 0, [], '('
    toks = re.findall(r'->|<<|>>|&&|\|\||[-+*/%&|^<>?:()\[\]]|[\w.]+|\S', v)
    for t in toks:
        if t in '([':
            depth += 1
        elif t in ')]':
            depth -= 1
        elif depth == 0 and t not in ('->',) and not re.match(r'[\w.]', t) and prev not in ('(', 'op'):
            out.append(t)
            prev = 'op'
            continue
        prev = t if t in '([' else 'x'
    return out


def update(line, p):
    """`line` with the update of *p written on E's placeholder `\\0`, or None."""
    q = re.escape(p)
    m = re.match(r'^(?P<ind>[ \t]*)\(\*%s\)(?P<op>\+\+|--);[ \t]*$' % q, line)
    if m:
        return '%s\0%s;' % (m.group('ind'), m.group('op'))
    m = re.match(r'^(?P<ind>[ \t]*)\*%s (?P<op>%s)= (?P<v>[^;]+);[ \t]*$' % (q, OPS), line)
    if not m:
        m = re.match(r'^(?P<ind>[ \t]*)\*%s = \*%s (?P<op>%s) (?P<v>[^;]+);[ \t]*$' % (q, q, OPS), line)
        # `*p - a + b` is not `*p - (a + b)`: the rest may only continue the
        # same associative operation (+ and - for +).
        if m and not set(top_ops(m.group('v'))) <= {'+': {'+', '-'}, '|': {'|'}, '&': {'&'}, '^': {'^'}}.get(
                m.group('op'), set()):
            return None
    if not m or mentions(p, m.group('v')):
        return None
    v = m.group('v')
    if m.group('op') in '+-' and re.fullmatch(r'1|1U|1u|0x1', v.strip()):
        return '%s\0%s;' % (m.group('ind'), m.group('op') * 2)
    return '%s\0 %s= %s;' % (m.group('ind'), m.group('op'), v)


def site(lines, k, lo, hi):
    t = TEMP.match(lines[k])
    if not t or k + 1 > hi:
        return None
    p, e = t.group('p'), t.group('e').strip()
    if IMPURE.search(e) or mentions(p, e):
        return None
    new = update(lines[k + 1], p)
    if new is None:
        return None
    if not dead_after(lines, lo, hi, k, k + 1, (p,)):
        return None
    others = [j for j in range(lo + 1, hi + 1) if j not in (k, k + 1) and mentions(p, lines[j])]
    if re.search(r'[^\w\s.\->\[\]:]', e.replace('->', '')):
        e = '(%s)' % e                       # anything but a plain member path keeps its parentheses
    out = [new.replace('\0', e)]
    if t.group('decl') and others:
        out.insert(0, t.group('ind') + t.group('decl').rstrip() + ' ' + p + ';')
    return [lines[k], lines[k + 1]], out


def candidates(rows, status, pats):
    """tu -> [(original start, first line, old lines, new lines)] (1-based)."""
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
            lo, hi = min(mine) - 1, max(mine) - 1
            k = lo
            while k < hi:
                r = site(lines, k, lo, hi)
                if r:
                    out[tu].append((s, k + 1, r[0], r[1]))
                    k += 2
                else:
                    k += 1
    return out


def block_edit(cpp, ln, old, new):
    def fn(texts):
        lines = texts[cpp].split('\n')
        # Edits below this one may have changed the line count above it only
        # if they were applied first: the batch applies them bottom-up.
        if lines[ln - 1:ln - 1 + len(old)] != old:
            return None
        lines[ln - 1:ln - 1 + len(old)] = new
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
    print('%d sites in %d TUs' % (total, len(cands)))
    if not args.apply:
        for tu in sorted(cands):
            for s, ln, old, new in cands[tu]:
                print('%s:%d  %08x' % (tu, ln, s))
                for l in old:
                    print('    - ' + l.strip()[:160])
                for l in new:
                    print('    + ' + l.strip()[:160])
        return 0
    _ref, model = audit.load_context()
    syms = audit.sym_map(rows)
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    groups = {}
    for tu in sorted(cands):
        cpp = os.path.join(REPO, tu + '.cpp')
        order = sorted(cands[tu], key=lambda c: -c[1])     # bottom-up: no edit moves another's line
        groups[tu] = ([fixlib.Edit('%s:%d' % (tu, ln), [s], [s], block_edit(cpp, ln, old, new), require_gain=True)
                       for s, ln, old, new in order], [cpp])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    print('kept %d of %d edits, MATCH %+d' % (kept, total, gained))
    fixlib.record('fix_inplace', total, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
