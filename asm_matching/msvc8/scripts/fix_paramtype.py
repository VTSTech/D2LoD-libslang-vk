#!/usr/bin/env python3
"""fix_paramtype.py - an int parameter where the original's callers pass a whole register.

    fix_paramtype.py [--apply] [PATTERN ...]

Ghidra types a parameter by what its function reads of it: a function that
looks only at the low byte of an argument gets `uint8_t nState`, the low word
`uint16_t`. Its callers then differ from the original's: for a __fastcall
argument in ECX or EDX, VC8 loads a byte parameter's value into CL or DL
where the original loads the whole register - `mov dl, 5` against `mov edx,
5`, `mov cl, byte ptr [esi + 4]` against `mov ecx, dword ptr [esi + 4]` -
and the callee zero-extends what the original uses as it is. The parameter
was an int (QUEST_SetStateAndBroadcast, 00544300, and its callers in the
quest code).

A site is such a load, aligned with the original's, ahead of a call to a
__fastcall function (mangled `@@YI`) with nothing between them writing that
register: ECX is its first parameter, EDX its second. A parameter the tree
declares uint8_t, int8_t, char, uint16_t, int16_t, short, BYTE or WORD -
not bool, whose conversion is not a truncation - becomes int32_t in the
definition and in the declarations in its own TU and header; a declaration
elsewhere left as it was makes the callers there call another overload,
which their judgement sees.

What the function computes changes only for arguments outside the narrow
type's range, which the original then passed whole as well. The edits are
made at once, the tree rebuilt (most headers are in the precompiled header),
and the function judged with every caller the original has, under its new
mangled name. An edit is dropped when a function it affects stops matching
or no longer compiles, or when the functions it affects come out less
similar in sum and none of them comes to match; the rest is tried again, a
few rounds at most.
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
import check            # noqa: E402
import compare          # noqa: E402
import fixlib           # noqa: E402
import ours as ourslib  # noqa: E402
from fix_fastcall import build_all, callers_of, params_of  # noqa: E402
from fix_static import def_pattern  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

ROUNDS = 3
FULL = {'cl': 'ecx', 'cx': 'ecx', 'dl': 'edx', 'dx': 'edx'}
ARG = {'ecx': 0, 'edx': 1}
NARROW = re.compile(r'^(?P<c>(?:const\s+)?)(?P<t>uint8_t|int8_t|unsigned char|signed char|char|uint16_t|int16_t|'
                    r'unsigned short|short|BYTE|WORD|UCHAR|USHORT)(?P<rest>\s+[A-Za-z_]\w*|\s*)$')
MOV = re.compile(r'^mov\s+(?P<d>\w+),\s*(?P<s>.+)$')
CALL = re.compile(r'^call\s+fn_(?P<a>[0-9a-f]{8})$')


def writes(insn, reg):
    """Whether the instruction writes the 32-bit register reg (or a part of it)."""
    parts = {'ecx': ('ecx', 'cx', 'cl', 'ch'), 'edx': ('edx', 'dx', 'dl', 'dh')}[reg]
    if insn.mnemonic in ('call',) or insn.mnemonic.startswith('j') or insn.mnemonic.startswith('ret'):
        return True
    if insn.mnemonic in ('cmp', 'test', 'push'):
        return False
    ops = insn.text.split(None, 1)
    return len(ops) > 1 and ops[1].split(',')[0].strip() in parts


def sites(ours, ref):
    """[(callee original start, parameter index)] of the narrow loads ahead of calls."""
    sm = difflib.SequenceMatcher(None, [x.mnemonic for x in ours], [y.mnemonic for y in ref], autojunk=False)
    out = set()
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op not in ('equal', 'replace'):
            continue
        for i, j in zip(range(i1, i2), range(j1, j2)):
            a, b = MOV.match(ours[i].text), MOV.match(ref[j].text)
            if not a or not b or FULL.get(a.group('d')) != b.group('d'):
                continue
            reg = b.group('d')
            for t in range(i + 1, min(i + 9, len(ours))):
                c = CALL.match(ours[t].text)
                if c:
                    out.add((int(c.group('a'), 16), ARG[reg]))
                    break
                if writes(ours[t], reg):
                    break
    return out


def retype_params(params, idx):
    """The parameter list text with the entries idx made int32_t, or None."""
    entries = params_of(params)
    if not entries or max(idx) >= len(entries):
        return None
    for k in idx:
        m = NARROW.match(entries[k])
        if not m:
            return None
        entries[k] = m.group('c') + 'int32_t' + m.group('rest')
    return ', '.join(entries)


def retype(text, leaf, idx, definition):
    """`text` with the definition (or every declaration) of `leaf` given int
    parameters at idx; None when there is nothing to change."""
    if definition:
        rx = re.compile(def_pattern(leaf), re.M)
    else:
        rx = re.compile(r'^(?P<head>[A-Za-z_][^;{}()\n]*?\b' + re.escape(leaf) +
                        r'\s*\((?P<params>[^;{}]*)\)[ \t]*);', re.M)
    out, last, changed = [], 0, False
    for m in rx.finditer(text):
        new = retype_params(m.group('params'), idx)
        if new is None:
            continue
        out.append(text[last:m.start('params')] + new)
        last = m.end('params')
        changed = True
        if definition:
            break
    if not changed:
        return None
    out.append(text[last:])
    return ''.join(out)


def edit_fn(cpp, hdr, leaf, idx):
    def fn(texts):
        new = retype(texts[cpp], leaf, idx, True)
        if new is None:
            return None
        decl = retype(new, leaf, idx, False)
        out = {cpp: decl if decl is not None else new}
        if hdr in texts:
            h = retype(texts[hdr], leaf, idx, False)
            if h is not None:
                out[hdr] = h
        return out
    return fn


def candidates(rows, model, status, syms, pats):
    """original start -> sorted parameter indices, from the sites of every unmatched caller."""
    found = collections.defaultdict(set)
    objs = {}
    for s, _e, _name, tu, sym, _m, _d in rows:
        if status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
            continue
        if tu not in objs:
            path = os.path.join(BUILD, 'obj', tu + '.obj')
            objs[tu] = {f.symbol: f for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj')} \
                if os.path.exists(path) else {}
        f = objs[tu].get(sym)
        if f is None:
            continue
        for callee, k in sites(compare.our_view(f, syms), compare.ref_view(s, model[s])):
            found[callee].add(k)
    by_start = {r[0]: r for r in rows}
    out = {}
    for s, idx in found.items():
        r = by_start.get(s)
        if r is None or status.get(s) in (None, 'QUARANTINE', 'MISSING') or '@@YI' not in r[4]:
            continue
        if pats and not any(p in r[3] for p in pats):
            continue
        leaf = r[2].rsplit('::', 1)[-1]
        m = re.search(def_pattern(leaf), fixlib.read(os.path.join(REPO, r[3] + '.cpp')), re.M)
        if m and retype_params(m.group('params'), sorted(idx)) is not None:
            out[s] = sorted(idx)
    return out


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
    cands = candidates(rows, model, status, syms, args.patterns)
    by_start = {r[0]: r for r in rows}
    tu_of = {r[0]: r[3] for r in rows}
    callers = callers_of(model, set(cands), set(tu_of))
    print('%d functions, %d callers' % (len(cands), sum(len(callers[s]) for s in cands)))
    if not args.apply:
        for s, idx in sorted(cands.items()):
            print('  %08x %-36s %s  parameters %s, %d callers' % (
                s, tu_of[s], by_start[s][2].rsplit('::', 1)[-1], ','.join(str(k + 1) for k in idx), len(callers[s])))
        return 0
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    edits, affected_tus = [], set()
    for s, idx in sorted(cands.items()):
        tu, leaf = tu_of[s], by_start[s][2].rsplit('::', 1)[-1]
        cpp, hdr = os.path.join(REPO, tu + '.cpp'), os.path.join(REPO, tu + '.h')
        aff = {s} | callers[s]
        edits.append((fixlib.Edit('%s:%s' % (tu, leaf), [s], aff, edit_fn(cpp, hdr, leaf, idx),
                                  excused_by_match=True), {tu_of[x] for x in aff}, [cpp, hdr]))
        affected_tus |= {tu_of[x] for x in aff}
    pats = sorted(affected_tus)
    failed_before = build_all()                 # the baseline must be current
    base = check.judge(rows, pats, model, syms, quarantined)
    paths = sorted({p for _e, _t, ps in edits for p in ps if os.path.exists(p)})
    pristine = {p: fixlib.read(p) for p in paths}
    active = [e for e, _t, _p in edits]
    tus_of = {e.label: t for e, t, _p in edits}
    state = None
    for rnd in range(ROUNDS):
        active = fixlib.materialise(pristine, active)
        if not active:
            break
        failed = build_all() - failed_before
        # A retyped parameter breaks a typed function pointer or a declaration
        # elsewhere: the TU that fails names the function.
        unclaimed = fixlib.claim_failed(active, tus_of, failed)
        # The new mangled names stand for the original functions, in the callers' references too.
        for e in active:
            s = next(iter(e.owners))
            r = by_start[s]
            path = os.path.join(BUILD, 'obj', r[3] + '.obj')
            if os.path.exists(path):
                for f in ourslib.load_object(os.path.join(BUILD, 'obj'), r[3] + '.obj'):
                    if f.name == r[2] and f.symbol != r[4]:
                        syms[f.symbol] = s
        state = check.judge(rows, pats, model, syms, quarantined)
        lost = (fixlib.matches(base) - fixlib.matches(state)) | fixlib.vanished(base, state)
        won = fixlib.matches(state) - fixlib.matches(base)
        bad = [e for e in active if tus_of[e.label] & failed or e.affects & lost
               or (sum(state[s][1] - base[s][1] for s in e.affects if s in state and s in base) < -fixlib.SIM_EPS
                   and not e.affects & won)]
        print('round %d: %d edits, %d TUs failed (%s), %d lost, %d won, %d to drop' % (
            rnd + 1, len(active), len(failed), ' '.join(sorted(unclaimed)) or 'all claimed',
            len(lost), len(won), len(bad)), flush=True)
        for e in bad:
            print("  drop %s: %s" % (e.label, " ".join("%08x" % x for x in sorted(e.affects & lost)[:6])), flush=True)
        if lost and not any(e.affects & lost for e in bad) or unclaimed and not bad:
            active = []                      # a loss or failure no edit claims: keep nothing
        if not bad and not failed and active:
            break
        active = [e for e in active if e not in bad]
        state = None
    if state is None:
        fixlib.materialise(pristine, [])
        build_all()
        print('kept 0 of %d edits' % len(edits))
        fixlib.record('fix_paramtype', len(edits), 0, 0)
        return 0
    gained = len(fixlib.matches(state) - fixlib.matches(base))
    check.update_rows(state)
    for e in active:
        print('  kept %s' % e.label)
    print('kept %d of %d edits, MATCH %+d' % (len(active), len(edits), gained))
    fixlib.record('fix_paramtype', len(edits), len(active), gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
