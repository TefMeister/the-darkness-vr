# portingpete's port v0.1.9 (2026-11-11): the engine draws a velocity pass nobody reads with motion blur off, and three other draw-count levers

**Status:** 🆕 new · **Priority:** high. Our board's `[PD]` row "THE CARD'S COST PER DRAW" says the card spends about
2-3 microseconds per draw and an eye is ~2,700 draws, so the ~7 ms that does not shrink with the picture size is a
draw-COUNT cost. The cheapest way to cut a per-draw cost is to have fewer draws. The other public Darkness port has
just measured where the game's own draws go, at the engine level, with guest addresses.

## What is public

Read online on 2026-11-11 from the repository viewer and API: the v0.1.9 release notes, pull requests #153 and #155,
and `RENDERING.md` (sections "Busy-scene engine thread speedup", "Level of detail in these views", "Redundant
geometry passes", "Per-draw overheads"). Nothing was downloaded. All `[reported 2026-11-11]` unless marked.

Their port is not ours: it draws through the engine's own D3D11 backend and does not interpret the console's GPU
packets (topic of 2026-10-26). But the items below happen in the GAME's code, before any GPU translation, so they
should apply to a packet-emulating port like ours too `[hypothesis]`.

1. **The velocity ("MotionMap") pass is drawn even with motion blur off.** It redraws moving and static geometry
   into a velocity target that only the motion-blur input of the final composite reads. They return early from its
   emitter at guest **0x82276148** when motion blur is off. On their village route this removed **17-23 % of the
   draws** and gave 8-25 % more frames a second (for example 9,177 draws at 88.0 FPS to 7,575 at 96.6).
   Our dossier's screen-effects table already names `MotionMapBlur` and switches motion blur off with a flag on the
   post-process (`DK_VR_SCREEN_FX_OFF`), but nothing says the velocity pass stops being drawn `[hypothesis]`.
2. **Otherworld levels (`OW1_*`, `OW2_*`) draw a depth-fog pass per object**: every depth-prepass object again with
   `XREngine_DepthFog` (depth EQUAL, stencil EQUAL then increment, blended). They replace it with one screen-space
   pass (5-25 % fewer draws there). New York levels have no such pass (checked in Chinatown), so it does not touch
   the alley. Their replacement is renderer work in their own D3D11 renderer; for us it is a note on where the draw
   count will jump, not a lever yet.
3. **Draw distance.** Guest **0x82498C48** copies the view's world-LOD ranges into the client render context each
   frame (enable bits at +80, squared ranges at +88..+104). Shortening only the generic and object ranges cut a busy
   village view from 9,681 to 6,873 draws (1,000 units) or 6,259 (300 units). Their default is now "Near" (1,500
   units). This changes the look, and Tefa keeps the look, so it is an option for a slow scene, not a default.
4. **Field of view costs draws:** FOV 95 instead of 70 added about 6 % of the draws and 7 % of the engine time in
   one view. Our full headset view (b018, 108 x 109 degrees an eye) will cost more than that, since the engine culls
   by its own projection `[hypothesis]`.
5. **Model level of detail does not change draw counts** (`xr_lodoffset`, `xr_lodscale`): LOD meshes have fewer
   triangles, not fewer draws. So LOD is not a lever for a per-draw cost.
6. **The engine's own CPU cost per draw** is about 1.3-1.5 microseconds on their machine (i9-14900K), and the
   engine thread is now their limit. Ours is the card (2-3 microseconds a draw), so the same draws cost us more.
7. Not for us: they skip the original indexed draw at 0x82868FE8 because their renderer never reads the console
   command stream. Our port draws FROM that stream, so this one does not transfer.

Their port checks for one supported retail revision of `default.xex` at launch. Whether it is the same revision as
our disc (serial TT-2030) is not established; the addresses sit in the same neighbourhood as ours (our VdSwap caller
`sub_82867620`, the fast clear `sub_828621D0`) `[hypothesis]`.

Also checked: Estacado (the other public port) has no release since 0.9.8 and only a sponsor link since 11-07. No VR
work on either. No other Darkness (2007) VR project found on GitHub (a "Darkness II VR" mod exists; that is a
different, PC game).

## Why it matters here

- If the velocity pass is drawn in our build, it is roughly a fifth of an eye's ~2,700 draws, which at 2-3
  microseconds a draw is about 1-1.5 ms of card time an eye, before any change to the port `[hypothesis]`.
- It is a guest-side hook of the kind we already make (weak-symbol `REX_HOOK_RAW`), with a clear off switch.
- It also tells the `[PD]` "BOTH EYES FROM ONE GAME FRAME" row which passes are worth drawing twice: a pass nobody
  reads is not.

## Next step it unlocks (modding side)

1. Statically: read `sub_82276148` in our unpacked image and confirm it is a function start and the MotionMap
   emitter (a call site from the frame build, the velocity target), which also settles whether the revision matches.
2. Live, flat, simulator: count calls to it per frame in the alley with `DK_VR_SCREEN_FX_OFF=1`; then a hook that
   returns early (off switch), and compare draws an eye (`DK_GPU_MARKERS=1` trace) and game frames a second with
   `dk_try.py`, plus one still to check the picture.
3. Note the OW depth fog and the 0x82498C48 draw distance for when a slow level turns up.

## Sources

- portingpete, *The Darkness Recomp*: v0.1.9 release notes
  <https://github.com/portingpete/The-Darkness-Recomp/releases/tag/v0.1.9>; PR #153 "Cut per-draw cost on both render
  threads" <https://github.com/portingpete/The-Darkness-Recomp/pull/153>; PR #155
  <https://github.com/portingpete/The-Darkness-Recomp/pull/155>; `RENDERING.md`
  <https://github.com/portingpete/The-Darkness-Recomp/blob/main/RENDERING.md>
- invinceble55-wq, *Estacado*: <https://github.com/invinceble55-wq/Estacado-The-Darkness-Recomp>
