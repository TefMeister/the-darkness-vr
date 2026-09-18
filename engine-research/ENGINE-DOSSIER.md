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

### ⭐⭐ A developer menu AND a free camera are in the retail data — present, not yet reachable (2026-09-18)

A `/gr` lead (Riddick: Assault on Dark Athena enables a debug menu by editing `CubeWnd.xrg`,
`[reported]`) checked out against this game's own files, with a passing positive control.

**In the compiled `Content/Gui/CubeWnd.xcr`** (`MOS DATAFILE2.0`, one `XCR_LE` + one `XCR_BE` copy,
ASCII payload) `[measured 2026-09-18]`:

```
Z15FreezeCam On        cl_toggledebugcamera();pause(1)
Z20Toggle DebugCamera2 cl_toggledebugcamera2()
Z20Noclip              cmd_forced(noclip); cg_clearmenus()
Z20God-mode            cmd_forced(godmode); cg_clearmenus()
```

Menus present: `DevMenu`, `ZoneMenu`, `GAMEMENU2`, `INGAME_DEBUG`, `INGAME_DEBUG2`, `AIDEBUG`,
`INGAME_DEBUG_DARKLINGS`, `options_video_dev(2)`, `DemoMenu`, and `Milestone*` pages with direct
`campaignmap("NY1_Chinatown")`-style level jumps.

**In the executable**, one contiguous bindable-input-action table between `primary`/`jump`/`crouch`
and `button0..5` `[measured 2026-09-18]`: `toggledebugcamera` (0x82080434), `toggledebugcamera2`,
`dbgcam_moveforward`, `dbgcam_movebackward`, `dbgcam_moveleft`, `dbgcam_moveright`,
`dbgcam_lookvelocity_x`, `dbgcam_lookvelocity_y`, `noclip`, `noclip2`, `godmode`, `cyclecamera`
(0x8208078C) — plus `Noclip %s`, `Unable to switch to noclip.`, `NOCLIPPING`. **The free camera has
its own four movement actions and two look axes: a real one, not a stub.**

⚠️ **The Riddick *console* half is NOT confirmed here** — no `console` string, no Ctrl+Alt+~ binding.
Only the *menu* half.

**🚧 THE GATE: `cheat()` IS NOT LOCKED — IT IS EMPTY. NO CONFIG LINE CAN EVER OPEN IT.**
`[verified-numerically 2026-09-18]`

Both doorways are wrapped in it — `GUI_BUTTON2,,cheat("cg_submenu('DevMenu')")` on MainMenu and
`cheat("cg_rootmenu('GAMEMENU2')")` on the in-game pause menu. The chain is: name 0x82071894 →
handler **0x8236F300** (exactly one xref in the whole executable, so it is `cheat`-only) → a vtable
stub reading **+260** → front-end vtable 0x82071A10 (installed by constructor `sub_82367510`); the
derived front end installs 0x82075E20. **Both** have slot +260 = **`sub_8276AA20`**, which in full is
`mr r3,r4; b sub_821F8AD0` — and `sub_821F8AD0` is a **destructor**. `cheat()` takes the string,
destroys it and returns. It tests nothing.

The comparison is what settles it — every other GUI command's slot lands on a real front-end
implementation, `cheat` alone lands on a shared destructor in a different module:

| command | slot | implementation |
| --- | --- | --- |
| `cg_grabscreen` | +256 | `sub_8236D338` |
| **`cheat`** | **+260** | **`sub_8276AA20`** ← the only outsider |
| `cachecommand` | +264 | `sub_8236D420` |
| `cg_backout` | +272 | `sub_8236D4F0` |
| `cg_showdevtext` | +300 | `sub_8236D720` |

**The retail build shipped the base class's default for that one virtual.** So there is no condition
to satisfy: reaching the dev menu needs **code**, not configuration.

❌ **`SHOW_DEVELOPMENT` is a dead flag.** A complete `.text` scan of every byte access at +1027 shows
it is **written in three places and read nowhere** `[measured 2026-09-18]`; `cg_showdevtext` sets the
same dead flag. `SHOW_CONFIDENTIAL` (+1026) is read exactly once, at 0x823A757C in `sub_823A73D0`,
where it makes a GUI element named "Confidential Text" — **a cosmetic watermark**. Confirmed live:
with both set, `X` twice and `Space` on the main menu changed nothing
`[verified-live 2026-09-18, n=1]`.

✅ **The gate is only those two doorways, though.** Everything *inside* is unwrapped: DevMenu's
`Zones`, `Load Scriptlayer`, `Soakmode`, `Unlock all` and the `Milestone*` level jumps; GAMEMENU2's
`Noclip`, `God-mode`, `FreezeCam On`, `cg_rootmenu('INGAME_DEBUG')`.

