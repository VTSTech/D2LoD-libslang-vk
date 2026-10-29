#!/usr/bin/env python3
"""quick.py - the whole measurement after a tree change, doing only what changed.

    quick.py [-j N] [--verify] [--exe-map FILE]

Runs build, map, libmap and audit, each incrementally:

  build    compiles only TUs no stored compile matches (build.py, objcache.py)
  map      skipped when no object, source map or CRT row changed; otherwise
           derived again whole - pairing is global (bracket, align and votes
           read the neighbours) - with what one pair of functions decides
           alone (aligned call votes) kept per pair (ours.PairMemo)
  libmap   skipped when the funcmap, the linked image's map, the libraries and
           our objects are as before; else derived again
  audit    every row again, but a row whose two functions' code and whose
           callees' mapped addresses are as before is not compared again

The global parts - the funcmap, the libmap, the datamap the audit writes -
are always derived from all functions, so the result is the full run's.

Prints the audit summary, the time of each step, and the functions that
gained or lost MATCH against the audit the run replaced (kept as
build/audit.prev.tsv).

--verify runs the incremental measurement, then the full one (ORACLE_NOCACHE=1:
every TU compiled, every object decoded, every function judged), and diffs
the two runs' funcmap, libmap, audit and datamap. Exit status 1 if they differ.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from oracle_config import ORACLE, BUILD, EXPERIMENT  # noqa: E402

EVIDENCE = os.path.join(ORACLE, 'evidence')
OUTPUTS = ('funcmap.tsv', 'funcmap.tsv.unmapped', 'libmap.tsv', 'libmap.tsv.summary',
           'audit.tsv', 'audit.tsv.summary', 'datamap.tsv')


def run(step, args, env=None):
    t0 = time.time()
    p = subprocess.run([sys.executable, '-I', os.path.join(HERE, step)] + args, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    secs = time.time() - t0
    return p.returncode, p.stdout, secs


def pipeline(out, jobs, exe_map, env=None, force=False, log=print):
    """build, map, libmap, audit into directory `out` (evidence/ for the real
    run). Returns {step: seconds}, or raises on a failed step."""
    times = {}
    fm, lm, au = (os.path.join(out, n) for n in ('funcmap.tsv', 'libmap.tsv', 'audit.tsv'))
    steps = [
        ('build', 'build.py', ['-j', str(jobs)] + (['--force'] if force else [])),
        ('map', 'funcmap.py', ['--out', fm]),
        ('libmap', 'libmap.py', ['--funcmap', fm, '--out', lm] + (['--exe-map', exe_map] if exe_map else [])),
        ('audit', 'audit.py', ['--funcmap', fm, '--libmap', lm, '--out', au]),
    ]
    for name, script, args in steps:
        rc, text, secs = run(script, args, env)
        times[name] = secs
        lines = text.splitlines()
        if name == 'build':
            log('build:  %5.1fs  %s' % (secs, ' | '.join(l for l in lines if l.startswith(('translate', 'support', 'compiled'))
                                                         or ' TUs: ' in l)))
        elif name != 'audit':
            log('%-7s %5.1fs  %s' % (name + ':', secs, lines[-1] if lines else ''))
        else:
            log('audit:  %5.1fs' % secs)
        if rc != 0:
            if name == 'build' and 'TUs OK' in text:
                log('  (some TUs failed to compile: build/compile_status.tsv)')
                continue
            sys.stdout.write(text)
            raise SystemExit('%s failed (rc=%d)' % (name, rc))
    return times


def statuses(path):
    out = {}
    if os.path.exists(path):
        for line in open(path):
            p = line.rstrip('\n').split('\t')
            if p[0] != 'start' and len(p) > 9:
                out[p[0]] = (p[2], p[9])
    return out


def gained_lost(prev, cur, limit=40):
    a, b = statuses(prev), statuses(cur)
    gained = sorted(k for k, (_n, st) in b.items() if st == 'MATCH' and a.get(k, ('', ''))[1] != 'MATCH')
    lost = sorted(k for k, (_n, st) in a.items() if st == 'MATCH' and b.get(k, ('', ''))[1] != 'MATCH')
    lines = ['vs previous audit: gained %d, lost %d MATCH' % (len(gained), len(lost))]
    for tag, keys, src in (('GAINED', gained, b), ('LOST', lost, a)):
        for k in keys[:limit]:
            lines.append('  %s %s %s%s' % (tag, k, src[k][0],
                                           '' if tag == 'GAINED' else ' -> ' + b.get(k, ('', 'unmapped'))[1]))
        if len(keys) > limit:
            lines.append('  ... %d more %s' % (len(keys) - limit, tag.lower()))
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-j', '--jobs', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--verify', action='store_true', help='also run in full and diff the two')
    ap.add_argument('--exe-map', default=None, help="libmap.py's --exe-map (default: build-exe/d2recon-vc8.map)")
    args = ap.parse_args()
    if EXPERIMENT:
        sys.exit('quick.py: the baseline only (it writes evidence/)')

    prev = os.path.join(BUILD, 'audit.prev.tsv')
    os.makedirs(BUILD, exist_ok=True)
    if os.path.exists(os.path.join(EVIDENCE, 'audit.tsv')):
        shutil.copyfile(os.path.join(EVIDENCE, 'audit.tsv'), prev)
    t0 = time.time()
    pipeline(EVIDENCE, args.jobs, args.exe_map)
    total = time.time() - t0
    print(open(os.path.join(EVIDENCE, 'audit.tsv.summary')).read(), end='')
    print('\n'.join(gained_lost(prev, os.path.join(EVIDENCE, 'audit.tsv'))))
    print('incremental: %.1fs' % total)
    if not args.verify:
        return 0

    incr = os.path.join(BUILD, 'verify', 'incremental')
    full = os.path.join(BUILD, 'verify', 'full')
    for d in (incr, full):
        os.makedirs(d, exist_ok=True)
    for n in OUTPUTS:
        shutil.copyfile(os.path.join(EVIDENCE, n), os.path.join(incr, n))
    print('\n--verify: the full run (every TU compiled, every object decoded, every function judged)')
    env = dict(os.environ, ORACLE_NOCACHE='1')
    t0 = time.time()
    pipeline(full, args.jobs, args.exe_map, env=env, force=True)
    print('full: %.1fs' % (time.time() - t0))
    differ = []
    for n in OUTPUTS:
        a, b = os.path.join(incr, n), os.path.join(full, n)
        if open(a, 'rb').read() != open(b, 'rb').read():
            differ.append(n)
            subprocess.run(['diff', a, b], stdout=sys.stdout)
    if differ:
        print('VERIFY FAILED: %s differ (%s vs %s)' % (', '.join(differ), incr, full))
        return 1
    print('verify: incremental and full agree on %s' % ', '.join(OUTPUTS))
    return 0


if __name__ == '__main__':
    sys.exit(main())
