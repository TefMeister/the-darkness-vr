#!/usr/bin/env python3
"""
find_missing_functions.py - bulk discovery of function starts ReXGlue's analyser missed.

WHY THIS EXISTS
---------------
The recompiled game dies with:

    [FATAL] Call to invalid or unregistered function at guest address 0xXXXXXXXX

That message is produced by exactly one place in the SDK: the REX_CALL_INDIRECT_FUNC
macro (darknessrecomp_pch.h), i.e. a `bctr` / `bctrl` / `blrl` whose runtime target has
no entry in the dispatch table.  So every one of these is an INDIRECT call target -
a function pointer the static analyser never saw.

ReXGlue v0.10.0 discovers functions from: the entry point, imports, .pdata, direct
`bl` targets, RTTI vtables, and gap fill.  Two sources it does NOT use:

  A. absolute 32-bit big-endian pointers stored in DATA sections (.rdata/.data/...)
     - i.e. non-RTTI vtables and static function-pointer tables.  The SDK's
       VTableScanner only finds vtables via MSVC RTTI Complete Object Locators, and
       this binary has ZERO of those (measured), so that whole pass finds nothing.
  B. addresses MATERIALISED in code by a `lis`/`addi` or `lis`/`ori` pair and then
     stored / moved to CTR / passed as an argument.  The SDK has this written
     (functionPointerScan in phase_discover.cpp) but it is COMMENTED OUT in
     analyze.cpp with "causes too many false positives".

This script reproduces both, offline, from the .xex - no build, no launch.

USAGE
-----
    pip install pycryptodome
    python find_missing_functions.py                      # print a report
    python find_missing_functions.py -o missing.toml      # also write a TOML fragment

Then paste the contents of missing.toml into darknessrecomp_config.toml under
[functions] (this script never writes to the config itself) and re-run codegen.

IMPORTANT
---------
Addresses that are already intra-function labels in the generated C++ (loc_XXXXXXXX)
are EXCLUDED.  Registering one of those as a function splits a real function and turns
its internal branches into unresolved calls - it makes things worse, not better.
"""

import argparse
import collections
import glob
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)

DEFAULT_XEX = os.path.join(PROJ, "..", "..", "game-files", "default.xex")
DEFAULT_GEN = os.path.join(PROJ, "generated", "default")

RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")
DEVKIT_KEY = bytes(16)


# ---------------------------------------------------------------- xex unpacking
def unpack_xex(path):
    """Decrypt + decompress a basic-compressed XEX2 into a flat RVA-indexed image."""
    from Crypto.Cipher import AES

    d = open(path, "rb").read()
    if d[:4] != b"XEX2":
        raise SystemExit("not an XEX2 file: " + path)
    be = lambda o: struct.unpack_from(">I", d, o)[0]

    pe_data_offset = be(8)
    sec_info_offset = be(16)
    opt_count = be(20)
    opts = {be(24 + i * 8): be(28 + i * 8) for i in range(opt_count)}

    si = sec_info_offset + 8                       # past size + image_size
    load_address = be(si + 264)
    file_key = d[si + 328:si + 344]

    ffi = opts[0x000003FF]                         # XEX_HEADER_FILE_FORMAT_INFO
    ffi_size, enc, comp = struct.unpack_from(">IHH", d, ffi)
    if comp != 1:
        raise SystemExit("compression type %d not supported (only 1 = basic)" % comp)

    blocks = []
    o = ffi + 8
    while o + 8 <= ffi + ffi_size:
        blocks.append(struct.unpack_from(">II", d, o))
        o += 8

    raw = d[pe_data_offset:]

    def build(key):
        out = bytearray()
        pos = 0
        cipher = AES.new(AES.new(key, AES.MODE_ECB).decrypt(file_key),
                         AES.MODE_CBC, iv=bytes(16)) if enc else None
        for ds, zs in blocks:
            chunk = raw[pos:pos + ds]
            pos += ds
            out += cipher.decrypt(chunk) if cipher else chunk
            out += bytes(zs)
        return bytes(out)

    for key in (RETAIL_KEY, DEVKIT_KEY):
        img = build(key)
        if img[:2] == b"MZ":
            return img, load_address
    raise SystemExit("could not decrypt: neither the retail nor the devkit key produced a PE")


