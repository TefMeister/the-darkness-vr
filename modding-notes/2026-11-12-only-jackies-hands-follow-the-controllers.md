# 2026-11-12: only Jackie's hands follow the controllers (b027)

Tefa, b025 worn 2026-11-12: *"when i get close to npc's, their hands copy mine, but crossed"*. Developer PC, Opus then
Fable, simulator, test copy `build-test-b027`, then installed as `build-home-xr-b027` (the Desktop shortcut picks the
highest number).

## Cause `[measured]` from the code and that wear's log

`vr_hand_ik.cpp` picked "the player" as the character whose shoulders are nearest the camera and then fed every hand IK
object on the same skeleton instance OR within `kSameCharacter` (8 units) of the player's distance from the camera. The
skeleton-instance test never held (the player's two objects logged as two characters 36,000 times in the 10-minute
wear), so the distance rule did all the work, and a person whose shoulders came within ~53 units (1.3 m) of the camera
got the controller targets too. Crossed, because they face the player.

The kinds of character have their own ClientData vtables: `0x82061FE0` the player's two objects (New York and the
otherworld alike), `0x82064FD0` everyone else (13,707 object lines against 2,313), `0x820647C0` seen 16 times.

## Fix

Only objects of kind `0x82061FE0` are picked, and only objects of the picked kind are fed. The seen-list holds 32
objects (was 8: a street scene overflowed it 16,000 times). The "player changed" log line is by kind and nearness now.
A first version fell back to "anyone" when no player-kind object was near: in the opening car ride (the camera 53-70
units from Jackie, then further) it picked a passenger 15 units away; the fallback is gone.

## Checked `[verified-live 2026-11-12, simulator, n=1 each]`

- `NY1_Tunnel` (the car ride, passengers at 15-18 units): Jackie picked, then "nobody near the camera" when the camera
  leaves him; never a passenger.
- `NY1_Pier19`: Jackie picked once, his hands fed (2,735 of 9,000 calls), 146 game frames a second as before.
- Not checked: a person walking up to the player in the headset (Tefa's case itself): the wear decides.

## Builds store

Entry `v0.1.0-b026` is a mis-snap (the old unnumbered folder, snapped without `--source`); the real one is `v0.1.0-b027`
(exe 50b558f5a343). `PROJECT.conf`'s `app` still names the unnumbered folder: every Darkness snap needs `--source`.
