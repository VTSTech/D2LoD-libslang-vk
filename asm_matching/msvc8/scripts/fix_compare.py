#!/usr/bin/env python3
"""fix_compare.py - write comparisons with a constant the way the original did.

    fix_compare.py [--apply] [PATTERN ...]

Ghidra prints every ordered comparison in one canonical form and moves the
constant to fit it: `x >= 0` comes out as `-1 < x`, `x <= 4` as `x < 5`. The
two are the same comparison, but VC8 compiles each as written - `cmp x, -1;
jle` against the original's `test x, x; jl` - so the tree differs from the
original wherever the canonical form is not the one Blizzard wrote.

The original's instruction says which form it was. A site is a `cmp`/`test`
and the conditional jump or set after it, aligned with the original's, where

  * against a constant, the jump differs only in strictness and the constant
    by one in the matching direction: (jle, k) against (jl, k+1) means
    `x > k` / `x <= k` was written `x >= k+1` / `x < k+1`; (jl, k) against
    (jle, k-1) the other way round; the same for jg/jge, the unsigned jumps
    and setcc. The comparison with that constant is rewritten, the constant
    keeping its base.
  * against zero, `je`/`jne` where the original has `jbe`/`ja`: an unsigned
    `x <= 0` / `x > 0` that Ghidra prints as `x == 0` / `x != 0`, written
    `(uint32_t)x <= 0` - the same test for every x. The zero may be a
    register VC8 zeroed for something else (zero_operand), or the
    character `'\\0'` Ghidra prints for a byte; the test may be bare -
    `if (!n)`, `!(!n || ...)`, `} while (n);`, a for loop's guard
    `for (; n; n--)` (Lane E's P13: Blizzard's `n > 0` on an unsigned n).
    A register zeroed earlier on every path to the compare, past calls if
    it is callee-saved, is found by propagation (zero_on_all_paths).
  * the same with the jump inverted: `jne` where the original has `jbe`
    (`je` for `ja`) - the if/else written the other way round as well. The
    branches are swapped (fix_ifelse.swap_at) and the negated condition
    written unsigned, one edit: `if (!p) {A} else {B}` becomes `if
    ((uint32_t)p > 0) {B} else {A}` (GFX_PrecacheMonsterCofSprites,
    0046f650). Either half alone leaves the jump wrong.
  * the same relation with the other signedness (`jl` against `jb`, ...): the
    original compared unsigned where the tree compares signed, or the other
    way round - a variable's type; the operands get a cast to what the
    original compared. That changes what the line computes for negative or
    large values, toward what the original computed.
  Both kept only where VC8 then emits the original's jump on the line
  (site_check) and the function comes out no less similar. More similar
  it need not come out: the similarity compares jumps with their targets,
  which the function's other differences shift (FormatAddressDebugString,
  00402d60: `jbe 0x153` against the original's `jbe 0x162`), so the right
  condition alone adds nothing there.
  * between two operands, the jump is the mirror image (jg against jl, jae
    against jbe, ...): the original compared them in the other order, as in
    `a >= b`, which Ghidra prints `b <= a` (it prints only < and <=;
    InsertBlockIntoSortedArray, 006b2e40... `cmp [esi+0x7a0], eax; jl`).
    The comparison's operands are swapped and its operator mirrored.

Comparisons are paired by aligning the two listings, and what the
alignment leaves unpaired is aligned again: VC8 may lay out a function's
blocks in another order than the original's (aligned()).

The /Z7 line table gives the source line; a line with more candidate
comparisons than sites is left alone.

Without --apply it lists the sites. With --apply, per TU, the edits go
through fixlib: rebuilt, re-judged, kept only where nothing that matched stops
matching and no function they are for gets less similar. These are edits to
generated files: undoing the canonicalisation where the binary shows the
original form is the generator's job.
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
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

JCC = {'jl', 'jle', 'jg', 'jge', 'jb', 'jbe', 'ja', 'jae'}
ANY_JCC = JCC | {'je', 'jne', 'js', 'jns', 'jp', 'jnp', 'jo', 'jno'}     # all align as one kind
SETCC = {'set' + j[1:] for j in JCC}
# (ours, original) condition -> constant delta original - ours. +1: the
# relation of x to k was `>` or `<=`, and becomes `>=` / `<` with k+1;
# -1: it was `<` or `>=`, and becomes `<=` / `>` with k-1.
STRICT = {('le', 'l'): +1, ('g', 'ge'): +1, ('be', 'b'): +1, ('a', 'ae'): +1,
          ('l', 'le'): -1, ('ge', 'g'): -1, ('b', 'be'): -1, ('ae', 'a'): -1}
NEW_REL = {(+1, '>'): '>=', (+1, '<='): '<', (-1, '<'): '<=', (-1, '>='): '>'}
# Against zero: `x == 0` where the original jumps `jbe` was `x <= 0`, `x != 0`
# for `ja` was `x > 0` - the same test for an unsigned x, which the original's
# unsigned jump says it was; ours may be signed, so the rewrite is kept only
# where VC8 then emits the original's jump (site_check).
UNSIGNED_ZERO = {('e', 'be'): 'le0', ('ne', 'a'): 'gt0'}
# The same with the jump inverted too: `if (x == 0) {A} else {B}` compiles to
# `jne B` where the original has `jbe A` - Blizzard's `if (x > 0) {B} else
# {A}`. The branches are swapped (fix_ifelse.swap_at) and the condition, now
# `x != 0`, written unsigned in one edit: either alone leaves the jump wrong.
INVERTED_ZERO = {('ne', 'be'): 'swap_gt0', ('e', 'a'): 'swap_le0'}
# The same relation, the other signedness: the comparison is made unsigned
# (or signed) with a cast on its operands - what the original compared,
# whatever the operands' declared types.
SIGN_FLIP = {('l', 'b'): 'unsigned', ('le', 'be'): 'unsigned', ('g', 'a'): 'unsigned', ('ge', 'ae'): 'unsigned',
             ('b', 'l'): 'signed', ('be', 'le'): 'signed', ('a', 'g'): 'signed', ('ae', 'ge'): 'signed'}
CAST = {'unsigned': '(uint32_t)', 'signed': '(int)'}
# Zero, also as the character Ghidra prints for a byte: `c == '\0'`.
EQ_RIGHT = re.compile(r' (?P<op>==|!=) (?P<k>0[uU]?|0x0[uU]?|\'\\0\')(?![\w.])')
EQ_LEFT = re.compile(r'(?<![\w.)\]])(?P<k>0[uU]?|0x0[uU]?|\'\\0\') (?P<op>==|!=) ')
MIRROR = {'<': '>', '<=': '>=', '>': '<', '>=': '<='}
# A bare truth test - `if (p)`, `!p && ...`, `... || q)`, `} while (n);`, a
# for loop's `; n;`, inside parentheses `if (!(!n || ...` - of a plain
# operand, maybe under an integer cast, or a dereference, `*(int*)(p + 0x200)`.
INT_CAST = r'\((?:u?int(?:8|16|32)_t|int|char|short|DWORD|WORD|BYTE)\)'
TRUTH_OPERAND = (r'(?:(?:' + INT_CAST + r')?\*?[A-Za-z_]\w*(?:(?:\.|->)[A-Za-z_]\w*|\[[^\[\]]*\])*'
                 r'|\*\((?:u?int(?:8|16|32)_t|int|char|short|DWORD|WORD|BYTE)\s*\*\)\((?:[^()]|\([^()]*\))*\))')
BARE_TEST = re.compile(r'(?:(?<=\bif \()|(?<=\bwhile \()|(?<=&& )|(?<=\|\| )|(?<=\(\()|(?<=!\())(?P<neg>!?)(?P<x>' +
                       TRUTH_OPERAND + r')(?=\)(?: \{)?\s*$|\);\s*$| &&| \|\||\) &&|\) \|\|)')
FOR_GUARD = re.compile(r'(?<=\bfor \()[^;()]*; (?P<neg>!?)(?P<x>' + TRUTH_OPERAND + r'); ')

MIRROR_JCC = {'l': 'g', 'g': 'l', 'le': 'ge', 'ge': 'le', 'b': 'a', 'a': 'b', 'be': 'ae', 'ae': 'be'}
CMP_REG = re.compile(r'^cmp\s+(?P<a>.+?),\s*(?P<b>[^,]+)$')
CMP_IMM = re.compile(r'^cmp\s+(?P<x>.+?),\s*(?P<k>-?(?:0x[0-9a-f]+|\d+))$')
TEST_SELF = re.compile(r'^test\s+(?P<x>\w+),\s*(?P=x)$')
CONST = r'-?(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*'
# A comparison operator, not part of << >> -> <<= >>= or a template bracket
# glued to a name: the generator writes binary operators with spaces around.
OP = r'(?<![<>=!-])(?P<op><=|>=|<|>)(?![<>=])'
RIGHT = re.compile(r' ' + OP + r' (?P<k>' + CONST + r')(?![\w.])')
LEFT = re.compile(r'(?<![\w.)\]])(?P<k>' + CONST + r') ' + OP + r' ')


def cond(mnemonic):
    if mnemonic in ANY_JCC:
        return mnemonic[1:]
    if mnemonic.startswith('set') and 'j' + mnemonic[3:] in ANY_JCC:
        return mnemonic[3:]
    return None


def width_mask(insn):
    """The mask of the compared width: `cmp ax, 0xffff` compares -1 as a word,
    which `test ax, ax; jl` against `jle` makes k+1 = 0 (MONSTER_TransformToClassId,
    004aedd0)."""
    t = insn.text
    if re.search(r'\b[abcd][lh]\b|\bbyte ptr', t):
        return 0xff
    if re.search(r'\b(?:[abcd]x|si|di|bp)\b|(?<!d)word ptr', t):
        return 0xffff
    return 0xffffffff


def cmp_operand(insn):
    """(operand, constant) of `cmp x, k` / `test x, x` (k = 0), else None."""
    m = CMP_IMM.match(insn.text)
    if m:
        k = int(m.group('k'), 0)
        return m.group('x'), k
    m = TEST_SELF.match(insn.text)
    if m:
        return m.group('x'), 0
    return None


CMP_TO_REG = re.compile(r'^cmp\s+(?P<x>.+?),\s*(?P<r>e[abcd]x|e[sd]i|[abcd][lh]|[abcd]x|si|di)$')
FULL = {'al': 'eax', 'ah': 'eax', 'ax': 'eax', 'eax': 'eax', 'bl': 'ebx', 'bh': 'ebx', 'bx': 'ebx', 'ebx': 'ebx',
        'cl': 'ecx', 'ch': 'ecx', 'cx': 'ecx', 'ecx': 'ecx', 'dl': 'edx', 'dh': 'edx', 'dx': 'edx', 'edx': 'edx',
        'si': 'esi', 'esi': 'esi', 'di': 'edi', 'edi': 'edi'}
JUMP_TO = re.compile(r'^j\w+\s+(0x[0-9a-f]+|\d+)$')


def zero_operand(view, i):
    """(x, 0) for `cmp x, r` where r holds zero: VC8 compares with a register
    it zeroed for something else (Q05_SetCompletionForTrackedPlayers,
    00595420: `xor eax, eax` for the loop counter, then `cmp word ptr [edx +
    0x80], ax; jbe`). The zeroing must come earlier in the same straight run
    of code - no jump, call or write of the register between, and no jump
    from elsewhere landing between."""
    m = CMP_TO_REG.match(view[i].text)
    if not m:
        return None
    r = FULL[m.group('r')]
    targets = {int(j.group(1), 0) for j in (JUMP_TO.match(v.text) for v in view) if j}
    for k in range(i - 1, max(-1, i - 16), -1):
        v = view[k]
        if any(view[k].addr < t <= view[i].addr for t in targets):
            break
        if v.mnemonic in ANY_JCC or v.mnemonic == 'jmp' or v.mnemonic.startswith('ret') or \
                (v.mnemonic == 'call' and r not in CALLEE_SAVED):
            break
        if zeroes(v, r):
            return m.group('x'), 0
        if writes(v, r):
            return None
    if zero_on_all_paths(view, i, r):
        return m.group('x'), 0
    return None


CALLEE_SAVED = ('ebx', 'esi', 'edi')
ZEROING = re.compile(r'^(?:(?:xor|sub)\s+(?P<a>e[abcd]x|e[sd]i),\s*(?P=a)|mov\s+(?P<b>e[abcd]x|e[sd]i),\s*0)$')
STRING_OP = re.compile(r'^(?:rep\w*\s+)?(?:movs|stos|lods|scas|cmps)[bwd]?\b')


def zeroes(v, r):
    m = ZEROING.match(v.text)
    return bool(m) and (m.group('a') or m.group('b')) == r


def writes(v, r):
    """Whether the instruction can change register r (its full name)."""
    t = v.text
    if ' ' not in t:
        return v.mnemonic in ('cdq', 'cpuid', 'rdtsc', 'popal', 'popad')
    ops = [o.strip() for o in t.split(None, 1)[1].split(',')]
    if FULL.get(ops[0]) == r and v.mnemonic not in ('cmp', 'test', 'push'):
        return True
    if v.mnemonic in ('xchg', 'xadd', 'cmpxchg') and any(FULL.get(o) == r for o in ops):
        return True
    if r in ('esi', 'edi', 'ecx') and STRING_OP.match(t):
        return True
    if r in ('eax', 'edx') and v.mnemonic in ('mul', 'imul', 'div', 'idiv') and len(ops) == 1:
        return True
    return False


def zero_on_all_paths(view, i, r):
    """Whether r holds zero at view[i] on every path from the entry: a
    forward propagation over the function's jumps, r known zero after a
    zeroing, unknown after any write, and after a call unless r is
    callee-saved (VC8 keeps 0 in EDI for a whole loop and compares `cmp
    dword ptr [g_0075af98], edi` far from the `xor edi, edi`, past calls,
    where zero_operand's straight run does not reach). An indirect jump or a
    handler frame (fs:) leaves the edges unknown: no proof then."""
    if any((v.mnemonic == 'jmp' and not JUMP_TO.match(v.text)) or 'fs:' in v.text for v in view):
        return False
    index = {v.addr: k for k, v in enumerate(view)}
    state = [None] * len(view)              # None: not reached; else whether r is zero on every path so far
    state[0] = False
    work = [0]
    while work:
        k = work.pop()
        v, z = view[k], state[k]
        if zeroes(v, r):
            z = True
        elif writes(v, r) or (v.mnemonic == 'call' and r not in CALLEE_SAVED):
            z = False
        m = v.mnemonic
        jt = JUMP_TO.match(v.text)
        t = index.get(int(jt.group(1), 0)) if jt else None
        nxt = k + 1 if k + 1 < len(view) else None
        if m.startswith('ret') or m in ('int3', 'hlt', 'ud2'):
            succ = []
        elif m == 'jmp':
            succ = [t] if t is not None else []
        elif jt or m.startswith('loop'):
            succ = ([t] if t is not None else []) + ([nxt] if nxt is not None else [])
        else:
            succ = [nxt] if nxt is not None else []
        for s in succ:
            new = z if state[s] is None else (state[s] and z)
            if new != state[s]:
                state[s] = new
                work.append(s)
    return state[i] is True


REGS = re.compile(r'\b(e[abcd]x|e[sd]i|ebp|esp|[abcd][lh]|[abcd]x|si|di)\b')


def norm(insn):
    """Alignment key: jumps and sets by kind only, `test r, r` as a compare,
    registers and numbers masked - register allocation differs more often
    than anything else, and must not keep two listings from lining up."""
    m = insn.mnemonic
    if m in JCC or m in ANY_JCC:
        return 'jcc'
    if m in SETCC or (m.startswith('set') and len(m) <= 5):
        return 'setcc'
    if m in ('cmp', 'test'):
        return 'cmp'
    return compare_key(insn)


def compare_key(insn):
    return re.sub(r'0x[0-9a-f]+|\b\d+\b', 'N', REGS.sub('R', insn.key))


FLAG_NEUTRAL = {'mov', 'lea', 'push', 'pop', 'movzx', 'movsx', 'nop'}


def flags_user(view, i):
    """The jump or set that reads the flags the compare at view[i] sets: the
    next one, past instructions that leave the flags alone (VC8 schedules a
    store between them: scan_tree, `cmp ebx, -1; mov word ptr [...], -1; jle`;
    Q05_SetCompletionForTrackedPlayers, 00595420, five of them)."""
    for k in range(i + 1, min(i + 9, len(view))):
        if cond(view[k].mnemonic) is not None:
            return view[k]
        if view[k].mnemonic not in FLAG_NEUTRAL:
            return None
    return None


REALIGN_MIN = 3         # instructions in a block the second alignment pairs


def aligned(ours, ref):
    """[(i, j)] of instructions the alignment pairs. The first pass pairs the
    code in order; VC8 may lay out the blocks of a function in another order
    than the original's (Fog::Memory::Alloc, 0040a080: the pool test
    `cmp eax, [ebx + 0x1c]; jl` lies after a block that ours has later), so
    what it leaves unpaired on both sides is aligned again, in order among
    itself, and its blocks of REALIGN_MIN or more taken too."""
    a, b = [norm(x) for x in ours], [norm(y) for y in ref]
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    pairs = []
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == 'equal':
            pairs += zip(range(i1, i2), range(j1, j2))
    used_i, used_j = {i for i, _ in pairs}, {j for _, j in pairs}
    ri = [i for i in range(len(a)) if i not in used_i]
    rj = [j for j in range(len(b)) if j not in used_j]
    sm = difflib.SequenceMatcher(None, [a[i] for i in ri], [b[j] for j in rj], autojunk=False)
    for blk in sm.get_matching_blocks():
        if blk.size >= REALIGN_MIN:
            pairs += [(ri[blk.a + k], rj[blk.b + k]) for k in range(blk.size)]
    return pairs


def sites(ours, ref):
    """[(our cmp insn, delta, ours k)] where only the comparison's form differs."""
    out = []
    for i, j in aligned(ours, ref):
        if ours[i].mnemonic not in ('cmp', 'test') or ref[j].mnemonic not in ('cmp', 'test'):
            continue
        ja, jb = flags_user(ours, i), flags_user(ref, j)
        if ja is None or jb is None:
            continue
        ca, cb = cond(ja.mnemonic), cond(jb.mnemonic)
        a, b = cmp_operand(ours[i]), cmp_operand(ref[j])
        if ((ca, cb) in UNSIGNED_ZERO or (ca, cb) in INVERTED_ZERO) and (a is None or b is None):
            a, b = a or zero_operand(ours, i), b or zero_operand(ref, j)
        if a is not None and b is not None and a[1] == 0 and b[1] == 0 and (ca, cb) in UNSIGNED_ZERO:
            out.append((ours[i], UNSIGNED_ZERO[(ca, cb)], 0, cb))
            continue
        if a is not None and b is not None and a[1] == 0 and b[1] == 0 and (ca, cb) in INVERTED_ZERO:
            out.append((ours[i], INVERTED_ZERO[(ca, cb)], 0, cb))
            continue
        if (ca, cb) in SIGN_FLIP and (a is None) == (b is None) and (a is None or a[1] == b[1]):
            out.append((ours[i], SIGN_FLIP[(ca, cb)], a[1] if a else None, cb))
            continue
        if a is not None and b is not None:
            if (ca, cb) not in STRICT:
                continue
            delta = STRICT[(ca, cb)]
            mask = width_mask(ours[i])
            if (b[1] - a[1]) & mask == delta & mask:
                out.append((ours[i], delta, a[1], cb))
        elif a is None and b is None and ours[i].mnemonic == 'cmp' and ref[j].mnemonic == 'cmp' \
                and MIRROR_JCC.get(ca) == cb:
            out.append((ours[i], 'swap', None, cb))
    return out


