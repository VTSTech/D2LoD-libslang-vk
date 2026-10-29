#!/usr/bin/env python3
"""fix_idioms.py - write the decompiler's arithmetic idioms back as the operators they were.

    fix_idioms.py [--apply] [--idiom NAME] [PATTERN ...]

Ghidra decompiles what VC8 made of an operator, and sometimes cannot fold it
back. Each idiom below is a textual rewrite with the same value, for every
input, as what it replaces (the docstring of each says why), applied to the
lines of a function that does not match yet - one edit per function, kept
through fixlib's batch loop only where the oracle confirms it.

  seed_mod   `s.nSeedLow + (int)(((uint64_t)d2_bits_of(s) & 0xffffffff) / N) * -N`
             is `s.nSeedLow % N`: D2SeedStrc's low dword is nSeedLow (uint32_t,
             offset 0), the quotient times N never exceeds it, and the sum is
             uint32_t either way. Written with the 64-bit cast, VC8 divides by
             calling _aulldiv; the original's 32-bit `%` is a multiply by a
             reciprocal (MONSTER_FindAndChaseNearestEnemy and 160 more).
  seed_divides
             `s.nSeedLow != (int)(((uint64_t)d2_bits_of(s) & 0xffffffff) / N) * N`
             is `s.nSeedLow % N != 0`, and `==` is `== 0`: the same identity.
  range      `(uint32_t)(x - C) <= K` is `(x >= C && x <= C + K)` for every
             32-bit x (below C the subtraction wraps past K). Ghidra folds a
             range check - or a switch over consecutive cases - into the
             subtraction, and VC8 compiles the two forms differently: the
             range check as `add eax, -C`, the subtraction of a call's result
             as `sub eax, C` (CLIENT_CheckWorldstoneShake, 0046bf30). Kept
             only where the function then comes out more similar: the
             subtraction may be what was written.

  alldiv     `__alldiv((uint32_t)E, (uint32_t)((uint64_t)E >> 0x20), D, H)` is
             `(int64_t)(E) / D`: d2_platform.h's helper puts E's 64 bits back
             together from its two dwords and the divisor from (D, H) - (K, 0)
             is K, (n, (int)n >> 0x1f) is n sign-extended, (n, 0) n
             zero-extended - and divides. The same for __aulldiv
             (`(uint64_t)(E) / D`), __allrem and __aullrem (`%`). E is
             evaluated once instead of twice, so it must not call or assign
             anything. Ghidra writes the original's call to the runtime's
             64-bit division this way; VC8 inlines the helper's reassembly
             where the original pushes the halves (D2ApplyPercent, 00483360).

  seed_rem   `(T)(((uint64_t)d2_bits_of(s) & 0xffffffff) % (uint64_t)n)` is
             `(T)(s.nSeedLow % n)` for a 32-bit T: the low dword is nSeedLow,
             and the remainder of a 32-bit value by a 32-bit divisor fits 32
             bits. Ghidra writes the original's `xor edx, edx; div` this way,
             a 64-bit division of a zero-extended dividend; compiled as
             written, VC8 calls _aullrem where the original divides inline
             (UNIT_GetModuloFromSeed, 0045c390, the inlined
             SEED_RollLimitedRandomNumber). Exact where n is unsigned or
             positive; a negative n the original converts to unsigned, as
             `%` with an unsigned operand does - kept only where the code
             comes out more similar, that is where the original divides as
             the rewrite does.
  seed_div   `(T)(((uint64_t)d2_bits_of(s) & 0xffffffff) / d)` is
             `(T)(s.nSeedLow / d)`: the same for the quotient, d a constant
             or a `(uint64_t)` operand as for seed_rem.
  seed_bit   `(uint64_t)d2_bits_of(s) & K`, K below 2^32, is `s.nSeedLow & K`
             where only its truth is used (an if or loop condition, an
             operand of `!`, `&&`, `||` or a `?:` condition): the high
             dword is masked off either way.
  pow2div    `(int)(x + (x >> 0x1f & 3U)) >> 2` is `x / 4`, for every 32-bit
             x and every power of two 2^n with the mask 2^n-1 (`(int)x >>
             0x1f` for an unsigned x makes it `(int)x / 4`): a negative x is
             biased by 2^n-1 so the arithmetic shift rounds toward zero, as
             `/` does; a non-negative x gets nothing added. It is VC8's own
             lowering of a signed division by 2^n, which Ghidra prints
             unfolded; compiled as written, VC8 copies x for the sign
             (`mov edx, eax; sar edx, 0x1f`) where the original's `/` has
             `cdq` (DRAW_UnitUnderMouse, 00454f30: ScreenSizeX / 4). x
             appears twice in the idiom and once in the rewrite, so it
             must not call or assign anything; the shift must be an
             operand of its own.
  maskcast   `x & 0xff` is `(uint8_t)x`, `x & 0xffff` `(uint16_t)x`: both
             keep the low byte (word) of x and zero the rest. Ghidra prints
             the original's zero extension - a cast to a byte or a word - as
             the mask; compiled as written, VC8 masks (`and eax, 0xff`)
             where the original zero-extends (`movzx eax, al`; SFILE_Hash,
             00415330: `nUpper & 0xff`). The cast's type is int where x's
             may be unsigned, which a comparison or division using the
             value could tell apart: kept only where the code comes out
             more similar. x must be the whole left operand of `&`.

An idiom marked `gain` is kept only where an owner comes out more similar
(fixlib's require_gain); the others where nothing gets worse.
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

LVALUE = r'[A-Za-z_][\w]*(?:(?:\.|->)[A-Za-z_]\w*|\[[^\[\]]*\])*'
SCOPED = r'[A-Za-z_][\w]*(?:::[A-Za-z_]\w*)*(?:(?:\.|->)[A-Za-z_]\w*|\[[^\[\]]*\])*'
NUM = r'0x[0-9a-fA-F]+|\d+'


def _range(m):
    x, c, op, k = m.group('x'), m.group('c'), m.group('op'), m.group('k')
    kv = int(k, 0)
    if re.fullmatch(NUM, c):
        hi = int(c, 0) + kv
        top = ('0x%x' % hi) if c.lower().startswith('0x') else str(hi)
    else:
        top = '%s + %s' % (c, k)
    return '(%s >= %s && %s %s %s)' % (x, c, x, op, top)


class _Call:
    """A match of a call idiom: what fix_idioms' rewrite needs of a re match."""

    def __init__(self, string, start, end, new):
        self.string, self._start, self._end, self.new = string, start, end, new

    def start(self):
        return self._start

    def end(self):
        return self._end

    def group(self, _k=0):
        return self.string[self._start:self._end]


