"""dk_clash.py -- one game at a time on this PC: the Lanes clash guard for our launch scripts (2026-11-14).

Asked for by another session on Tefa's behalf (Fallout was in the real headset; VR Bench presses keys into whatever
window is in front): every script that starts The Darkness (dk_sim_drive.py, dk_try.py, dk_place.py, dk_controls_test.py)
first runs `clash.py check live`, refuses on exit 1, and holds the lock with `clash.py take live`; dk_close.py gives it
back once the game is gone, and only when one of our scripts took it (a marker file), so it never frees an /lm or /ms
session's lock.

    import dk_clash; dk_clash.take_or_exit()     # before the launch
    import dk_clash; dk_clash.release()          # after the game is closed
"""
import os, subprocess, sys, tempfile

CLASH = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/clash.py")
MARKER = os.path.join(tempfile.gettempdir(), "dk_clash_taken.txt")
TAKEN_ENV = "DK_CLASH_TAKEN"   # set for child scripts, so a script started by another does not take it twice


def take_or_exit():
    if os.environ.get(TAKEN_ENV) == "1":
        return
    r = subprocess.run([sys.executable, CLASH, "check", "live"], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout.strip() or r.stderr.strip(), flush=True)
        sys.exit("refused: another game holds this PC (clash.py check live); nothing launched")
    r = subprocess.run([sys.executable, CLASH, "take", "live"], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout.strip() or r.stderr.strip(), flush=True)
        sys.exit("refused: could not take the one-game lock; nothing launched")
    with open(MARKER, "w", encoding="utf-8") as f:
        f.write(str(os.getpid()))
    os.environ[TAKEN_ENV] = "1"


def release():
    if not os.path.exists(MARKER):
        return
    subprocess.run([sys.executable, CLASH, "release", "live"], capture_output=True, text=True)
    os.remove(MARKER)
    os.environ.pop(TAKEN_ENV, None)
