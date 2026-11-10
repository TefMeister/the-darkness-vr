# The polite close was asked of the wrong window (2026-11-10, late, home PC, desk)

**The fault as written** (note 2026-11-10f, board row): in a level under the OpenXR Simulator the game does not
answer a polite close; every measuring run was ended by force after 24 s of asking.

**What was found** `[verified-live 2026-11-10, n=3 for the game window, n=1 for the preview window]`, b025, in the
first level, launched and driven by `dk_sim_drive.py`:

- Under the simulator the process shows TWO windows: the simulator's preview ("OpenXR Simulator - ...") and the
  game's own ("darknessrecomp [rexglue-...]"). The preview comes FIRST in the window list.
- A close message to the preview only: the preview goes away, the game plays on (still running after 25 s).
- A close message to the game's own window: closed in 0.7 s, three runs of three, with the game's own quit in the
  log (`Window closing, shutting down...`, `Execution complete`, `Title terminated`). No crash dump.

So the game does answer a polite close in a level. A script that asks "the process's window" asks the preview.
The afternoon's close script was not saved, so that it asked the preview is `[hypothesis]`; it fits "the menus
close fine" only if a different way of closing was used there.

**The repair.** `dev-archive/build-scripts/dk_close.py`: asks only the window whose title starts with
"darknessrecomp", then waits until no copy of the game is left (so the next launch never starts beside one still
closing), exit 0 or 1, never forces. Run on a real level: `closed after 0.7 s`.

**Also seen in these runs.** VR Bench's own picture command returned both eyes 3 of 3 with simulator 1.7.1; the
collector filed all three runs under `the-darkness-vr` (it had used the program's name since the builds got
their own numbered folders).
