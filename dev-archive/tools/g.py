#!/usr/bin/env python3
"""grep the generated disassembly comments and report the owning function."""
import os, re, sys, bisect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from idx import load, GEN

def main(pat, limit=200):
    rx = re.compile(pat)
    idx, per_file = load()
    n = 0
    for fn in sorted(os.listdir(GEN)):
        if not fn.endswith('.cpp'):
            continue
        marks = per_file.get(fn) or []
        lns = [m[0] for m in marks]
        with open(os.path.join(GEN, fn), 'r', encoding='utf-8', errors='replace') as f:
            for i, line in enumerate(f, 1):
                if not line.lstrip().startswith('//'):
                    continue
                t = line.strip()[2:].strip()
                if rx.search(t):
                    k = bisect.bisect_right(lns, i) - 1
                    owner = marks[k][1] if k >= 0 else '?'
                    print(f"{owner}\t{fn}:{i}\t{t}")
                    n += 1
                    if n >= limit:
                        return

if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 200)
