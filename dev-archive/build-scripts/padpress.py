"""Virtual Xbox 360 pad for The Darkness bring-up - input route #1.

Creates the pad BEFORE the game starts (so SDL sees it at init), waits for the game
process to appear, then presses buttons on a fixed schedule measured from that moment.
Writes a timeline so presses can be lined up with window captures and the verbose log.
It never launches or closes the game - lookwin.ps1 does that.
"""
import ctypes, subprocess, sys, time
import vgamepad as vg

LOG = sys.argv[1] if len(sys.argv) > 1 else "padpress.log"
TOTAL = float(sys.argv[2]) if len(sys.argv) > 2 else 150.0

# (seconds after the game process appears, button, note)
SCHEDULE = [
    (55, "START", "title: press start"),
    (60, "A", "menu: select first item"),
    (65, "A", "confirm"),
    (70, "START", "in case the title only now accepts start"),
    (75, "A", "select"),
    (80, "A", "confirm / difficulty"),
    (85, "A", "confirm"),
    (90, "A", "last press, before the idle trailer at ~94 s"),
]
# Look test, after the level is in (it appeared at ~105 s last run). Small turns, per the
# standing rule: moderate deflection, short holds. (start, end, x, y, note) on the RIGHT stick.
LOOK = [
    (118.0, 119.0,  0.55, 0.0, "look RIGHT, small"),
    (126.0, 128.0, -0.55, 0.0, "look LEFT, past centre"),
    (136.0, 137.0,  0.0,  0.55, "look UP, small"),
]
BTN = {
    "A": vg.XUSB_BUTTON.XUSB_GAMEPAD_A,
    "START": vg.XUSB_BUTTON.XUSB_GAMEPAD_START,
}

def log(f, msg):
    line = f"{time.strftime('%H:%M:%S')} {msg}"
    print(line, flush=True)
    f.write(line + "\n"); f.flush()

def game_running():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq darknessrecomp.exe", "/NH"],
                         capture_output=True, text=True).stdout
    return "darknessrecomp.exe" in out

with open(LOG, "w", encoding="utf-8") as f:
    pad = vg.VX360Gamepad()
    pad.reset(); pad.update()
    log(f, "virtual Xbox 360 pad created (idle)")

    deadline = time.time() + 60
    while not game_running():
        if time.time() > deadline:
            log(f, "game never appeared - giving up"); sys.exit(1)
        time.sleep(0.2)
    t0 = time.time()
    log(f, "game process seen: t0")

    for at, name, note in SCHEDULE:
        while time.time() - t0 < at:
            if not game_running():
                log(f, f"game exited at t={time.time()-t0:.1f}s before press '{name}'"); sys.exit(2)
            time.sleep(0.1)
        pad.press_button(button=BTN[name]); pad.update()
        time.sleep(0.15)
        pad.release_button(button=BTN[name]); pad.update()
        log(f, f"t={time.time()-t0:5.1f}s pressed {name:<5} ({note})")

    for start, end, x, y, note in LOOK:
        while time.time() - t0 < start:
            if not game_running():
                log(f, f"game exited at t={time.time()-t0:.1f}s before look"); sys.exit(3)
            time.sleep(0.05)
        pad.right_joystick_float(x_value_float=x, y_value_float=y); pad.update()
        log(f, f"t={time.time()-t0:5.1f}s right stick x={x:+.2f} y={y:+.2f} ({note})")
        while time.time() - t0 < end:
            time.sleep(0.02)
        pad.right_joystick_float(x_value_float=0.0, y_value_float=0.0); pad.update()
        log(f, f"t={time.time()-t0:5.1f}s right stick released")

    while time.time() - t0 < TOTAL and game_running():
        time.sleep(0.5)
    log(f, f"done at t={time.time()-t0:.1f}s, game running={game_running()}")
    del pad
