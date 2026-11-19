# 2026-11-19: ideas and board check (the /lm reader's report, condensed by the /lm session)

The one background reader of the /lm session of 2026-11-19 (read-only; nothing launched, nothing edited; about 15
minutes). Its report is second-hand; what the live session checked is said in the last section. Tables are the
reader's; wording shortened.

## 1. Ideas, one by one (for THIS game)

| # | Idea | Verdict | Evidence |
|---|---|---|---|
| 1 | Front page: how to get your disc onto the PC | NOT STARTED as a guide | README credits Redump / redumper / xdvdfs, no steps |
| 2 | Menus and logos on a screen in front | DONE for menus, films, logos; journal PARTLY | b021; Tefa: "like the start logos and stuff at the moment are already doing"; journal = b029, no word yet |
| 3 | Cutscenes: the game turns left / right only | NOT STARTED for cutscenes | b020 drops the right stick's up / down in a level only |
| 4 | The game's animations take over the hands, then hand back | NOT STARTED | no row or note |
| 5 | Two hands = no spread | NOT STARTED | no row |
| 6 | Controller drift canceller | NOT STARTED | nothing in the source or notes |
| 7 | Launcher to switch features | NOT STARTED | switches exist only as `DK_*` lines in the start scripts |
| 8 | Casings stay on the floor | NOT STARTED | the design note covers dropped guns only |
| 9 | Left- and right-handed profiles | NOT STARTED | no hit |
| 10 | VR comfort options | PARTLY | head walking, no stick pitch (b020), screen effects off, menu in the room; no snap turn, vignette or options page |

## 2. The board's rows by feature (line numbers of the board as it stood this morning)

| ID | Feature | Rows |
|---|---|---|
| DRK-001 | The opening sequence starts before the head is raised | 57 |
| DRK-002 | Hands follow the controllers / Hand Poser / hand size | 58, 66, 68, 69, 107, 112, 113, 116, 119, 122, 123, 124 |
| DRK-003 | Quest button layout / Darkness powers | 61, 62, 63, 95, 120, 121 |
| DRK-004 | One gun in the right hand | 64 |
| DRK-005 | QUIT on the main menu | 71, 72 |
| DRK-006 | The quit crash | 59, 83, 117 |
| DRK-007 | The journal in the room | 65 |
| DRK-008 | The menu as a screen in the room | 85, 96 |
| DRK-009 | The full headset view | 88, 91, 105, 106, 135, 140 |
| DRK-010 | Head walking / no stick pitch | 86, 90, 100, 136 |
| DRK-011 | Frame rate | 67, 73, 74, 75, 76, 79, 80, 81, 82, 84, 99, 114, 125, 138 |
| DRK-012 | Brightness / auto exposure / blacks | 70, 78, 104, 110, 111, 115, 134 |
| DRK-013 | Gun poses / which gun is held / aim | 87, 108, 118, 92 |
| DRK-014 | Screen effects off | 103, 109, 132 |
| DRK-015 | Reload by hand / weapon selection | 97, 98 (sub-lines only: need real rows) |
| DRK-016 | Test tools: level jump, developer menu | 77, 142, 143 |
| DRK-017 | Code shape / Inspector | 60, 127 |
| (none) | Old stereo and port groundwork | 126, 128-131, 133, 137, 139, 141, 144-150 |

