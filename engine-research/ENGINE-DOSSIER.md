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

## 3. VR leads — two checked and closed, one better one found

### `User.cfg` keys cannot be listed from the binary — `[disproved 2026-09-16]`

The retail `User.cfg` is three lines (`SENSITIVITYX=0.16`, `SENSITIVITYY=0.06`, `SYNC=0`), so the
engine does read plain `KEY=VALUE` text. The hope was that the executable held a findable list of every
key it accepts. **It does not.** With the image properly unpacked, `SENSITIVITYX` appears **nowhere** in
the executable — not ASCII, not UTF-16, not any capitalisation — and the only copy on the disc is
`User.cfg` itself `[measured]`. The search method is sound (it does find `User.cfg`, `QUICKMAP`,
`Videos/` and the WMV class names), so this is a real absence, not a broken search.

**Why:** the engine loads `.cfg` files into a generic settings store and looks keys up by name at run
time. Field-of-view or camera settings, if they exist, have to be found by following the code that
*reads* that store — not by string search.

### `Registry/SvDebug.xcr` is not a debug switch — `[disproved 2026-09-16]`

A `MOS DATAFILE2.0` container holding one tiny settings file, stored twice (once per byte order). Its
entire content is `GAMEMENU=GameMenu` and `GAMECLASS=GameDebug` `[measured]` — a **game-mode template**,
not a free camera, skip or debug-start flag. It does confirm a `GameDebug` game class exists in the
engine, which may be worth tracing later.

### ⭐ The lead found instead: built-in quick-start switches

