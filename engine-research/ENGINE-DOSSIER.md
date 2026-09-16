# Engine dossier — The Darkness (2007)

What is actually known, and how well. Confidence tags follow the house vocabulary:
`[verified-live]`, `[measured]`, `[verified-numerically]`, `[compile-verified]`,
`[inferred-static]`, `[reported]`, `[hypothesis]`, `[disproved]`.

## 1. The game

Starbreeze Studios, published by 2K Games, 2007. Xbox 360 and PlayStation 3. **No PC release ever.**
Starbreeze's own in-house engine, the lineage that ran *The Chronicles of Riddick: Escape from
Butcher Bay* `[reported]` — not yet confirmed against the files, and the file formats below are
consistent with it but are not proof.

- Disc: **Xbox 360 XGD2**, serial `TT-2030`, xemid `TT203001W0X11`, ringcode `0F213645`, NTSC
  (`TT` = Take-Two/2K) `[verified-live 2026-09-16, n=1]`.
- Dumped with redumper b751 on a Kreon-flashed TSSTcorp SH-D162D: **7,835,492,352 bytes,
  `SCSI: 0, EDC: 0`** — a flawless read `[verified-live 2026-09-16, n=1]`.
  md5 `309885767570e6c2afa2c89d43b04dd5`, sha1 `ab0cb9a12466c31fed079f83e3c65c81c5e3b222`.
- Extracted with xdvdfs v0.8.3: **498 files, 6.8 GB** `[verified-live 2026-09-16, n=1]`.
- **Local copies (dev PC, not in this repo):** image at `E:\the-darkness\disc\`, extracted files at
  `E:\the-darkness\game-files\`. The disc itself need never go back in the drive.

## 2. File layout — what the disc actually contains

```
default.xex            11.1 MB   the game program
Content/                         main data
  User.cfg                 46 B  plain-text settings
  Registry/                      *.xcr / *.xrg config and table files
  Textures/  Anim/  Gui/  Videos/  Waves_Xenon/  Xdf/  Feedback/
Content_Eng|Fre|Ger|Ita|Spa/     per-language packs
ExtraContent/                    incl. Offline01/Textures/Extra.000.xtc (308 MB)
System/  Gl/ Sound/ Xenon/
$SystemUpdate/
```

Largest files: `Waves_Xenon/Streamed_Music.xwc` 785 MB, `Textures/AllTextures.000.xtc.xt0` 604 MB,
`Streamed_Vo.xwc` 404 MB, `Anim/All.xac` 194 MB `[measured 2026-09-16]`.

## 3. ⭐ Two promising leads for VR

### `User.cfg` is plain text, and tiny

The entire retail file is three lines `[verified-live 2026-09-16, n=1]`:

```
SENSITIVITYX=0.16
SENSITIVITYY=0.06
SYNC=0
```

**The engine reads simple `KEY=VALUE` text settings.** A three-line file almost certainly means the
executable recognises many more keys than the retail config bothers to set — field of view, camera
and debug options are the obvious hopes. **Unverified**, and the way to test it is §4.
`[hypothesis]`

### `Registry/SvDebug.xcr` shipped on the retail disc

Alongside `Sv.xcr`, `SvCampaign.xcr`, `SvDM.xcr`, `SvCTF.xcr` there is a **`SvDebug.xcr`**
`[verified-live 2026-09-16, n=1]`. A debug configuration left in a shipping game is exactly the kind
of thing that unlocks free cameras and developer commands. Its format is unknown and unopened.
`[hypothesis]`

## 4. ⛔ The current blocker — the executable is packed

**`default.xex` cannot be read statically as it stands.** `[verified-live 2026-09-16, n=1]`

This was established with a **control test rather than a guess**, which matters: searching for
strings that *might* exist proves nothing if the search itself is broken. So the search was run for
`SENSITIVITYX` — a string we know for certain the program must contain, because the retail config
uses it. **Zero matches.** The handful of apparent hits for "fov", "camera" and so on are random
byte noise (`fOVm`, `GOd>`).

Retail Xbox 360 executables are normally **compressed and encrypted**, and this one behaves exactly
that way. Everything in §3 stays a hypothesis until it is unpacked.

**Next step:** decrypt/decompress the XEX — `xextool`, or whatever the recompilation toolchain uses
as its first stage — and re-run the same control test. **If `SENSITIVITYX` then appears, the
analysis is trustworthy; if it does not, something else is wrong and no conclusion drawn from string
searching is safe.**

## 5. The road to VR

The route runs **through** the PC recompilation, not around it:

1. Someone releases a working recompilation (a build has been shown publicly; source not yet out at
   the time of writing) `[reported 2026-09-16]` — Tefa saw the announcement and video; this session
   could not reach the post to confirm details, and no public repository exists under an obvious name.
2. It is rebuilt from source here, the way [`condemned-2-vr`](https://github.com/TefMeister/condemned-2-vr)
   was — that project's seven build fixes and toolchain notes apply directly.
3. Stereo rendering is added **at the presenter**, in source, rather than injected.

⚠️ **Step 1 is entirely outside our control, and step 2 fails if the port ships binaries only.**
Then this becomes a far harder problem and the cheap route is gone. Worth watching for the source
release specifically, not just the build.

## 6. Not yet looked at

Renderer, camera maths, input, the `.xcr`/`.xrg`/`.xtc`/`.xac` formats, and whether the PS3 version
offers anything easier. None of it is blocked by anything except §4.
