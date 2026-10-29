#!/usr/bin/env python3
"""fix_bitmask.py - flag tests through Blizzard's bit-mask table.

    fix_bitmask.py [--apply] [-j N] [PATTERN ...]

Blizzard tests and sets flags through a table of masks, gadwBitMasks[n] ==
1 << n (globals.cpp; Fog checks it at start-up): `dwState & gadwBitMasks[2]`,
`pTxt->dwFlags |= gadwBitMasks[5]`. The original reads the table where VC8
would otherwise fold a constant - `test dword ptr [gadwBitMasks + 8], eax`,
`mov cl, byte ptr [gadwBitMasks + 4]; test byte ptr [eax + 5], cl`
(INPUT_FormMouseWheel, 004fa340; D2Win's Draw, 004fd1f0) - and Ghidra, which
reads the loaded value as the constant it is, writes `dwState & 4`, or for a
flag byte a one-bit bitfield, `if (pMonStats2->isSmall)`.

The sites are taken in a function the original reads the table in at a
constant index n, more often than ours does:

  - `E & K`, `E | K`, `E |= K`, K a power of two 1 << n - a literal, or
    an enumerator or constexpr a header gives that value -, the constant
    standing alone (closed by `)`, `;`, `,`, `?`, `&&`, `||`, `==`, `!=`):
    K becomes gadwBitMasks[n], a primary expression of the same value;
  - a one-bit uint8_t bitfield `X->flag` (or `X.flag`) read where only its
    truth counts - an operand of `!`, `&&`, `||`, `?`, or the whole condition
    of an if or while: it becomes `(*((uint8_t *)X + OFF) & gadwBitMasks[n])`,
    OFF and n from the bitfield's declaration (`/* 0xOFF */ uint8_t flag : 1;`,
    the name declared once in the tree, or once at a byte offset the original
    tests).

Only as many sites per n as the original reads the table at n beyond what
ours already does; a bit with more sites than that is left alone. The value
of every rewritten expression is the same, or, for a bitfield, has the same
truth. Kept where the function comes out more similar.

With --inverse, the companion table gadwInvBitMasks[n] == ~(1 << n): the
original clears a flag with it, `mov edx, dword ptr [gadwInvBitMasks];
and dword ptr [ecx + 8], edx` (D2WINMAIN_SetControlVisible, 004f96f0),
where Ghidra writes `dwState &= -2`. The sites, in a function the original
reads that table at n: `E & K`, `E &= K` with K `~(1 << n)` - `~4`, `~NAME`,
`-5`, `0xfffffffb`, or at 16 or 8 bits `0xfffb`, `0xfb` -, and the clearing
of a one-bit bitfield, `X->flag = 0;`, which becomes `*((uint8_t *)X +
OFF) &= gadwInvBitMasks[n];`. A function that sets the flag it clears reads
both tables and matches only with both rewritten, so its gadwBitMasks sites
join the same edit: one edit per function.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audit            # noqa: E402
import compare          # noqa: E402
import fixlib           # noqa: E402
import ours as ourslib  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

TABLE = '?gadwBitMasks@@3PAIA'
NAME = 'gadwBitMasks'
INVERSE = False
TOKEN = r'(?:0x[0-9a-fA-F]+|\d+|[A-Z_][A-Z0-9_]*\b)[uUlL]*'
INVCONST = re.compile(r'(?<![&|])(?P<op>&=?)\s*(?P<k>~\s*\(\s*' + TOKEN + r'\s*\)|~\s*' + TOKEN +
                      r'|0x[0-9a-fA-F]+[uUlL]*|-\d+)(?=\s*(?:\)|;|,|\?|&&|\|\||==|!=))')
FIELDCLR = re.compile(r'(?<![\w>.])(?P<x>[A-Za-z_]\w*(?:(?:->|\.)[A-Za-z_]\w*|\[[^\[\]]*\])*)(?P<op>->|\.)'
                      r'(?P<f>[A-Za-z_]\w*)\s*=\s*(?:0|false)\s*;')
CONST = re.compile(r'(?<![&|])(?P<op>&|\||\|=)\s*(?P<k>0x[0-9a-fA-F]+|\d+|[A-Z_][A-Z0-9_]*\b)[uUlL]*'
                   r'(?=\s*(?:\)|;|,|\?|&&|\|\||==|!=))')
ENUMERATOR = re.compile(r'^\s*(?:constexpr\s+[\w:]+\s+)?(?P<n>[A-Z_][A-Z0-9_]*)\s*=\s*(?P<v>0x[0-9a-fA-F]+|\d+)'
                        r'[uUlL]*\s*[,;]?\s*(?://.*)?$', re.M)


def enumerators():
    """Name -> value of the enumerators (and constexpr constants) the tree's
    headers give a literal value; names given two values left out."""
    p = subprocess.run(['git', '-C', REPO, 'grep', '-h', '-E',
                        r'^\s*(constexpr\s+[A-Za-z_:0-9]+\s+)?[A-Z_][A-Z0-9_]*\s*=\s*(0x[0-9a-fA-F]+|[0-9]+)',
                        '--', '*.h'], capture_output=True, text=True)
    seen = collections.defaultdict(set)
    for m in ENUMERATOR.finditer(p.stdout):
        seen[m.group('n')].add(int(m.group('v'), 0))
    return {n: next(iter(v)) for n, v in seen.items() if len(v) == 1}
FIELD = re.compile(r'(?P<x>[A-Za-z_]\w*(?:(?:->|\.)[A-Za-z_]\w*|\[[^\[\]]*\])*)(?P<op>->|\.)(?P<f>[A-Za-z_]\w*)\b'
                   r'(?!\s*(?:\(|\[|->|\.|[-+*/%&|^]?=(?!=)|\+\+|--))')
BITDECL = re.compile(r'^\s*/\* 0x(?P<off>[0-9A-Fa-f]+) \*/ uint8_t (?P<name>\w+) : (?P<w>\d+);')
PAD = re.compile(r'^\s*uint8_t\s*: (?P<w>\d+);')


def table_address():
    for line in open(os.path.join(ORACLE, 'evidence', 'datamap.tsv')):
        p = line.split('\t')
        if p[0] == TABLE:
            return int(p[1], 16)
    return None


def bitfields():
    """name -> {(byte offset, bit)} of every one-bit uint8_t bitfield in the tree's headers."""
    hdrs = subprocess.run(['git', '-C', REPO, 'grep', '-l', '-E', r'uint8_t \w+ : 1;', '--', '*.h'],
                          capture_output=True, text=True).stdout.split()
    out = collections.defaultdict(set)
    for h in hdrs:
        off = pos = None
        for line in open(os.path.join(REPO, h), encoding='utf-8', errors='surrogateescape'):
            m = BITDECL.match(line)
            if m:
                o = int(m.group('off'), 16)
                if o != off:
                    off, pos = o, 0
                if m.group('w') == '1' and pos < 8:
                    out[m.group('name')].add((off, pos))
                pos += int(m.group('w'))
                continue
            m = PAD.match(line)
            if m and off is not None:
                pos += int(m.group('w'))
                continue
            if not re.match(r'^\s*(//|$)', line):
                off = None
    return out