**⭐ The way in, and it is surgical: hook `sub_8236F300`** — one xref, `cheat`-only — with the same
weak-symbol `REX_HOOK_RAW` route used for the stereo probe, forwarding to whichever front-end virtual
executes a command string immediately. Reading `sub_8236D420` (+264) and `sub_8236D570` (+276) is the
small static step that names it. ⚠️ **Do NOT hook `sub_8276AA20`** — it is shared, fills two vtable
slots and has four code references.

⚠️ **Until that hook exists, record the free camera as *present in the data and not reachable*,**
never as available.

Source: `external-research/topics/2026-09-17-starbreeze-dark-athena-xrg-debug-menu-and-rexglue-landscape.md`;
live results in `modding-notes/2026-09-18-quick-start-switches-guest-asserts-and-the-dev-menu.md`.

### ⭐ The quick-start switches: right file, wrong folder — and then a guest assert (2026-09-18)

Three corrections to the `QUICKMAP` / `QUICKMENU` entry above, all live.

1. **`EnvironmentXbox.cfg` goes in the GAME's disc root** (`E:\the-darkness\game-files\`, the
   `Assets\` junction, guest `D:\`) — **not** beside the host exe, where it is ignored entirely
   `[verified-live 2026-09-18, n=1 null host-side, n=1 control guest-side]`. The `.cfg` is plain
   `KEY=VALUE`, one per line; the `.xrg` `*KEY "value"` style was never needed.
2. **Both switches hit one of the game's own debug assertions** — guest 0x820DFC60 → `sub_828A7518`
   → the kernel import `DbgBreakPoint`, on a null field the normal front-end route fills in. That
   killed the host process with `0x80000003` and **no log line at all**, so it looks nothing like
   the known missing-function crash `[verified-live 2026-09-18, n=3]`. Fixed in the shared SDK:
   a guest `DbgBreakPoint` now logs and continues (`sdk-patches/06-…`), because that is what retail
   hardware does with no debugger attached.
3. **Past the assert it stops on a missing font**, not on the level:
   `d:\content\fonts\text.xfc` → `XamShowDirtyDiscErrorUI` `[verified-live 2026-09-18, n=2 —
   `QUICKMAP=NY1_Tunnel` and `QUICKMENU=DevMenu` fail identically]`. `NY1_Tunnel` is a real map, so
   the name is fine. **Every ENV quick-start route fails this way**, which rules out anything
   level-specific.

### ✅ The fonts are inside the `.XDF` archives, and the disc dump is byte-perfect (2026-09-18)

Answered by parsing the XDVDFS directory of the image directly (game partition 0x0FD90000, root
sector 14077) and diffing it against the extraction `[verified-numerically 2026-09-18]`:

| | disc | extracted |
| --- | --- | --- |
| files | **498** | 498 (+2 of ours) |
| bytes | **7,237,617,483** | 7,237,617,542 (+59 = our two `.cfg` files) |
| on disc but missing | **0** | |
| size mismatches | **0** | |

The 7.8 GB image against 6.8 GB of files is the XGD2 video partition (~266 MB) plus filesystem
padding. **Nothing is missing. There is no `Fonts/`, `Models/`, `Worlds/` or `Surfaces/` folder on
the disc at all.**

**The `.XDF` format, validated on 6 archives** `[measured 2026-09-18]`: `u32 0x101` version, `u32`
name-blob length, NUL-separated lower-case source names, `u32` source count, 24-byte source records
(name offset, first/last resource index, **file size**, FILETIME), `u32` resource count, 20-byte
resource records (id, owning source, size, offset-within-source, arena offset), then the packed
payload. The size field is trustworthy: **of every referenced file that does exist on disc, 278 of
278 matched their declared size exactly** `[verified-numerically, n=278]`.

Across all XDFs, 1,512 distinct names are referenced, and the classes with **zero** presence on disc
are whole file types folded into the payloads at build time with their original path kept only as
provenance: `.xmd` models (9,162 refs), `.xsa` (3,100), `.xah` (439), `.xw` worlds (276), **`.xfc`
fonts (185)**, most `.xtc`. `GUIPrecache.XDF` carries **11,313,452 bytes** of payload behind 33,559
bytes of tables; `Content/Xdf/` totals ~2.9 GB. **`fonts\text.xfc` is resource 308, size 175,043,
offset 82.**

Third line of evidence, from our own logs: across all 49 run logs, **127 guest paths resolved and 12
failed — and not one resolved path is a `.xfc` or `.xmd`.** What does resolve is
`d:\content\xdf\guiprecache.xdf`, and in the failing runs it is never opened at all.

**So the mechanism is** `[hypothesis]`, though it fits every observation: the ENV quick-start skips
the front-end sequence that precaches the GUI; the font is therefore not in the arena; the resource
manager falls back to opening the original source path; that path was never a real file; dirty disc.
Likely the surviving `DbgBreakPoint` and the font failure are **the same event** — a "not precached"
assertion.

❌ **`QUICKMENU` does NOT sidestep it.** Predicted to, because the front end still runs; tried, and
`QUICKMENU=DevMenu` failed at exactly the same font with the same dirty-disc error
`[disproved 2026-09-18, n=1]`.

⭐ **Therefore: prefer routes that keep the normal front end running** — the main menu's
`CHECKPOINTS` selector, or a `campaignmap(...)` issued from inside a menu. **The ENV quick-start
switches are a dead end until the precache question is solved.**

⭐ **Meanwhile the supported route exists and is untried:** the main menu offers
`CONTINUE / NEW GAME / CHECKPOINTS / MULTIPLAYER / OPTIONS / EXTRA CONTENT`
`[verified-live 2026-09-18, n=1]`. **`CHECKPOINTS` is an in-game level select** — no ENV switch, no
patch, nothing to fix first.

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

## 6. ⭐ The stereo seams — where VR attaches (found 2026-09-16)

All of this was established **by reading the ReXGlue SDK source** (`rexglue-sdk` v0.10.0, our self-built copy). How
the code *behaves* is `[inferred-static 2026-09-16]` — nothing below has been compiled or run in a modified form.
The line numbers are exact for that source tree.

**The single most important point, and it is not obvious:** the place a frame reaches the screen is **not** where 3D
depth comes from. By the time a frame gets there it is already a flat picture — doubling it only gives both eyes the
same flat image. **Real stereo needs each eye *drawn* from a slightly different camera position.** So VR has two
separate seams, and needs both.

### Seam A — OUTPUT: where finished frames can go to the headset

The full path of every presented frame:

```
game calls VdSwap (Xbox kernel)
  -> PM4 packet with kSwapSignature        graphics/command_processor.cpp  ~948
  -> IssueSwap(frontbuffer_ptr, w, h)       graphics/command_processor.cpp   961   then ++counter_
  -> D3D12CommandProcessor::IssueSwap       graphics/d3d12/command_processor.cpp 1894
       RequestSwapTexture()                                                   1919   <- THE FINISHED FRAME
       presenter->RefreshGuestOutput(...)                                     1988   (gamma, FXAA)
  -> Presenter::PaintAndPresent             ui/presenter.cpp                 1497
  -> D3D12Presenter::PaintAndPresentImpl    ui/d3d12/d3d12_presenter.cpp      557
  -> swap_chain->Present()                  ui/d3d12/d3d12_presenter.cpp     1158   <- reaches the monitor
