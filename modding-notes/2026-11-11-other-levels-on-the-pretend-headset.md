# Other levels on the pretend headset: b025 looks the same under both drawing methods, and the velocity pass is not the lever (2026-11-11)

Home PC (the Developer PC), Claude on Fable, OpenXR Simulator + VR Bench, nobody in the headset. Test copies only
(`build-test-b025`, `build-test-b026` beside the installed builds); the installed build is still b025, untouched.

## What was asked, and what was already there

Tefa asked for "each eye its own view" (the "2D view with depth", the small hands). Checked first: that was built
on 2026-11-10 as b018 (the game builds each eye's whole field itself, 108 x 109 degrees) and is inside b025; it has
not been worn. The two board rows that still described it as open were stale. A still from the simulator shows both
eyes filled edge to edge in every level seen today `[verified-live 2026-11-11, simulator, n=5 levels]`. Whether it
feels right, and whether the hands still look small, needs the headset.

## A level jump that works unattended

`DK_DEVMENU=1` + `DK_DEVMENU_MAP=<map>`, then the game's X on the main menu (the Quest's B through VR Bench): the
level loads `[verified-live 2026-11-11, n=30 runs]`. New tool `dev-archive/build-scripts/dk_place.py`: it drives the
menus in an order that can never press CONTINUE by mistake, waits for the log's `[VIEW] ... (widened)` line (the
level is live), takes pictures, reads the frame rate, and closes the game by its own window.

- Load: `NY1_Chinatown` (subway stairs), `NY1_Pier19` (a street at night), `NY1_Cemetery` (a yard, broken glass
  roof), `OW1_Cannon` (the otherworld), `NY1_Tunnel` (the opening car ride, black at the start).
- Do NOT load: `NY1_Orphanage`, `NY2_Turkish`, `NY2_Church`. Each asks for `content\textures\alltextures.000.xtc`,
  the open fails, the game calls its "dirty disc" screen and sits black at about 500 frames a second
  `[verified-live 2026-11-11, n=2 each]`. The game copy has `AllTextures.000.xtc.xt0` / `.xt1` (and 001) but no plain
  `.xtc` file, while `GUI.xtc` has all three. Whether the file is missing from the disc copy or is made by the game
  on first use is not established.

## b025 under `rtv` against `rov`, beyond the alley

Same exe, one toml line changed, pictures taken once the level was live `[measured 2026-11-11, simulator, n=1 pair
each; frame rates n=2 each, the first batch agreeing within 1]`:

| place | `rtv` game frames/s (eye pairs) | `rov` | picture difference (mean, of 255) |
| --- | --- | --- | --- |
| Chinatown, subway stairs | 115 (58) | 75 (37) | 1.2 |
| Pier 19, street | 149 (74) | 109 (54) | 0.5 |
| Cemetery, yard | 180 (90, the simulator's cap) | 111 (55) | 0.5 |
| Otherworld (Cannon) | 153 (77) | 91 (45) | 0.3 |

Pixels differing by more than 24 of 255: 0.09 % at most (the idle sway of the hands). Nothing missing, banded or
wrongly blended in these four stills. NOT seen: a darkness power in use, water, an enemy, blood decals being made,
the pause menu, anything moving. The labelled GPU trace under `rtv` was not taken.

## The black pictures were the preview window's

Half the window grabs came back all black, under both methods. The game's own per-eye brightness log
(`DK_XR_EYE_LIGHT=1`) read 0.118 for every one of 12 x 300 pairs over 40 s at Pier 19 while 12 of 20 grabs taken in
the same 40 s were black `[verified-live 2026-11-11, n=1 run]`. So the picture sent to the headset does not blink;
the preview does. Menus and films (the screen held in the room) never show in the preview.

## The velocity pass: found, switchable, and no gain where measured

Lead from `/gr` (the other public port: the "MotionMap" pass is drawn even with motion blur off, 17-23 % of their
draws). In our image `sub_82276148` is a function, stores the tag pointer 0x8206764C = "XRShader_MotionMap" into its
batch header, has an empty-list exit that is only the epilogue, and two callers (`sub_825CF178`, `sub_825BD270`) that
use no return value `[inferred-static 2026-11-11]`. New file `vr_no_velocity.cpp`: with `DK_VR_NO_VELOCITY=1` the
routine returns at once `[compile-verified 2026-11-11]`.

Measured in a test copy, on / off in the same exe `[measured 2026-11-11, simulator, n=1 each]`: Chinatown 116.2 /
115.7, the otherworld 151.2 / 149.1 game frames a second; about 1,200 batches a second skipped, each with ONE list
entry; the picture the same. Run-to-run spread here is about 1, so this is no gain. In a standing scene the pass is
one object a call; the other port's saving was on a route with many moving things. So the switch is OFF by default
and no build was made from it. Open: count the list entries in a busy scene (enemies, the car ride) before deciding.

## Not established

- How the full view looks and feels in the headset; the hands' size.
- `rtv` with powers, water, enemies, decals, the pause menu.
- Where the three maps' texture file should come from.
- Whether skipping the velocity pass pays anywhere.
