# b013: a brightness dial, a per-eye light log, and a clean quit (2026-11-06 evening)

Home PC, Opus, no game running. Answers to three faults from the b011 headset run that morning.

## The crash on the way out `[inferred-static 2026-11-06, n=1 dump]`
The dump (kept in `D:/the-darkness-build/logs-kept/2026-11-06-hands-jump/`) is 0xC0000409 with
fast-fail code 10: Control Flow Guard stopped a call to an address that is not valid code. Main
thread, right after "Title terminated; hard-exiting process." The stack: `ReXApp::OnClosing` ->
`std::_Exit(0)` -> Windows runs every DLL's unload code -> Virtual Desktop's OpenXR runtime
(virtualdesktop-openxr.dll, openxr-oculus-compatibility.dll, VirtualDesktop.LibOVRRT64_1.dll) cleans
up the headset session we never closed -> d3d11 / the NVIDIA driver -> the bad call.
Fix (SDK patch 13, rex_app.cpp): `TerminateProcess(GetCurrentProcess(), 0)` before `std::_Exit`.
Same hard exit, but no DLL runs its unload code. Proof still owed: a normal quit, no new dump.

## Too bright: the colour path is correct `[inferred-static 2026-11-06]`
The headset gets the guest output AFTER the game's own gamma ramp (the apply_gamma pass runs before
ComposeSideBySidePair). Its values are already gamma-encoded and go byte for byte into an sRGB
swapchain (format 29, typeless images drawn through the UNORM view, no decode). That is how an
ordinary sRGB picture should arrive, so the old "sRGB view of a linear picture" suspect is
`[disproved]`. What remains is the display curve: the game was graded for a TV (a plain power curve,
dark shadows), the headset decodes with the sRGB curve (lifts the darkest tones), and the Quest 3 LCD
has grey blacks `[hypothesis]`.
Fix: a dial. `DK_XR_GAMMA` at launch (1.0 = unchanged), the bridge var `xr.gamma` live. Every value c
becomes pow(c, dial) before it is stored: 1.1-1.3 deepens the shadows, white stays white. A value
outside 0.2-5 is ignored.

## Lights flicker: a log to tell the two causes apart
Each eye is a different game frame (~6.5 ms apart). Either a lamp the game animates per frame is lit
in one eye and not the other (the gap between the eyes swaps sides), or the old per-eye lighting
defect is back (a steady gap on one side). b013 copies both eye pictures to the CPU every frame,
measures the mean brightness (every 8th pixel each way) and logs every 300 pairs:
`[XR] eye light: N pairs, mean L= R=, |R-L| mean= max=, over 1%: N pairs, side swaps N, L range a-b, dial d`.
`DK_XR_EYE_LIGHT=0` turns it off. Cost: two 3.6 MB copies per frame on the GPU, ~29,000 pixel reads
on the CPU; not yet measured in a run.

## Where it is
Built `[compile-verified 2026-11-06]`: exe 4355fa6726e2, rexgpu-xenos.dll dd18f0f7b74c (the game
build's own copy from `rexglue-sdk/out/win-amd64/`), in its OWN folder
`C:/NonSteam/the-darkness/build-home-xr-b013/` (Assets junction re-made), so the b012 hands test in
`build-home-xr` stays clean. Builds repo: `headset/v0.1.0-b013`. Patch:
`dev-archive/sdk-patches/13-xr-brightness-dial-eye-light-clean-quit.patch`. b013 is b012 + these
three, so after b012's hands are confirmed, b013 can replace it.

## Test
`dk_vr_headset_hands.ps1` in the b013 folder. (1) Play in a lit place, read the eye light lines.
(2) Turn `xr.gamma` 1.0 -> 1.15 -> 1.3 while Tefa says which looks right. (3) Quit from the menu: no
new `darknessrecomp.exe.*.dmp` in the CrashDumps folder.

## b014: a switch for the game's auto exposure (same evening)
Tefa then described the flicker more exactly: *"it was like the brightness dial flickered super light
constantly"*, so the WHOLE picture pulses brighter, not single lamps `[reported 2026-11-06, headset]`.
First suspect: the game's auto exposure (XREngine_Histogram), which measures the picture every frame
while the frames alternate eyes `[hypothesis]`. Patch 12 (b011's log quieting) is ruled out: it only
changes logging `[inferred-static 2026-11-06]`.
`sub_825EC5C8` (the script command xr_pptoggledynamicexposure) only flips bit 0 of engine+656
`[inferred-static 2026-11-06]`; which state means "on" is unknown. So b014 (b013 + this, exe
b2a99ea9a7e7, folder `build-home-xr-b014`, builds repo `headset/v0.1.0-b014`) adds, in the
post-process hook: `fx.exposure` (DK_VR_EXPOSURE at launch) -1 = leave it, 0 = force the bit clear,
1 = force it set; `fx.exposure_seen` = the bit; a `[VP] auto exposure:` log line on each change.
`[compile-verified 2026-11-06]`. Test: read `fx.exposure_seen`, set `fx.exposure` to the other value
while Tefa watches: pulse gone = found; then check the picture is not stuck too dark or too light.
Use b014 instead of b013 for the headset run.

## ⭐ Tested on the pretend headset, 19:30-19:37: THE GAME'S AUTO EXPOSURE IS THE BRIGHTNESS FAULT
b014 on the OpenXR Simulator (home PC, VR Bench driving START + A into the CONTINUE save, the
corridor Tefa named), read through the bridge and b013's eye light lines `[measured 2026-11-06,
simulator, n=2 flips each way]`:
- The game leaves bit 0 of engine+656 at **1**. With it at 1 the picture's mean brightness is
  **0.83** (walls blown to white, screenshot), and it pumped: L range 0.759-0.834 within one
  300-pair window with the head still.
- `fx.exposure=0`: mean **0.31**, range 0.312-0.312 (dead steady), the corridor dim and grimy as the
  game is meant to look. Head turned 60 degrees: 0.242, still steady. Back to 1: 0.83 again; 0 again: 0.31.
- The two eyes agree (|R-L| under 1 %, never swapping), so the per-eye lighting defect is NOT it.
So bit 0 set = the game's auto exposure on, and in our port it overshoots badly and pumps: that is
both Tefa's "too bright" (since 2026-10-30) and "the brightness flickers super light". Likely why
`[hypothesis]`: the exposure is measured from the picture through GPU resolves, and our log counts
~3,000 failed "Resolve region is empty" a second in play, so it measures garbage. The real fix would be
making that measurement work; for now fixed exposure is the workaround.
`DK_VR_EXPOSURE=0` is now in b014's three launch scripts (copies in `dev-archive/build-scripts/`).
Not yet checked: outdoors or in very dark places, where fixed exposure may be too dark or too light
(the dial `xr.gamma` and the game's own brightness option cover that), and the flat window.
Log and both screenshots kept in `D:/the-darkness-build/logs-kept/2026-11-06-exposure-sim/`.
Side finds: b012 writes "[IK] the player's hands: IK object ..." about 15 times a second (1,437 lines in
~1.5 min): a new log flood to quiet. The log of a run goes to the folder's own `logs/`.
`dk_sim_drive.py` now takes the build folder as its first argument (a hand edit of the path launched
b012 instead of b014 once this evening).
