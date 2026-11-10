# One pass a frame: the console's anti-aliasing off (2026-11-10 afternoon, Fable, home PC, simulator; b023)

Tefa: *"can you work on the darkness and it's framerate next, it still works poorly"*. Last night's note (2026-11-10e):
every frame is drawn three times, in strips, because the 360 renderer asks for 4-sample anti-aliasing and that does not
fit the console's video memory in one go. Today: where that setting comes from, and it switched off.

## Where the strips come from `[inferred-static 2026-11-10]`, then `[verified-live 2026-11-10, simulator, n=1]`

- The console command `r_antialias` is registered in `sub_82241F98` (name at 0x82066614) with handler `sub_82240F28`:
  it stores the value at the renderer object's +3936, writes the setting `R_ANTIALIAS`, and raises +3956 (a "set the
  device up again" flag). `sub_82240C38` (start-up) fills +3936 from the option.
- The device set-up `sub_8223E268` (r3 = the renderer) turns +3936 into a sample setting (2 -> 1, 4 -> 2, else 0) and
  hands it in r9 to **`sub_8223D8E8`, the function that lays out the surfaces and the strips** (it doubles the height
  for setting 1 and both sides for setting 2 when it works out the video memory needed, and keeps the setting as the
  first field of the layout it returns). That function has FIVE callers: `sub_8223E268`, `sub_8223EB80`,
  `sub_8223F340`, `sub_8223FA80`, `sub_82254960`.
- **First try, not enough:** +3936 forced to 1 just before the device set-up. The log showed it (three set-ups, each
  "was 4, now 1") and the level still came in three strips of 4 samples, at ~40 game frames a second: another caller
  lays out the level's surfaces with its own setting.
- **The fix (b023):** a hook on `sub_8223D8E8` itself gives it sample setting 0 whoever calls
  (`vr_probe_screenfx.inc`; `DK_VR_NO_AA`, on unless set to 0). The log shows the callers asking 2, 2, 2, 0, 0, 2, 2,
  0, 0, 2 for 1280 x 720, all given 0.

## What it did (the alley after CONTINUE, the simulator's Quest-like profile, nobody in a headset)

| Build / setting | Game frames a second | Eye pairs a second | Card busy |
| --- | --- | --- | --- |
| b022 (three strips) | ~40 (37-43 today) | 18-21 | |
| **b023, drawn 2 x 3 larger (as b018 set it)** | **72** | **36** | 98 % |
| b023, 2 x 2 | 76 | 38 | 82 % |
| b023, the game's own size (1 x 1) | 99 | 48 | 64 % |
| b023, the port's other drawing path (`render_target_path_d3d12 = "rtv"`), 2 x 3 | 58 | 29 | 99 % |

`[measured 2026-11-10, simulator, n=1 each, 45-60 s in the level]`. "Resolve region is empty" and "Failed in backend":
**0** in b023 (thousands a second before): the picture is drawn once. A still of the alley looked right (the scene, the
light, the prompt text). The `rtv` path is slower here and logged 6 errors: `rov` stays.

## What limits it now

- At 2 x 3 the card is the limit (98 %); at 2 x 2 and below the game's own threads are (three threads at ~100 % of a
  core each, the card at 64-82 %). So the ceiling in the alley is about 100 game frames a second, whatever the card.
- **The headset shows half of that:** the game draws one eye per frame, so eye pairs = game frames / 2, and a headset
  that runs at 72 Hz snaps to 72, 36 or 24 pairs. b022 gave 18-21 in the alley (under 24); b023 gives 36 at 2 x 3 with
  nothing to spare, 38 at 2 x 2. 72 pairs would need 144 game frames a second: not in this place with one eye a frame.
- **The idea for the next big step `[hypothesis]`:** draw BOTH eyes from ONE game frame. The game hands the port one
  frame's drawing commands; the port already swaps the camera per eye in them (the eye-by-packet work). If the port
  ran that same command list a second time with the other eye's camera, a pair would cost one game frame, and the
  alley's ~76-99 game frames a second would be 76-99 pairs. The game's own three-strip replay was exactly such a
  re-run (by the console's design), which is why it cost what it did. A design job: what in a frame cannot simply be
  run twice (copies to textures, things drawn into off-screen pictures first, the exposure read-back).

## Tools fixed on the way

- `dev-archive/tools/idx.py` and `img.py` named the dev PC's folders (and `img.py` a file under a home folder): they
  now find the home PC's source (`D:/the-darkness-build/...`) and unpacked program by themselves, or take `DK_GEN` /
  `DK_IMG`.
- The game does not answer a polite close while it is in a level under the simulator: every run today had to be
  ended by force after 24 s of asking (the menus close fine). And twice a new copy was started while the old one was
  still there: wait until no `darknessrecomp.exe` is left before starting the next.

## The build

`C:/NonSteam/the-darkness/build-home-xr-b023` (exe 6c3e36590476; `Assets` a junction to `game-files`; the Desktop icon
starts the highest number, so it starts this one), also in `build-home-xr`; builds repo `headset/v0.1.0-b023` (b022,
the report-line build, was recorded late, just before it). The start scripts carry `DK_VR_NO_AA = "1"`.