def ref_bits(ref, lo):
    """Counter of n the original reads the table at, and the byte offsets it tests with them."""
    bits, offs = collections.Counter(), set()
    for k, i in enumerate(ref):
        for t in i.tokens:
            if t[0] == 'data' and isinstance(t[1], int) and lo <= t[1] < lo + 128 and '*' not in i.text:
                n = (t[1] - lo) // 4
                bits[n] += 1
                if k + 1 < len(ref):
                    m = re.match(r'(?:test|and) byte ptr \[\w+ \+ (0x[0-9a-f]+|\d+)\]', ref[k + 1].text)
                    if m:
                        offs.add((int(m.group(1), 0), n))
    return bits, offs


def our_bits(ours):
    bits = collections.Counter()
    for i in ours:
        for t in i.tokens:
            if t[0] == 'data' and isinstance(t[1], tuple) and t[1][0] == TABLE and '*' not in i.text:
                bits[t[1][1] // 4] += 1
    return bits


def inverse_bit(k, names):
    """n where the constant k clears bit n alone (`~K`, K == 1 << n, or the
    complement written out at 32, 16 or 8 bits), else None."""
    k = re.sub(r'\s+', '', k)
    if k.startswith('~'):
        t = k.lstrip('~').strip('()').rstrip('uUlL')
        v = int(t, 0) if t[0].isdigit() else names.get(t)
        if v is None or v <= 0 or v & (v - 1):
            return None
        return v.bit_length() - 1
    t = k.rstrip('uUlL')
    v = int(t, 0) & 0xffffffff
    w = 32 if v > 0xffff or t.startswith('-') else 16 if v > 0xff else 8
    c = ~v & ((1 << w) - 1)
    if c <= 0 or c & (c - 1):
        return None
    return c.bit_length() - 1


def boolean_at(line, a, b):
    """Whether line[a:b] is read only for its truth."""
    before, after = line[:a].rstrip(), line[b:].lstrip()
    if before.endswith('!') and not before.endswith('!='):
        return True
    if before.endswith('&&') or before.endswith('||') or re.match(r'(&&|\|\||\?)', after):
        return True
    return bool(re.search(r'\b(if|while)\s*\($', before)) and after.startswith(')')


def sites(lines, lo, hi, want, offs, fields, names):
    """[(line index, new line, n)] of the rewrites in lines[lo..hi]."""
    found = collections.defaultdict(list)
    for k in range(lo, hi + 1):
        line = lines[k]
        code = line.split('//', 1)[0]
        if code.lstrip().startswith(('#', '/*', '*')):
            continue
        if INVERSE:
            for m in INVCONST.finditer(code):
                n = inverse_bit(m.group('k'), names)
                if n is not None and n in want:
                    found[n].append((k, m.start('k'), m.end('k'), '%s[%d]' % (NAME, n)))
            for m in FIELDCLR.finditer(code):
                cand = fields.get(m.group('f'))
                if cand and len(cand) > 1:
                    cand = {c for c in cand if c in offs}
                if not cand or len(cand) != 1:
                    continue
                off, n = sorted(cand)[0]
                if n not in want:
                    continue
                x = m.group('x') if m.group('op') == '->' else '&' + m.group('x')
                found[n].append((k, m.start(), m.end(), '*((uint8_t *)%s + 0x%x) &= %s[%d];' % (x, off, NAME, n)))
            continue
        for m in CONST.finditer(code):
            v = int(m.group('k'), 0) if m.group('k')[0].isdigit() else names.get(m.group('k'))
            if v is None or v <= 0 or v & (v - 1) or v.bit_length() - 1 not in want:
                continue
            n = v.bit_length() - 1
            found[n].append((k, m.start('k'), m.end('k') + len(code[m.end('k'):]) - len(code[m.end('k'):].lstrip('uUlL')),
                             'gadwBitMasks[%d]' % n))
        for m in FIELD.finditer(code):
            cand = fields.get(m.group('f'))
            if not cand:
                continue
            if len(cand) > 1:
                cand = {c for c in cand if c in offs}
            bits_ = {c[1] for c in cand}
            if len(cand) != 1 and len(bits_) != 1 or not cand:
                continue
            off, n = sorted(cand)[0]
            if n not in want or not boolean_at(code, m.start(), m.end()):
                continue
            x = m.group('x') if m.group('op') == '->' else '&' + m.group('x')
            found[n].append((k, m.start(), m.end(), '(*((uint8_t *)%s + 0x%x) & gadwBitMasks[%d])' % (x, off, n)))
    out = []
    for n, ss in found.items():
        if len(ss) > want[n]:
            continue
        out.extend((k, a, b, new, n) for k, a, b, new in ss)
    return out


def occurrences(text, word):
    """Starts of `word` in text standing alone (not inside a longer token or a table index)."""
    return [m.start() for m in re.finditer(r'(?<![\w\[>.])' + re.escape(word) + r'(?![\w\]])', text)]


def site_edit(cpp, ln, old, a, b, new):
    # Several sites may share a line, rewritten in any order: this one is the
    # j-th standing occurrence of its text, before and after the others.
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


def chain(fns):
    """One edit applying several site edits in turn."""
    def fn(texts):
        texts = dict(texts)
        for f in fns:
            r = f(texts)
            if r is None:
                return None
            texts.update(r)
        return texts
    return fn


def candidates(rows, status, model, syms, pats, lo):
    """tu -> [(original start, line index, old line, start, end, new text)]."""
    fields = bitfields()
    names = enumerators()
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
            if s is None or s not in model or status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
                continue
            rb, offs = ref_bits(compare.ref_view(s, model[s]), lo)
            if not rb:
                continue
            ob = our_bits(compare.our_view(f, syms))
            want = {n: c - ob[n] for n, c in rb.items() if c > ob[n]}
            if not want:
                continue
            mine = [ln for _o, fn, ln in f.lines if fn.lower().replace('\\', '/').endswith(own)]
            if not mine:
                continue
            if lines is None:
                lines = fixlib.read(cpp).split('\n')
            for k, a, b, new, _n in sites(lines, min(mine) - 1, max(mine) - 1, want, offs, fields, names):
                out[tu].append((s, k, lines[k], a, b, new))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('-j', '--jobs', type=int, default=4)
    ap.add_argument('--inverse', action='store_true',
                    help='clear bits through gadwInvBitMasks instead')
    ap.add_argument('patterns', nargs='*')
    args = ap.parse_args()
    global TABLE, NAME, INVERSE
    rows = audit.load_funcmap(os.path.join(ORACLE, 'evidence', 'funcmap.tsv'))
    status = {}
    for line in open(os.path.join(ORACLE, 'evidence', 'audit.tsv')):
        p = line.rstrip('\n').split('\t')
        if p[0] != 'start' and len(p) > 9:
            status[int(p[0], 16)] = p[9]
    _ref, model = audit.load_context()
    syms = audit.sym_map(rows)
    if args.inverse:
        # A function setting and clearing one flag reads both tables, and
        # matches only with both rewritten: its gadwBitMasks sites go into
        # the same edit as its gadwInvBitMasks ones.
        TABLE, NAME, INVERSE = '?gadwInvBitMasks@@3PAIA', 'gadwInvBitMasks', True
    lo = table_address()
    if lo is None:
        print('0 sites: %s not in evidence/datamap.tsv' % TABLE)
        return 0
    cands = candidates(rows, status, model, syms, args.patterns, lo)
    if args.inverse:
        TABLE, NAME, INVERSE = '?gadwBitMasks@@3PAIA', 'gadwBitMasks', False
        lo = table_address()
        if lo is not None:
            fns = {c[0] for v in cands.values() for c in v}
            for tu, v in candidates(rows, status, model, syms, sorted(cands), lo).items():
                cands[tu].extend(c for c in v if c[0] in fns)
        TABLE, NAME, INVERSE = '?gadwInvBitMasks@@3PAIA', 'gadwInvBitMasks', True
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
        if not args.inverse:
            groups[tu] = ([fixlib.Edit('%s:%d:%d' % (tu, k + 1, a), [s], [s], site_edit(cpp, k, old, a, b, new),
                                       require_gain=True)
                           for s, k, old, a, b, new in cands[tu]], [cpp])
            continue
        per = collections.defaultdict(list)
        for s, k, old, a, b, new in cands[tu]:
            per[s].append(site_edit(cpp, k, old, a, b, new))
        groups[tu] = ([fixlib.Edit('%s:%08x' % (tu, s), [s], [s], chain(fs), require_gain=True)
                       for s, fs in sorted(per.items())], [cpp])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    print('kept %d of %d edits, MATCH %+d' % (kept, total, gained))
    fixlib.record('fix_bitmask' + (' --inverse' if INVERSE else ''), total, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
