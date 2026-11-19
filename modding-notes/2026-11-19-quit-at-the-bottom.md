# 2026-11-19: QUIT at the bottom of the main menu (b034), and how the menu keeps its texts

/lm, Fable, Developer PC, the OpenXR Simulator with VR Bench, Tefa away (the full-auto run, game 5 of 6).
Build: port b034 = `build-home-xr-b034` (builds store `headset/v0.1.0-b035`), b033 + this. NOT WORN.

## What it does now `[verified-live 2026-11-19, simulator, n=2 runs]`

The main menu reads CONTINUE / NEW GAME / CHECKPOINTS / EXTRA CONTENT / OPTIONS / QUIT. Seen in the game's own
picture (the bridge's `shot`): EXTRA CONTENT opens its page (ACHIEVEMENTS / CREDITS) with the heading EXTRA CONTENT;
OPTIONS opens its page, whose MULTIPLAYER line reads MULTIPLAYER as shipped; QUIT opens "Are you sure you want to
quit?" NO / YES; YES ends the game ("Execution complete" in the log). Back from each page the main menu reads the same.

Not the exact order Tefa's words imply (OPTIONS before EXTRA CONTENT): see "The exact order" below.

## What was learned about the menu data

- The loaded menu is the big-endian copy of `CubeWnd.xcr`'s TEXT TABLE, sitting in the guest's physical memory (seen
  at guest 0xB03B77EA..0xB03B78C1 for the three lines, and again at +0x20000000 and +0x40000000: the console's mirrored
  ranges of the same memory, so one write is enough) `[measured live, n=3 runs, same addresses]`.
- **Each text is stored once and shared** `[measured live and in the file]`: `0, 12, 20, 1` (a line's place) is used by
  up to 17 lines on other pages; the Multiplayer label is also the MULTIPLAYER line of the OPTIONS page; the Extra
  Content label is also the heading of the EXTRA CONTENT page. So b028 to b033 showed "QUIT" as a line on the OPTIONS
  page (nobody had looked).
- **Nothing points at the texts** `[measured live: no 32-bit value equal to any of the six text addresses anywhere in
  the guest's memory]`. The reader's decoding of the file explains it: lines are tied to texts by 16-bit numbers in a
  record table before the text table.
- **The game copies a page's texts when it builds the page** `[inferred from live behaviour]`: a text changed before
  a page opens shows on that page; the main menu is rebuilt when a sub-page is left.
- **Menu clicks do not pass the console's ExecuteString** (`sub_8277B388`): with `DK_CMD_LOG=1` only our own
  `cg_submenu('extra_content')` showed there, never a click's `cg_submenu('options_main')` or `cg_prevmenu()`
  `[verified-live, n=1 run]`. The commands' own handlers are thunks to the front end's vtable, registered in
  `sub_8237E660`: `cg_switchmenu` 0x8237EAB8, `cg_submenu` 0x8237EAF8 (+128), `cg_clearmenus` 0x8237EB28 (+132),
  `cg_prevmenu` 0x8237EA78 (+136), `cg_rootmenu` 0x8237EB08 (+144) `[inferred-static, hooks verified-live for
  submenu / prevmenu / rootmenu]`.
- The main menu opening shows in the command log as `cg_rootmenu('MainMenu')` then
  `cg_rootmenu('MainMenu_GotCheckpoint')`: a sure "the main menu is up" mark.

## How b034 does it (`vr_menu_quit.cpp`, `vr_devmenu_unlock.cpp`)

1. Once after the front end loads (the old look-through), where all four original texts are found at their fixed
   distances (Multiplayer label -0x20, Multiplayer command 0, Extra Content label +0x7C, Extra Content command +0x9C),
   all four are rewritten together: place 4 gets Extra Content's label (it uses the one pad byte) and the command
   `cheat("dk_extra")` (Extra Content's real command is 27 characters, the place holds 25); the bottom place gets
   `LMENU_QUIT` and `cg_submenu('quit_ask')`. A stray part copy still gets the old b028 rewrite.
2. Our `cheat()` hook turns `dk_extra` into `cg_submenu('extra_content')` (with the developer-menu unlock on or off).
3. The two shared labels follow the page: hooks on the game's own `cg_submenu` / `cg_prevmenu` / `cg_rootmenu` /
   `cg_clearmenus` handlers set, before the command runs: inside EXTRA CONTENT the bottom text is EXTRA CONTENT again
   (the heading); inside OPTIONS place 4's text is MULTIPLAYER again; on the main menu EXTRA CONTENT and QUIT. The log
   says `[QUIT] menu labels set for ...` at each change.

## The exact order (not done)

The reader's route (note `2026-11-19-ideas-and-board-check.md`, section 6): rotate the three lines' VALUE NUMBERS in
the record table (16 bytes a line, both main-menu pages), no text length involved. Checked live: the record bytes
`1a 04 0a 0d 00 00 00 00 ba 05 1b 0d` are found ONCE in the running game, at guest 0xAFA85EBF, inside the
little-endian copy (the one the game does not draw from); the byte-swapped form is found nowhere `[measured live,
n=1]`. So by the time the menu is up the big-endian records have been taken apart: the change has to be made where
the file's bytes are read, before that. Not started.

## The quit crash is still there

QUIT -> YES in b034: the log ends "Execution complete" and Windows wrote a crash record, heap corruption in ntdll
(`0xC0000374`, ntdll+0x112FC5) `[verified-live 2026-11-19, simulator, n=1]`; four closes by the window the same day
left none. Same as b027's four dumps (dossier 6h).

## Tools made today

- `dev-archive/build-scripts/dk_sim_menu.py <build folder>`: starts the VR build on the simulator and stops ON the
  main menu, by the game's own command log (never presses A on the main menu).
- `dev-archive/build-scripts/dk_vb_press.py <button|up|down|left|right> [seconds] [r|l]`: one VR Bench press.
- `dev-archive/tools/dk_memfind.py text|hex|word|read`: a read-only look through the running game's memory.

## NOT established

- Whether the labels follow the page on the PAUSE menu's OPTIONS (in a level): the same hooks apply, not looked at.
- A page reached from inside EXTRA CONTENT or OPTIONS and left by anything other than `cg_prevmenu` (a film, the
  achievements screen): the depth count could drift; it resets at the next root menu.
- The four-line main menu (no save present): the same texts, so the same result is expected; not seen.
- Nothing here was worn.