def _args(text, open_at):
    """(arguments, index of the closing paren) of the call opening at text[open_at]."""
    depth, cur, out = 0, '', []
    for k in range(open_at, len(text)):
        c = text[k]
        if c in '([':
            depth += 1
            if depth == 1:
                continue
        elif c in ')]':
            depth -= 1
            if depth == 0:
                out.append(cur.strip())
                return out, k
        elif c == ',' and depth == 1:
            out.append(cur.strip())
            cur = ''
            continue
        cur += c
    return None, None


def _bare(e):
    """e without parentheses around the whole of it."""
    e = e.strip()
    while e.startswith('(') and _args(e, 0)[1] == len(e) - 1 and len(_args(e, 0)[0]) == 1:
        e = e[1:-1].strip()
    return e


def _operand(e):
    """e as the operand of a cast: parenthesised unless it is a plain name."""
    return e if re.fullmatch(SCOPED, e) else '(%s)' % e


PURE = re.compile(r'\+\+|--|(?<![=!<>])=(?!=)|\b(?!sizeof\b)[A-Za-z_]\w*\s*\(')
INT = re.compile(r'(?:0x[0-9a-fA-F]+|\d+)[uU]?')


class LongDivision:
    """__alldiv / __aulldiv / __allrem / __aullrem of one 64-bit expression's
    two halves, written back as the operator (see `alldiv` above)."""
    CALL = re.compile(r'\b__(all|aull)(div|rem)\(')

    def search(self, line):
        return next(self._matches(line), None)

    def sub(self, fn, line):
        out, last = [], 0
        for m in self._matches(line):
            out.append(line[last:m.start()] + fn(m))
            last = m.end()
        return ''.join(out) + line[last:]

    def _matches(self, line):
        for c in self.CALL.finditer(line):
            args, close = _args(line, c.end() - 1)
            if not args or len(args) != 4:
                continue
            lo = re.fullmatch(r'\(uint32_t\)(.+)', args[0], re.S)
            hi = re.fullmatch(r'\(uint32_t\)\(\(uint64_t\)(.+) >> 0x20\)', args[1], re.S)
            if not lo or not hi or _bare(lo.group(1)) != _bare(hi.group(1)) or PURE.search(lo.group(1)):
                continue
            e, d, h = _bare(lo.group(1)), args[2], args[3]
            signed = c.group(1) == 'all'
            sign = re.fullmatch(r'\(int\)(.+) >> 0x1f', h)
            if h == '0' and INT.fullmatch(d):
                div = d
            elif h == '0' and not PURE.search(d):
                div = '(%s)(uint32_t)%s' % ('int64_t' if signed else 'uint64_t', _operand(_bare(d)))
            elif sign and _bare(sign.group(1)) == _bare(d) and not PURE.search(d):
                div = '(int)%s' % _operand(_bare(d))
            else:
                continue
            op = '/' if c.group(2) == 'div' else '%'
            new = '(%s)%s %s %s' % ('int64_t' if signed else 'uint64_t', _operand(e), op, div)
            yield _Call(line, c.start(), close + 1, new)


