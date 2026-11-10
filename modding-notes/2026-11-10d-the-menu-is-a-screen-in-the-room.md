# The menu is a screen held in the room (2026-11-10, b021, Fable, simulator)

Tefa 2026-11-07: *"the 2d screen for main menu should be smaller a little and not stuck to my view, but in the 3d world"*.

- **How:** out of a level (the same test the wide view uses: no camera build in the last half second) the game says so
  through a new export, `dk_xr_flat()`; the SDK's headset code (patch 14, `xr_output.cpp`) then hands the picture over
  with the eye poses of the moment the flat stretch began, level, and the game's tangents x 0.75. The runtime does the
  rest: the picture stays where it was while the head turns. `DK_XR_FLAT_WORLD=0` = stuck to the face as before;
  `DK_XR_FLAT_SCALE` sets the size; live `view.flat_world`.
- **Seen `[verified-live 2026-11-10, simulator, n=1]`:** the log "a flat picture began: held in the room at (-0.03 1.70
  0.00), scale 0.75" at the first film; with the head turned 25 degrees the layer was still submitted at the old pose
  with tangents 0.70 x 0.39 (0.934 x 0.525 times 0.75); entering the level: "the flat picture ended: back on the head",
  then the wide view as in b018; 61 eye pairs a second in the alley afterwards.
- **First build with the SDK's graphics library rebuilt by the new compiler** (`rexgpu-xenos.dll` of 02:43; b018-b020
  kept the 11-06 one). It runs; that is all that is known about it.
- **Not worn.** For Tefa: is the screen at a good size and place; it is held where the head pointed when the menu
  began, so turning right round leaves it behind (a re-centre button is the obvious next small thing); the pause menu
  inside a level stays as it was (it is in a level).