def operand_bounds(line, at, end):
    """The extent of the operands around line[at:end] (a relational
    operator): (start of the left operand, end of the right one). Operands
    reach as far as operators binding tighter than the relational ones do -
    arithmetic, shifts, unary operators, member access, calls and subscripts -
    and stop at parentheses that do not close inside them and at everything
    binding looser (== != & ^ | && || ?: = ,) or ending the expression."""
    def token_at(i):
        """The operator-ish token covering line[i], if it ends an operand."""
        for t in ('&&', '||', '==', '!=', '<=', '>=', '<<', '>>', '->'):
            for k in (i - 1, i):
                if k >= 0 and line.startswith(t, k):
                    return t, k
        return line[i], i

    def is_stop(i):
        if line[i] == ' ' and re.search(r'(?:^|[^\w])(?:return|case|else|do)$', line[:i]):
            return True
        t, k = token_at(i)
        if t in ('&&', '||', '==', '!=', '<=', '>='):
            return True
        if t in ('<<', '>>', '->'):
            return False
        if t == ':' and (line[i - 1:i] == ':' or line[i + 1:i + 2] == ':'):
            return False                  # D2Client::Mouse::nPosX: scope, not the ternary's colon
        if t in ('?', ':', ',', ';', '{', '}', '|', '^', '<', '>', '='):
            return True
        if t == '&':
            # Binary & stops; unary & (after an operator or a paren) belongs to the operand.
            j = i - 1
            while j >= 0 and line[j] == ' ':
                j -= 1
            if j >= 0 and line[j] == ')':
                # ... and so does the & after a cast: `*(int*)&p->x` (AI_Fn1_Overseer,
                # where the cast once landed under the &: `&(int)p->x`, which only VC8 takes).
                k, depth = j, 0
                while k >= 0:
                    depth += {')': 1, '(': -1}.get(line[k], 0)
                    if depth == 0:
                        break
                    k -= 1
                if k >= 0 and re.fullmatch(r'\s*(?:const\s+|struct\s+|unsigned\s+|signed\s+)*[A-Za-z_][\w:]*'
                                           r'(?:\s*\*)+\s*|\s*(?:unsigned\s+|signed\s+)?(?:u?int(?:8|16|32|64)_t|int|'
                                           r'char|short|long|DWORD|WORD|BYTE|BOOL|uintptr_t|size_t)\s*', line[k + 1:j]):
                    return False
            return j >= 0 and (line[j].isalnum() or line[j] in '_)]')
        return False

    i, depth = at - 1, 0
    while i >= 0:
        c = line[i]
        if c in ')]':
            depth += 1
        elif c in '([':
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and is_stop(i):
            break
        i -= 1
    left = i + 1
    if left > 0 and token_at(i)[0] in ('&&', '||', '==', '!=', '<=', '>='):
        left = token_at(i)[1] + len(token_at(i)[0])
    j, depth = end, 0
    while j < len(line):
        c = line[j]
        if c in '([':
            depth += 1
        elif c in ')]':
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and is_stop(j):
            j = token_at(j)[1]
            break
        j += 1
    return left, j


