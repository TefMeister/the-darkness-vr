#!/usr/bin/env python3
"""Callers/callees over the generated recomp sources, using the function index."""
import os, re, sys, json, bisect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from idx import load, GEN

CALLG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "callgraph.json")
CALL = re.compile(r'^\t(?:__imp__)?([A-Za-z_][A-Za-z0-9_]*)\(ctx, base\);')

def build():
    idx, per_file = load()
    callees = {}
    callers = {}
    for fn, marks in per_file.items():
        if not marks:
            continue
        lns = [m[0] for m in marks]
        path = os.path.join(GEN, fn)
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            for i, ln in enumerate(f, 1):
                m = CALL.match(ln)
                if not m:
                    continue
                k = bisect.bisect_right(lns, i) - 1
                if k < 0:
                    continue
                owner = marks[k][1]
                tgt = m.group(1)
                callees.setdefault(owner, set()).add(tgt)
                callers.setdefault(tgt, set()).add(owner)
    callees = {k: sorted(v) for k, v in callees.items()}
    callers = {k: sorted(v) for k, v in callers.items()}
    json.dump({'callees': callees, 'callers': callers}, open(CALLG, 'w'))
    return callees, callers

def load_cg():
    if os.path.exists(CALLG):
        d = json.load(open(CALLG))
        return d['callees'], d['callers']
    return build()

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'build':
        ce, cr = build(); print('nodes', len(ce), len(cr))
    else:
        ce, cr = load_cg()
        if cmd == 'callers':
            for n in sys.argv[2:]:
                print(n, '<-', cr.get(n, []))
        elif cmd == 'callees':
            for n in sys.argv[2:]:
                print(n, '->', ce.get(n, []))
        elif cmd == 'up':   # up <name> <depth>
            n = sys.argv[2]; d = int(sys.argv[3]) if len(sys.argv) > 3 else 3
            seen = {n}; frontier = [n]
            for lvl in range(d):
                nxt = []
                for x in frontier:
                    for c in cr.get(x, []):
                        if c not in seen:
                            seen.add(c); nxt.append(c)
                print('level', lvl+1, len(nxt), nxt[:60])
                frontier = nxt
