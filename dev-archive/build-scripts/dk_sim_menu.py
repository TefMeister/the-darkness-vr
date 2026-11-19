"""dk_sim_menu.py <build folder> -- start The Darkness VR on the pretend headset and stop ON THE MAIN MENU (2026-11-19).

Clears VR Bench's old hand file (a file left by the last run, even another game's, is still obeyed), switches VR Bench
on for the folder's exe, starts dk_vr_sim_hands.ps1 with DK_CMD_LOG=1, holds the menu button on the title (= START),
then presses A on the save notice until the game's own log shows the main menu's opening command
(`cg_rootmenu('MainMenu')`), so A is never pressed on the main menu itself (where it is CONTINUE). Exit 0 = on the main menu.
Afterwards: dk_vb_press.py for single presses, dk_close.py to end, `vrbench.py stop`.
"""
import glob, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
VB = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench")
sys.path.insert(0, VB)
sys.path.insert(0, HERE)
import vb_input as I, vb_settings as S
import dk_clash

TITLE_WAIT_S = 54        # logos and films before the title takes START
START_HOLD_S = 1.2
A_EVERY_S = 5.0
A_TRIES = 5
MENU_MARK = "cg_rootmenu('MainMenu"   # the game's own command when the main menu opens (seen in the log 2026-11-19)


def press(name, hold):
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **{name: True}))
    time.sleep(hold)
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **{name: False}))


def menu_seen(folder, since):
    for p in glob.glob(os.path.join(folder, "logs", "darknessrecomp*.log")):
        if os.path.getmtime(p) >= since:
            with open(p, encoding="utf-8", errors="replace") as f:
                if any(MENU_MARK in line and "[CMD]" in line for line in f):
                    return True
    return False


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    folder = os.path.abspath(sys.argv[1])
    exe = os.path.join(folder, "darknessrecomp.exe")
    script = os.path.join(folder, "dk_vr_sim_hands.ps1")
    if not (os.path.isfile(exe) and os.path.isfile(script)):
        sys.exit("no darknessrecomp.exe / dk_vr_sim_hands.ps1 in " + folder)
    dk_clash.take_or_exit()                      # one game at a time on this PC
    if os.path.exists(S.INPUT_FILE):
        os.remove(S.INPUT_FILE)
    r = subprocess.run([sys.executable, os.path.join(VB, "vrbench.py"), "start", exe], capture_output=True, text=True)
    print(r.stdout.strip() or r.stderr.strip(), flush=True)
    if r.returncode != 0:
        sys.exit(1)
    started = time.time()
    env = dict(os.environ, DK_CMD_LOG="1")
    p = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script],
                       capture_output=True, text=True, env=env)
    print(time.strftime("%H:%M:%S"), "launched", p.stdout.strip(), flush=True)
    time.sleep(TITLE_WAIT_S)
    press("menu", START_HOLD_S)
    for k in range(A_TRIES):
        time.sleep(A_EVERY_S)
        if menu_seen(folder, started):
            print(time.strftime("%H:%M:%S"), "on the main menu after", k, "press(es) of A", flush=True)
            return
        press("a", 0.2)
    time.sleep(A_EVERY_S)
    if menu_seen(folder, started):
        print(time.strftime("%H:%M:%S"), "on the main menu", flush=True)
        return
    sys.exit("the main menu's command never showed in the log: look before pressing more")


main()