SEED_LO = r'\(\(uint64_t\)d2_bits_of\((?P<s>' + LVALUE + r')\) & 0xffffffff\)'
CAST32 = r'\((?P<t>u?int(?:32|16|8)_t|int|unsigned int|unsigned|DWORD|WORD|BYTE|char|short|uint)\)'
# A divisor: `(uint64_t)` of a name, of a cast name or of a parenthesised
# expression - or a constant.
DIVISOR = (r'(?:\(uint64_t\)(?P<v>(?:\(\w+\))?' + SCOPED + r'|\((?:[^()]|\([^()]*\))*\))|(?P<c>' + NUM + r'))')


def _seed_arith(op):
    def fn(m):
        return '(%s)(%s.nSeedLow %s %s)' % (m.group('t'), m.group('s'), op, m.group('v') or m.group('c'))
    return fn


class SeedBit:
    """`(uint64_t)d2_bits_of(s) & K` where only its truth is used (see
    `seed_bit` above)."""
    RX = re.compile(r'\(uint64_t\)d2_bits_of\((?P<s>' + LVALUE + r')\) & (?P<k>' + NUM + r')(?![\w.])')
    BEFORE = re.compile(r'(?:\b(?:if|while) \(|!\(|&& |\|\| )$')
    AFTER = re.compile(r'^(?:\)|\s*(?:&&|\|\||\?))')

    def search(self, line):
        return next(self._matches(line), None)

    def sub(self, fn, line):
        out, last = [], 0
        for m in self._matches(line):
            out.append(line[last:m.start()] + fn(m))
            last = m.end()
        return ''.join(out) + line[last:]

    def _matches(self, line):
        for m in self.RX.finditer(line):
            if int(m.group('k'), 0) >= 1 << 32:
                continue
            if not self.BEFORE.search(line[:m.start()]) or not self.AFTER.match(line[m.end():]):
                continue
            yield _Call(line, m.start(), m.end(), '%s.nSeedLow & %s' % (m.group('s'), m.group('k')))


