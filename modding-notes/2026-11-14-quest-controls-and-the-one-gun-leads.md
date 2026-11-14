# 2026-11-14: Tefa's Quest controls (job 1 built), and two leads for the one gun

Developer PC, Opus, no game launched (Fallout held the PC). Design: `dev-archive/design/2026-11-14-quest-controls-design.md`;
plan: `dev-archive/design/2026-11-14-quest-controls-job1-plan.md`.

## The game's own pad layout

The game's controls page (`Content/Gui/CubeWnd.xcr`, `gamepad_config_templates_1`, preset 1) places these labels on the
controller picture `[inferred-static 2026-11-14, from label positions; not yet checked in play]`:
Back journal, Start pause, LT secondary fire, RT primary fire, LB drain (the Darkness out / in), RB the Darkness (use
the power), left stick move, left stick click crouch, right stick look, right stick click aim, Y jump, A use, B reload,
X redirect (darklings), D-pad up / down a power, D-pad left / right a weapon. There are six presets
(`option_setdefaultbinds(1..6)`).

## Job 1 built (not yet run)

- `dkqc::QuestControls` (`port-project/src/vr_quest_controls.cpp`): Tefa's layout as presses of that pad; 19 tests
  (`build-scripts/test_quest_controls.bat`) `[compile-verified 2026-11-14, tests pass]`.
- SDK patch 17: the left thumb rest as bit 0x0400 (Touch only; Touch suggested again without it if refused).
- Wired into `BridgePadDriver`; `DK_VR_QUEST_CTL=0` = the b009 map. Log line `[DKXR] quest pad 0x....` per change.
- Test copy `C:/NonSteam/the-darkness/build-test-b030` (b029 + this). Test: `build-scripts/dk_controls_test.py`.
- Because LT now goes to RB (the power), the game's secondary fire (the left pistol) is never pressed any more.
- Our launch scripts now refuse while another game holds the PC (`dk_clash.py`, Lanes `clash.py check live`).

## Lead 1: one gun in the right hand

The player's character template `CharPlayer` (`Content/Registry/Sv.xcr`) carries the flags
`nohud+alwaysvisible+autounequipweapons+dualwieldsupported` `[measured 2026-11-14, strings]`. Dropping
`dualwieldsupported` from the player is the first thing to try for "one gun only" `[hypothesis]`. The loose menu files
were not what the game read for the QUIT line (2026-11-12), so this may need the same in-memory edit. The two pistols'
magazines are two 16-bit counts at ClientData + 0x1BC4 (12 and 12) `[measured 2026-11-10]`.

## Lead 2: the game already throws away empty guns

Many weapon templates carry `throwawayonempty` (most with `nolastthrowaway`, `cloneattachitem`, `autopickup`)
`[measured 2026-11-14, strings]`: the game's own "empty gun is thrown away" (probably the flip Tefa saw) and its own
pickups. That is where the "guns as world objects" look (job 4) starts.

### Lead 1, narrowed (static, later the same day)

The game program holds the character flag names as one table of 30 pointers at 0x82AE5910 (stunnable, waitspawn, ...,
autounequipweapons = 25, **dualwieldsupported = 26**, projectileinvulnerable, attachforcetriggers, disableik)
`[measured 2026-11-14, unpacked image]`. If the parser sets bit N for name N, dual wield is bit 26 = 0x04000000 of the
character's flags word `[hypothesis]`; `rlwinm rX,rY,6,31,31` reads exactly that bit, and `sub_821DC6F8` does so
`[inferred-static, one candidate, not followed]`. Live test (next): find the player's flags word (snapshot / diff with
the bridge's `gsnap` / `gdiff`, or the word that holds bits 8 and 25 set: alwaysvisible, autounequipweapons), clear
bit 26, draw the pistols: one pistol? The weapon flag names `throwawayonempty` / `nolastthrowaway` sit elsewhere
(0x82069E6C / 0x82069E5C).

In the journal (a paused level) the in-level button map stays on, so Y and the left trigger send LB / RB there: they may switch
the journal's pages. Noticed while reviewing; harmless, see on the wear.

## Live, the same evening (simulator, NY3_Mansion by the level jump, test copy build-test-b030 + `gwrite`)

- **The power picks work.** With the Darkness out (pad LB), D-pad up changed the power twice: the power icon (top left)
  and the colour of the heads' glow went yellow -> red -> green `[verified-live 2026-11-14, simulator, n=2]`. Tefa's
  b030 wear was early in the game, with one power. Pictures `D:/the-darkness-build/pics/2026-11-14-live/pp.png`.
- **The player's flags word is found:** the object at ClientData + 0x10, + 0x2B4, reads exactly 0x06000100 = bits 8, 25,
  26 = alwaysvisible + autounequipweapons + dualwieldsupported (the table order holds) `[verified-live, n=1]`.
  Cleared live (0x02000100), then the weapon stepped away and back: the two pistols came out as before `[n=1]`. So the
  flag is read somewhere else (at spawn, at pick-up) or is not what puts the left pistol in the hand. Next: clear it
  before the level spawns the player (from the start of the run), or find its readers.
- **The held word:** ClientData + 0x1BE8 reads 0x00820000 with the two pistols (magazines 7 / 7 at + 0x1BC4) and
  0x00D50000 for the other choice D-pad right gives (one count, 7; nothing seen in the hands) `[measured, n=2 each]`.
  The HUD showed two weapon icons with their own counts, left and right: each hand has its own gun and ammo in the game.
- New tools: `gwrite` on the bridge (refuses unless the game starts with DK_GWRITE=1); `dk_place.py --keep` leaves the
  game running in the level for live work.

## The second live run (evening), and b032

- **"The Darkness is out" is found:** the player's ClientData + 0x1DF0 gains bits 0x00300000 while the Darkness is out
  (0x000000D2 -> 0x003000D2; 0x74 -> 0x300074 after a respawn); five toggles in a row, pictures agree
  `[verified-live 2026-11-14, simulator, n=5]`. Both of the player's IK objects' ClientData carry it; `ik.player`
  picks one of them.
- **b032:** the left trigger uses the power only while that bit is set (`dkpose::DarknessOut`, read each solve from the
  player's ClientData by `vr_hand_ik.cpp`; no reading for 1 s = as before). Controls test: 17 of 17 gestures right,
  including "left trigger with the Darkness away = nothing" `[verified-live 2026-11-14, simulator, n=1]`. Installed
  `build-home-xr-b032`.
- **The dual-wield flag is NOT what gives two pistols.** After a checkpoint restart the player is rebuilt with the flag
  set again (from the template); cleared at once, before any weapon was drawn, the D-pad still brought out the pair
  (0x82, magazines 7 / 7) `[verified-live, n=1]`. The pair looks like a weapon of its own (0x82) beside a one-magazine
  choice (0xD5). Next for one gun: what 0xD5 is (nothing showed in the hands), and the left gun's model / its slot in
  the per-hand weapon data, rather than the character flag.
- Tefa: **B skips cutscenes** (in the Quest layout: B tap = pad B).
