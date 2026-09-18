#!/usr/bin/env python3
"""Minimal XEX2 -> raw PE image extractor.

Handles encryption_type 1 (AES-128-CBC with a session key wrapped by the retail
key) and compression_type 1 ("basic": a list of (data_size, zero_size) blocks).
Static file work only - nothing is executed.
"""
import struct, sys, hashlib
from Crypto.Cipher import AES

RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")
DEVKIT_KEY = bytes(16)

def u32(b, o):
    return struct.unpack_from('>I', b, o)[0]

def unpack(path, out):
    d = open(path, 'rb').read()
    assert d[:4] == b'XEX2', d[:4]
    module_flags = u32(d, 4)
    pe_off = u32(d, 8)
    sec_off = u32(d, 0x10)
    hdr_count = u32(d, 0x14)
    headers = {}
    for i in range(hdr_count):
        k = u32(d, 0x18 + i*8)
        v = u32(d, 0x18 + i*8 + 4)
        headers[k] = v
    ff = headers.get(0x000003FF)
    assert ff, "no file format info"
    info_size = u32(d, ff)
    enc = struct.unpack_from('>H', d, ff+4)[0]
    comp = struct.unpack_from('>H', d, ff+6)[0]
    print(f"module_flags=0x{module_flags:08X} pe_off=0x{pe_off:X} sec_off=0x{sec_off:X} "
          f"enc={enc} comp={comp} info_size={info_size}")
    load_addr = u32(d, sec_off + 0x110)
    image_size = u32(d, sec_off + 0x04)
    file_key = d[sec_off+0x150: sec_off+0x160]
    print(f"load_address=0x{load_addr:08X} image_size=0x{image_size:X} file_key={file_key.hex()}")

    body = d[pe_off:]
    if enc == 1:
        for name, k in (("retail", RETAIL_KEY), ("devkit", DEVKIT_KEY)):
            sk = AES.new(k, AES.MODE_ECB).decrypt(file_key)
            trial = AES.new(sk, AES.MODE_CBC, iv=bytes(16)).decrypt(
                body[:0x200] if len(body) >= 0x200 else body)
            if trial[:2] == b'MZ' or trial[:4] == b'XUIZ' or True:
                pass
            print(f"  {name} session key {sk.hex()} -> first bytes {trial[:8].hex()}")
        # choose by which gives a sane basic-block layout / MZ
        chosen = None
        for name, k in (("retail", RETAIL_KEY), ("devkit", DEVKIT_KEY)):
            sk = AES.new(k, AES.MODE_ECB).decrypt(file_key)
            t = AES.new(sk, AES.MODE_CBC, iv=bytes(16)).decrypt(body[:16])
            if t[:2] == b'MZ':
                chosen = (name, sk); break
        if chosen is None:
            print("WARNING: neither key yields 'MZ'; defaulting to retail")
            chosen = ("retail", AES.new(RETAIL_KEY, AES.MODE_ECB).decrypt(file_key))
        print("using", chosen[0])
        n = len(body) & ~0xF
        body = AES.new(chosen[1], AES.MODE_CBC, iv=bytes(16)).decrypt(body[:n])

    if comp == 1:
        nblocks = (info_size - 8) // 8
        out_buf = bytearray()
        p = 0
        for i in range(nblocks):
            ds = u32(d, ff + 8 + i*8)
            zs = u32(d, ff + 8 + i*8 + 4)
            print(f"  block {i}: data=0x{ds:X} zero=0x{zs:X}")
            out_buf += body[p:p+ds]
            out_buf += bytes(zs)
            p += ds
    elif comp == 0:
        out_buf = bytearray(body)
    else:
        raise SystemExit(f"compression type {comp} not supported")

    open(out, 'wb').write(out_buf)
    print(f"wrote {out} size=0x{len(out_buf):X} first={bytes(out_buf[:4])!r}")
    print(f"image base 0x{load_addr:08X}; guest addr A maps to file offset A-0x{load_addr:08X}")

if __name__ == '__main__':
    unpack(sys.argv[1], sys.argv[2])