```

- **`IssueSwap` runs exactly once per game frame**, and `++counter_` right after it is a clean frame counter — its
  even/odd parity is a natural "which eye" switch.
- **`swap_texture_resource`** (line 1919) is the game's final frame as an `ID3D12Resource`. For OpenXR, this — or the
  gamma-corrected guest output — is what gets copied into the headset's swapchain image for the current eye, with
  `xrEndFrame` submitted from the same place. The existing `Present()` can stay as a desktop mirror.
- **This is shared SDK code**, so it is the part that reaches every ReXGlue title, Condemned 2 included.
- The DXGI swapchain is created with `swap_chain_desc.Stereo = FALSE` (d3d12_presenter.cpp:394) — that is the old
  quad-buffer 3D-monitor mode, **not** relevant to OpenXR; noted only so nobody chases it.

### Seam B — RENDER: where each eye gets its own camera

- The game uploads its shader numbers — **including the camera's view and projection matrices** — by writing the
  emulated Xenos GPU's constant registers, `XE_GPU_REG_SHADER_CONSTANT_000_X + i`. **Indices 0–255 are vertex-shader
  constants, 256–511 pixel-shader** (`D3D12CommandProcessor`, the register-write handler around **line 1776**, plus a
  range writer around **line 1841**). A write marks `cbuffer_binding_float_vertex_` stale and it is re-uploaded as the
  `kFloatConstants` constant buffer.
- **So every camera matrix passes through one SDK function.** Offsetting the view matrix there per eye is possible
  *without touching the game's own code* — but **which constant slots hold the matrix is game-specific** (and can vary
  between shaders). The plumbing would be shared; the payload would not.
- **The alternative, which only a source-built port allows:** hook The Darkness's own camera function in the recompiled
  C++. No slot-guessing and more robust, but written per game.

### ✅ The camera constants, identified — no guessing needed (2026-09-16)

**c0–c3 is the full object × camera × screen matrix; c4–c6 is the object-to-view 3×4 (rotation in .xyz, translation in .w); c7 is a translation term.**
Established two independent ways:

1. **The engine's own shader template says so**, in plain words — `System/Gl/VP.xrg` lines 58–60:
   `c[0..3] Model*Projection (Model-space to clip-space)`, `c[4..6] Model rotate (3x3) (Model-space to view-space)`,
   `c[7] Model translate (Model-space to view-space)`; and lines 1070–1074 build the position as
   `ADD R4, R8, c[7]` then `DP4 oPos.xyzw, c[0..3], R4`. `[verified-live 2026-09-16 — read directly from the file]`
2. **Every shader the game actually ships agrees.** All **420** vertex shaders in `System/Xenon/ProgramCache.xpc`,
   decoded against the SDK's `ucode.h` layout, write the position from `dp4` against **c0, c1, c2, c3** after `add … c7`,
   each with one `float4 c[128]` table at register 0 `[verified-numerically 2026-09-16, n=420]`.

⚠️ **The trap this avoided — a frame-end snapshot is unsound.** Constants live in one persistent array
(`RegisterFile::values`), overwritten on every write with no per-draw history. c0–c3 is rewritten **for every object
drawn**, so at `IssueSwap` it holds whatever was drawn *last* — probably a HUD or post-processing pass. **Sample per draw
in `D3D12CommandProcessor::IssueDraw` (~line 2271), right after the vertex shader is fetched (~2290)** and before its early
returns. Reading a constant: `memcpy(&v, &regs[XE_GPU_REG_SHADER_CONSTANT_000_X + (i<<2) + c], 4)` — base `0x4000`, stored
as the host-order IEEE-754 bit pattern `[inferred-static]`.

**What it means for VR** `[hypothesis]`: because c0–c3 already has the camera baked in *per object*, the cleanest per-eye
change is not to decompose c0–c3 after the fact but to shift the **view** before the game multiplies it in. Where the game
does that multiplication is the open question now with the reader. For geometry drawn with an identity object matrix, c4–c6 would be the whole camera view matrix, position included
(`[hypothesis]` — whether world geometry is drawn that way is unconfirmed). **Correction:** an earlier version said
"pure camera rotation"; the emulation below shows c4–c6 also carries translation in `.w` `[verified-numerically]`.

### ⭐⭐ THE PER-EYE INJECTION POINT — found statically and checked numerically (2026-09-16)

**Offsetting the camera for one eye is a single change at one function.** From disassembly of the recompiled game,
with the key function emulated in Python and its algebra checked against random inputs:

- **The projection matrix P is held separately**, at **`RC+17088..17151` (0x82A6DCC0)**, where `RC = 0x82A69B00` is the
  global render context. `[inferred-static]`
- **`sub_82249580` ("apply viewport") sets it**: it copies the current viewport's projection into `RC+17088`, marks the
  matrices dirty (`[RC+8228] |= 1`), then scales column 0 by 2/width and column 1 by 2/height. It runs **once per viewport
  apply — a few times per frame, not per object.** `[inferred-static]`
- **`sub_8275EEF8` builds that projection**: FOV in **degrees at viewport `+260`** (clamped 0.1–179), aspect at `+264`,
  near/far planes at `+300`/`+304`; a D3D-style row-vector perspective, with an orthographic branch for 2D. Who *sets*
  the FOV is not traced — `vp_fov` / `vp_zoom` / `vp_frontplane` / `vp_backplane` are registered as entity keys in
  `sub_82389D18`, most likely a camera/viewport entity `[hypothesis]`.
- **`sub_82248A78` is the only function that writes c0–c7** — a scan of every generated function found no other writer.
  It is called only from the pre-draw commit `sub_82249488`, when the matrix-dirty bits are set. `[inferred-static]`
- **What it computes, verified against the emulation** `[verified-numerically 2026-09-16, n=50 random M and P]`:

  | output | equals | error |
  | --- | --- | --- |
  | c0–c3 | transpose(M with translation zeroed) × P | 9e-16 |
  | c4–c6 | first three columns of M, translation in `.w` | 0 |
  | c7.xyz | t·R⁻¹ (t = M's translation, R = its 3×3); c7.w = 0 | 8e-6 |
  | shader path (add c7, dp4) | reproduces v·M·P and v·M | 7e-8 |

  where **M** is the object's model-to-view matrix from the render context's matrix stack.

**⭐ The change for one eye:** at the **end of `sub_82249580`**, after the width/height scaling, replace **P** with
**T(e)·P**, where T(e) is the identity matrix with row 3 = (e, 1) — a sideways shift of `e` in view space. The emulation
gives exactly **P((v·M) + e)**, error 2e-6 `[verified-numerically, n=1 case on top of the 50-case base path]`.

- **Apply it only to perspective viewports:** `RC+17132 == 1.0` and `RC+17148 == 0`. The HUD, menus and other orthographic
  viewports go through the same function and must be left untouched, or the interface would be doubled and shifted.
- User clip planes (`sub_82249250`) read the same P, so they stay consistent. `[inferred-static]`
- ⚠️ Culling uses the viewport's own frustum planes, built from the *unshifted* projection, so objects right at the edge
  may be culled slightly early in one eye. `[hypothesis]`
- ⚠️ **The emulation's own assumption:** that its reading of each AltiVec vector operation matches the SDK's translation
  (checked for `vmrghw`, `vmsum*`, `dp_ps` against the generated C++) and that `vupkd3d128` yields w = 1. The exact c7 match
  supports it, but **nothing here has run in the game yet.**

### ✅✅ RUN LIVE 2026-09-18 — THE MODEL HOLDS AND THE SHIFT WORKS

Everything above this line was static. It has now been observed in the running game with a read-only
weak-symbol hook (`src/vr_viewport_probe.cpp`). Full write-up:
`modding-notes/2026-09-18-the-stereo-injection-point-works.md`.

**Two viewport applies per frame during gameplay, in exact 1:1 alternation**
`[verified-live 2026-09-18, n=3 runs]`:

| | P00 | vertical FOV | near | far | where |
| --- | --- | --- | --- | --- | --- |
| **A** | 1.07111 | 55.4° | **1.80** | ~5000 | gameplay; varies slightly between runs |
| **B** | 0.75000 | 73.7° | **4.00** | ~2045 | gameplay **and** menus; never varies |

⚠️ **CORRECTION to the perspective filter above.** The plan was to shift only viewports with
`RC+17132 == 1.0 && RC+17148 == 0` so the orthographic HUD would be spared. **Every one of the 9,004
matrices logged in the longest run was perspective — not a single orthographic viewport passes
through this function** `[verified-live 2026-09-18, n=3 runs]`. That filter passes everything and
protects nothing; the HUD does not come through here at all. **Tell the viewports apart by their near
plane instead** (1.80 vs 4.00), which is what the probe does.

✅ **P does not change when the camera turns** — with the look stick at **full** deflection, right
then left, the set of applied matrices is bit-identical to a no-input control seconds earlier, while
the picture plainly changed `[verified-live 2026-09-18, n=1 run, 2 turns + 1 control]`. ⚠️ A first
attempt with a 0.55 deflection moved nothing at all and proved nothing; the control profile's
"look speed ramps" note is the reason. **So the turn lives in M, not P, exactly as assumed.**

✅ **The shift itself works.** `P' = T(e)·P` reduces to "add e × row0 to row3". With
`DK_STEREO_EYE=0.5` applied to viewport A during gameplay: `row3[0]/P00 = 0.50000` exactly, over 600
applications with **no drift** (checked, because a compounding offset looks correct for one second
and then destroys the picture), viewport B bit-identical to the control, and the game rendering
normally `[verified-live 2026-09-18, n=1 run]`. At deliberately absurd offsets (3 and 50) the 3D
content distorts violently **while the 2D overlay stays exactly put** `[verified-live, n=2]` — the
clearest demonstration that the hook reaches 3D geometry and nothing else.

