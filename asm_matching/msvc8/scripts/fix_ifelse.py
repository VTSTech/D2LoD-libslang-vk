#!/usr/bin/env python3
"""fix_ifelse.py - put if/else branches back in the original's order.

    fix_ifelse.py [--apply] [PATTERN ...]

VC8 lays `if (c) { A } else { B }` out in source order: the jump on !c to B,
A falling through. The decompiler does not keep that order - it often hands
the branches back the other way round, condition negated - so where the
original falls through into what the tree has as the else branch, the
compare-and-jump is the inverse of the original's (DumpLog, 004082e0: `test
edx, edx; je` into CRASH_FormatStackTrace, where the tree jumps on jne). A
site is a compare and conditional jump aligned with the original's whose
jump is the inverse (je/jne, jl/jge, jb/jae, ...); at its source line an
`if (c) {` - or `} else if (c) {` - with an `} else {` branch is rewritten
`if (!c) { B } else { A }`. The two bodies trade places, so the number of
lines stays the same. An `if` without an else, or followed by `else if`, is
left alone. (An assertion inside an else-if chain is the common case:
`else if (tag_ok) { ... } else { halt; }`, ASYNCDATA_GetResult, 0040a9e0.)

With the branches the other way round the two layouts often do not align at
all, and no inverted jump shows. So every `if (!x) {A} else {B}` - x one
operand, the if alone or that of an `} else if` - in a function that does not
match is tried as `if (x) {B} else {A}` too: Blizzard tests the pointer and
handles its absence after (`if (p) x = f(p); else x = 0;`, Lane E's P15).
Found by its form alone, such a swap is kept only where its function comes
out more similar; --sites-only leaves them out.

Without --apply it lists the sites; with --apply they go through fixlib's
batch loop like every fixer: kept only where nothing that matched stops
matching and the function gets no less similar.
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
from fix_assert import negate  # noqa: E402
from fix_compare import norm  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

INVERSE = {'je': 'jne', 'jne': 'je', 'jl': 'jge', 'jge': 'jl', 'jle': 'jg', 'jg': 'jle', 'jb': 'jae',
           'jae': 'jb', 'jbe': 'ja', 'ja': 'jbe', 'js': 'jns', 'jns': 'js', 'jp': 'jnp', 'jnp': 'jp'}
IF_OPEN = re.compile(r'(?P<ind>[ \t]*)(?P<pre>(?:\} else )?)if \(')   # an if, or the if of an `} else if`


def skip_literal(text, i):
    """Index after the string or character literal starting at text[i]."""
    q = text[i]
    i += 1
    while i < len(text) and text[i] != q:
        i += 2 if text[i] == '\\' else 1
    return i + 1


def match_close(text, i, open_ch, close_ch):
    """Index of the bracket closing the one at text[i]; None if unbalanced."""
    depth = 0
    while i < len(text):
        c = text[i]
        if c in '"\'':
            i = skip_literal(text, i)
            continue
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = len(text) if j < 0 else j
            continue
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = len(text) if j < 0 else j + 2
            continue
        if c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return None


def swap_at(text, at):
    """`text` with the if/else starting at offset `at` swapped, or None."""
    m = IF_OPEN.match(text, at)
    if not m:
        return None
    ind = m.group('ind')
    cond_open = m.end() - 1
    cond_close = match_close(text, cond_open, '(', ')')
    if cond_close is None or not text.startswith(') {\n', cond_close):
        return None
    then_open = cond_close + 2
    then_close = match_close(text, then_open, '{', '}')
    if then_close is None or not text.startswith('} else {\n', then_close):
        return None
    if text[text.rfind('\n', 0, then_close) + 1:then_close] != ind:
        return None
    else_open = then_close + len('} else ')
    else_close = match_close(text, else_open, '{', '}')
    if else_close is None or text[text.rfind('\n', 0, else_close) + 1:else_close] != ind:
        return None
    then_body = text[then_open + 2:then_close - len(ind)]       # whole lines, newline-terminated
    else_body = text[else_open + 2:else_close - len(ind)]
    cond = text[cond_open + 1:cond_close]
    new = '%s%sif (%s) {\n%s%s} else {\n%s%s}' % (ind, m.group('pre'), negate(cond), else_body, ind, then_body, ind)
    return text[:at] + new + text[else_close + 1:]


def sites(ours, ref):
    """Our compare-and-jump instructions whose jump is the inverse of the original's."""
    sm = difflib.SequenceMatcher(None, [norm(x) for x in ours], [norm(y) for y in ref], autojunk=False)
    out = []
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op != 'equal':
            continue
        for i, j in zip(range(i1, i2), range(j1, j2)):
            a, b = ours[i].mnemonic, ref[j].mnemonic
            if INVERSE.get(a) == b and i > 0 and ours[i - 1].mnemonic in ('cmp', 'test'):
                out.append(ours[i])
    return out