REL = re.compile(r'(?<![<>=!-])(?P<op><=|>=|<|>)(?![<>=])')


def swap_line(line, count):
    """The line with its relational comparisons between two non-constant
    operands swapped (`a <= b` -> `b >= a`), if there are exactly `count`."""
    found = []
    for m in REL.finditer(line):
        if line[m.start() - 1:m.start()] != ' ' or line[m.end():m.end() + 1] != ' ':
            continue                  # not a spaced binary operator: a template bracket, ->
        lo, hi = operand_bounds(line, m.start(), m.end())
        a, b = line[lo:m.start()], line[m.end():hi]
        if not a.strip() or not b.strip():
            return None
        if re.fullmatch(r'\s*' + CONST + r'\s*', a) or re.fullmatch(r'\s*' + CONST + r'\s*', b):
            continue
        found.append((lo, m, hi, a, b))
    if len(found) != count:
        return None
    out = line
    for lo, m, hi, a, b in sorted(found, key=lambda t: -t[0]):
        la, ra = len(a) - len(a.lstrip()), len(a.rstrip())
        lb, rb = len(b) - len(b.lstrip()), len(b.rstrip())
        new = a[:la] + b.strip() + a[ra:] + MIRROR[m.group('op')] + b[:lb] + a.strip() + b[rb:]
        out = out[:lo] + new + out[hi:]
    return out