class Pow2Division:
    """`(int)(x + (x >> 0x1f & 2^n-1)) >> n` written back as `x / 2^n` (see
    `pow2div` above)."""
    TAIL = re.compile(r' >> (?P<n>' + NUM + r')(?![\w.])')
    MASK = re.compile(r'(?P<b>.+) >> 0x1f & (?P<m>' + NUM + r')(?P<u>[uU]?)', re.S)
    # The shift must be an operand of its own: after and before nothing that
    # binds more tightly than `>>` (a cast, `+`, `-`, `<<`, `>>`, ...).
    BEFORE = re.compile(r'(?:^|[(,=?:{\[;]|\breturn|(?<![<>])[<>]=?|[=!]=|&&|\|\||(?<!&)&|\||\^)\s*$')
    AFTER_BAD = re.compile(r'^\s*(?:[*/%.\[(+\-]|->)')
    # What may stand at the top level of x, the left operand of that `+`.
    LOOSE = re.compile(r'<<|>>|[<>=!?&|^,]')

    def search(self, line):
        return next(self._matches(line), None)

    def sub(self, fn, line):
        out, last = [], 0
        for m in self._matches(line):
            if m.start() < last:
                continue
            out.append(line[last:m.start()] + fn(m))
            last = m.end()
        return ''.join(out) + line[last:]

    @staticmethod
    def _top(e):
        """e with everything inside parentheses blanked."""
        out, depth = [], 0
        for c in e:
            if c in '([':
                depth += 1
            out.append(c if depth == 0 else ' ')
            if c in ')]':
                depth -= 1
        return ''.join(out)

    @staticmethod
    def _uncast(e):
        """e without an `(int)` cast of the whole of it."""
        e = _bare(e)
        rest = e[len('(int)'):]
        if e.startswith('(int)') and (re.fullmatch(SCOPED, rest) or
                                      (rest.startswith('(') and _args(rest, 0)[1] == len(rest) - 1)):
            return _bare(rest)
        return e

    def _matches(self, line):
        for k in range(len(line)):
            if line[k] != '(' or line.startswith('(int)', k):
                continue
            args, close = _args(line, k)
            if not args or len(args) != 1:
                continue
            tail = self.TAIL.match(line, close + 1)
            content = args[0]
            if not tail or not content.endswith(')'):
                continue
            depth, g = 0, None                   # the group closing the content
            for j in range(len(content) - 1, -1, -1):
                depth += {')': 1, ']': 1, '(': -1, '[': -1}.get(content[j], 0)
                if depth == 0:
                    g = j
                    break
            if g is None or not content[:g].endswith(' + '):
                continue
            x = content[:g - 3].strip()
            mask = self.MASK.fullmatch(content[g + 1:-1])
            n = int(tail.group('n'), 0)
            if not mask or not x or not 0 < n < 31 or int(mask.group('m'), 0) != (1 << n) - 1:
                continue
            b = mask.group('b')
            if self.LOOSE.search(self._top(b).replace('->', '  ')) or \
                    self.LOOSE.search(self._top(x).replace('->', '  ')) or \
                    self._uncast(b) != self._uncast(x) or PURE.search(x):
                continue
            start = k - len('(int)') if line[:k].endswith('(int)') else k
            # Without the cast the sum must be signed: no `U`, no cast of x.
            signed = start < k or (not mask.group('u') and not _bare(b).startswith('(int)'))
            if not signed or not self.BEFORE.search(line[:start]) or self.AFTER_BAD.match(line[tail.end():]):
                continue
            xs = self._uncast(x)
            cast = start < k or _bare(b).startswith('(int)')
            new = '%s%s / %d' % ('(int)' if cast else '', _operand(xs), 1 << n)
            yield _Call(line, start, tail.end(), new)


