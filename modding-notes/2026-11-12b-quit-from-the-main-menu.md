# 2026-11-12: quit from the main menu (b027's launch scripts), and what a real QUIT line still needs

Tefa: *"we do need to build a quit button into the main menu"*; *"like an extention of the menu, same font if doable ...
but if looks too much work then a rectangle that says hold Y to quit ... is absolutelly fine"*; *"replace multiplayer
with quit and move it to the bottom if you need a slot and can't add one, no one will play this multiplayer"*.

## What works now `[verified-live 2026-11-12, simulator, n=2]`

The game's menu data (`Content/Gui/CubeWnd.xcr`) holds a page `quit_ask`: "Are you sure you want to quit?" NO / YES in
the game's own font and look, YES runs `quit()`. The console's main menu never links to it. `quit()` ends the port
cleanly (log "Execution complete", process gone). The main menu's X doorway already goes through our `cheat()` hook, so
the launch scripts (`dk_vr_headset_hands.ps1`, `dk_vr_sim_hands.ps1`) now set `DK_DEVMENU=1`,
`DK_DEVMENU_MAIN_CMD=cg_submenu('quit_ask')`, `DK_DEVMENU_GAME_CMD=noop()` unless a test set its own command. In the
headset: on the main menu the Quest's B (the game's X) opens the question; stick down to YES, A.
Not there yet: a visible line on the main menu saying so.

## The real QUIT line: what was tried, what it needs

Plan: the MULTIPLAYER line becomes QUIT by overwriting its two texts in place (`dev-archive/tools/menu_quit_patch.py`):
label `nc, <mark>LMENU_MULTIPLAYER` -> `LMENU_QUIT` (the word exists), command `cg_submenu('multiplayer')` ->
`cg_submenu('quit_ask')`. Learned `[verified-live 2026-11-12]`:
- The file is two copies (little- and big-endian; the second starts at the u32 at "XCR_BE" + 32). Texts are null-ended,
  padded to an even length, each after 8 bytes of record, read front to back: a new text must be exactly as long as the
  old (spaces pad it; the tool does that).
- The game loads EVERY file in `Content/Gui`: a stray backup copy there stopped it at start-up, black window, five
  starts. Backups go in `<build>/kept-originals`.
- A build made with `cp -r` has its OWN 6.7 GB copy of the game files (b025's `Assets` is a junction to the shared
  `game-files`; `build-test-b027` and `build-home-xr-b027` are real copies, by my mistake, useful here: the shared files
  stay untouched).
- With BOTH `CubeWnd.xcr` and `MP_CubeWnd.xcr` patched correctly the main menu STILL shows MULTIPLAYER and opens the
  multiplayer page. So the menu the game draws does not come from these loose files. The text is nowhere else in the
  game files in the clear: it is packed (an XDF precache, most likely `GUIPrecache.XDF`) `[inferred]`.
- Next: either find and patch the packed copy, or overwrite the two texts in the game's memory once the menu is loaded
  (a scan for `cg_submenu('multiplayer')` after the front end is up). Moving the line to the bottom = swapping the
  rows' cell texts (`0, 8, 20, 1` and so on) the same way. `MODEL: OPUS`.
The test copy's two files were put back (hashes equal the shared ones).

## The visible QUIT line: done in memory (b028, same evening) `[verified-live 2026-11-12, simulator, n=2]`

`port-project/src/vr_menu_quit.cpp` (`DK_MENU_QUIT=1`, set by the launch scripts): a thread of ours looks through the
game's committed memory 3 s after the first frame and rewrites the Multiplayer line's two texts, same length, spaces as
padding: label -> `LMENU_QUIT`, command -> `cg_submenu('quit_ask')`. Found at the first look: 2 labels, 1 command. It
looks five more times, then stops (a look goes through all the game's memory and must not run into play). The main menu
shows QUIT in the game's own font where MULTIPLAYER was (the 4th line); A opens the question; YES ends the game cleanly.
The B button on the main menu still opens the same question.

- Not done: QUIT at the BOTTOM of the list. The lines keep their places; swapping the texts needs OPTIONS' and EXTRA
  CONTENT's commands to fit Multiplayer's 25 characters (they are 26 and 27). Way in: swap the three lines' text POINTERS
  in the loaded menu tree (find what points at each string). `MODEL: OPUS`.
- The picture taker (the simulator window grabbed through winshot) gives all-black pictures in some runs from a few
  seconds after the first one, with the game running and answering normally: a capture fault, not the game's.
- The two builds that had their own copy of the game files are links to the shared files again (Tefa's yes, 13.5 GB
  back); b028 was made with a link from the start.
