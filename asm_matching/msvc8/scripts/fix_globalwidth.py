#!/usr/bin/env python3
"""fix_globalwidth.py - a dword global where Ghidra typed a byte or a word.

    fix_globalwidth.py [--apply] [PATTERN ...]

Ghidra types a global by the accesses it saw: a flag the code only ever sets
to 0 or 1 and tests becomes `bool gbUIStatButtonHovered`, a value of which a
caller kept the low half `uint16_t gnTradePlayerGoldAmount`. VC8 then reads
and writes a byte or a word, `mov byte ptr [g], 1`, `movzx eax, word ptr
[g]`, where the original reads and writes a dword, `mov dword ptr [g], 1`
(D2Client's ui.cpp, the trade gold amount in D2Client.cpp).

A site is an instruction of ours that reads or writes a global as a byte or a
word, aligned with an instruction of the original's doing the same as a
dword. A global with more such sites than sites where the widths agree, and
declared - in globals.h, globals/*.cpp, or as a static of its file or
function - as a scalar `bool`, `char`, `int8_t`, `uint8_t`, `BYTE`, `int16_t`,
`uint16_t`, `short`, `WORD` (signed or unsigned), is given the dword type:
`BOOL` for a bool, `int32_t` or `uint32_t` by the signedness of the rest,
in every declaration and definition of its name. What the code computes
changes only where it stored a value the narrow type truncated, which the
original then kept whole.

The edits are made at once, the tree rebuilt (globals.h is in the
precompiled header) and judged whole; an edit is dropped when a function
that reads the global stops matching, or the functions that read it come out
less similar in sum and none comes to match. A loss no edit accounts for
keeps nothing.
"""
import argparse
import collections
import difflib
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audit            # noqa: E402
import check            # noqa: E402
import compare          # noqa: E402
import fixlib           # noqa: E402
import ours as ourslib  # noqa: E402
from fix_fastcall import build_all  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

ROUNDS = 3
WIDTH = re.compile(r'\b(byte|word|dword) ptr \[')
WIDE = {'bool': 'BOOL', 'char': 'int32_t', 'signed char': 'int32_t', 'int8_t': 'int32_t',
        'unsigned char': 'uint32_t', 'uint8_t': 'uint32_t', 'BYTE': 'uint32_t', 'UCHAR': 'uint32_t',
        'short': 'int32_t', 'signed short': 'int32_t', 'int16_t': 'int32_t',
        'unsigned short': 'uint32_t', 'uint16_t': 'uint32_t', 'WORD': 'uint32_t', 'USHORT': 'uint32_t'}
TYPES = '|'.join(sorted((re.escape(t).replace(r'\ ', r'\s+') for t in WIDE), key=len, reverse=True))


def name_of(sym):
    if sym.startswith('?'):
        return sym[1:].split('@', 1)[0]
    return sym[1:] if sym.startswith('_') else sym


def decl_rx(name):
    return re.compile(r'^(?P<pre>[ \t]*(?:(?:static|extern|volatile)\s+)*)(?P<t>%s)(?P<post>\s+%s\s*(?:=[^;\[]*)?;)'
                      % (TYPES, re.escape(name)), re.M)


def votes(rows, status, model, syms, pats):
    """our data symbol -> (narrow sites, agreeing sites, functions with a narrow site)."""
    narrow, same, fns = collections.Counter(), collections.Counter(), collections.defaultdict(set)
    for tu in sorted({r[3] for r in rows}):
        if pats and not any(p in tu for p in pats):
            continue
        obj = os.path.join(BUILD, 'obj', tu + '.obj')
        if not os.path.exists(obj):
            continue
        by_sym = {r[4]: r[0] for r in rows if r[3] == tu}
        for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj'):
            s = by_sym.get(f.symbol)
            if s is None or s not in model or status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
                continue
            ours, ref = compare.our_view(f, syms), compare.ref_view(s, model[s])
            sm = difflib.SequenceMatcher(None, [x.mnemonic for x in ours], [y.mnemonic for y in ref], autojunk=False)
            for op, i1, i2, j1, j2 in sm.get_opcodes():
                if op not in ('equal', 'replace'):
                    continue
                for i, j in zip(range(i1, i2), range(j1, j2)):
                    ds = [t[1][0] for t in ours[i].tokens if t[0] == 'data' and isinstance(t[1], tuple)]
                    if len(ds) != 1 or len([t for t in ref[j].tokens if t[0] == 'data']) != 1:
                        continue
                    a, b = WIDTH.search(ours[i].text), WIDTH.search(ref[j].text)
                    if not a or not b:
                        continue
                    if a.group(1) in ('byte', 'word') and b.group(1) == 'dword':
                        narrow[ds[0]] += 1
                        fns[ds[0]].add(s)
                    elif a.group(1) == b.group(1):
                        same[ds[0]] += 1
    return {d: (narrow[d], same[d], fns[d]) for d in narrow}


