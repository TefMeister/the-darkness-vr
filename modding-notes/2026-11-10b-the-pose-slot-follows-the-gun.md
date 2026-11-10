# The pose slot follows the gun by itself, and a way to look inside the game's memory (2026-11-10, b019, Fable, simulator)

Board row: "the gun poser: which gun is in the hand?" (each gun needs its own pose, switched by itself).

## The tool: guest memory through the bridge (`vr_mem_probe.cpp`)

`gread <addr> [words]`, `gsnap <addr> <len>` (keeps the two newest copies), `gdiff [max]` (the words that differ),
`gfind <value> <addr> <len>`. Reads go through ReadProcessMemory on our own process, so a bad address is an error line,
not a crash. Nothing writes. Use: snapshot, do one thing in the game, snapshot, diff.

## Found `[measured 2026-11-10, simulator, n=2 runs]`

The player's ClientData = the hand IK object - 4384. Two snapshots of its first 0x4000 bytes with nothing happening
differ in 25 words (clocks, positions). After ONE pull of the right trigger from empty hands (the guns come out):

- **+0x1BE8: 0 -> 6**, and stays 6 (12 s later still 6);
- +0x1BC4: 0 -> 0x000C000C (two 16-bit counts of 12: the two pistols' clips), +0x1BC8 = 0xC4, +0x1C04 = 0x00820082;
- +0x2A0: 0xFFFF0000 -> a number that keeps changing (a handle or a clock, not a gun);
- a block at +0x2640..+0x2790 fills in (pointers and small counts: the weapon animation state by the look of it).

While the level loads the same word runs through 7 and then 6 within 26 ms (read from whoever the hand code had picked
at that moment), so it is only believed from the player's own object and after it has held for half a second.

**What +0x1BE8 is, is not settled:** 0 = empty hands is certain; 6 = "the two pistols are out". Whether another gun
gives another number (a weapon id) or the same 6 (a state) cannot be told on this save: it holds one weapon (Y, the
game's weapon change, did nothing). `[hypothesis]` either way.

## Built: the slot follows it (`vr_hand_pose.cpp`, `pose.auto` = 1)

0 -> slot 0 (the empty-hands pose). A number met for the first time takes the gun slot that was in use when the pose
file was saved (Tefa's hand-posed pistols, slot 2), else the next free one, and is remembered in `dk_hand_pose.txt`
(`gun <slot> <number>`). Not in pose mode (there Tefa picks). Seen: start of the alley `pose.held 0, pose.set 0`; the
trigger: `pose.held 6, pose.set 2`. `[verified-live 2026-11-10, simulator, n=1]`. **Not worn.**

## Next

A save (or the dev menu's give) with a second kind of gun: one `gsnap` / `gdiff` round with each gun out says whether
+0x1BE8 tells them apart, and if not, which word near it does (the block at +0x2640 is the first place to look).