def wrap(expr, cast):
    """`expr` under `cast`, parenthesised unless it is one unary expression;
    an operand already under that cast is left as it is."""
    e = expr.strip()
    lead, trail = expr[:len(expr) - len(expr.lstrip())], expr[len(expr.rstrip()):]
    if e.startswith(cast) and re.fullmatch(r'[\w.\->\[\]:]+|\(.*\)', e[len(cast):]):
        return expr
    inner = e if re.fullmatch(r'[\w.\->\[\]:]+', e) else '(%s)' % e
    return lead + cast + inner + trail


def rewrite_zero(line, kind, count):
    """`x == 0` -> `(uint32_t)x <= 0` (le0), `x != 0` -> `(uint32_t)x > 0`
    (gt0): the same test for every x, written as the original's unsigned
    comparison - for exactly `count` such comparisons in the line, else None.
    A line with no `== 0` and one bare test has it written out the same way:
    `if (p)` -> `if ((uint32_t)p > 0)`, `!p` -> `(uint32_t)p <= 0` - the same
    for any x of 32 bits or fewer (the original tests a register of 32)."""
    want, new_op = ('==', '<=') if kind == 'le0' else ('!=', '>')
    found = [(m, False) for m in EQ_RIGHT.finditer(line) if m.group('op') == want] + \
            [(m, True) for m in EQ_LEFT.finditer(line) if m.group('op') == want]
    if not found:
        # The other equality: which of the two the jump tests depends on the
        # layout too (`if (c != '\0')` skipping its body on `je`); `x != 0`
        # is `(uint32_t)x > 0` and `x == 0` `(uint32_t)x <= 0` all the same.
        want, new_op = ('!=', '>') if kind == 'le0' else ('==', '<=')
        found = [(m, False) for m in EQ_RIGHT.finditer(line) if m.group('op') == want] + \
                [(m, True) for m in EQ_LEFT.finditer(line) if m.group('op') == want]
    if not found and count == 1 and not EQ_RIGHT.search(line) and not EQ_LEFT.search(line):
        # No `== 0` written: a bare test, `x` being `x != 0` and `!x` `x == 0` -
        # which of the two the jump tests depends on the layout, not on kind.
        bare = list(BARE_TEST.finditer(line)) + list(FOR_GUARD.finditer(line))
        if len(bare) != 1:
            return None
        m = bare[0]
        test = '%s %s 0' % (wrap(m.group('x'), CAST['unsigned']), '<=' if m.group('neg') else '>')
        return line[:m.start('neg')] + test + line[m.end('x'):]
    if len(found) != count:
        return None
    out = line
    for m, left in sorted(found, key=lambda t: -t[0].start()):
        if left:
            op_at = m.start() + len(m.group('k')) + 1
            lo, hi = operand_bounds(out, op_at, op_at + 2)
            out = out[:m.start()] + '%s %s ' % (zero(m), MIRROR[new_op]) + wrap(out[m.end():hi], CAST['unsigned']) + out[hi:]
        else:
            lo, hi = operand_bounds(out, m.start() + 1, m.start() + 3)
            out = out[:lo] + wrap(out[lo:m.start()], CAST['unsigned']) + ' %s %s' % (new_op, zero(m)) + out[m.end():]
    return out