def pe_sections(img, base):
    e = struct.unpack_from("<I", img, 0x3C)[0]
    n = struct.unpack_from("<H", img, e + 6)[0]
    opt = struct.unpack_from("<H", img, e + 20)[0]
    sh = e + 24 + opt
    secs = []
    for i in range(n):
        o = sh + i * 40
        name = img[o:o + 8].rstrip(b"\0").decode("latin1")
        vsize, vaddr, _rs, _rp = struct.unpack_from("<IIII", img, o + 8)
        chars = struct.unpack_from("<I", img, o + 36)[0]
        secs.append(dict(name=name, va=base + vaddr, size=vsize,
                         exec=bool(chars & 0x20000000)))
    return secs


# ---------------------------------------------------------------- scans
def scan_data_pointers(img, base, secs, is_exec):
    """A: absolute BE pointers into executable space held in data sections."""
    hits = collections.Counter()
    sites = collections.defaultdict(list)
    for s in secs:
        # .reloc is relocation metadata, .pdata is already consumed by the analyser.
        if s["exec"] or s["name"] in (".reloc", ".pdata"):
            continue
        start = s["va"] - base
        for o in range(start, start + s["size"] - 3, 4):
            v = struct.unpack_from(">I", img, o)[0]
            if v & 3 or not is_exec(v):
                continue
            hits[v] += 1
            if len(sites[v]) < 3:
                sites[v].append(base + o)
    return hits, sites


def scan_materialised(img, base, exec_ranges, is_exec, use_filter=True):
    """B: lis/addi and lis/ori pairs that build a code address, then store it,
    move it to CTR, or pass it in an argument register."""
    hits = collections.Counter()
    sites = collections.defaultdict(list)

    def word(a):
        return struct.unpack_from(">I", img, a - base)[0]

    for lo, hi in exec_ranges:
        words = [word(a) for a in range(lo, hi - 3, 4)]
        n = len(words)
        for i, w in enumerate(words):
            if (w & 0xFC1F0000) != 0x3C000000:      # lis rD,hi  ==  addis rD,r0,hi
                continue
            rd = (w >> 21) & 31
            himm = (w & 0xFFFF) << 16
            for j in range(i + 1, min(i + 33, n)):
                w2 = words[j]
                op = w2 >> 26
                target = None
                dest = None
                if op == 14 and ((w2 >> 16) & 31) == rd:          # addi rX,rD,lo
                    lo16 = w2 & 0xFFFF
                    target = (himm + (lo16 - 0x10000 if lo16 >= 0x8000 else lo16)) & 0xFFFFFFFF
                    dest = (w2 >> 21) & 31
                elif op == 24 and ((w2 >> 21) & 31) == rd:        # ori rX,rD,lo
                    target = (himm | (w2 & 0xFFFF)) & 0xFFFFFFFF
                    dest = (w2 >> 16) & 31
                if target is None:
                    # rD overwritten by another lis? stop tracking this one.
                    if (w2 & 0xFC1F0000) == 0x3C000000 and ((w2 >> 21) & 31) == rd:
                        break
                    continue
                if target & 3 or not is_exec(target):
                    break
                if use_filter and not _used_as_pointer(words, j, dest, n):
                    break
                hits[target] += 1
                if len(sites[target]) < 3:
                    sites[target].append(lo + i * 4)
                break
    return hits, sites


_STORE_OPS = {36, 37, 38, 39, 44, 45, 54, 55, 62}   # stw/stwu/stb/stbu/sth/sthu/std...


