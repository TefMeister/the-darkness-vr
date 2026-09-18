#!/usr/bin/env python3
"""Read the decrypted guest image by GUEST address.
Sections are loaded flat at base 0x82000000 (XEX images are one contiguous blob),
so guest A -> file offset A - 0x82000000.  Verified by the PE section table.
"""
import struct, sys, re

IMG = r"C:\Users\Tefa\AppData\Local\Temp\claude\D--Program-Files--x86--Steam-steamapps-common\5cc76241-ec9a-4a00-b537-308d0510b993\scratchpad\darkness_image.bin"
BASE = 0x82000000
_data = None

def data():
    global _data
    if _data is None:
        _data = open(IMG, 'rb').read()
    return _data

_secs = None
def _sectab():
    global _secs
    if _secs is None:
        _secs = sections()[1]
    return _secs

def off(a):
    """guest VA -> raw file offset, via the PE section table"""
    for name, va, vs, ra, rs, ch in _sectab():
        if va <= a < va + max(vs, rs):
            o = a - va
            if o >= rs:
                return None   # in the zero-filled tail (BSS)
            return ra + o
    return None

def rd(a, n):
    o = off(a)
    if o is None:
        return bytes(n)
    return data()[o:o+n]

def va_of_off(o):
    for name, va, vs, ra, rs, ch in _sectab():
        if ra <= o < ra + rs:
            return va + (o - ra)
    return None

def u32(a):  return struct.unpack('>I', rd(a,4))[0]
def s32(a):  return struct.unpack('>i', rd(a,4))[0]
def f32(a):  return struct.unpack('>f', rd(a,4))[0]
def f64(a):  return struct.unpack('>d', rd(a,8))[0]
def cstr(a, n=200):
    b = rd(a, n)
    z = b.find(b'\0')
    return b[:z if z>=0 else n].decode('latin1')

def sections():
    d = data()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    assert d[pe:pe+4] == b'PE\0\0', d[pe:pe+4]
    nsec = struct.unpack_from('<H', d, pe+6)[0]
    opt = struct.unpack_from('<H', d, pe+20)[0]
    imgbase = struct.unpack_from('<I', d, pe+24+28)[0]
    out = []
    for i in range(nsec):
        o = pe+24+opt+i*40
        name = d[o:o+8].rstrip(b'\0').decode('latin1')
        vsize, vaddr, rsize, raddr = struct.unpack_from('<IIII', d, o+8)
        chars = struct.unpack_from('<I', d, o+36)[0]
        out.append((name, imgbase+vaddr, vsize, raddr, rsize, chars))
    return imgbase, out

def find_float(val, tol=0.0, sec=None):
    """all guest addresses whose big-endian float equals val"""
    import struct as st
    pat = st.pack('>f', val)
    d = data(); res = []; i = 0
    while True:
        i = d.find(pat, i)
        if i < 0: break
        res.append(BASE + i); i += 1
    return res

def find_bytes(b):
    d = data(); res = []; i = 0
    while True:
        i = d.find(b, i)
        if i < 0: break
        res.append(BASE + i); i += 1
    return res

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'sections':
        ib, ss = sections()
        print('PE ImageBase 0x%08X' % ib)
        for n, va, vs, ra, rs, ch in ss:
            print(f"{n:10s} va=0x{va:08X} vsize=0x{vs:06X} raw=0x{ra:06X} rawsz=0x{rs:06X} chars=0x{ch:08X}")
    elif cmd == 'f':   # f <addr> [count]
        a = int(sys.argv[2], 0); n = int(sys.argv[3]) if len(sys.argv)>3 else 8
        for i in range(n):
            print('0x%08X  % .9g   0x%08X' % (a+4*i, f32(a+4*i), u32(a+4*i)))
    elif cmd == 'd':   # dump hex
        a = int(sys.argv[2], 0); n = int(sys.argv[3]) if len(sys.argv)>3 else 64
        b = rd(a, n)
        for i in range(0, len(b), 16):
            print('0x%08X  %s  %s' % (a+i, b[i:i+16].hex(' '),
                  ''.join(chr(c) if 32<=c<127 else '.' for c in b[i:i+16])))
    elif cmd == 's':   # string at addr
        print(repr(cstr(int(sys.argv[2],0))))
    elif cmd == 'findf':
        v = float(sys.argv[2])
        for a in find_float(v): print('0x%08X' % a)
    elif cmd == 'finds':
        for a in find_bytes(sys.argv[2].encode()): print('0x%08X %r' % (a, cstr(a)))
