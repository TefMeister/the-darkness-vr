# Two public PC ports of The Darkness appeared in September/October 2026 (one with a native D3D11 renderer)

**Status:** 🆕 new · **Priority:** high. Neither was known to this project. One draws through the engine's own
D3D11 backend instead of emulating the Xbox GPU, which changes what the cheapest route to stereo could be; the
other is on the same ReXGlue SDK as ours and has already found where the scene projection can be offset per frame.

## What is public

Read online from each repository's README, docs and changelog (GitHub viewer and API) on 2026-10-26. Nothing was
downloaded. All `[reported 2026-10-26]`.

### portingpete, *The Darkness Recomp* (GPL-3.0, created 2026-09-15, v0.1.7 on 2026-10-14, 342 stars)

- The 360 executable is translated to C++ ahead of time and built into `DarkRecomp.exe` **with a D3D11
  renderer**. Its `RENDERING.md`: it "renders through the engine's D3D11 backend", using "original engine geometry,
  shader sources and completed device state", and **"does not interpret console GPU packets"**.
- When the display size changes, "the engine then uses those dimensions for both the **perspective projection and
  CPU visibility checks**". So the engine's own culling follows its own projection; a wider per-eye view would have
  to reach it through the same path.
- Settings: resolution up to 2160p and ultrawide, **FOV**, bloom and **motion blur toggles** (applied at the start of
  each render frame, no restart), frame cap, VSync, AA.
- **F5 developer tools: mission selection, player speed, invincibility, noclip, all Darkness abilities and maximum
  Darkness level.** That is an unlock-everything route in a public build.
- Runs through Proton; Steam Deck unverified.

### invinceble55-wq, *Estacado* (MIT, created 2026-09-29, v0.9.8 pre-release on 2026-10-12, 104 stars)

- Built on **ReXGlue** (Direct3D 12) and **XenonRecomp**, like ours. Needs AVX2.
- Frame rates beyond 30 "with the game's timing kept correct (scripted scenes, physics and audio run at their
  original speed)".
- FOV setting, experimental widescreen (more view at the sides), **motion blur on/off**, HD texture packs, keyboard
  and mouse, an F1 overlay.
- **0.9.8 temporal upscaling:** "at the title's final composite the scene is drawn jittered and its depth and camera
  motion are kept", with the HUD separated from the scene so it is laid over afterwards. So they found where the
  scene's projection can be offset per frame and where the HUD is drawn, both in a ReXGlue build.
- Its `docs/` holds `THREADING_MODEL.md`, `SYNCHRONIZATION.md`, `RUNTIME_WAIT_GRAPH.md` and `ARCHITECTURE.md`,
  which bear on this project's guest-threading and world-pause questions.

A third repo with the same name (FermataRest, 2026-10-01) is empty. No VR work on either port was found
`[reported 2026-10-26, n=1 search]`.

## Why it matters here

1. **Route choice.** Our port runs the Xbox GPU's command stream through the SDK; the headset patches sit there.
   A port that draws through the engine's own D3D11 backend exposes the camera and projection as ordinary D3D11
   state, and could take a more standard stereo approach `[hypothesis]`. This is a design question for the
   modding side, not a recommendation to switch: GPL-3.0 and a rebase cost come with it.
2. **The 2026-10-25 headset faults** ("two eyes look different ways", curved head turn). Estacado's jitter point is
   a second, independently found place where the scene projection is set late in the frame; comparing it with our
   set-view hook may show whether we are offsetting the right matrix `[hypothesis]`.
3. **Unlock everything / screen effects.** Both ports already expose motion blur off; portingpete's F5 panel has
   mission select and all abilities. We already opened the game's own DevMenu (dossier, 2026-10-24); these confirm
   which switches matter to players.
4. **Culling.** portingpete's note that the engine uses one size for projection and visibility checks is the same
   bottom-band pop-in question Condemned 2's board asks for its own engine.

## Next step it unlocks

Read Estacado's `THREADING_MODEL.md` and its jitter description (online only) against the "two eyes look different
ways" row, and note which game function each port names for the projection.

## Sources

- portingpete, The Darkness Recomp: <https://github.com/portingpete/The-Darkness-Recomp> (README, RENDERING.md)
- invinceble55-wq, Estacado: <https://github.com/invinceble55-wq/Estacado-The-Darkness-Recomp> (README, CHANGELOG.md, docs/)