def readers(rows, model, syms, targets):
    """our data symbol -> original starts of the functions of ours that refer to it."""
    out = collections.defaultdict(set)
    for tu in sorted({r[3] for r in rows}):
        obj = os.path.join(BUILD, 'obj', tu + '.obj')
        if not os.path.exists(obj):
            continue
        by_sym = {r[4]: r[0] for r in rows if r[3] == tu}
        for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj'):
            s = by_sym.get(f.symbol)
            if s is None:
                continue
            for _off, kind, tgt in f.refs:
                if tgt in targets and kind not in ('call', 'fptr', 'string', 'import', 'self'):
                    out[tgt].add(s)
    return out


def declarations(name):
    """Tree files declaring `name` as a narrow scalar."""
    p = subprocess.run(['git', '-C', REPO, 'grep', '-l', '-w', '-E', name, '--', '*.h', '*.cpp'],
                       capture_output=True, text=True)
    rx = decl_rx(name)
    return [os.path.join(REPO, f) for f in p.stdout.split() if rx.search(fixlib.read(os.path.join(REPO, f)))]


def widen(name):
    rx = decl_rx(name)

    def sub(m):
        t = re.sub(r'\s+', ' ', m.group('t'))
        return m.group('pre') + WIDE[t] + m.group('post')

    def fn(texts):
        out = {}
        for path, text in texts.items():
            new = rx.sub(sub, text)
            if new != text:
                out[path] = new
        return out or None
    return fn


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', action='store_true')
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
    cands = {}
    for d, (n, same, fns) in sorted(votes(rows, status, model, syms, args.patterns).items()):
        if n <= same:
            continue
        files = declarations(name_of(d))
        if files:
            cands[d] = (n, same, files)
    print('%d globals' % len(cands))
    if not args.apply:
        for d, (n, same, files) in sorted(cands.items()):
            print('  %-60s narrow %d, same %d  %s' % (name_of(d), n, same, ' '.join(os.path.relpath(f, REPO) for f in files)))
        return 0
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    reads = readers(rows, model, syms, set(cands))
    edits = [fixlib.Edit(name_of(d), reads[d], reads[d], widen(name_of(d)), excused_by_match=True)
             for d in sorted(cands)]
    paths = sorted({f for _n, _s, files in cands.values() for f in files})
    failed_before = build_all()                 # the baseline must be current
    base = check.judge(rows, [''], model, syms, quarantined)
    pristine = {p: fixlib.read(p) for p in paths}
    active, state = edits, None
    for rnd in range(ROUNDS):
        active = fixlib.materialise(pristine, active)
        if not active:
            break
        failed = build_all() - failed_before
        state = check.judge(rows, [''], model, syms, quarantined)
        lost = (fixlib.matches(base) - fixlib.matches(state)) | fixlib.vanished(base, state)
        won = fixlib.matches(state) - fixlib.matches(base)
        failed_starts = {r[0] for r in rows if r[3] in failed}
        bad = [e for e in active if e.affects & (lost | failed_starts)
               or (sum(state[s][1] - base[s][1] for s in e.affects if s in state and s in base) < -fixlib.SIM_EPS
                   and not e.affects & won)]
        print('round %d: %d edits, %d TUs failed, %d lost, %d won, %d to drop' % (
            rnd + 1, len(active), len(failed), len(lost), len(won), len(bad)), flush=True)
        if (lost or failed) and not any(e.affects & (lost | failed_starts) for e in bad):
            active = []                      # a loss no edit claims: keep nothing
        if not bad and not failed and active:
            break
        active = [e for e in active if e not in bad]
        state = None
    if state is None:
        fixlib.materialise(pristine, [])
        build_all()
        print('kept 0 of %d edits' % len(edits))
        fixlib.record('fix_globalwidth', len(edits), 0, 0)
        return 0
    gained = len(fixlib.matches(state) - fixlib.matches(base))
    check.update_rows(state)
    for e in active:
        print('  kept %s' % e.label)
    print('kept %d of %d edits, MATCH %+d' % (len(active), len(edits), gained))
    fixlib.record('fix_globalwidth', len(edits), len(active), gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
