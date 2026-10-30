#!/usr/bin/env python3
"""audit.py - compare every mapped function with the original.

    audit.py [--funcmap evidence/funcmap.tsv] [--out evidence/audit.tsv]

One row per mapped function, sorted by original address:

  start end name tu method ref_insns our_insns byte_diffs similarity status notes

`method` is the funcmap tier that paired the two (an 'align' row is a
similarity guess, a 'string' row strong evidence). `similarity` is the
difflib ratio of the two normalised instruction listings, the progress
measure for a function that does not match yet. Status classes: see
compare.py. A function whose body build.py quarantined (the compiler could not build it,
and blanking it let the rest of its TU compile) is reported as QUARANTINE
with the compiler's message. A summary goes to <out>.summary.

Never quote counts from this file in prose; they go stale. Read the file.
"""
import argparse
import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare       # noqa: E402
import ours as ourslib  # noqa: E402
import refimage      # noqa: E402

from oracle_config import ORACLE, REPO, BUILD, REFCACHE, EXPERIMENT, VC, CFLAGS  # noqa: E402


def load_funcmap(path):
    rows = []
    for line in open(path):
        if line.startswith('start\t'):
            continue
        s, e, name, tu, sym, method, detail = line.rstrip('\n').split('\t')
        rows.append((int(s, 16), int(e, 16), name, tu, sym, method, detail))
    return rows


def embedded_address(name):
    """The address a Ghidra-derived name ends in (FOO_00512340, FOO_512340), if any."""
    import re
    m = re.search(r'_(00[4-9a-f][0-9a-f]{5}|[4-9a-f][0-9a-f]{5})(?:@|$)', name)
    return int(m.group(1), 16) if m else None


def load_quarantine(path):
    """(tu, function name) -> the compile error that quarantined it."""
    out = {}
    if os.path.exists(path):
        for line in open(path):
            parts = line.rstrip('\n').split('\t')
            if len(parts) >= 5 and parts[0] != 'file':
                tu = parts[0][:-4] if parts[0].endswith('.cpp') else parts[0]
                out[(tu, parts[3].rsplit('::', 1)[-1])] = parts[4]
    return out


def load_context():
    ref = refimage.Reference(os.path.join(ORACLE, 'reference', 'Game.exe'))
    ref.load(os.path.join(REFCACHE, 'functions.tsv'))
    model = ref.model(os.path.join(REFCACHE, 'model.pkl'))
    return ref, model


class SymMap(dict):
    """Our symbol -> original start: the funcmap's unambiguous symbols, and,
    for a symbol defined in no object of ours, the CRT function crtmap.tsv
    names (so a call to _toupper is checked against the original's)."""

    def __init__(self, base, crt):
        super().__init__(base)
        self.crt = crt

    def __missing__(self, sym):
        import objfuncs
        return self.crt.get(objfuncs.qualified(sym))

    def get(self, sym, default=None):
        v = self[sym]
        return default if v is None else v


def sym_map(rows, librows=None):
    """Our symbol -> original start, for symbols that are unambiguous: the
    funcmap's, then the library functions evidence/libmap.tsv pairs (a call
    to _fclose is checked against the original's fclose), then crtmap.tsv's
    names."""
    import funcmap
    seen = collections.defaultdict(set)
    for s, _e, _n, _tu, sym, _m, _d in rows:
        seen[sym].add(s)
    base = {k: next(iter(v)) for k, v in seen.items() if len(v) == 1}
    if librows is None:
        librows = load_funcmap(LIBMAP) if os.path.exists(LIBMAP) else []
    lib = collections.defaultdict(set)
    for s, _e, _n, _tu, sym, _m, _d in librows:
        lib[sym].add(s)
    for k, v in lib.items():
        if k not in seen and len(v) == 1:
            base[k] = next(iter(v))
    return SymMap(base, funcmap.load_crtmap(os.path.join(ORACLE, 'evidence', 'crtmap.tsv')))


LIBMAP = os.path.join(ORACLE, 'evidence', 'libmap.tsv')
RUNTIME = os.path.join(ORACLE, 'evidence', 'runtime.tsv')