def zero(m):
    """The zero of an EQ_RIGHT/EQ_LEFT match as the unsigned comparison
    writes it: a number, not the character '\\0'."""
    return '0' if m.group('k').startswith("'") else m.group('k')


def rewrite_sign(line, kind, k, count):
    """The `count` relational comparisons of the line - against the constant
    k, or between two operands when k is None - with their non-constant
    operands cast to the other signedness, else None."""
    cast = CAST[kind]
    found = []
    for m in REL.finditer(line):
        if line[m.start() - 1:m.start()] != ' ' or line[m.end():m.end() + 1] != ' ':
            continue
        lo, hi = operand_bounds(line, m.start(), m.end())
        a, b = line[lo:m.start()], line[m.end():hi]
        ka = re.fullmatch(r'\s*' + CONST + r'\s*', a)
        kb = re.fullmatch(r'\s*' + CONST + r'\s*', b)
        if kind == 'unsigned' and k is not None and same_value('0', k) and \
                ((kb and m.group('op') in ('<', '>=')) or (ka and m.group('op') in ('>', '<='))):
            return None                   # unsigned x < 0 is never true: not a comparison any more
        if k is None:
            if not ka and not kb:
                found.append((lo, m, hi, a, b, True, True))
        elif kb and same_value(b.strip(), k) and not ka:
            found.append((lo, m, hi, a, b, True, False))
        elif ka and same_value(a.strip(), k) and not kb:
            found.append((lo, m, hi, a, b, False, True))
    if len(found) != count:
        return None
    out = line
    for lo, m, hi, a, b, wa, wb in sorted(found, key=lambda t: -t[0]):
        na = wrap(a, cast) if wa else a
        nb = wrap(b, cast) if wb else b
        out = out[:lo] + na + m.group('op') + nb + out[hi:]
    return out