class MaskCast:
    """`x & 0xff` / `x & 0xffff` written back as the cast `(uint8_t)x` /
    `(uint16_t)x` (see `maskcast` above)."""
    RX = re.compile(r'(?P<x>' + SCOPED + r'|\((?:[^()]|\((?:[^()]|\([^()]*\))*\))*\)) & '
                    r'(?P<m>0xff|0xffff|255|65535)(?P<u>[uU]?)(?![\w.])')
    # x must be the whole left operand of `&`: after nothing binding more
    # tightly (no cast, `+`, `<<`, `==`, ...), and the `&` before nothing
    # binding more tightly either.
    BEFORE = re.compile(r'(?:^|[(,?:{\[;]|\breturn|&&|\|\||(?<![&|^=!<>+\-*/%])[&|^]=?|(?<![<>=!+\-*/%&|^])=)\s*$')
    AFTER = re.compile(r'^\s*(?:[),;:?\]]|&&|\|\||&(?!&)|\||\^|$)')

    def search(self, line):
        return next(self._matches(line), None)

    def sub(self, fn, line):
        out, last = [], 0
        for m in self._matches(line):
            out.append(line[last:m.start()] + fn(m))
            last = m.end()
        return ''.join(out) + line[last:]

    def _matches(self, line):
        for m in self.RX.finditer(line):
            x = m.group('x')
            pre = line[:m.start()]
            if not self.BEFORE.search(pre) or not self.AFTER.match(line[m.end():]):
                continue
            if x.startswith('(') and re.search(r'\b(?:if|while|for|switch|sizeof|return)\s*$|[\w\]]\s*$', pre):
                continue                      # a call's or a statement's parentheses, not a group
            if x.startswith('(') and _args(x, 0)[1] != len(x) - 1:
                continue
            if re.fullmatch(SCOPED, x) and re.fullmatch(r'\d\w*|0x\w+|[A-Z_][A-Z0-9_]*', x):
                continue                      # a constant expression, not a variable
            t = 'uint8_t' if int(m.group('m'), 0) == 0xff else 'uint16_t'
            start, end = m.start(), m.end()
            if pre.endswith('(') and line[end:end + 1] == ')' and \
                    not re.search(r'(?:[\w\]\)]|\b(?:if|while|for|switch|sizeof|return))\s*\($', pre):
                start, end = start - 1, end + 1   # the group's own parentheses go: a cast needs none
            yield _Call(line, start, end, '(%s)%s' % (t, x))


IDIOMS = [
    ('seed_mod',
     re.compile(r'(?P<s>' + LVALUE + r')\.nSeedLow \+ \(int\)\(\(\(uint64_t\)d2_bits_of\((?P=s)\) & 0xffffffff\) / '
                r'(?P<n>' + NUM + r')\) \* -(?P=n)'),
     lambda m: '%s.nSeedLow %% %s' % (m.group('s'), m.group('n'))),
    ('seed_divides',
     re.compile(r'(?P<s>' + LVALUE + r')\.nSeedLow (?P<op>!=|==) \(int\)\(\(\(uint64_t\)d2_bits_of\((?P=s)\) & 0xffffffff\) / '
                r'(?P<n>' + NUM + r')\) \* (?P=n)(?![\w.])'),
     lambda m: '%s.nSeedLow %% %s %s 0' % (m.group('s'), m.group('n'), m.group('op'))),
    ('range',
     re.compile(r'\(uint32_t\)\((?P<x>' + SCOPED + r') - (?P<c>' + SCOPED + r'|' + NUM + r')\) '
                r'(?P<op><=|<) (?P<k>' + NUM + r')(?![\w.])'),
     _range, 'gain'),
    ('alldiv', LongDivision(), lambda m: m.new),
    ('seed_rem', re.compile(CAST32 + r'\(' + SEED_LO + r' % ' + DIVISOR + r'\)'), _seed_arith('%'), 'gain'),
    ('seed_div', re.compile(CAST32 + r'\(' + SEED_LO + r' / ' + DIVISOR + r'\)'), _seed_arith('/'), 'gain'),
    ('seed_bit', SeedBit(), lambda m: m.new),
    ('pow2div', Pow2Division(), lambda m: m.new),
    ('maskcast', MaskCast(), lambda m: m.new, 'gain'),
]
GAIN = {i[0] for i in IDIOMS if len(i) > 3 and i[3] == 'gain'}


# The rewritten expression must stand where an additive expression stands
# whole: after something that binds less tightly than `+` (not a cast, not
# `-`, `*`, `!`, `&`-address), and before nothing that binds more tightly.
BEFORE_OK = re.compile(r'(?:^|[(,=?:{\[]|return|[<>]=?|[=!]=|&&|\|\||[^&]&|\||\^|[^+]\+)\s*$')
AFTER_BAD = re.compile(r'^\s*(?:[*/%.\[(]|->|\+\+|--)')


