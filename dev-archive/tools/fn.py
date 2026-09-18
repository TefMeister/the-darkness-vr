#!/usr/bin/env python3
"""Print a recompiled function as PPC assembly (from the // comments), with
lis/addi and lis/lfs pairs resolved to real guest addresses and the constant
values read out of the decrypted image."""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from idx import load, GEN
import img

COMMENT = re.compile(r'^\s*//\s*(.*)$')
LABEL = re.compile(r'^(loc_[0-9A-Fa-f]+):')

def asm(name):
    idx, _ = load()
    if name not in idx:
        raise SystemExit(f"{name} not found")
    fn, a, b = idx[name]
    out = []
    with open(os.path.join(GEN, fn), 'r', encoding='utf-8', errors='replace') as f:
        lines = f.readlines()
    for ln in lines[a-1:b]:
        m = COMMENT.match(ln)
        if m:
            out.append(('i', m.group(1).rstrip()))
            continue
        m = LABEL.match(ln)
        if m:
            out.append(('l', m.group(1)))
    return fn, a, b, out

LIS = re.compile(r'^lis\s+(r\d+),\s*(-?\d+)')
ADDI = re.compile(r'^addi\s+(r\d+),\s*(r\d+),\s*(-?\d+)')
ORI = re.compile(r'^ori\s+(r\d+),\s*(r\d+),\s*(-?\d+)')
LOAD = re.compile(r'^(lfs|lfd|lwz|lha|lhz|lbz|ld|lwa)\s+(f\d+|r\d+),\s*(-?\d+)\((r\d+)\)')
STORE = re.compile(r'^(stfs|stfd|stw|sth|stb|std)\s+(f\d+|r\d+),\s*(-?\d+)\((r\d+)\)')

def render(name, annotate=True):
    fn, a, b, ops = asm(name)
    print(f"; {name}  [{fn}:{a}-{b}]")
    hi = {}
    for kind, t in ops:
        if kind == 'l':
            print(t + ':')
            hi = {}
            continue
        note = ''
        m = LIS.match(t)
        if m:
            hi[m.group(1)] = (int(m.group(2)) << 16) & 0xFFFFFFFF
            note = ' ; = 0x%08X' % hi[m.group(1)]
        else:
            m = ADDI.match(t) or ORI.match(t)
            if m and m.group(2) not in hi:
                hi.pop(m.group(1), None)
                m = None
            if m and m.group(2) in hi:
                v = (hi[m.group(2)] + int(m.group(3))) & 0xFFFFFFFF if ADDI.match(t) \
                    else (hi[m.group(2)] | (int(m.group(3)) & 0xFFFF))
                hi[m.group(1)] = v
                note = ' ; = 0x%08X' % v
            else:
                m2 = LOAD.match(t)
                if m2 and m2.group(4) in hi:
                    ea = (hi[m2.group(4)] + int(m2.group(3))) & 0xFFFFFFFF
                    note = ' ; @0x%08X' % ea
                    try:
                        if m2.group(1) == 'lfs':
                            note += ' = %.9g' % img.f32(ea)
                        elif m2.group(1) == 'lfd':
                            note += ' = %.17g' % img.f64(ea)
                        elif m2.group(1) in ('lwz','lwa'):
                            note += ' = 0x%08X' % img.u32(ea)
                        elif m2.group(1) == 'lbz':
                            note += ' = 0x%02X' % img.rd(ea,1)[0]
                    except Exception:
                        pass
                else:
                    m3 = STORE.match(t)
                    if m3 and m3.group(4) in hi:
                        note = ' ; @0x%08X' % ((hi[m3.group(4)] + int(m3.group(3))) & 0xFFFFFFFF)
            # any instruction writing a GPR we did not model invalidates it
            mw = re.match(r'^[a-z.]+\s+(r\d+)', t)
            if mw and not (LIS.match(t) or ADDI.match(t) or ORI.match(t)) and mw.group(1) in hi:
                del hi[mw.group(1)]
        print('    ' + t + note)

if __name__ == '__main__':
    for n in sys.argv[1:]:
        render(n)
