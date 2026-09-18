#!/usr/bin/env python3
"""Index the recompiled sources: function -> (file, start line, end line).
Also: which function encloses a given file:line.  Static text work only."""
import os, re, sys, json, bisect

GEN = r"E:\the-darkness\src\DarknessRecomp\generated\default"
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "funcidx.json")

DEF = re.compile(r'^(?:DEFINE_REX_FUNC|PPC_FUNC_IMPL|PPC_FUNC)\(\s*(?:__imp__)?([A-Za-z_][A-Za-z0-9_]*)\s*\)')

def build():
    idx = {}   # name -> [file, startline, endline]
    per_file = {}  # file -> sorted list of (line, name)
    for fn in sorted(os.listdir(GEN)):
        if not fn.endswith('.cpp'):
            continue
        path = os.path.join(GEN, fn)
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
        marks = []
        for i, ln in enumerate(lines, 1):
            m = DEF.match(ln)
            if m:
                marks.append((i, m.group(1)))
        for j, (ln, name) in enumerate(marks):
            end = marks[j+1][0]-1 if j+1 < len(marks) else len(lines)
            idx[name] = [fn, ln, end]
        per_file[fn] = marks
    json.dump({'idx': idx, 'per_file': per_file}, open(CACHE, 'w'))
    return idx, per_file

def load():
    if os.path.exists(CACHE):
        d = json.load(open(CACHE))
        return d['idx'], d['per_file']
    return build()

if __name__ == '__main__':
    idx, per_file = load()
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'stats'
    if cmd == 'build':
        idx, per_file = build(); print('functions:', len(idx))
    elif cmd == 'where':      # where <name>
        for n in sys.argv[2:]:
            print(n, idx.get(n))
    elif cmd == 'at':         # at <file> <line>
        fn, line = sys.argv[2], int(sys.argv[3])
        marks = per_file[fn]
        lns = [m[0] for m in marks]
        k = bisect.bisect_right(lns, line) - 1
        print(marks[k][1] if k >= 0 else '?')
    elif cmd == 'body':       # body <name> [start] [count]
        n = sys.argv[2]
        fn, a, b = idx[n]
        with open(os.path.join(GEN, fn), 'r', encoding='utf-8', errors='replace') as f:
            L = f.readlines()
        s = a + (int(sys.argv[3]) if len(sys.argv) > 3 else 0)
        e = min(b, s + (int(sys.argv[4]) if len(sys.argv) > 4 else (b-a+1)))
        sys.stdout.write(''.join(L[s-1:e]))
    else:
        print('functions:', len(idx), 'files:', len(per_file))
