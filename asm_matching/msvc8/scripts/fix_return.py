#!/usr/bin/env python3
"""fix_return.py - give back the return value the database dropped.

    fix_return.py [--apply] [PATTERN ...]

A function the tree declares `void` whose original loads EAX with a constant
right before it returns - `mov eax, 1` ahead of the epilogue
(NET_SID_CLIENT_Send_0x3D_CreateAccount2, 0051a740), `xor eax, eax` - did
return a value: the database's signature lost it. Where the original returns
one value on every path the edit is mechanical: the return type becomes
`int`, every `return;` returns that value, and so does the end of the body;
the declaration in the TU's header follows. Every path is checked, not only
the loads ours lacks: EAX at each of the original's rets comes from a
constant propagation over its control flow (classify.eax_at_returns), and
all of them must hold the one constant. A path that returns 0 from the
`xor eax, eax` that zeroed a buffer (SKILLDESC_FormatSignedValue, 004e79e0),
or a callee's result (SKILLDESC_FormatMinMaxRange, 004e7f40), is a function
whose returns the database has to give one by one; it is left alone. A
constant in AL alone (`xor al, al`, `mov al, 1`) is a bool's return: the
function becomes `bool`. A setter whose original hands back what it was
given - EAX at every ret the same argument, read from its stack slot or
copied from ECX/EDX on entry (AUTOMAP_SetFade, Lane E's P2: `mov eax, esi`
before `ret`) - returns that parameter, as its type, where it is a dword
the body never assigns (arg_at_returns). A static function may have a
convention of its own, an argument in any register, the callee-saved ones
too: there EAX holding one such register unchanged at every ret returns the
function's one parameter that qualifies. Callers did not use a value
before and do not now. Only free functions named nowhere but in calls,
their definition and their own header's declaration are taken - or under
an explicit cast, `(void *)F`, which still compiles with another return
type, and whose callers through the pointer do not read EAX; a function
whose address goes uncast into a typed pointer would no longer fit it. The return
type is part of the mangled name; the judge finds the function by its name
until the next `oracle map`. Kept, through fixlib's batch loop, only where
the function comes out more similar.
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
import classify         # noqa: E402
import compare          # noqa: E402
import fixlib           # noqa: E402
import ours as ourslib  # noqa: E402
from fix_compare import norm  # noqa: E402
from fix_ifelse import match_close  # noqa: E402
from fix_static import Tree, def_pattern, strip_comments  # noqa: E402
from oracle_config import BUILD, ORACLE, REPO  # noqa: E402

EPILOGUE = ('pop ', 'mov esp', 'leave', 'ret', 'call ')
SET_EAX = re.compile(r'mov eax, (0x[0-9a-f]+|\d+)$|xor eax, eax$')
SET_AL = re.compile(r'mov al, (0x[0-9a-f]+|\d+)$|xor al, al$')


def returned_values(ours, ref, rx=SET_EAX):
    """The constants the original loads into EAX (rx=SET_AL: AL) just before
    returning, where ours loads nothing at the aligned place."""
    vals = set()
    sm = difflib.SequenceMatcher(None, [norm(x) for x in ours], [norm(y) for y in ref], autojunk=False)
    for op, _i1, _i2, j1, j2 in sm.get_opcodes():
        if op not in ('insert', 'replace'):
            continue
        for j in range(j1, j2):
            m = rx.match(ref[j].text)
            if m and j + 1 < len(ref) and ref[j + 1].text.startswith(EPILOGUE):
                vals.add(0 if m.group(1) is None else int(m.group(1), 0))
    return vals


_FRAME_SLOT = re.compile(r'(?:dword|word|byte) ptr \[ebp \+ (0x[0-9a-f]+|\d+)\]')


def arg_at_returns(decoded, own_convention=False):
    """The one argument EAX holds at every ret of the original, or None:
    ('stack', k) for the k-th stack argument (`mov eax, dword ptr [ebp + 8 +
    4k]` in a function with an EBP frame, a slot nothing writes or takes the
    address of), or ('reg', 'ecx'/'edx') for a register argument EAX was
    copied from on entry. The same forward propagation as
    classify.eax_at_returns, over symbols instead of constants: a setter
    that returns what it was given (AUTOMAP_SetFade: `mov eax, esi` before
    `ret`)."""
    insns = getattr(decoded, 'insns', decoded)
    if len(insns) < 3:
        return None
    first = [next(classify._MD.disasm(i.bytes, i.addr, 1), None) for i in insns[:2]]
    if None in first or (first[0].mnemonic, first[0].op_str) != ('push', 'ebp') or \
            (first[1].mnemonic, first[1].op_str) != ('mov', 'ebp, esp'):
        frame = False
    else:
        frame = True
    dirty = set()
    decoded_all = []
    for i in insns:
        ins = next(classify._MD.disasm(i.bytes, i.addr, 1), None)
        if ins is None:
            return None
        decoded_all.append(ins)
        o = ins.op_str.split(', ') if ins.op_str else []
        for k, op in enumerate(o):
            m = _FRAME_SLOT.search(op)
            if m and (ins.mnemonic == 'lea' or (k == 0 and ins.mnemonic not in ('cmp', 'test', 'push'))):
                dirty.add(int(m.group(1), 0))
    regs = ('eax', 'ecx', 'edx', 'ebx', 'esi', 'edi')
    index = {i.addr: k for k, i in enumerate(insns)}
    tables = {t: entries for t, entries in getattr(decoded, 'tables', ())}
    state = [None] * len(insns)
    state[0] = {r: None for r in regs}
    state[0].update(ecx=('reg', 'ecx'), edx=('reg', 'edx'))
    if own_convention:
        # A static function VC8 gave a convention of its own takes arguments in
        # other registers too, the callee-saved ones. (Not EAX: an argument
        # there, never written, would be at every ret without being returned.)
        state[0].update({r: ('reg', r) for r in ('ebx', 'esi', 'edi')})
    work = [0]
    while work:
        k = work.pop()
        i, ins = insns[k], decoded_all[k]
        st = dict(state[k])
        m = ins.mnemonic
        o = ins.op_str.split(', ') if ins.op_str else []
        if m == 'call':
            if getattr(i, 'target', None) not in classify.NOCLOBBER:
                for r in classify.CALLER_SAVED:
                    st[r] = None
        elif m == 'mov' and len(o) == 2 and o[0] in st and o[1] in st:
            st[o[0]] = st[o[1]]
        elif m == 'mov' and len(o) == 2 and o[0] in st and frame and \
                re.fullmatch(r'dword ptr \[ebp \+ (0x[0-9a-f]+|\d+)\]', o[1]):
            n = int(_FRAME_SLOT.search(o[1]).group(1), 0)
            st[o[0]] = ('stack', (n - 8) // 4) if n >= 8 and n % 4 == 0 and n not in dirty else None
        else:
            try:
                _rd, wr = ins.regs_access()
                written = {classify._reg(ins.reg_name(r)) for r in wr}
            except Exception:
                written = set(st)
            for r in written:
                if r in st:
                    st[r] = None
        nxt = index.get(i.addr + i.size)
        if m.startswith('ret') or m in ('int3', 'hlt', 'ud2'):
            edges = []
        elif m == 'jmp':
            if i.target is not None:
                edges = [index[i.target]] if i.target in index else []
            else:
                edges = [index[t] for t in tables.get(i.table, ()) if t in index]
        elif m in classify._JCC:
            edges = ([index[i.target]] if i.target in index else []) + ([nxt] if nxt is not None else [])
        else:
            edges = [nxt] if nxt is not None else []
        for j in edges:
            old = state[j]
            new = dict(st) if old is None else {r: (old[r] if old[r] == st[r] else None) for r in regs}
            if new != old:
                state[j] = new
                work.append(j)
    at = {state[k]['eax'] for k, i in enumerate(insns) if i.mnemonic.startswith('ret') and state[k] is not None}
    # A jump out of the function is a tail call: it returns what the callee
    # leaves in EAX (AUTOMAP_SetMiniMapType, 0045a720: `mov eax, ecx` serves
    # its early ret only; the other path ends `jmp fn_00457640`).
    if any(i.mnemonic == 'jmp' and i.target is not None and i.target not in index and state[k] is not None
           for k, i in enumerate(insns)):
        at.add(None)
    return at.pop() if len(at) == 1 and None not in at else None


def split_params(params):
    out, depth, cur = [], 0, ''
    for c in params:
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
            continue
        depth += {'(': 1, ')': -1, '<': 1, '>': -1}.get(c, 0)
        cur += c
    if cur.strip():
        out.append(cur.strip())
    return [] if out == ['void'] else out


WORD_PARAM = re.compile(r'^(?P<type>(?:const\s+)?(?:unsigned\s+)?[A-Za-z_]\w*(?:\s*\*+\s*|\s+))(?P<name>[A-Za-z_]\w*)$')


def returned_param(src, leaf, arg, conv, own_convention=False):
    """(type, name) of the parameter `arg` (arg_at_returns) of `leaf` as
    defined in `src`, or None: a dword-sized parameter the body never
    assigns. Under a convention of the function's own, which parameter a
    register carries is not known: there it is the one such parameter."""
    m = re.search(def_pattern(leaf), src, re.M)
    if not m:
        return None
    params = split_params(m.group('params'))
    pm = [WORD_PARAM.match(p) for p in params]
    if not all(pm) or any(re.search(r'\b(?:double|float|int64_t|uint64_t|__int64|LONGLONG|ULONGLONG)\b', p.group('type'))
                          for p in pm):
        return None                      # not every argument one dword: the slots would not count right
    kind, k = arg
    if own_convention:
        if kind != 'reg':
            return None
        fit = [q for q in range(len(pm)) if returned_param(src, leaf, ('stack', q), 'YA')]
        return returned_param(src, leaf, ('stack', fit[0]), 'YA') if len(fit) == 1 else None
    if conv == 'YI':
        k = {'ecx': 0, 'edx': 1}.get(k) if kind == 'reg' else k + 2
    elif kind == 'reg':
        return None
    if k is None or k >= len(pm):
        return None
    ptype, name = pm[k].group('type').strip(), pm[k].group('name')
    if not re.search(r'\*|\b(?:int|int32_t|uint32_t|long|DWORD|BOOL|UINT|LPARAM|WPARAM|HANDLE|size_t)\b', ptype):
        return None                      # a byte or a word: EAX holds more than it
    close = match_close(src, m.end() - 1, '{', '}')
    body = strip_comments(src[m.end():close or len(src)])
    if re.search(r'(?:\+\+|--|&)\s*%s\b|\b%s\s*(?:[-+*/%%&|^]?=(?!=)|<<=|>>=|\+\+|--)' % (name, name), body):
        return None                      # assigned, or its address taken: returning it is not the argument
    return ptype, name


def retyped(texts, cpp, hdr, leaf, value, rtype='int'):
    """The texts with `leaf` returning `value` as `rtype`, or None."""
    src = texts[cpp]
    m = re.search(def_pattern(leaf), src, re.M)
    if not m:
        return None
    head = m.group('head')
    hm = re.match(r'(static\s+)?void(\s)', head)
    if not hm:
        return None
    open_at = m.end() - 1
    close = match_close(src, open_at, '{', '}')
    if close is None:
        return None
    body = src[open_at + 1:close]
    v = str(value)
    body = re.sub(r'\breturn\s*;', 'return %s;' % v, body)
    lines = body.rstrip().split('\n')
    if not lines[-1].strip().startswith('return'):
        first = next((l for l in body.split('\n') if l.strip()), '    ')
        ind = first[:len(first) - len(first.lstrip())] or '    '
        body = body.rstrip() + '\n' + ind + 'return %s;\n' % v
    new_head = head[:hm.start()] + (hm.group(1) or '') + rtype + hm.group(2) + head[hm.end():]
    out_src = src[:m.start()] + new_head + src[m.start() + len(head):open_at + 1] + body + src[close:]
    # Forward declarations in the .cpp (a static promotion may have added one).
    decl = re.compile(r'^(?P<pre>[ \t]*(?:static\s+)?)void(?P<mid>\s+(?:__\w+\s+)?)%s(?=\s*\([^;{}]*\);)' % re.escape(leaf), re.M)
    out_src = decl.sub(lambda d: d.group('pre') + rtype + d.group('mid') + leaf, out_src)
    out = {cpp: out_src}
    if hdr in texts:
        out[hdr] = decl.sub(lambda d: d.group('pre') + rtype + d.group('mid') + leaf, texts[hdr])
    return out


def retype(cpp, hdr, leaf, value, rtype='int'):
    return lambda texts: retyped(texts, cpp, hdr, leaf, value, rtype)


def candidates(rows, model, syms, status, pats):
    """tu -> [(original start, leaf, value)]."""
    tree = Tree()
    out = collections.defaultdict(list)
    objs = {}
    for s, _e, name, tu, sym, _m, _d in rows:
        if pats and not any(p in tu for p in pats):
            continue
        if status.get(s) in (None, 'MATCH', 'QUARANTINE', 'MISSING') or '@@' not in sym:
            continue
        rest = sym[sym.index('@@') + 2:]
        if rest[:1] != 'Y' or rest[2:3] != 'X':
            continue                         # a free function returning void, by its mangled name
        if tu not in objs:
            path = os.path.join(BUILD, 'obj', tu + '.obj')
            objs[tu] = {f.symbol: f for f in ourslib.load_object(os.path.join(BUILD, 'obj'), tu + '.obj')} \
                if os.path.exists(path) else {}
        f = objs[tu].get(sym)
        if f is None:
            continue
        ours_v, ref_v = compare.our_view(f, syms), compare.ref_view(s, model[s])
        vals, rtype = returned_values(ours_v, ref_v), 'int'
        if len(vals) == 1:
            at_rets = {v for _a, v in classify.eax_at_returns(model[s][0])}
        else:
            # A bool comes back in AL: `xor al, al` / `mov al, 1` before the epilogue.
            vals, rtype = returned_values(ours_v, ref_v, SET_AL), 'bool'
            at_rets = {v for _a, v in classify.eax_at_returns(model[s][0], 'al')}
            if len(vals) == 1 and next(iter(vals)) not in (0, 1):
                rtype = 'uint8_t'
        leaf = name.rsplit('::', 1)[-1]
        cpp, hdr = os.path.join(REPO, tu + '.cpp'), os.path.join(REPO, tu + '.h')
        if len(vals) != 1 or at_rets != {next(iter(vals)) & (0xff if rtype != 'int' else 0xffffffff)}:
            # Not one constant on every path: perhaps one argument.
            arg = arg_at_returns(model[s][0], own_convention=f.static)
            got = arg and returned_param(fixlib.read(cpp), leaf, arg, rest[:2], own_convention=f.static)
            if not got:
                continue
            rtype, v = got
            vals = {v}
        files = tree.where.get(leaf, set())
        if any(p.endswith('.h') and p != hdr for p in files):
            continue                         # declared in another header too
        if not address_safe(leaf, files):
            continue                         # its address is taken where the type would no longer fit
        v = vals.pop()
        out[tu].append((s, leaf, ('true' if v else 'false') if rtype == 'bool' else v, rtype))
    return out


def address_safe(leaf, files):
    """Every use of `leaf` other than a call names it under an explicit cast
    (`(void *)F`, `(PFN)&F`): a pointer of another type than the function's
    then still compiles, and code calling through it does not read EAX.
    An uncast use (an initialiser of a typed table) would no longer fit."""
    bare = re.compile(r'(?P<pre>\)\s*&?\s*)?\b%s\b(?!\s*\()' % re.escape(leaf))
    for p in files:
        for m in bare.finditer(strip_comments(fixlib.read(p))):
            if not m.group('pre'):
                return False
    return True


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
    print('%d functions in %d TUs' % (total, len(cands)))
    if not args.apply:
        for tu in sorted(cands):
            print(tu, ', '.join('%s->%s %s' % (leaf, t, v) for _s, leaf, v, t in cands[tu]))
        return 0
    quarantined = audit.load_quarantine(os.path.join(BUILD, 'quarantine.tsv'))
    groups = {}
    for tu in sorted(cands):
        cpp, hdr = os.path.join(REPO, tu + '.cpp'), os.path.join(REPO, tu + '.h')
        groups[tu] = ([fixlib.Edit(leaf, [s], [s], retype(cpp, hdr, leaf, v, t), require_gain=True)
                       for s, leaf, v, t in cands[tu]], [cpp, hdr])
    kept, gained = fixlib.apply_batch(groups, (rows, model, syms, quarantined), jobs=args.jobs,
                                      log=lambda m: print(m, flush=True))
    print('kept %d of %d edits, MATCH %+d' % (kept, total, gained))
    fixlib.record('fix_return', total, kept, gained)
    return 0


if __name__ == '__main__':
    sys.exit(main())
