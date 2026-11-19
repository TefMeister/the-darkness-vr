"""dk_memfind.py -- look through the RUNNING game's memory from outside, read-only (2026-11-19).

    python dk_memfind.py text "cg_submenu('options')"      every place the text sits (host and guest address)
    python dk_memfind.py word 0x4012AB40 [0x4012AB5C ...]  every place a 32-bit big-endian value sits (who points at it)
    python dk_memfind.py read 0x4012AB40 [bytes=128]       a hex + text dump at a GUEST address
    python dk_memfind.py hex "00 02 0c f9"                 every place these bytes sit

Only reads (OpenProcess with read rights, ReadProcessMemory); never writes. The guest's 4 GB sit at one host base;
it is found by itself: the first text hit of the game's own name table is not needed, the base is the mapping whose
low part holds the XEX image "MZ"-less header at guest 0x82000000 (tried bases: 0x100000000 first, then every 4 GB
step). Guest addresses are printed when a hit lies inside that 4 GB. Used for: which menu record points at which
text (the QUIT line at the bottom of the main menu).
"""
import ctypes, ctypes.wintypes as wt, struct, subprocess, sys

EXE = "darknessrecomp.exe"
PROCESS_QUERY_INFORMATION, PROCESS_VM_READ = 0x0400, 0x0010
MEM_COMMIT = 0x1000
PAGE_GUARD, PAGE_NOACCESS = 0x100, 0x01
CHUNK = 16 * 1024 * 1024
GUEST_SIZE = 0x100000000
BASE_TRIES = [0x100000000 * k for k in range(1, 9)]
GUEST_IMAGE = 0x82000000        # the game program's first page in the guest

k32 = ctypes.WinDLL("kernel32", use_last_error=True)


class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_ulonglong), ("AllocationBase", ctypes.c_ulonglong),
                ("AllocationProtect", wt.DWORD), ("PartitionId", wt.WORD), ("RegionSize", ctypes.c_ulonglong),
                ("State", wt.DWORD), ("Protect", wt.DWORD), ("Type", wt.DWORD)]


k32.OpenProcess.restype = wt.HANDLE
k32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_ulonglong, ctypes.POINTER(MBI), ctypes.c_size_t]
k32.VirtualQueryEx.restype = ctypes.c_size_t
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_ulonglong, ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.POINTER(ctypes.c_size_t)]


def pid_of(exe):
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq " + exe, "/FO", "CSV", "/NH"], capture_output=True,
                         text=True).stdout
    for line in out.splitlines():
        parts = line.replace('"', "").split(",")
        if len(parts) > 1 and parts[0].lower() == exe.lower():
            return int(parts[1])
    sys.exit("the game is not running")


def read(h, addr, n):
    buf = ctypes.create_string_buffer(n)
    got = ctypes.c_size_t(0)
    if not k32.ReadProcessMemory(h, addr, buf, n, ctypes.byref(got)):
        return b""
    return buf.raw[:got.value]


def regions(h, lo, hi):
    a = lo
    m = MBI()
    while a < hi and k32.VirtualQueryEx(h, a, ctypes.byref(m), ctypes.sizeof(m)):
        ok = m.State == MEM_COMMIT and not (m.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        if ok:
            yield m.BaseAddress, m.RegionSize
        a = m.BaseAddress + m.RegionSize


def guest_base(h):
    for b in BASE_TRIES:
        if len(read(h, b + GUEST_IMAGE, 16)) == 16:
            return b
    sys.exit("could not find where the guest's memory sits")


def find(h, base, needles, align=1):
    """every place any of the byte strings sits inside the guest's 4 GB"""
    hits = []
    keep = max(len(n) for n in needles) - 1
    for start, size in regions(h, base, base + GUEST_SIZE):
        off, tail = 0, b""
        while off < size:
            n = min(CHUNK, size - off)
            data = read(h, start + off, n)
            if not data:
                off += n
                tail = b""
                continue
            blob = tail + data
            blob_at = start + off - len(tail)
            for needle in needles:
                i = blob.find(needle)
                while i != -1:
                    at = blob_at + i
                    if (at - base) % align == 0:
                        hits.append((at - base, needle))
                    i = blob.find(needle, i + 1)
            tail = blob[-keep:] if keep else b""
            off += n
    return sorted(set(hits))


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("text", "word", "read", "hex"):
        sys.exit(__doc__)
    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid_of(EXE))
    if not h:
        sys.exit("could not open the game for reading")
    base = guest_base(h)
    print("guest base 0x%X" % base)
    if sys.argv[1] == "text":
        for g, _ in find(h, base, [sys.argv[2].encode("latin-1")]):
            around = read(h, base + g - 16, 16 + len(sys.argv[2]) + 24)
            print("guest 0x%08X  before: %s  after: %r" % (g, around[:16].hex(" "), around[16 + len(sys.argv[2]):]))
    elif sys.argv[1] == "hex":
        for g, needle in find(h, base, [bytes.fromhex(sys.argv[2])]):
            ctx = read(h, base + g - 8, 8 + len(needle) + 16)
            print("guest 0x%08X  %s" % (g, ctx.hex(" ")))
    elif sys.argv[1] == "word":
        values = [int(v, 16) for v in sys.argv[2:]]
        for g, needle in find(h, base, [struct.pack(">I", v) for v in values], align=4):
            ctx = read(h, base + g - 16, 48)
            print("0x%08X is at guest 0x%08X  words around: %s" % (
                struct.unpack(">I", needle)[0], g, " ".join("%08X" % w for w in struct.unpack(">12I", ctx))))
    else:
        g = int(sys.argv[2], 16)
        n = int(sys.argv[3], 0) if len(sys.argv) > 3 else 128
        data = read(h, base + g, n)
        for i in range(0, len(data), 16):
            row = data[i:i + 16]
            print("0x%08X  %-47s  %s" % (g + i, row.hex(" "), "".join(chr(c) if 32 <= c < 127 else "." for c in row)))


main()