def same_value(text, k):
    v = int(text.rstrip('uUlL'), 0)
    return (v - k) & 0xffffffff == 0 or (v - k) & 0xff == 0 and -128 <= k < 256 or \
        (v - k) & 0xffff == 0 and -0x8000 <= k < 0x10000


def fmt(old, value):
    """`value` written like `old` (hex stays hex, suffix kept)."""
    suffix = old[len(old.rstrip('uUlL')):]
    body = old[:len(old) - len(suffix)]
    neg = value < 0
    if body.lstrip('-')[:2].lower() == '0x':
        s = '0x%x' % abs(value)
    else:
        s = '%d' % abs(value)
    return ('-' if neg else '') + s + suffix


def rewrite_line(line, k, delta, count):
    """The line with its `count` comparisons against constant k rewritten for
    `delta`, or None unless exactly `count` such comparisons are there and all
    have a relation the rule applies to."""
    found = []
    for m in RIGHT.finditer(line):
        if same_value(m.group('k'), k):
            found.append((m, m.group('op'), False))
    for m in LEFT.finditer(line):
        if same_value(m.group('k'), k):
            found.append((m, MIRROR[m.group('op')], True))
    if len(found) != count:
        return None
    for m, rel, _left in found:
        if (delta, rel) not in NEW_REL:
            return None
    out = line
    for m, rel, left in sorted(found, key=lambda t: -t[0].start()):
        new_rel = NEW_REL[(delta, rel)]
        v = int(m.group('k').rstrip('uUlL'), 0) + delta
        newk = fmt(m.group('k'), v)
        if left:
            repl = '%s %s ' % (newk, MIRROR[new_rel])
        else:
            repl = ' %s %s' % (new_rel, newk)
        out = out[:m.start()] + repl + out[m.end():]
    return out


