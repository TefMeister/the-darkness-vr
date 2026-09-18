#!/usr/bin/env python3
"""Find which recompiled functions reference a given guest address.

Scans every function's disassembly comments, tracking lis/addi/ori/lfs/lwz/stw
effective addresses, and reports functions whose computed EAs hit the target.
Static text analysis only.
"""
import os, re, sys, json, bisect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from idx import load, GEN

COMMENT = re.compile(r'^\s*//\s*(.*)$')
LIS  = re.compile(r'^lis\s+(r\d+),\s*(-?\d+)')
ADDI = re.compile(r'^addi\s+(r\d+),\s*(r\d+),\s*(-?\d+)')
ORI  = re.compile(r'^ori\s+(r\d+),\s*(r\d+),\s*(-?\d+)')
MEM  = re.compile(r'^(lfs|lfd|lwz|lha|lhz|lbz|ld|lwa|stfs|stfd|stw|sth|stb|std|lwzu|stwu)\s+'
                  r'(?:f\d+|r\d+),\s*(-?\d+)\((r\d+)\)')
DEF  = re.compile(r'^(?:DEFINE_REX_FUNC|PPC_FUNC_IMPL|PPC_FUNC)\(\s*(?:__imp__)?([A-Za-z_][A-Za-z0-9_]*)\s*\)')

def scan(targets, lo=None, hi=None):
    """targets: set of guest addresses. Returns {addr: [(func, insn)]}"""
    idx, per_file = load()
    hits = {t: [] for t in targets}
    tset = set(targets)
    for fn in sorted(os.listdir(GEN)):
        if not fn.endswith('.cpp'):
            continue
        cur = '?'
        regs = {}
        with open(os.path.join(GEN, fn), 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                m = DEF.match(line)
                if m:
                    cur = m.group(1); regs = {}; continue
                if line.startswith('loc_'):
                    regs = {}; continue
                c = COMMENT.match(line)
                if not c:
                    continue
                t = c.group(1).strip()
                m = LIS.match(t)
                if m:
                    v = (int(m.group(2)) << 16) & 0xFFFFFFFF
                    regs[m.group(1)] = v
                    if v in tset: hits[v].append((cur, t))
                    continue
                m = ADDI.match(t) or ORI.match(t)
                if m:
                    b = regs.get(m.group(2))
                    if b is None:
                        regs.pop(m.group(1), None)
                    else:
                        v = ((b + int(m.group(3))) & 0xFFFFFFFF) if t.startswith('addi') \
                            else (b | (int(m.group(3)) & 0xFFFF))
                        regs[m.group(1)] = v
                        if v in tset: hits[v].append((cur, t))
                    continue
                m = MEM.match(t)
                if m:
                    b = regs.get(m.group(3))
                    if b is not None:
                        ea = (b + int(m.group(2))) & 0xFFFFFFFF
                        if ea in tset: hits[ea].append((cur, t))
                # invalidate any GPR written by something we did not model
                mw = re.match(r'^[a-z][a-z0-9._]*\s+(r\d+)', t)
                if mw and not (LIS.match(t) or ADDI.match(t) or ORI.match(t)):
                    regs.pop(mw.group(1), None)
    return hits

if __name__ == '__main__':
    tg = [int(x, 0) for x in sys.argv[1:]]
    h = scan(set(tg))
    for a in tg:
        print('0x%08X:' % a)
        seen = set()
        for fnm, insn in h[a]:
            if fnm in seen: continue
            seen.add(fnm)
            print('   ', fnm, '|', insn)