def rewrite(line, idioms):
    """The line with every idiom rewritten, and whether a `gain` idiom fired."""
    gain = False
    for idiom in idioms:
        name, rx, fn = idiom[:3]
        def guarded(m, fn=fn):
            if not BEFORE_OK.search(m.string[:m.start()]) or AFTER_BAD.match(m.string[m.end():]):
                # A call is a primary expression: what replaces it goes in parentheses.
                return '(%s)' % fn(m) if isinstance(m, _Call) else m.group(0)
            return fn(m)
        new = rx.sub(guarded, line)
        if new != line and name in GAIN:
            gain = True
        line = new
    return line, gain


def candidates(rows, status, idioms, pats):
    """tu -> [(original start, {line: (old, new)})]."""
    out = collections.defaultdict(list)
    for tu in sorted({r[3] for r in rows}):
        if pats and not any(p in tu for p in pats):
            continue
        cpp = os.path.join(REPO, tu + '.cpp')
        obj = os.path.join(BUILD, 'obj', tu + '.obj')
        if not os.path.exists(cpp) or not os.path.exists(obj):
            continue
        lines = fixlib.read(cpp).split('\n')
        if not any(i[1].search(l) for l in lines for i in idioms):
            continue
        own = tu.lower() + '.cpp'
        by_sym = {r[4]: r[0] for r in rows if r[3] == tu}
        for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj'):
            s = by_sym.get(f.symbol)
            if s is None or status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING'):
                continue
            mine = [ln for _o, fn, ln in f.lines if fn.lower().replace('\\', '/').endswith(own)]
            if not mine:
                continue
            changes, gain = {}, False
            for ln in range(min(mine), max(mine) + 1):
                new, g = rewrite(lines[ln - 1], idioms)
                if new != lines[ln - 1]:
                    changes[ln] = (lines[ln - 1], new)
                    gain = gain or g
            if changes:
                out[tu].append((s, changes, gain))
    return out


def line_edits(cpp, changes):
    def fn(texts):
        lines = texts[cpp].split('\n')
        for ln, (old, new) in changes.items():
            if ln > len(lines) or lines[ln - 1] != old:
                return None
            lines[ln - 1] = new
        return {cpp: '\n'.join(lines)}
    return fn


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--idiom', action='append', help='only these idioms (default: all)')
    ap.add_argument('-j', '--jobs', type=int, default=4)
    ap.add_argument('patterns', nargs='*')
    args = ap.parse_args()
    idioms = [i for i in IDIOMS if not args.idiom or i[0] in args.idiom]
    rows = audit.load_funcmap(os.path.join(ORACLE, 'evidence', 'funcmap.tsv'))
    status = {}
    for line in open(os.path.join(ORACLE, 'evidence', 'audit.tsv')):
        p = line.rstrip('\n').split('\t')
        if p[0] != 'start' and len(p) > 9:
            status[int(p[0], 16)] = p[9]
    cands = candidates(rows, status, idioms, args.patterns)
    nlines = sum(len(c) for v in cands.values() for _s, c, _g in v)
    print('%d lines in %d functions, %d TUs' % (nlines, sum(len(v) for v in cands.values()), len(cands)))
    if not args.apply:
        for tu in sorted(cands):
            for s, changes, _gain in cands[tu]:
                for ln, (old, new) in sorted(changes.items()):
                    print('%s:%d  %08x\n    - %s\n    + %s' % (tu, ln, s, old.strip()[:160], new.strip()[:160]))
        return 0
    _ref, model = audit.load_context()
    syms = audit.sym_map(rows)
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    groups = {}
    for tu in sorted(cands):
        cpp = os.path.join(REPO, tu + '.cpp')
        groups[tu] = ([fixlib.Edit('%s:%d' % (tu, min(c)), [s], [s], line_edits(cpp, c), require_gain=g)
                       for s, c, g in cands[tu]], [cpp])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    n = sum(len(v) for v in cands.values())
    print('kept %d of %d edits, MATCH %+d' % (kept, n, gained))
    fixlib.record('fix_idioms', n, kept, gained, ', '.join(i[0] for i in idioms))
    return 0


if __name__ == '__main__':
    sys.exit(main())