The front-end start routine `sub_82367AA0` reads the engine's **ENV** settings `[inferred-static, from
disassembly]`:

- **`QUICKMAP`** set → `campaignmap('<map>')` immediately. **`QUICKMAP=NY1_Tunnel` should drop straight
  into the first 3D level with no menus.**
- else **`QUICKMENU`** set → `cg_rootmenu('<menu>')`. `QUICKMENU=main` should skip only the logos.
- else → `cg_rootmenu('intro')`, the normal chain.

ENV comes from (`sub_827784F0`) `[inferred-static]`: **`EnvironmentXbox.cfg`** beside the executable,
falling back to **`Environment.cfg`**; **`-env <file>`** overrides the name; a `COMMANDLINE` setting in it
is appended to the command line, whose parser accepts **`-MAP <name>`**, **`-DEMO`** and **`-EXEC`**.
**Neither file ships on the disc** `[measured]`.

⚠️ **Untested.** File format is `[hypothesis]` — probably `KEY=VALUE`, since the executable keeps
`.xrg`, `.cfg` and `User.cfg` together (suggesting one loader dispatching on extension); if that does
nothing, try the `.xrg` style `*QUICKMAP "NY1_Tunnel"`. Whether ReXGlue counts `game-files/` as "the
executable's folder" is also untested. **For VR work this is worth more than any single fix** — every
test run can land directly in 3D instead of sitting through the logos.

### Where the intro comes from `[measured 2026-09-16]`

`Content/Gui/CubeWnd.xcr` chains **`logo_topcow → logo_union → logo_2k → logo_sbz → main`**. Every video
on the disc — **none is Bink**:

- **Logos (WMV):** `logo_topcow` 6.3 MB, `logo_union` 4.0 MB, `logo_2k` 4.8 MB, `sbz` 10.6 MB.
- **Other WMV:** `DarknessAttractionVideo` 27.2 MB (the trailer), `Consite_FinalRender-1s` 8.0 MB,
  `Endcredits_Logo` 8.2 MB; ExtraContent adds two unlockable WMVs.
- **Theora `.ogg`:** 7 in `IngameMisc` (in-world TV screens) and **53 files, 710 MB, in `DarknessTV`** —
  the in-game television channels (Popeye, Flash Gordon, music videos, old films).
- No `Content_<lang>` folder contains a `Videos` folder.

## 4. The executable — packing was never the real blocker

⛔→✅ **The earlier "packed executable" blocker is resolved.** `[verified-live 2026-09-16]`

The original control test was right: string-searching the raw `default.xex` found nothing, because it is
packed. But **ReXGlue unpacks it itself** during codegen, and the reader reconstructed the format
independently: **encryption=1, compression=1 (basic)** — AES-128-CBC with the retail key, then plain
`(data, zero)` blocks, no LZX `[verified-numerically]`.

⚠️ **One trap worth keeping:** the unpacked image is laid out **by RVA** (file offset = guest address −
`0x82000000`), **not** by PE raw pointer. Using raw pointers produces a plausible-looking but entirely
wrong disassembly from `.text` onward — the reader built a confident, wrong set of conclusions that way
before catching it.

### Why every run crashed on a missing function — root cause `[verified-numerically 2026-09-16]`

ReXGlue's `VTableScanner` finds virtual-function tables **only** through MSVC RTTI Complete Object
Locators. **The Darkness is a `/GR-` no-RTTI build — there are zero of them in `.rdata`.** So that
entire discovery path contributes nothing on this game, and every function reached only through a
vtable or a stored pointer went unregistered. `.pdata` gives 15,722 entries, all already registered, so
that source is fully mined.

The missed functions are not ordinary functions: **98.39% of real function starts here begin with
`mflr r12` (`0x7D8802A6`), and 0% of the missed ones do** `[verified-numerically, n=15722]`. They are
four-instruction **vtable dispatch stubs** — `lwz r12,0(r3); lwz r11,0x350(r12); mtctr r11; bctr` — so a
prologue-pattern scan is the wrong instrument.

**Fixed in bulk** by `dev-archive/tools/find_missing_functions.py`: absolute pointers in data sections
plus `lis`/`addi` address materialisation in code, filtered to addresses preceded by
`blr`/`bctr`/tail-branch/padding. Of 11,107 raw hits, **97.3% were addresses ReXGlue already knew** —
good precision. **235 candidates; 13 were bodiless import thunks (`0x829C5xxx`–`0x829C6xxx`) and had to
be removed; 226 active.** One address, `0x827686E8`, was **in the list before** a crash independently
hit it — a genuine prediction, not a retrofit.

- **No knob does this already** `[verified-numerically]`: raising `max_discovery_iterations` and the
  other caps does nothing, since those passes already converge. `functionPointerScan()` exists in the SDK
  but its call is commented out as *"causes too many false positives"* — it lacks the use-filter above.
- **Permanent fix:** the `Genesis5500/ArmyOfTwo-Recomp-rexglue` fork adds data-pointer discovery (~80
  lines, one bool cvar) to `phase_discover.cpp`. Porting it would make this self-heal for every title.
- ⚠️ **Expect more.** The generated C++ has **27,790** indirect-call sites, and unrecognised jump tables
  are a separate class needing `[[switch_tables]]`, not `[functions]` `[measured]`.

## 5. The road to VR — we no longer wait on anyone

**Our own recompilation now runs.** `[verified-live 2026-09-16, n=2]` Project at
`E:\the-darkness\src\DarknessRecomp\`, working source mirrored in `dev-archive/port-project/`.

What has actually been seen, hands-off, no input sent, captured from the game's own window:

| time | on screen | what it proves |
| --- | --- | --- |
| ~10 s | the moon over a starfield | video decode + presenter work |
| ~20 s | storm clouds, crows, **Union Entertainment** logo | the intro chain runs |
| ~50 s | black frame; memory **+180 MB**, CPU spike | a large load |
| ~70 s | **THE DARKNESS** title screen | the front end runs |
| ~95 s | **"BUTCHER JOYCE"** name card over a lit close-up | ❌ **the attract trailer** — see below |

It is **not hung**: ~2.65 cores of CPU continuously, memory 403→650 MB, threads rising `[measured]`.

✅ **REAL-TIME 3D GAMEPLAY CONFIRMED** `[verified-live 2026-09-16, n=2]`. Pressing START and A on a
**virtual Xbox 360 pad** (ViGEmBus + `vgamepad`, see `dev-archive/build-scripts/padpress.py`) walks title → menu →
new game. The verbose log then resolves **`NY1_Tunnel_00001010.XDF`** and its `_Load`, `_Common` and `_Precache`
companions, memory climbs **632 → 1,111 MB** through a loading screen, and the window shows **first-person hands
in the back of the car with the tutorial prompt "Use to look around"**, then the full opening car chase: the
driver, the gesturing passenger, the tunnel with passing lights, traffic ahead, dust particles and the opening
cast credits. **The flat bar for VR is met, and the game is first-person.**

✅ **The look stick turns the camera** `[verified-live 2026-09-16, n=1 run; 3 directions, 3 clean controls]`.
`dev-archive/build-scripts/looktest.py` — one process owns launch, virtual pad, window capture and analysis — takes
a burst of frames just before and just after each stick hold, interleaved with no-input controls, and measures the
picture's slide by phase correlation (validated on known shifts of +24/−24/+60/0 px). At full deflection for 0.8 s,
on the **lower half of the frame**:

| trial | lower-half shift | reading |
| --- | --- | --- |
| control | 0 | still |
| **right** | **dx −52** | picture slides left = camera turned right ✓ |
| control | 0 | still |
| **left** | **dx +60** | picture slides right = camera turned left ✓ |
| control | 0 | still |
| **up** | **dy +56** | vertical ✓ — and **by eye** the hands drop well down the frame |

Opposite signs on cue, zero between: the car's own motion cannot do that. The game had focus throughout, so
whether it reads the pad **without** focus is still untested.

⚠️ **Two measurement traps, both hit before getting this right:**
1. **Whole-frame correlation is pinned at zero by fixed HUD overlays.** The "Use to look around" prompt never
   moves, so it anchors the match at no shift even while the scene turns. Measure a region that excludes the HUD.
2. **A small nudge (0.55 deflection for 0.35 s) produced no visible turn at all**, not even by eye. The game very
   likely ramps look speed up over the first moments of a hold. A null result from a short nudge is not evidence
   that looking is disabled.

One oddity, not explained: the control taken straight after the up-look showed a large vertical change with a poor
match quality — most likely the camera **easing back down** after the look, which seated scripted scenes often do.
`[hypothesis]`

**How it was established that the earlier "Butcher Joyce" card was not this:** with verbose logging a normal idle run opened the **attract trailer**: with verbose logging on, a normal run opened
`Content/Videos/Wmv/DarknessAttractionVideo.wmv` at about 94 s — exactly when the card appears — and opened
**no level file at all**. An earlier lean towards "probably in-engine", including reading isolated bright
pixels as real-time artefacts, was **wrong** `[disproved 2026-09-16]`. The trailer plays because a hands-off
run sits idle at the main menu.

**`QUICKMAP` is read but traps.** `QUICKMAP=NY1_Tunnel` in `EnvironmentXbox.cfg` is resolved and does skip the
videos, but about a second later the runtime hits a deliberate debug break (`0x80000003`) with no logged
message. A control run without the switch survives, so the switch is the trigger. Two tempting explanations
were tested and **ruled out**: no `.xw` world file is ever found in *either* run (there is no `WORLDS` folder
on the disc), and the font `MONOPRO.XFC` is missing in *both* runs too — neither is the cause.

**Verbose logging** (`log_verbose = true`, `log_flush_interval = 1` in `darknessrecomp.toml`) records every
successfully resolved file and was what settled the trailer question. Leave it off by default: it slows the
game and produced 26 MB of logs in a few minutes.

Then, in order:
1. ~~Reach a level and confirm real-time 3D~~ — **DONE 2026-09-16** via the virtual pad. Look stick turning the camera: **proven**. Next: the stereo seam.
2. Find the **stereo seam** — `rex::ui::d3d12::D3D12Presenter` / `D3D12CommandProcessor::IssueSwap`,
   where the one finished frame reaches the swapchain. Shared SDK code, so the same work lands on
   Condemned 2 and every other ReXGlue title.
3. Find where the game builds its view/projection matrix, so each eye gets its own camera.

The other community port remains worth watching — its fixes may save us time — but **we are no longer
blocked on it.**

## 6. Not yet looked at

Renderer, camera maths, input, the `.xcr`/`.xrg`/`.xtc`/`.xac` formats, and whether the PS3 version
offers anything easier. None of it is blocked by anything except §4.