⚠️ **Still open:** which viewport the player actually looks through is not confirmed by eye (A is
very probably it `[hypothesis]`); no left/right pair has been produced; the right scale for `e` in
this engine's units is unknown; nothing has been seen in a headset.

**Original static plan, kept for the record:** log `RC+17088..17151` and `RC+17132` at each return of
`sub_82249580`. Expect a few distinct values per frame, and the perspective P should **not** change
when the look stick turns — the camera turn lives in M, not P.

### ⚠️ The design choice this exposes — decide it deliberately, it is the foundation

How to produce two eyes, in rough order of effort:

| approach | what it needs | quality / cost |
| --- | --- | --- |
| **Alternate-eye rendering** — camera offset left on even frames, right on odd, each frame sent to its eye | Seam A + a view-matrix offset (Seam B) + OpenXR head pose | **least invasive**; halves each eye's framerate, can shimmer. Painful on the dev PC's CPU, fine on the home PC. The UEVR-style route. |
| **Depth reprojection** — one render plus the depth buffer, second eye synthesised | Seam A + depth buffer access | cheapest to build; visible artefacts at edges |
| **True dual render** — whole frame drawn twice per swap | the guest to issue its draws twice | best quality; **very hard**, because a recompiled console game issues each frame's draws exactly once, interleaved with state |

