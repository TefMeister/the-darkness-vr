# Starbreeze xrg debug menu lead

**From:** `/gr` estate sweep, home PC, 2026-09-17.

**Lead for:** the camera experiments (a debug menu or third-person toggle would help), dossier §2 file layout.

Starbreeze's **Riddick: Assault on Dark Athena** (PC, 2009) opens a console with Ctrl+Alt+~, has
`cmd(cyclecamera)` / `cmd(noclip)`, and **enables a debug menu by editing `CubeWnd.xrg`** `[reported]`.
The Darkness ships the same `.xrg` / `.xcr` family in `Content/Registry/`, so a similar switch may exist
`[hypothesis]`. Cheap static check: grep the extracted Registry files for `CubeWnd`, `debug`, `cmd(`.

Topic: `external-research/topics/2026-09-17-starbreeze-dark-athena-xrg-debug-menu-and-rexglue-landscape.md`