def _used_as_pointer(words, j, dest, n):
    """Within 8 instructions of the materialisation, is the value stored, moved to
    CTR, or left sitting in an argument register at a call?  (rexauto's filter.)"""
    for k in range(j + 1, min(j + 9, n)):
        w = words[k]
        op = w >> 26
        if op in _STORE_OPS and ((w >> 21) & 31) == dest:
            return True
        if op == 31 and ((w >> 1) & 0x3FF) == 467 and ((w >> 21) & 31) == dest:
            spr = ((w >> 16) & 31) | (((w >> 11) & 31) << 5)
            if spr == 9:                                   # mtctr
                return True
        if (w & 0xFC000001) == 0x48000001:                 # bl - argument register?
            return 3 <= dest <= 10
        if op == 31 and ((w >> 1) & 0x3FF) == 444 and ((w >> 16) & 31) == dest:
            continue                                       # mr rDest,rX (or.)
    return False


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xex", default=DEFAULT_XEX)
    ap.add_argument("--generated", default=DEFAULT_GEN)
    ap.add_argument("-o", "--out", help="write a [functions] TOML fragment here")
    ap.add_argument("--no-use-filter", action="store_true",
                    help="keep every lis/addi code address, even unused ones (noisy)")
    ap.add_argument("--all", action="store_true",
                    help="also emit candidates that are NOT preceded by a terminator")
    args = ap.parse_args()

    img, base = unpack_xex(args.xex)
    secs = pe_sections(img, base)
    exec_ranges = [(s["va"], s["va"] + s["size"]) for s in secs if s["exec"]]
    elo = min(a for a, _ in exec_ranges)
    ehi = max(b for _, b in exec_ranges)

    def is_exec(a):
        return elo <= a < ehi and any(l <= a < h for l, h in exec_ranges)

    def word(a):
        o = a - base
        return struct.unpack_from(">I", img, o)[0] if 0 <= o + 4 <= len(img) else None

    part = os.path.join(args.generated, "codegen.partition.json")
    registered = set(int(k, 16) for k in json.load(open(part))["assignments"])

    labels = set()
    rx = re.compile(r"loc_([0-9A-F]{8})")
    for f in glob.glob(os.path.join(args.generated, "*recomp.*.cpp")):
        with open(f, "r", errors="ignore") as fh:
            for line in fh:
                for m in rx.finditer(line):
                    labels.add(int(m.group(1), 16))

    print("image        : 0x%08X .. 0x%08X" % (base, base + len(img)))
    print("code         : 0x%08X .. 0x%08X (%d executable sections)"
          % (elo, ehi, len(exec_ranges)))
    print("registered   : %d functions" % len(registered))
    print("loc_ labels  : %d intra-function labels in the generated C++" % len(labels))

    A, Asites = scan_data_pointers(img, base, secs, is_exec)
    B, Bsites = scan_materialised(img, base, exec_ranges, is_exec,
                                  use_filter=not args.no_use_filter)
    print("\n[A] data-section pointers into code : %d distinct targets, %d sites"
          % (len(A), sum(A.values())))
    print("[B] materialised code addresses     : %d distinct targets, %d sites"
          % (len(B), sum(B.values())))

    TERM = {0x4E800020, 0x4E800420, 0x00000000, 0x60000000}   # blr, bctr, pad, nop

    def boundary(a):
        p = word(a - 4)
        if p is None:
            return False
        if p in TERM:
            return True
        return (p >> 26) == 18 and not (p & 1)                # plain `b` = tail call

    rows = []
    for a in sorted(set(A) | set(B)):
        if a in registered or a in labels:
            continue
        w = word(a)
        if w is None or w in (0x00000000, 0xFFFFFFFF):
            continue
        rows.append(dict(addr=a, data=A.get(a, 0), mat=B.get(a, 0), bnd=boundary(a),
                         sites=(Asites.get(a) or Bsites.get(a) or [])))

    strong = [r for r in rows if r["bnd"]]
    weak = [r for r in rows if not r["bnd"]]
    print("\ncandidates (unregistered, not an existing label): %d" % len(rows))
    print("  STRONG - previous word is blr/bctr/b/padding : %d" % len(strong))
    print("  WEAK   - lands mid-instruction-stream        : %d" % len(weak))
    print("  reached via data pointer                     : %d"
          % sum(1 for r in rows if r["data"]))
    print("  reached only via a materialised address      : %d"
          % sum(1 for r in rows if not r["data"]))

    emit = rows if args.all else strong
    if args.out:
        with open(args.out, "w") as fh:
            fh.write("# Generated by tools/find_missing_functions.py - do not hand-edit.\n")
            fh.write("# %d candidate function starts the ReXGlue analyser did not register.\n"
                     % len(emit))
            fh.write("# Evidence per line: D=n absolute pointers in data sections,\n")
            fh.write("#                    M=n lis/addi|ori materialisations in code.\n")
            fh.write("# All of these sit immediately after a blr/bctr/tail-branch/padding word\n")
            fh.write("# and none of them is already a loc_ label in the generated C++.\n\n")
            fh.write("[functions]\n")
            for r in emit:
                ev = []
                if r["data"]:
                    ev.append("D=%d" % r["data"])
                if r["mat"]:
                    ev.append("M=%d" % r["mat"])
                where = " ".join("0x%08X" % s for s in r["sites"][:2])
                fh.write('0x%08X = { name = "sub_%08X" }  # %s from %s\n'
                         % (r["addr"], r["addr"], ",".join(ev), where))
        print("\nwrote %s (%d entries)" % (args.out, len(emit)))
    else:
        for r in emit[:40]:
            print("  0x%08X  D=%d M=%d" % (r["addr"], r["data"], r["mat"]))
        if len(emit) > 40:
            print("  ... %d more (use -o to write them all)" % (len(emit) - 40))


if __name__ == "__main__":
    main()