~~**Current recommendation, not a decision:** alternate-eye rendering…~~ — superseded by the decision below.

### 🧭 DECIDED 2026-09-18 — GEOMETRY STEREO, ONE EYE PER GUEST FRAME, THE WORLD HELD STILL FOR THE SECOND EYE

**The decision (made on Fable, with the first live pair in hand):** each eye is a **real render of the
scene from its own position**. The guest draws eye L on one frame and eye R on the next; the offset is
flipped at the proven site (end of `sub_82249580`, viewport A only); **the simulation must not advance
between the two**, so both eyes show the same instant; finished frames are routed to the headset by
frame parity at Seam A. This is "synced sequential" in UEVR's vocabulary. Depth reprojection is the
**named fallback**, not the plan.

**Why this and not the others:**

- **Depth reprojection is worst exactly where this game lives.** The Darkness keeps hands, guns and
  tentacles within arm's reach of the camera all game long. Measured on the first pair: the hands
  carry **≥ 200 px** of disparity at 1280 wide against **47 px** for the seat backs
  `[measured 2026-09-18, n=1 pair]`. A synthesised second eye has to invent everything the near
  object uncovers, and with disparities that large the smear halo would sit permanently in the middle
  of the picture. It also needs deep work in the shared render-target cache (EDRAM depth, resolves).