def runtime_starts(model, librows, path=RUNTIME):
    """The original functions that belong to the statically linked C runtime.

    Four sources, each evidence on its own: the functions libmap.tsv pairs with
    a libcmt.lib function; everything in the run of library code those pairs
    span (the linker lays the library objects out after the game's); the import
    thunks the linker writes (jmp [__imp_X]); and runtime.tsv, the runtime
    functions Ghidra places outside that run (the database's `runtime` tag:
    libcmt code the linker put first, and what only the runtime reaches).
    """
    import libmap
    out = set()
    lo = hi = None
    for s, e, *_rest in librows:
        out.add(s)
        lo = s if lo is None else min(lo, s)
        hi = e if hi is None else max(hi, e)
    if lo is not None:
        out.update(s for s in model if lo <= s < hi)
    out.update(s for s in model if libmap.is_import_thunk(model[s]))
    if os.path.exists(path):
        for line in open(path):
            if line.startswith('start\t') or not line.strip():
                continue
            s = int(line.split('\t', 1)[0], 16)
            if s in model:
                out.add(s)
    return out, (lo or 0, hi or 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--funcmap', default=os.path.join(ORACLE, 'evidence', 'funcmap.tsv'))
    ap.add_argument('--libmap', default=LIBMAP, help='library pairs (libmap.py); judged like the rest')
    ap.add_argument('--out', default=None)
    args = ap.parse_args()
    if args.out is None:
        # An experiment is audited with the baseline's pairs, and never into evidence/.
        args.out = os.path.join(BUILD, 'audit.tsv') if EXPERIMENT else os.path.join(ORACLE, 'evidence', 'audit.tsv')
    elif EXPERIMENT and os.path.realpath(os.path.dirname(args.out)) == os.path.realpath(os.path.join(ORACLE, 'evidence')):
        sys.exit('refusing to write an experiment audit into evidence/')

    ref, model = load_context()
    rows = load_funcmap(args.funcmap)
    fns = {f.key: f for f in ourslib.load_all(os.path.join(BUILD, 'obj'))}
    librows = load_funcmap(args.libmap) if os.path.exists(args.libmap) else []
    if librows:
        import libmap
        fns.update((f.key, f) for f in libmap.load_lib_functions())
    syms = sym_map(rows, librows)
    quarantined = load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    out = []
    statuses = collections.Counter()
    memo = ourslib.PairMemo('audit', BUILD)
    wiring = collections.defaultdict(collections.Counter)   # our global -> implied original address
    for s, e, name, tu, sym, method, _d in rows + librows:
        f = fns.get(tu + '|' + sym)
        if f is None or s not in model:
            out.append((s, e, name, tu, method, 0, 0, 0, 0.0, 'MISSING', 'object not compiled'))
            statuses['MISSING'] += 1
            continue
        q = quarantined.get((tu, name.rsplit('::', 1)[-1]))
        if q is not None:
            # Its body was blanked so that the rest of its TU compiles: nothing to judge.
            out.append((s, e, name, tu, method, len(model[s][0].insns), 0, 0, 0.0, 'QUARANTINE', q))
            statuses['QUARANTINE'] += 1
            continue
        # A library pair may join carved pieces (libmap.py); a funcmap row's
        # end is its carve's, which makes this ref_view.
        # A judgement depends on the two functions' code and on where the
        # functions ours calls are mapped: kept across runs by exactly that.
        extra = (e, tuple(syms.get(t) for _o, kind, t in f.refs if kind in ('call', 'fptr')))
        v = memo.get(f, s, extra)
        if v is None:
            r = compare.compare(compare.our_view(f, syms), compare.ref_view_span(s, e, model))
            v = memo.put(f, s, (r.ref_insns, r.our_insns, r.byte_diffs, r.similarity, r.status, r.ref_diffs,
                                r.unverified, tuple(r.wiring)), extra)
        ref_insns, our_insns, byte_diffs, similarity, status, ref_diffs, unverified, rwiring = v
        for sym, addr in rwiring:
            wiring[sym][addr] += 1
        notes = []
        if ref_diffs:
            notes.append('%d refs differ' % ref_diffs)
        if unverified:
            notes.append('%d refs unverified' % unverified)
        out.append((s, e, name, tu, method, ref_insns, our_insns, byte_diffs, similarity, status,
                    '; '.join(notes)))
        statuses[status if not status.startswith('OUR') else 'OUR+-N'] += 1
    memo.save()
    out.sort()
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, 'w') as fh:
        fh.write('start\tend\tname\ttu\tmethod\tref_insns\tour_insns\tbyte_diffs\tsimilarity\tstatus\tnotes\n')
        for row in out:
            fh.write('%08x\t%08x\t%s\t%s\t%s\t%d\t%d\t%d\t%.3f\t%s\t%s\n' % row)
    total_ref = len(model)
    ref_bytes = sum(model[s][0].end - s for s in model)
    match_bytes = sum(r[1] - r[0] for r in out if r[9] == 'MATCH')
    import subprocess
    tree = subprocess.run(['git', '-C', REPO, 'describe', '--always', '--dirty', '--tags'],
                          capture_output=True, text=True).stdout.strip() or 'unknown'
    table = [l.split('\t')[:2] for l in open(os.path.join(ORACLE, 'flags.tsv'))
             if l.strip() and not l.startswith('#')] if os.path.exists(os.path.join(ORACLE, 'flags.tsv')) else []
    lines = ['tree %s; compiler %s; flags %s%s' % (tree, VC, CFLAGS,
                                                 ''.join('; %s %s' % (p, f) for p, f in table)),
             'original functions: %d (%d bytes of code)' % (total_ref, ref_bytes),
             'mapped and compared: %d' % len(out)]
    for k in ('MATCH', 'REF_DIFF', 'INSN_MATCH', 'INSN_COUNT_MATCH', 'OUR+-N', 'QUARANTINE', 'MISSING'):
        lines.append('  %-17s %6d' % (k, statuses.get(k, 0)))
    # The headline: the game without the statically linked runtime, which is
    # Microsoft's, linked rather than reconstructed, so it is out of both the
    # matched and the total; the whole binary follows as the second figure.
    runtime, (rlo, rhi) = runtime_starts(model, librows)
    game = [s for s in model if s not in runtime]
    game_bytes = sum(model[s][0].end - s for s in game)
    game_out = [r for r in out if r[0] not in runtime]
    game_match = [r for r in game_out if r[9] == 'MATCH']
    game_match_bytes = sum(r[1] - r[0] for r in game_match)
    lines[1:1] = ['Without the C runtime: %d of %d functions MATCH (%.2f%%), %d of %d bytes (%.2f%%)' % (
        len(game_match), len(game), 100.0 * len(game_match) / max(1, len(game)),
        game_match_bytes, game_bytes, 100.0 * game_match_bytes / max(1, game_bytes)),
        '  runtime left out: %d functions (%d bytes): libmap pairs, the library run %08x..%08x, import thunks, %s' % (
        len(runtime), ref_bytes - game_bytes, rlo, rhi, os.path.basename(RUNTIME)),
        'whole binary:']
    lines.append('matched code: %d bytes (%.2f%% of the original)' % (match_bytes, 100.0 * match_bytes / ref_bytes))
    libtus = {r[3] for r in librows}
    lib = [r for r in out if r[3] in libtus]
    lib_match = [r for r in lib if r[9] == 'MATCH']
    lines.append('  of which the statically linked runtime (%s): %d compared (%d bytes), %d MATCH (%d bytes)' % (
        os.path.basename(args.libmap), len(lib), sum(r[1] - r[0] for r in lib), len(lib_match),
        sum(r[1] - r[0] for r in lib_match)))
    by_method = collections.Counter(r[4] for r in out)
    lines.append('pairings by funcmap tier: ' + ', '.join('%s %d' % kv for kv in by_method.most_common()))
    # Globals: each of ours must imply one original address wherever it is used.
    dm = os.path.join(os.path.dirname(args.out), 'datamap.tsv')
    consistent = 0
    with open(dm, 'w') as fh:
        fh.write('symbol\taddress\tuses\tother_addresses\n')
        for sym in sorted(wiring):
            (addr, n), = wiring[sym].most_common(1)
            others = ['%08x:%d' % (a, k) for a, k in wiring[sym].most_common() if a != addr]
            consistent += not others
            fh.write('%s\t%08x\t%d\t%s\n' % (sym, addr, n, ','.join(others)))
    lines.append('globals seen at aligned instructions: %d, of which consistent %d (%s)' % (
        len(wiring), consistent, os.path.basename(dm)))
    # Self-check: a generated name that carries its Ghidra address must land there.
    agree, disagree = 0, []
    for s0, _e, name, _tu, _m, *_rest in out:
        a = embedded_address(name)
        if a is not None and 0x401000 <= a < 0x6cc000:
            if a == s0:
                agree += 1
            else:
                disagree.append('%s at %08x' % (name, s0))
    for sym in wiring:
        a = embedded_address(sym)
        if a is not None and a >= 0x6cc000:
            if wiring[sym].most_common(1)[0][0] == a:
                agree += 1
            else:
                disagree.append('%s at %08x' % (sym, wiring[sym].most_common(1)[0][0]))
    # A disagreement is a place to look, not necessarily a wrong map: a global
    # is implied by its position, so code laid out differently around it (an
    # if/else in the other order) moves it as surely as a wrong global does.
    lines.append('self-check, names carrying their own address: %d agree, %d elsewhere%s' % (
        agree, len(disagree), ''.join('\n  look at: ' + d for d in disagree[:20])))
    with open(args.out + '.summary', 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
