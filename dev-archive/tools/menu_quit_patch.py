"""menu_quit_patch.py <.../Assets/Content/Gui/CubeWnd.xcr> [--keep DIR] [--undo]  -- a QUIT line on the main menu
(2026-11-12, Tefa: "we do need to build a quit button into the main menu ... like an extention of the menu, same font if
doable"; "replace multiplayer with quit ... no one will play this multiplayer").

The game's own menu data already holds the question page `quit_ask` ("Are you sure you want to quit?" NO / YES; YES runs
quit(), which ends the port cleanly) and the word LMENU_QUIT; the console's main menu just never links to it. The
MULTIPLAYER line (Xbox Live: dead in the port) becomes QUIT:
    label    "nc, <mark>LMENU_MULTIPLAYER"  ->  "nc,        <mark>LMENU_QUIT"
    opens    "cg_submenu('multiplayer')"    ->  "cg_submenu('quit_ask')   "
The file (MOS DATAFILE2.0: a little-endian and a big-endian copy) is read front to back: 8 bytes of record, then the
text, null-ended and padded to an even length. So a new text must be EXACTLY as long as the old one; a shorter one padded
with nulls blacked the whole main menu [verified-live 2026-11-12]. Spaces pad it: after the label's comma (the file itself
has "nc,  " with two) and after the command. Only the FIRST of each in each copy (the main menu's line).

Run on a build's OWN copy of the file, never on the shared game files. The original is kept ONCE in the keep folder
(default: kept-originals beside the Assets folder), NEVER beside the file: the game loads every file in Content/Gui, and a
stray copy there stops it at start-up with a black window [verified-live 2026-11-12, 5 starts]. --undo puts it back."""
import os, shutil, sys

MARK = b"\xa7"


def same_length(old, head, tail=b""):
    return head + b" " * (len(old) - len(head) - len(tail)) + tail


LABEL = b"nc, " + MARK + b"LMENU_MULTIPLAYER"
OPENS = b"cg_submenu('multiplayer')"
PAIRS = [(LABEL, same_length(LABEL, b"nc, ", MARK + b"LMENU_QUIT")),
         (OPENS, same_length(OPENS, b"cg_submenu('quit_ask')"))]


def main():
    path = os.path.abspath(sys.argv[1])
    build = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(path))))   # <build>/Assets/Content/Gui/file
    keep_dir = sys.argv[sys.argv.index("--keep") + 1] if "--keep" in sys.argv else os.path.join(build, "kept-originals")
    keep = os.path.join(keep_dir, os.path.basename(path))
    if "--undo" in sys.argv:
        shutil.copyfile(keep, path)
        print("put back from", keep_dir)
        return 0
    d = bytearray(open(path, "rb").read())
    be = d.find(b"XCR_BE")
    split = int.from_bytes(d[be + 32:be + 36], "little") if be >= 0 else 0   # where the big-endian copy starts
    if not 0 < split < len(d):
        sys.exit("could not find the two copies; nothing changed")
    n = 0
    for lo, hi in ((0, split), (split, len(d))):
        for old, new in PAIRS:
            assert len(new) == len(old)
            i = d.find(old + b"\x00", lo, hi)
            if i < 0:
                continue
            d[i:i + len(old)] = new
            n += 1
    if n != 4:
        sys.exit("expected 4 places, found %d (already patched, or another file); nothing written" % n)
    os.makedirs(keep_dir, exist_ok=True)
    if not os.path.exists(keep):
        shutil.copyfile(path, keep)
    open(path, "wb").write(d)
    print("QUIT line written (4 places, size unchanged: %d bytes)" % len(d))
    return 0


if __name__ == "__main__":
    sys.exit(main())