def candidates(rows, model, syms, status, pats):
    """tu -> [(original start, function name, line number, old line, new line)]."""
    cache = {}
    out = collections.defaultdict(list)
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
        if not found:
            continue
        cpp = os.path.join(REPO, tu + '.cpp')
        own = tu.lower() + '.cpp'
        per_line = collections.defaultdict(list)
        expect = collections.defaultdict(set)
        expect_by = collections.defaultdict(list)
        for insn, delta, k, cb in found:
            at = f.line_at(insn.addr)
            if at and at[0].lower().replace('\\', '/').endswith(own):
                per_line[at[1]].append((delta, k))
                if delta in ('unsigned', 'signed', 'le0', 'gt0') or delta in INVERTED_ZERO.values():
                    expect[at[1]].add(cb)
                    expect_by[at[1]].append((delta, cb))
        if not per_line:
            continue
        whole = fixlib.read(cpp)
        text = whole.split('\n')
        for ln, lst in sorted(per_line.items()):
            if not 0 < ln <= len(text):
                continue
            if any(d in INVERTED_ZERO.values() for d, _k in lst):
                # A swap is the whole statement's edit: only where it is the line's one kind
                # of site; beside others it is left out and they are taken as before.
                if len(set(lst)) == 1:
                    if swap_zero(whole, ln, text[ln - 1], lst[0][0], len(lst)) is not None:
                        out[tu].append((s, name, ln, text[ln - 1], (lst[0][0], len(lst)), True, sym,
                                        frozenset(expect.get(ln, ()))))
                    continue
                lst = [(d, k) for d, k in lst if d not in INVERTED_ZERO.values()]
                expect[ln] = {c for d, c in expect_by[ln] if d not in INVERTED_ZERO.values()}
            new = text[ln - 1]
            ok, gain = True, False
            for (delta, k), n in collections.Counter(lst).items():
                if delta == 'swap':
                    new2 = swap_line(new, n)
                elif delta in ('le0', 'gt0'):
                    new2, gain = rewrite_zero(new, delta, n), True
                elif delta in ('unsigned', 'signed'):
                    new2, gain = rewrite_sign(new, delta, k, n), True
                else:
                    new2 = rewrite_line(new, k, delta, n)
                if new2 is None:
                    ok = False
                    break
                new = new2
            if ok and new != text[ln - 1]:
                out[tu].append((s, name, ln, text[ln - 1], new, gain, sym, frozenset(expect.get(ln, ()))))
    return out


