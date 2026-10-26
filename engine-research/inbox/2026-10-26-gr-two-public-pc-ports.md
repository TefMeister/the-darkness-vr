# Two public PC ports of The Darkness exist; one draws through the engine's own D3D11 backend

From: `/gr`, 2026-10-26 (estate sweep, CHECK-IN). Topic: `external-research/topics/2026-10-26-two-public-pc-ports-of-the-darkness.md`.

**Dossier section it touches:** §7 "Not yet looked at: Renderer, camera maths, input ...", and the board's
`[PD]` row on the three headset faults (two eyes look different ways, curved head turn).

- **portingpete/The-Darkness-Recomp** (GPL-3.0, v0.1.7, 2026-10-14): renders "through the engine's D3D11 backend",
  "does not interpret console GPU packets"; the engine uses one display size for "both the perspective projection
  and CPU visibility checks"; F5 dev tools (mission select, all Darkness abilities, noclip) `[reported 2026-10-26]`.
- **invinceble55-wq/Estacado** (MIT, ReXGlue + XenonRecomp, v0.9.8, 2026-10-12): draws the scene jittered "at the
  title's final composite" with depth and camera motion kept, HUD separated; docs on guest threading,
  synchronisation and the runtime wait graph `[reported 2026-10-26]`.

**Suggested dossier change:** add both ports to §7 as public references (read online only, nothing copied), and
note under the headset-fault row that Estacado's projection-jitter point is an independent second location to
compare with our set-view hook `[hypothesis]`. Whether a D3D11-backend port is a better base for stereo is a
design question for the modding side.
