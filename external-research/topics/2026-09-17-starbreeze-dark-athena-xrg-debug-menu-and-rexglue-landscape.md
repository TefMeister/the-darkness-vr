# Riddick: Assault on Dark Athena (same studio, PC release) enables a debug menu through an `.xrg` file; no ReXGlue project has done VR

**Status:** 🆕 new · **Priority:** medium.

## What is public

- **The Chronicles of Riddick: Assault on Dark Athena** (Starbreeze, 2009) did get a PC release. Its
  console opens with **Ctrl + Alt + ~** and accepts `cmd(noclip)`, `cmd(cyclecamera)` (first/third
  person), `cmd(godmode)` and others `[reported]`. **An in-game debug menu is enabled by editing
  `CubeWnd.xrg`** `[reported]`. Starbreeze overhauled the engine between Butcher Bay and Dark Athena, so
  Butcher Bay codes do not carry over `[reported]`.
- The Darkness's disc has `Content/Registry/*.xcr` / `*.xrg` files (dossier §2) — the same file family.
- **ReXGlue ecosystem, 2026-09-17:** many static recompilations are active (Army of Two, Perfect Dark Zero,
  Dante's Inferno, Skate, NBA LIVE and others), plus a shared **recomp-framework** (BSD-3-Clause) for
  installers and menus. A GitHub search for ReXGlue with VR, OpenXR, FOV or camera found **no project
  except this estate's own** `[reported 2026-09-17, GitHub repository search]`.

## Why it matters here

1. **The Darkness sits between Butcher Bay (2004) and Dark Athena (2009)**, so a `CubeWnd.xrg`-style debug
   switch or `cmd(...)` console syntax may exist in its `Registry` files `[hypothesis]`. A debug menu or
   `cyclecamera` would help the camera experiments.
2. No one else in the ReXGlue world is doing VR, so there is no prior art to wait for; notes this project
   publishes are the first.

## Next step

Search the extracted `Content/Registry/` files for `CubeWnd`, `debug`, `console` and `cmd(`.

## Sources

- GameRevolution / GameFAQs / PCGamingWiki pages on Dark Athena PC console and cheats — <https://www.pcgamingwiki.com/wiki/The_Chronicles_of_Riddick:_Assault_on_Dark_Athena>, <https://gamefaqs.gamespot.com/pc/954872-the-chronicles-of-riddick-assault-on-dark-athena/cheats>
- rexglue-sdk — <https://github.com/rexglue/rexglue-sdk>
- furqanagwan, recomp-framework — <https://github.com/furqanagwan/recomp-framework>
