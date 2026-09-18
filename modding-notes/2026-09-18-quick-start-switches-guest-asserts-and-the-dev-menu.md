# 2026-09-18 — where the ENV file really lives, a guest assert that was killing us, and a real dev menu in the retail data

`/lm`, dev PC `DESKTOP-V8GTSIR`, Claude driving, Tefa at work. Five live runs plus one debugger
session. Reader (one background helper) ran static work alongside.

The session set out to do the `[FLAT]` **RESUME HERE** row — the stereo sanity check at
`sub_82249580`. It did not get there. What it did instead was remove the thing that makes every
stereo test run expensive, and turn up a much bigger lead. Both are written up here; the stereo row
is untouched and still first.

---

## 1. `EnvironmentXbox.cfg` is read from the GAME's disc root, not from beside the exe

The dossier said "beside the executable" (from `sub_827784F0`). That is true of the **guest's** idea
of beside-the-executable, which ReXGlue maps to the `Assets\` junction — i.e.
`E:\the-darkness\game-files\`, the extracted disc, guest `D:\`.

- Host-side (`E:\the-darkness\build-local\EnvironmentXbox.cfg`): **completely ignored.** A run with
  `QUICKMAP=NY1_Tunnel` there was indistinguishable from no file at all — logos, title screen at
  72 s, the usual `[verified-live 2026-09-18, n=1]`.
- Guest-side (`game-files\EnvironmentXbox.cfg`): **read.** Behaviour changed instantly and
  reproducibly.
- **Control run** with an unrecognised key (`HARMLESSKEY=1`) guest-side booted normally through the
  logos `[verified-live 2026-09-18, n=1]`. So the change is caused by the quick-start keys, not by
  the presence of the file.

`EnvironmentXbox.cfg.off` had been sitting in `game-files/` since 2026-09-16, in the right place,
never switched on.

## 2. Both quick-start switches were killing the process — and it was the game's own assert

`QUICKMAP=NY1_Tunnel` **and** `QUICKMENU=main` both exited at ~4 s with `0x80000003`
(`EXCEPTION_BREAKPOINT`), no `[FATAL]` in the log, nothing on stdout `[verified-live 2026-09-18,
n=3]`. Not the known missing-function signature, so the crash loop and `find_missing_functions.py`
would not have found it.

Run under x64dbg, the stack said it plainly: `rexruntime.rex::debug::Break()`, reached through the
Xbox kernel import **`DbgBreakPoint`**, called from guest **0x820DFC60**
(`generated/default/darknessrecomp_recomp.4.cpp` ~line 2672):

```
lwz   r11,1936(r31)
cmplwi cr6,r11,0
bne   cr6,0x820dfc64        # field non-null -> skip
bl    0x828a7518            # -> DbgBreakPoint
```

So it is **a conditional debug assertion inside the game**, on a field the normal front-end route
fills in and the quick-start route does not.

**The fix, and why it is the right one:** on retail hardware with no debugger attached,
`DbgBreakPoint()` is ignored and the title carries on. Breaking the host process turns every
surviving debug assert in a shipped game into a hard exit. Changed the shared SDK
(`rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_debug.cpp`) to log a warning and continue, with
`REXGLUE_BREAK_ON_GUEST_ASSERT=1` to get the old hard break back when you *want* to catch one in a
debugger. Patch saved as `dev-archive/sdk-patches/06-guest-dbgbreakpoint-is-not-a-host-fault.patch`.

⚠️ **This SDK tree is shared with `condemned-2-vr`** (`E:\condemned-2-vr\src\rexglue-sdk\`). The
change is not Darkness-specific and should help there too, but Condemned 2 has not been re-run
against it. Flagged rather than assumed.

Rebuilt and deployed; the quick-start path now survives the assert `[verified-live 2026-09-18,
n=1]`.

## 3. …and then stops on a missing font, not on the level

With the assert survived, `QUICKMAP=NY1_Tunnel` gets further and stops with:

```
[NtCreateFile] FAILED: path='d:\content\fonts\text.xfc' -> 0xc000000f
XamShowDirtyDiscErrorUI called! user_index=0
```

then sits at ~455 MB with nothing drawing `[verified-live 2026-09-18, n=1]`.

`NY1_Tunnel` **is** a real map (`Content/Xdf/NY1_Tunnel*.XDF`, 218 XDF files, NY1_* and NY2_*
throughout), so the map name is not the problem. But there is **no `Content/Fonts/` folder and no
`*.xfc` anywhere** in the 498-file extraction — checked case-insensitively across the whole tree.

⚠️ **Do not read that as "the extraction is broken".** The front-end renders text perfectly (see
§4), so the fonts the menus use are reaching the game somehow — most likely packed inside the
`.XDF` archives. Which of packed / missing / elsewhere it is, is with the reader. **Until that is
settled, `QUICKMAP` is not yet a usable shortcut.**

## 4. A real developer menu and free camera exist in the retail data — but did not open

The reader found, in the **compiled** `Content/Gui/CubeWnd.xcr` (`MOS DATAFILE2.0`, ASCII payload)
`[measured 2026-09-18]`:

```
Z15FreezeCam On        cl_toggledebugcamera();pause(1)
Z20Toggle DebugCamera2 cl_toggledebugcamera2()
Z20Noclip              cmd_forced(noclip); cg_clearmenus()
Z20God-mode            cmd_forced(godmode); cg_clearmenus()
```

plus menus `DevMenu`, `ZoneMenu`, `GAMEMENU2`, `INGAME_DEBUG*`, `AIDEBUG`, `Milestone*` with direct
`campaignmap("…")` level jumps; and in the executable a contiguous bindable-action table
`toggledebugcamera`, `toggledebugcamera2`, `dbgcam_moveforward/backward/left/right`,
`dbgcam_lookvelocity_x/y`, `noclip`, `noclip2`, `godmode`, `cyclecamera` (0x82080434–0x8208078C).
**A free camera with its own movement and look axes is not a stub.** Its positive control passed
(the same search finds `logo_topcow`, `QUICKMAP`, `GameDebug`).

Every entrance is wrapped in a gate command `cheat(...)`. The reader also found two ENV keys read by
the same routine that reads `QUICKMAP`: **`SHOW_DEVELOPMENT`** and **`SHOW_CONFIDENTIAL`**.

**Tested live, and it did not work** `[verified-live 2026-09-18, n=1]`: with
`SHOW_DEVELOPMENT=1` and `SHOW_CONFIDENTIAL=1` guest-side, driving title → START → the save-notice
dialogue → A → START → main menu, then pressing **X** (the `GUI_BUTTON2` the menu binds the DevMenu
entrance to) twice, and **Space** as a keyboard fallback — the main menu did not change. No dev
menu.

So **the two ENV keys alone are not enough.** What `cheat()` actually tests (registered 0x8236D860,
handler 0x8236F300, front-end virtual at vtable+260) is now the deciding question and is with the
reader. ⚠️ Do not record the free camera as available; record it as **present in the data and not
reachable yet**.

## 5. Two incidental findings worth keeping

- ⭐ **The main menu has `CHECKPOINTS`** — `CONTINUE / NEW GAME / CHECKPOINTS / MULTIPLAYER /
  OPTIONS / EXTRA CONTENT` `[verified-live 2026-09-18, n=1]`. A supported in-game level select is
  its own route to any level, needing no ENV switch and no patch. **It is the cheapest thing to try
  next for fast level entry**, and nobody has driven it yet.
- **The focus ambiguity from 2026-09-16 is resolved, and the answer is "sometimes".** The test
  harness now records *which* process owns the foreground at every sample. The game took focus at
  launch in one run and left focus with PowerShell for a whole 40 s run in another
  `[verified-live 2026-09-18, n=2 of 5 runs each way]`. So a focus change during a run is **not**
  evidence the game grabbed it, and the 2026-09-16 reading was rightly left open.

## What is NOT established

- Whether the missing `text.xfc` is packed, absent, or elsewhere.
- Whether `cheat()` can be opened at all in retail.
- Whether the debug camera, if opened, writes the same render-context matrix (M) that §6's
  projection sits downstream of — which is what would make it useful for stereo.
- Whether the SDK change is safe for `condemned-2-vr`. Not re-run there.
- **Nothing about stereo.** The `[FLAT]` RESUME HERE row is exactly where it was.
