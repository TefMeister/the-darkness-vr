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