- **True dual render inside one guest frame** means doubling the emulated GPU's render targets,
  resolves and post chain in shared SDK code — the reason Xenia itself has no stereo. Same picture
  quality as the chosen route, an order of magnitude more work, and none of it reusable from what is
  already proved.
- **The chosen route reuses everything already verified live** — the site, the exact offset, the
  viewport selection, the untouched HUD — and its one new requirement (hold the world still on
  alternate frames) is a small, testable thing.

**What it costs, stated plainly:** the pair rate is **half the guest frame rate**. On the dev PC the opening scene runs at about **15 real frames a second** (swap to swap ~66 ms
`[measured 2026-09-18]` — an earlier "29.4 fps" counted viewport applies, not frames), so here that is
**~7 pairs a second**. The dev PC is known to be slow and is not the judge: **what the home PC
reaches, and whether the guest is capped at 30, is unmeasured.** Head rotation
is smoothed by the OpenXR runtime's reprojection regardless, but world motion at 15 Hz will look
choppy. **So raising the guest above 30 fps is now on the critical path**, not a nicety — and if it
cannot be raised and 15 Hz proves unusable in the headset, that is the trigger for the depth fallback.

**What was proved the same day, which is why this is a decision and not a hope:**

- ✅ **A true left/right pair exists.** With `DK_STEREO_EYE=0.6`, `DK_STEREO_PERIOD=4`, two window
  grabs 62 ms apart, labelled L and R from the hook's own log lines: far interior **+47 px**, hands
  **≥ +200 px** (search limit), HUD prompt **0 px** (match quality 0.98)
  `[verified-live 2026-09-18, n=1 pair measured, 2 more by eye]`. Sign is right: nearer things sit
  further right in the left eye. Evidence: `dev-archive/recon/2026-09-18-first-stereo-pair/`.