Wear rows the reader found ANSWERED by Tefa in a later row: 63 (by the b031 row's quote), 62 ("LT stayed as it should,
awesome"), 88 ("without the 2d window and running well it's unbelivable how lived in this game world feels"), 104 /
110 / 115 ("lighting look so good now and the framerate is much better"). Merely old, so they stay: 58, 68, 65, 71,
85 (the re-centre), 86, 87, 108, 111 (the gamma dial), 112, 117.

## 3. Gate tags that looked wrong

No leading legal tag: 61, 66, 67, 77, 78, 84, 105, 106, 131 (start with a tick), 72 (struck), 90-102 (sub-lines).
70 `[PD]` says "grab both under the simulator" -> `[FLAT]`. 69 `[PD]` needs Tefa -> `[VR USER]`. 107 `[PD]`: its open
part is "to try in the headset". 139, 140 `[PD]` need the game -> `[FLAT]`. 103 `[VR USER]` says the pretend headset
is enough -> `[FLAT]`. 133 `[FLAT]` "needs ears" -> `[USER]`. 83, 130, 137 tagged but done. 57 `[PD]` likely needs a run.

## 4. Facts above the block that were untrue

Line 53 "the build to wear is b025 ... NOT WORN since b017": b025 was worn 2026-11-12, b030 and b032 on 2026-11-14,
port b033 installed 2026-11-15; "57 eye pairs a second" is the simulator's figure, the headset measured 72. Lines
37-42 ("stereo on Condemned 2 first") are history. The OPEN heading's date was older than its first row.

## 5. Code shape

Scan: nothing over 1,500 lines; one over 800: `sdk-patches/14-.../xr_output.cpp`, 924 lines, ours (fix later: split
the flat-screen part move-only when patch 15 touches it). Files with many loose numbers: keep (drawing coordinates,
file-format offsets, guest offsets commented in place, test data, the pad button table); `vr_hand_poser_link.cpp`:
fix later (three already named, I-0002).

`GetDeviceState` (`vr_bridge.cpp` 367-441) `[measured: read]`: the 7th level is lines 406-411 (the head-walk turn
inside the Quest branch). Move-only split: lines 388-417 (the Quest branch's body, without its `return`) into
`void ApplyQuestLayout(rex::input::X_INPUT_STATE* out, uint32_t b, const float ax[6], bool in_level)` after `g_quest`;
the lambdas `trig` and `axis` (383-384) become file-level functions (both branches use them); `static uint16_t
last_logged` moves with the body. The old b009 branch (420-436) the same way into `ApplyB009Map` brings the function
to 4 deep.

## 6. QUIT at the bottom: what the menu file is

`[measured: decoded, 9,882 records parsed cleanly; the last of 232 top-level pages did not parse and is not needed]`
Each of the file's two copies is: a 20-byte header, 232 page hashes, a RECORD TABLE (file 0x278 to 0x1C628 in the
first copy), then the TEXT TABLE. A record is 8 bytes: name index, value index, child count x 2, one more field
(16-bit each). A text is stored once and shared. There are two main-menu pages: `MainMenu_GotCheckpoint` (six lines)
and `MainMenu_NoCheckpoint` (four). A button holds CLASSNAME, TEXT, SCRIPT_PRESSED, RGN.

| Line | TEXT (length, value index, users) | SCRIPT_PRESSED (length, index, users) | RGN (index, users) |
|---|---|---|---|
| CONTINUE | `LMENU_CONTINUE` 19, 0DEA, 1 | `cachecommand("cg_blackloading(); ...` 138, 0E3E, 1 | `0, 6, 20, 1` 0E48, 7 |
| NEW GAME | `LMENU_NEWGAME` 18, 0CE0, 2 | `cg_submenu('newgame')` 21, 0CEF, 2 | `0, 8, 20, 1` 0CFA, 9 |
| CHECKPOINTS | `LMENU_CHECKPOINTS` 22, 0E58, 1 | `cg_submenu('select_savegame')` 29, 0E6B, 1 | `0, 10, 20, 1` 0D27, 9 |
| MULTIPLAYER | `LMENU_MULTIPLAYER` 22, 0D0A, 3 | `cg_submenu('multiplayer')` 25, 0D1B, 2 | `0, 12, 20, 1` 0809, 14 |
| OPTIONS | `LMENU_OPTIONS` 18, 0D36, 6 | `cg_submenu('options_main')` 26, 0D48, 4 | `0, 14, 20, 1` 0824, 10 |
| EXTRA CONTENT | `LMENU_EXTRACONTENT` 23, 0D57, 3 | `cg_submenu('extra_content')` 27, 0D6A, 2 | `0, 16, 20, 1` 0E77, 17 |

Options weighed: (a) rewriting the position texts: no, each is shared by 7 to 17 lines. (b) rotating labels and
commands: no, OPTIONS' command and EXTRA's label and command do not fit, and there is no shorter twin page. (c)
recommended: rotate the VALUE INDEX numbers in the records (TEXT record then SCRIPT_PRESSED record, 16 bytes a line):

| Window | Old | New |
|---|---|---|
| 4th | `1a 04 0a 0d 00 00 00 00 ba 05 1b 0d 00 00 00 00` | `1a 04 36 0d 00 00 00 00 ba 05 48 0d 00 00 00 00` (OPTIONS) |
| 5th | `1a 04 36 0d 00 00 00 00 ba 05 48 0d 00 00 00 00` | `1a 04 57 0d 00 00 00 00 ba 05 6a 0d 00 00 00 00` (EXTRA CONTENT) |
| 6th | `1a 04 57 0d 00 00 00 00 ba 05 6a 0d 00 00 00 00` | `1a 04 0a 0d 00 00 00 00 ba 05 1b 0d 00 00 00 00` (Multiplayer's texts = QUIT) |

First copy: six-line page at file 0x1944, 0x1978, 0x19AC; four-line page 0x16DC, 0x1710, 0x1744; second copy +0x30C0E
(its records byte-swapped). The 5th-window pattern also occurs in the two pause menus (0xD578, 0xD748): accept a hit
only with the 4th 0x34 before and the 6th 0x34 after. Not established from the file: whether the game keeps the
records packed in memory, in which byte order, whether the drawn copy has the same numbers (`GUIPrecache.XDF` holds
neither texts nor records in the clear), what the record's 4th field means, the navigation order.

## What the /lm session did with it

- **Ideas:** the list and the reader's verdicts are in the board's `[USER]` questions row for Tefa's numbers; nothing
  picked or dropped (Tefa's call).
- **Board:** re-cut the same day by one script that proved no line was lost: 32 finished or answered rows moved under
  "Done, moved out of the OPEN block on 2026-11-19" (the six wear rows above closed quoting Tefa; the b017 wear report
  with its sub-lines closed as finished, its open points carried by rows), seven gate tags corrected (57, 70, 103,
  139, 140 to `[FLAT]`, 133 to `[USER]`, 69 to `[VR USER]`), IDs DRK-001 to DRK-017 on every row of each feature and
  in `feature-ids.md`, two real rows written for DRK-015, the "Where it stands" line above the block brought up to
  date, the OPEN heading re-dated with no round brackets. 107 was left `[PD]` (not re-read).
- **QUIT at the bottom:** done in b034 before the report came, by rewriting the four texts in place and letting the
  two shared labels follow the page (note `2026-11-19-quit-at-the-bottom.md`); the order is EXTRA CONTENT / OPTIONS /
  QUIT. The reader's option (c) was checked live: the record bytes are found only once in the running game, in the
  little-endian copy the game does not draw from, so (c) needs the change made where the file is read. Left as a
  board row; Tefa is asked whether the order matters.
- **Code shape:** no split made today (the game-specific job and the saves came first; usage was short). The
  `GetDeviceState` split is on the `[FLAT]` "left for the simulator" row with the reader's exact lines; verdicts
  otherwise as in section 5.
- **Nothing static is left for the reader**; it was not started again.