def swap_zero(text, ln, old, kind, count):
    """The text with the if/else at line `ln` swapped and its condition, now
    negated, written as the original's unsigned test against zero; None
    unless the line still reads `old`, has an else and takes the rewrite."""
    from fix_ifelse import swap_at          # fix_ifelse imports this module
    starts = [0] + [k + 1 for k, c in enumerate(text) if c == '\n']
    if ln > len(starts) or not text.startswith(old + '\n', starts[ln - 1]):
        return None
    new = swap_at(text, starts[ln - 1])
    if new is None:
        return None
    lines = new.split('\n')
    line = rewrite_zero(lines[ln - 1], kind[len('swap_'):], count)
    if line is None:
        return None
    lines[ln - 1] = line
    return '\n'.join(lines)


def edit_for(cpp, ln, old, new):
    """A line edit; for a swap (`new` is (kind, count)) the swap made again
    on the file as it is when the edit is applied."""
    if isinstance(new, str):
        return line_edit(cpp, ln, old, new)

    def fn(texts):
        t = swap_zero(texts[cpp], ln, old, new[0], new[1])
        return None if t is None else {cpp: t}
    return fn


def site_check(tu, sym, ln, expect):
    """After the rebuild: the edited line still compiles to a conditional
    jump or set, with each condition the original has there."""
    def check():
        for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj'):
            if f.symbol == sym:
                conds = {cond(i.mnemonic) for i in f.decoded.insns
                         if f.line_at(i.addr) and f.line_at(i.addr)[1] == ln}
                return expect <= conds
        return False
    return check


def line_edit(cpp, ln, old, new):
    def fn(texts):
        lines = texts[cpp].split('\n')
        if lines[ln - 1] != old:
            return None
        lines[ln - 1] = new
        return {cpp: '\n'.join(lines)}
    return fn


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('-j', '--jobs', type=int, default=4)
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
    cands = candidates(rows, model, syms, status, args.patterns)
    total = sum(len(v) for v in cands.values())
    print('%d lines in %d functions, %d TUs' % (
        total, len({(tu, c[0]) for tu, v in cands.items() for c in v}), len(cands)))
    if not args.apply:
        for tu in sorted(cands):
            for s, name, ln, old, new, _gain, _sym, _expect in cands[tu]:
                if not isinstance(new, str):
                    swapped = swap_zero(fixlib.read(os.path.join(REPO, tu + '.cpp')), ln, old, new[0], new[1])
                    new = '%s  [%s, branches swapped]' % (swapped.split('\n')[ln - 1], new[0])
                print('%s:%d  %08x %s\n    - %s\n    + %s' % (tu, ln, s, name.rsplit('::', 1)[-1], old.strip(), new.strip()))
        return 0
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    ctx = (rows, model, syms, quarantined)
    groups = {}
    for tu in sorted(cands):
        cpp = os.path.join(REPO, tu + '.cpp')
        groups[tu] = ([fixlib.Edit('%s:%d' % (tu, ln), [s], [s], edit_for(cpp, ln, old, new),
                                   require_gain=gain and not expect,
                                   check=site_check(tu, sym, ln, expect) if expect else None)
                       for s, _name, ln, old, new, gain, sym, expect in cands[tu]], [cpp])
    kept, gained = fixlib.apply_batch(groups, ctx, jobs=args.jobs, log=lambda m: print(m, flush=True))
    print('kept %d of %d edits, MATCH %+d' % (kept, total, gained))
    fixlib.record('fix_compare', total, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