- ✅ **Only viewport A matters.** Shifting viewport B alone moved **nothing** — interior, hands and
  HUD all 0 px at match quality ≥ 0.99 across five pairs `[verified-live 2026-09-18, n=5 pairs]`.
  A draws the world **and** the first-person hands; B draws nothing visible here (its 73.7° FOV,
  near 4 / far 2045 would suit a light's shadow view `[hypothesis]`). The HUD needs no protection
  at this site.
- ⚠️ **CORRECTION, same day — a frame is a SWAP, not an apply of A.** This section first said the
  frame boundary could be "one apply of A". **Wrong:** in gameplay A is applied **three times per
  frame** (histogram over 2,400 frames: 847 frames with 3 applies, 99 with 2, menus 0)
  `[measured 2026-09-18]`, in a burst a few ms wide, then a ~66 ms gap. Flipping on applies
  therefore changed eye mid-frame. **The eye now flips only in a hook on `sub_82867620`, the one
  guest function that calls `VdSwap`** (three call sites), so every draw of a frame shares one eye.
- ✅ **With frame-true eyes the pair is repeatable:** interior **47 px**, hands **182–208 px**, HUD
  **0 px** in 7 of 7 usable pairs `[verified-live 2026-09-18, n=7 pairs]`. From
  `px = 2e·P00·640 / w`: seat backs ≈ 17.5 units away, hands ≈ 4 units. ⚠️ The measured sign came
  out inverted against the hook's labels — consistently — which fits the window showing the frame
  built one eye-span earlier (GPU queue + present) `[hypothesis]`. **Which grab is which eye cannot
  be settled from outside the process; it is exact at Seam A**, one more reason the next step
  lives there.
- ❌ **RETRACTED: "the shift does not disturb lighting".** That was concluded from 50 frames whose
  eye labels were scrambled by the mid-frame flipping, so the test could not have found the effect
  `[disproved 2026-09-18]`. With frame-true labels, **the two front-seat heads render red-lit in one
  eye and as dark silhouettes in the other, in lock-step with the eye for 12 consecutive
  alternations** `[verified-live 2026-09-18, n=12 alternations]`; a fixed `+0.5` run shows the red
  too, the unshifted control does not. So **something in the frame reads P and does not get a
  consistent answer per eye** — a light, a projected texture, a clip plane, or one of the three
  passes. **Open defect, cause unknown.** It does not change the decision (every geometry route has
  to make the eyes consistent), but it is the first real cost of it, and the depth-reprojection
  fallback would not have this class of problem.

**The parity rule for Seam A** `[hypothesis]`: flip the eye at the **guest's `VdSwap` call**, on the
guest thread, not at the viewport apply. The command processor executes swaps in order, so
`IssueSwap` number N then carries the same parity as the hook by construction, with no cross-thread
signalling — provided both sides count from the same swap.

**Build order from here:** (1) hold the world still on the second eye's frame; (2) side-by-side
composition of consecutive frames in the presenter, so a pair can be seen on the monitor and in the
OpenXR simulator with no headset; (3) OpenXR submission per eye, replacing P wholesale with the
headset's own per-eye projection at the same site; (4) head pose into M (the debug-camera matrix at
`client+8880` is the candidate); (5) get the guest above 30 fps.

### ✅ Who sets the FOV — answered: set once, not per frame (2026-09-18)

An exhaustive scan of `.text` for every D-form store to viewport displacement 260 or 264 with base
≠ r1, mapped to owning functions `[measured 2026-09-18]`. Only four functions write +260:

- `sub_82273760` — constructor, clears 256…316.
- **`sub_8275EE08` — the defaults, and they confirm the field map outright: +260 = 90.0 (FOV
  degrees), +264 = 1.3333334 (4:3), +300 = 4.0 near, +304 = 2048.0 far** `[measured, read from
  `.rdata`]`.
- `sub_8275EEF8` — the projection builder, clamping +260 in place (0.1–179).
- `sub_8271FE08` — field-by-field copy-assignment (0…285); carries a value, does not originate one.

⚠️ **Gap, stated rather than glossed:** indexed stores (`stfsx`/`stwx`, 2,528 in `.text`) are outside
that scan's reach, and the copy path could deliver a zoom/cutscene FOV as a whole-struct assignment.
Treat "set once" as the **working assumption**; the live log settles it.

Separately, `sub_82389A18` reads level keys **`VP_FOV` → camera entity +276**, `VP_ASPECTRATIO` →
+288 (and 1/aspect → +292), `VP_FRONTPLANE` → +316. The lower-case `vp_*` keys are referenced only
from `sub_82389D18`. **There are two FOV homes** — camera entity +276 and render viewport +260 — and
the hand-off between them was not found statically `[inferred-static]`.

**What it means for VR, and it is good news:** a headset's per-eye FOV and asymmetric frustum need
**no** per-frame interception. The injection point above (end of `sub_82249580`) is downstream of
FOV, aspect and the clamp, so replacing P there overrides all of it in one place.

### ✅ How to attach the probe: the weak-symbol hook, not `midasm_hook` (2026-09-18)

`sub_82249580` is generated in `generated/default/darknessrecomp_recomp.160.cpp`, body lines
**5423–5780**, with **exactly one `return;` at 5779** — no early exits, no tail calls
`[verified-numerically 2026-09-18]`. Guest range 0x82249580…0x822497DF; epilogue `addi r1,r1,176` at
0x822497D8. The 2/width and 2/height column scaling is the last work it does (lines 5699–5774), so
the injection point is after 5774. `RC` is confirmed inside the function itself
(`lis r11,-32089; addi r31,r11,-25856` → 0x82A69B00).

