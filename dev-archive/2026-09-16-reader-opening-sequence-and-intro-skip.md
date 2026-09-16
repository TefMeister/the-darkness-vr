# Opening sequence: video or in-engine? And how to skip the intro hands-off

Author: PD reader session, 2026-09-16. Static only — nothing launched, no game file changed.
Image searched = `default.xex` decrypted (AES-128-CBC, retail key) and laid out by RVA
(offset = guest − 0x82000000).

## 1. Every video file on the disc [measured 2026-09-16]

There is **no `Videos` folder in any `Content_<lang>`** folder. All video lives in `Content/Videos` and `ExtraContent`:

| file | size | what it is |
| --- | --- | --- |
| Wmv/logo_topcow.wmv | 6.3 MB | publisher logo |
| Wmv/logo_union.wmv | 4.0 MB | Union Entertainment logo |
| Wmv/logo_2k.wmv | 4.8 MB | 2K logo |
| Wmv/sbz.wmv | 10.6 MB | Starbreeze logo |
| Wmv/DarknessAttractionVideo.wmv | 27.2 MB | attract-mode trailer (menu `attract`) |
| Wmv/Consite_FinalRender-1s.wmv | 8.0 MB | construction-site render |
| Wmv/Endcredits_Logo.wmv | 8.2 MB | end credits |
| Theora/IngameMisc/*.ogg (7) | 0.01–4.6 MB | in-world TV screens (Paulie, consite, e3tv, fultontv, orphanage) |
| Theora/DarknessTV/Xenon/** (53 .ogg, 710 MB) | — | the in-game TV channels: Popeye, Flash Gordon, music videos, films |
| ExtraContent/Offline01/Videos/*.wmv (2) | 30 MB, 3 MB | unlockable extras |

No Bink. Nothing is named for a title screen or a character introduction.

The GUI file `Content/Gui/CubeWnd.xcr` spells out the logo chain:
`logo_topcow → logo_union → logo_2k → logo_sbz → cg_switchmenu('main')`. **[measured]**

## 2. Verdict: the title screen and the BUTCHER JOYCE card are rendered in-engine

- No video matches either one (table above). **[measured]**
- The name "Butcher Joyce" is **localised text**: `StringTable_Eng.xcr` has `=Butcher Joyce` under
  `CHAR_NAME_AI_NY1_CHARACTER_BUTCHER` (75 `CHAR_NAME_*` keys in total). **[measured]**
- The executable builds `§LCHAR_NAME_<AI template>` + `§LCHAR_DESC_` in `sub_82145E60`, a
  character-object setup routine (the same string block holds `FaceSetup/%s.xsa`, `$PLAYER`).
  **[inferred-static]** — I found where the string is built, not the draw call.
- `Content/Xdf/NY1_Tunnel_00001010.XDF` (a level load list) contains
  `anim\vocap\butcher_f_xpress.xsa`, which is Butcher's facial animation, so he appears live in the
  Tunnel level. **[measured]** That the card is shown in NY1_Tunnel is **[inferred-static]**.
- The memory jump at t50 (467 → 649 MB) looks like a level load, not a streamed WMV. **[hypothesis]**

**The one thing left to rule out:** `DarknessAttractionVideo.wmv`, a 27 MB trailer, could contain
title and name-card shots. It only plays from the `attract` menu, which normally starts after
the main menu sits idle. How to tell: in the run log, see whether the game opened `Worlds/NY1_*.XW`
or `Xdf/NY1_*` files (in-engine) or `Videos/DarknessAttractionVideo.wmv` (video) around t50.

## 3. Skipping the intro: the engine already has a switch for it

`sub_82367AA0`, the front-end start routine (it also holds `CWFrontEnd_Mod::Cube_MouseClick`),
does this **[inferred-static, read from disassembly]**:

```
look up the ENV registry under SYSTEM
QUICKMAP  set and not ""  ->  ConExecute("campaignmap('<QUICKMAP>')")   and return
QUICKMENU set and not ""  ->  ConExecute("cg_rootmenu('<QUICKMENU>')")  and return
otherwise: read SHOW_CONFIDENTIAL, SHOW_DEVELOPMENT, then ConExecute("cg_rootmenu('intro')")
```

So `QUICKMAP=NY1_Tunnel` would jump straight into a 3D level, and `QUICKMENU=main` would skip only the logos.
These keys are a debug path, so they are not in User.cfg.

**Where ENV comes from** (`sub_827784F0`, engine init) **[inferred-static]**:
- default file name `EnvironmentXbox.cfg`; `-env <file>` on the command line overrides it;
- a relative name gets the executable's folder in front; if that file is missing it falls back to
  `Environment.cfg` in the same folder;
- it also reads `T:\Options.cfg`, `-remote`, `-nowriteenv`, and an **ENV key `COMMANDLINE`**, which is
  added to the command line. The command-line parser (`sub_820DFF48`) accepts `-MAP <name>`
  (runs `map("<name>")` after the profile check), `-DEMO <file>` and `-EXEC <script>`.
- **Neither `EnvironmentXbox.cfg` nor `Environment.cfg` ships on the disc.** **[measured]**

The file format is **[hypothesis]**. Most likely `KEY=VALUE` lines, the same as `User.cfg`: the
executable keeps `.xrg`, `.cfg` and `User.cfg` together, which suggests one loader picks the format by file extension.
Suggested first try (the user places it): `game-files/EnvironmentXbox.cfg` containing
`QUICKMAP=NY1_Tunnel`. If nothing changes, try the `.xrg` style: `*QUICKMAP "NY1_Tunnel"`. Also untested: whether
ReXGlue maps the executable's folder to `game-files/`.

**No `cmdline.txt`** is read — neither that string nor `cmdline` appears in the image. **[measured]**

## 4. Control test on User.cfg keys — the name search came up empty [measured]

`SENSITIVITYX` / `SENSITIVITYY` appear **nowhere** in the executable: not in ASCII, not in UTF-16,
and not in any case. The only copy anywhere on the disc is `User.cfg` itself. The search itself does work: `User.cfg`,
`QUICKMAP`, `Videos/` and the WMV class names are all found. **Conclusion:** the engine loads `.cfg`
files into a generic registry and looks keys up by name at run time, so **the executable does not hold a
list of the keys it accepts.** Key names live in the scripts and data. A "full list of User.cfg keys" cannot be
pulled from the executable.

## 5. What happens if a logo video is missing — not settled

The logo menus play `*VIDEO_logo_*` through the WMV video texture class
(`CTextureContainer_Video_WMV`) and move on with `cg_switchmenu` **when the video finishes**. I did
not trace what the WMV texture reports for a missing file. It could count as "finished at once"
(the chain skips ahead) or "never finishes" (a stuck black screen). **[not determined]** The
QUICKMENU/QUICKMAP route avoids that risk.

## 6. SvDebug.xcr [measured]

`MOS DATAFILE2.0` container. Its directory lists `XCR_LE` and `XCR_BE` copies of the same small
registry. The whole content is two keys: `GAMEMENU = GameMenu` and `GAMECLASS = GameDebug`. It is a
game-mode template (the executable builds `Registry/Sv<mode>`), **not** a skip-intro or debug-start flag.
