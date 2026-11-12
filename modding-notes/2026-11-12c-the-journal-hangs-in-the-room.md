# 2026-11-12: the journal hangs in the room (b029)

Tefa: *"the journal - pressing menu button on left controller - is following my head and i need it to just float in front
of me so i can freely look around, like the start logos and stuff at the moment are already doing"*.

- Why it followed the head: "a flat picture" was decided as "no camera was built in the last half second"
  (`vr_wide_view.cpp` InLevel). With the journal open in a level the camera is still built, so the picture stayed the
  level's wide, head-fixed one.
- The signal `[verified-live 2026-11-12, simulator, n=3 opens]`: every command text the game hands its console goes
  through `sub_8277B388` (now hooked in `vr_devmenu_unlock.cpp`; `DK_CMD_LOG=1` writes them down, repeats folded). The
  left menu button in a level: `releaseall()`, `deferredscriptgrabscreen("cg_cubeseq('fromgame'); cg_rootmenu('Darklings')")`,
  `pause(1)`; closing: `cg_cubeseqforce('backtogame')`, `pause(0)`. (While a front-end menu is up `pausetimer()` runs
  every frame.)
- The fix: `dkview::NoteCommand` keeps "paused" from `pause(1)` / `pause(0)`; InLevel() is false while paused, so
  `dk_xr_flat()` says "flat" and SDK patch 14 holds the picture in the room, at 0.75 size, exactly as it does the logos
  and the main menu; the widened view steps back to the game's own for that time.
- Checked: at the press the log says `[VIEW] paused ...` and 2 ms later `[XR] a flat picture began: held in the room`;
  at the second press `un-paused` and `the flat picture ended: back on the head`. The journal draws (INVENTORY / MAP /
  DARKNESS). The simulator's picture is the eye image itself, so it cannot show the screen staying put when the head
  turns: that is the headset's to confirm.
- Not checked: other things that pause (a film in a level, the game-over screen): they become flat screens too, which is
  what the rule wants.