def candidates(rows, model, syms, status, pats, negated=True):
    """tu -> [(original start, line, old text of the statement, new text, trial)];
    trial: found by its form alone, not by an inverted jump."""
    out = collections.defaultdict(list)
    cache = {}
    texts = {}
    for s, _e, name, tu, sym, _m, _d in rows:
        if pats and not any(p in tu for p in pats):
            continue
        if status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
            continue
        if tu not in cache:
            obj = os.path.join(BUILD, 'obj', tu + '.obj')
            cache[tu] = {f.symbol: f for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj')} \
                if os.path.exists(obj) else {}
        f = cache[tu].get(sym)
        if f is None:
            continue
        found = sites(compare.our_view(f, syms), compare.ref_view(s, model[s]))
        cpp = os.path.join(REPO, tu + '.cpp')
        own = tu.lower() + '.cpp'
        if not found and not negated:
            continue
        if cpp not in texts:
            texts[cpp] = fixlib.read(cpp)
        text = texts[cpp]
        starts = [0] + [k + 1 for k, c in enumerate(text) if c == '\n']
        lines = []
        for insn in found:
            at = f.line_at(insn.addr)
            if at and at[0].lower().replace('\\', '/').endswith(own):
                lines.append((at[1], False))
        if negated:
            # Every `if (!x) {` in the function's lines, inverted jump or not:
            # with the branches swapped the jumps rarely align at all.
            own_lines = [ln for _o, fn, ln in f.lines if fn.lower().replace('\\', '/').endswith(own)]
            if own_lines:
                for ln in range(min(own_lines), min(max(own_lines), len(starts)) + 1):
                    m = NEGATED_IF.match(text, starts[ln - 1])
                    if m and simple_operand(m.group('x') or m.group('y')):
                        lines.append((ln, True))
        done = set()
        for ln, trial in lines:
            if ln in done or not 0 < ln <= len(starts):
                continue
            new = swap_at(text, starts[ln - 1])
            if new is None:
                continue
            done.add(ln)
            # Keep only the statement: what differs between the two texts.
            a = starts[ln - 1]
            b = 0
            while b < len(text) - a and text[len(text) - 1 - b] == new[len(new) - 1 - b]:
                b += 1
            out[tu].append((s, ln, text[a:len(text) - b], new[a:len(new) - b], trial))
    return out


# `if (!x) {` or `} else if (!x) {`, x one operand: no top-level && || ?: -
# or the same test written out, `x == 0`, `x == NULL`, `c == '\0'`.
NEGATED_IF = re.compile(r'[ \t]*(?:\} else )?if \((?:!(?!=)(?P<x>[^&|?\n]+)|(?P<y>[^&|?\n=!<>]+) == '
                        r'(?:0[uU]?|0x0|NULL|nullptr|\'\\0\'))\) \{[ \t]*$', re.M)


def simple_operand(x):
    """Balanced, and no space outside brackets: one operand, not `!a == b`."""
    depth = 0
    for c in x:
        depth += {'(': 1, ')': -1, '[': 1, ']': -1}.get(c, 0)
        if depth < 0 or (depth == 0 and c == ' '):
            return False
    return depth == 0


def file_edit(cpp, ln, before, after):
    """The swap at line `ln`, applied to whatever the file holds now."""
    def fn(texts):
        t = texts[cpp]
        starts = [0] + [k + 1 for k, c in enumerate(t) if c == '\n']
        if ln > len(starts) or not t.startswith(before, starts[ln - 1]):
            return None                  # another swap moved or changed it
        new = swap_at(t, starts[ln - 1])
        return None if new is None else {cpp: new}
    return fn


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('-j', '--jobs', type=int, default=4)
    ap.add_argument('--sites-only', action='store_true', help='no trial of `if (!x)` forms without an inverted jump')
    ap.add_argument('patterns', nargs='*')
    args = ap.parse_args()
    rows = audit.load_funcmap(os.path.join(ORACLE, 'evidence', 'funcmap.tsv'))
    syms = audit.sym_map(rows)
    status = {}
    for line in open(os.path.join(ORACLE, 'evidence', 'audit.tsv')):
        p = line.rstrip('\n').split('\t')
        if p[0] != 'start' and len(p) > 9:
            status[int(p[0], 16)] = p[9]
    _ref, model = audit.load_context()
    cands = candidates(rows, model, syms, status, args.patterns, negated=not args.sites_only)
    total = sum(len(v) for v in cands.values())
    trials = sum(1 for v in cands.values() for c in v if c[4])
    print('%d sites in %d TUs (%d by form alone)' % (total, len(cands), trials))
    if not args.apply:
        for tu in sorted(cands):
            for s, ln, before, after, trial in cands[tu]:
                print('%s:%d  %08x%s\n%s\n---\n%s\n' % (tu, ln, s, ' (form)' if trial else '', before, after))
        return 0
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    groups = {}
    for tu in sorted(cands):
        cpp = os.path.join(REPO, tu + '.cpp')
        # A swap found by its form alone must make its function more similar:
        # nothing in the code asked for it.
        groups[tu] = ([fixlib.Edit('%s:%d' % (tu, ln), [s], [s], file_edit(cpp, ln, before, after), require_gain=trial)
                       for s, ln, before, after, trial in cands[tu]], [cpp])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    print('kept %d of %d edits, MATCH %+d' % (kept, total, gained))
    fixlib.record('fix_ifelse', total, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