⚠️ **`[[midasm_hook]]` is real but the wrong tool here.** The emitted call passes only the named
registers — **no `ctx`, no `base`** — so it cannot read guest memory, which is exactly what logging P
requires. It also forces a codegen re-run. `[inferred-static 2026-09-18]`

✅ **Use the weak-symbol hook.** `DEFINE_REX_FUNC(name)` emits `name` as a weak alias of
`__imp__name` precisely so hooks can replace it, and all **nine** generated call sites call
`sub_82249580`, never `__imp__`. A strong definition in `src/` wins at link and sees every call — no
generated code edited, no codegen re-run. `REX_HOOK_RAW(sub_82249580)` expands to
`extern "C" void sub_82249580(PPCContext& __restrict ctx, uint8_t* base)`, so `REX_LOAD_U32` works
inside it; call `__imp__sub_82249580(ctx, base)` first, then read the 16 floats at `RC+17088`.

**Read them as `p[0..15]`: `RC+17132` is `p[11]` and `RC+17148` is `p[15]`** — the perspective test
is `p[11] == 1.0 && p[15] == 0.0`. **Log only when P changes**, with a repeat counter: the function
runs a few times per frame and unfiltered that is hundreds of lines a second through a spdlog lock
on the render thread.

### ⭐⭐ The debug camera is a full second camera, seeded from the player camera (2026-09-18)

`[verified-numerically 2026-09-18]` for the layout, `[inferred-static]` for the substitution.

Client vtable base **0x820807E0** (two constructors install it; it holds `sub_823FF1D0` at +32 and
`sub_823FB188` at **+1236**, the exact slot `toggledebugcamera`'s stub reads).

**State, offsets from the client object:**

| offset | what |
| --- | --- |
| **+2032** | the live **player** camera matrix |
| **+8816** | mode byte; code reads `(b>>1)&3` → 0 = off, **1 and 3 = active** (three states cycled, which is why the menu's "Off" calls the toggle twice) |
| **+8880…+8943** | the **debug camera's own 4×4 matrix**, 16-byte rows: +8880 forward axis, +8896 right axis, +8912, **+8928 = position** |
| +8944…+8964 | the six `dbgcam_*` inputs (one-line setters: `stfs f1,N(this); blr`) |
| +8968 | movement speed |

- **`sub_823FAD28` (set mode)** — when the mode leaves 0 it copies **64 bytes (4× `lvx128`/`stvx128`)
  from `client+2032` into `client+8880`**, then re-orthogonalises. **The debug camera starts as a
  copy of the player camera, in the same space and the same layout** — the strongest single piece of
  evidence that the two are interchangeable.
- **`sub_823FABC0` (per-frame update)** — textbook free-fly:
  `pos += forward*(fwd-back)*speed*dt; pos += right*(left-right)*speed*dt; rotate(lookvel_x*dt, lookvel_y*dt)`.
- **`sub_823F9B00` (client view build, vtable slot +428)** — calls `sub_82499C88(this, buf)` to fetch
  the player camera from `+2032`, then immediately
  `if ((client[8816] & 6) != 0) { m = client+8880; …rebuild basis… }`. **That branch is the
  substitution point.**

**Why it matters for VR** `[hypothesis]`: if that function's output reaches the render context's
matrix stack, the debug camera moves the view **without moving the player, entirely inside M** —
upstream of the `P` this section replaces at the end of `sub_82249580`. That makes it both a test
instrument and a plausible 6DoF path: write the headset pose into `client+8880` rows 0–3 and leave
the stereo split to the P injection.

⚠️ **The last link is NOT proved** — nobody has traced `sub_823F9B00`'s output into the render
context at 0x82A69B00. Two ways to settle it: statically, follow its output buffer to its consumer
and check it reaches the matrix stack `sub_82248A78` reads when it writes c0–c7; or live, with the
debug camera on and the P probe running — the picture should move while the player does not, and the
perspective P should **not** change. If P *does* change, this section's model needs revisiting first.

⚠️ And none of it is reachable yet: see §3, `cheat()` is an empty stub.

### Next concrete step — connects the look test to Seam B

**Find which vertex-constant slots hold The Darkness's view matrix, by measurement.** Log every vertex float-constant
write around the Seam B handler while `looktest.py` pushes the right stick. **The constants that change in lock-step
with the stick are the camera.** This needs the game running (`[FLAT]`), but no design decision — and its answer decides
whether Seam B is usable game-agnostically.

## 7. Not yet looked at

Renderer, camera maths, input, the `.xcr`/`.xrg`/`.xtc`/`.xac` formats, and whether the PS3 version
offers anything easier. None of it is blocked by anything except §4.
