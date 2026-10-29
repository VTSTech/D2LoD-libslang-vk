#!/usr/bin/env python3
"""fix_restore.py - give a function back the body it matched with before a regeneration.

    fix_restore.py [--apply] [--chunk K] REV OLD_AUDIT [PATTERN ...]

A regenerated tree loses the source work done on the one before it: the
fixers' edits, and the hand fixes no fixer makes. The fixers are re-run
(fix_all.py); what they do not find again is still in the old branch. For
every original function that was MATCH in OLD_AUDIT (that tree's
evidence/audit.tsv, rows keyed by the original's address, so a function is
found whatever file or name it has now) and is not MATCH here, this takes the
body of its definition in REV - the old tree's commit, in the file the old
audit filed it under - and puts it in place of the body of its definition
here, the signature left as the regenerated tree has it.

The evidence is the oracle's alone: an edit is kept only if the function
then matches (its code is the original's, byte for byte, so whatever
upstream changed in it since was not the original's code), and nothing that
matched stops matching. A definition that is not unique in its file, on
either side, is left alone. --chunk K tries at most K edits per TU at a
time, so one body that no longer compiles does not cost its TU's others.
"""
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audit            # noqa: E402
import check            # noqa: E402
import fixlib           # noqa: E402
from fix_ifelse import match_close  # noqa: E402
from fix_static import def_pattern, git_show  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402


def body_span(text, leaf):
    """(start, end) of the braces of `leaf`'s one definition in `text`, else None."""
    ms = list(re.finditer(def_pattern(leaf), text, re.M))
    if len(ms) != 1:
        return None
    i = ms[0].end() - 1
    j = match_close(text, i, '{', '}')
    return None if j is None else (i, j + 1)


def restored(texts, cpp, leaf, body):
    src = texts[cpp]
    sp = body_span(src, leaf)
    if sp is None or src[sp[0]:sp[1]] == body:
        return None
    return {cpp: src[:sp[0]] + body + src[sp[1]:]}


def load_status(path):
    out = {}
    for line in open(path):
        p = line.rstrip('\n').split('\t')
        if p[0] != 'start' and len(p) > 9:
            out[int(p[0], 16)] = (p[9], p[2], p[3])
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('-j', '--jobs', type=int, default=4)
    ap.add_argument('--chunk', type=int, default=0)
    ap.add_argument('rev')
    ap.add_argument('old_audit')
    ap.add_argument('patterns', nargs='*')
    args = ap.parse_args()
    rows = audit.load_funcmap(os.path.join(ORACLE, 'evidence', 'funcmap.tsv'))
    syms = audit.sym_map(rows)
    now = load_status(os.path.join(ORACLE, 'evidence', 'audit.tsv'))
    old = load_status(args.old_audit)
    olds = {}
    cands = collections.defaultdict(list)
    skipped = collections.Counter()
    for s, _e, name, tu, _sym, _m, _d in rows:
        if args.patterns and not any(p in tu for p in args.patterns):
            continue
        if old.get(s, ('',))[0] != 'MATCH' or now.get(s, ('MATCH',))[0] == 'MATCH':
            continue
        _st, oname, otu = old[s]
        if otu not in olds:
            olds[otu] = git_show(args.rev, os.path.join(REPO, otu + '.cpp'))
        otext = olds[otu]
        sp = body_span(otext, oname.rsplit('::', 1)[-1]) if otext else None
        if sp is None:
            skipped['old definition not found once'] += 1
            continue
        cpp = os.path.join(REPO, tu + '.cpp')
        leaf = name.rsplit('::', 1)[-1]
        if not os.path.exists(cpp) or body_span(fixlib.read(cpp), leaf) is None:
            skipped['new definition not found once'] += 1
            continue
        if restored({cpp: fixlib.read(cpp)}, cpp, leaf, otext[sp[0]:sp[1]]) is None:
            skipped['body already the same'] += 1
            continue
        cands[tu].append((s, leaf, otext[sp[0]:sp[1]]))
    total = sum(len(v) for v in cands.values())
    print('%d functions in %d TUs (skipped: %s)' % (total, len(cands), dict(skipped)), flush=True)
    if not args.apply:
        for tu in sorted(cands):
            print(tu, ', '.join(leaf for _s, leaf, _b in cands[tu]))
        return 0
    _ref, model = audit.load_context()
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    ctx = (rows, model, syms, quarantined)

    def matched(tu, s):
        def test():
            q = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
            return check.judge(rows, [tu], model, syms, q).get(s, ('',))[0] == 'MATCH'
        return test

    chunk = args.chunk or max(len(v) for v in cands.values() or [[]]) or 1
    kept = gained = 0
    for k in range(0, max((len(v) for v in cands.values()), default=0), chunk):
        groups = {}
        for tu in sorted(cands):
            part = cands[tu][k:k + chunk]
            if part:
                cpp = os.path.join(REPO, tu + '.cpp')
                groups[tu] = ([fixlib.Edit(leaf, [s], [s], (lambda c, l, b: lambda t: restored(t, c, l, b))(cpp, leaf, body),
                                           check=matched(tu, s)) for s, leaf, body in part], [cpp])
        n, g = fixlib.apply_batch(groups, ctx, args.jobs)
        kept += n
        gained += g
        print('chunk %d: kept %d, MATCH %+d' % (k // chunk, n, g), flush=True)
    fixlib.record('fix_restore', total, kept, gained, 'bodies from %s' % args.rev)
    print('kept %d of %d, MATCH %+d' % (kept, total, gained))
    return 0


if __name__ == '__main__':
    sys.exit(main())
