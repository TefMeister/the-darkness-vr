"""Set The Darkness's MUSIC volume to 0 in the save profile, leaving effects and speech alone.

    python set_music_zero.py            (default profile path below; game must be CLOSED)
    python set_music_zero.py <path to _profile>

The profile is a list of named options (SND_VOLUMESFX, SND_VOLUMEMUSIC, GAME_BLOOD, ...). Floats are
little-endian; the first 4 bytes are a CRC-32 of everything after them, stored big-endian (checked
against the dev-PC file, 2026-10-12). This finds SND_VOLUMEMUSIC by name, sets it to 0.0, recomputes
the CRC, and keeps a backup next to the file. Run it with the game closed: the game writes this file
when options are saved.
"""
import os, shutil, struct, sys, time, zlib

DEFAULT = os.path.expandvars(
    r"%USERPROFILE%\Documents\darknessrecomp\B13EBABEBABEBABE\545407EE\00000001\DEFAULT\_profile")
KEY = b"SND_VOLUMEMUSIC"
TYPE_FLOAT = 0x0C
ENTRY_TAG = 0x0040


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT
    data = bytearray(open(path, "rb").read())
    if struct.unpack(">I", data[:4])[0] != zlib.crc32(bytes(data[4:])):
        raise SystemExit("CRC does not match before editing: unknown file layout, nothing changed")
    p = data.index(KEY) - 8
    tag, typ, name_len = struct.unpack_from("<HHI", data, p)
    if tag != ENTRY_TAG or typ != TYPE_FLOAT or name_len != len(KEY):
        raise SystemExit("unexpected entry layout, nothing changed")
    at = p + 8 + ((name_len + 3) // 4) * 4
    old = struct.unpack_from("<f", data, at)[0]
    backup = f"{path}.bak-{time.strftime('%Y%m%d-%H%M%S')}"
    shutil.copy2(path, backup)
    struct.pack_into("<f", data, at, 0.0)
    struct.pack_into(">I", data, 0, zlib.crc32(bytes(data[4:])))
    open(path, "wb").write(data)
    print(f"SND_VOLUMEMUSIC {old:.3f} -> 0.0 at offset 0x{at:X}; backup {backup}")


if __name__ == "__main__":
    main()
