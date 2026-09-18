#!/usr/bin/env python3
"""Rough reader for the .xcr registry files (MOS DATAFILE2.0).

Layout observed by hand: a NUL-terminated key name, padded to a 2-byte boundary,
then a 2-byte word, then a 4-byte little-endian value, then a 2-byte type:
  0x0A = int32, 0x0C = float32, 0x02 = inline NUL-terminated string.
Only used to READ shipped data; nothing is written.
"""
import sys, struct, re

def entries(path):
    d = open(path, 'rb').read()
    out = []
    for m in re.finditer(rb'[A-Za-z_][A-Za-z0-9_]{2,63}\x00', d):
        name = m.group()[:-1].decode('latin1')
        p = m.end()
        if p % 2:
            p += 1
        # 2-byte word then value
        if p + 8 > len(d):
            continue
        w = struct.unpack_from('<H', d, p)[0]
        val_off = p + 2
        raw = d[val_off:val_off+4]
        typ = struct.unpack_from('<H', d, val_off+4)[0]
        if typ == 0x0A:
            v = struct.unpack('<i', raw)[0]
        elif typ == 0x0C:
            v = struct.unpack('<f', raw)[0]
        elif typ == 0x02:
            z = d.find(b'\0', val_off)
            v = d[val_off:z].decode('latin1')
        else:
            continue
        out.append((m.start(), name, typ, v, w))
    return out

if __name__ == '__main__':
    path = sys.argv[1]
    pat = re.compile(sys.argv[2], re.I) if len(sys.argv) > 2 else None
    for off, name, typ, v, w in entries(path):
        if pat and not pat.search(name):
            continue
        t = {0x0A: 'int', 0x0C: 'float', 0x02: 'str'}[typ]
        print('%08X  %-34s %-6s %r' % (off, name, t, v))
