# The developer menu opens; the two eyes look different ways (2026-10-24)

`/lm` on the home PC, Claude on Opus, one background reader.

## In one paragraph

Yesterday's Virtual Desktop update removed the home PC's ViGEm game-pad bus, so the usual virtual pad could not
start (`VIGEM_ERROR_BUS_NOT_FOUND`), and putting it back needs a Windows "Yes" that was not given. So the home PC's
port was rebuilt to match the dev PC (SDK patches 08, 08b, 08c added; the newest probe) with the MCP Game Bridge³ in
it, which presses buttons from inside the engine and takes pictures of its finished frames. That drove the game from
the title into the car scene with no window focus. The reader worked out how to give the game's empty `cheat()`
command a body, and with that hook **X on the main menu opens the retail developer menu**. The stereo pair test then
showed something new: on the home PC the two halves of a pair look in clearly different directions, with the
world-freeze on or off.

## What was measured

- Home PC build: SDK 760/760, game 232/232, no errors `[compile-verified 2026-10-24]`. Installed in its own folder,
  `build-home-bridge` (older builds untouched; the bridge-only exe kept in `_archive-2026-10-24-bridge-only`).
- Bridge driving: title ~48-51 s, main menu, car-scene camera at ~75 s `[verified-live 2026-10-24, n=6 runs]`.
- Developer menu: DevMenu draws (Load Scriptlayer / Zones / Soakmode / Unlock all / CUSTOMIZE CONTROLS / CCTest);
  Unlock all opens EXTRA CONTENT with everything available; Zones is empty `[verified-live 2026-10-24, n=3 / 1 / 1]`.
  The save files were not rewritten (backup kept beside them anyway).
- Stereo pair, car scene, H (hold on) and O (freeze off): each half steady, the halves looking different ways,
  log bookkeeping perfect (`same_eye_twice=0`, no orphans, `frame_start_dt` 0.00 ms) `[verified-live 2026-10-24,
  n=1 run each, by eye]`.
- Music muted in this PC's save profile (0.7 → 0.0, backup beside it) `[verified-numerically 2026-10-24]`.

## Driving lessons

- A colour count mistakes the intro films for the title; a saved title picture works.
- Recognising the save notice and the main menu by picture fails (the background drifts and glows). START once,
  then A every 4.5 s until the log shows a level camera, is reliable.
- Read the probe counters from the log files: the bridge's in-memory log is full of `[VP] PERSP` lines.
- In the side-by-side build a bridge picture is 2560 x 720, both eyes at full size.

## Not established

- Why the two eyes look different ways (the next job; harder than it looked).
- God mode, noclip and the free camera from the pause menu (`GAMEMENU2`), and a level jump with `DK_DEVMENU_MAP`.
- Whether the 2026-10-12 dev-PC result (same pose in both halves) and today's are the same scene moment.

## Lost and rebuilt

The reader's inbox file was deleted before it was folded (a script failed after the delete command had been
chained to it). Its content was rebuilt from the reader's final report into dossier §6c; nothing of substance is
missing, but the original wording is gone.
