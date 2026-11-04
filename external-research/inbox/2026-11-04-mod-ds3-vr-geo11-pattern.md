# DS3-VR (scubasteve2365): the geo-11 + OpenXR pattern, looked at for the Darkness (2026-11-04, modding session)

Tefa, 2026-11-04: *"a discord member told me to get it running in Geo 11 and send the github link to you as an example"*:
https://github.com/scubasteve2365/DS3-VR

What it is `[reported, from its README 2026-11-04]`: Dark Souls III (Steam 1.15.2) in VR. A `dinput8.dll` in the game
folder takes the stereo frame that **Helifax's geo-11 fix** makes (geo-11 is a Direct3D 11 stereo driver, the 3D Vision
successor), sends both eyes to an **OpenXR** runtime (SteamVR, Virtual Desktop's VDXR tested on a Quest 3), adds head
tracking (the camera follows the head), a HUD on a floating, resizable panel, a 3D lock-on marker, an in-headset menu
(General / Graphics / Developer), DLAA and per-eye DLSS on RTX cards, frame rate capped at the headset's refresh. No
motion controls. Both eyes come from one game frame. Install: zip into the Game folder, `install-ds3vr.bat` fetches
geo-11 and sets the graphics options; the VR runtime must be up first.

Does it apply to the Darkness? **Not as a drop-in** `[inferred-static]`: our Darkness is the Xbox 360 game run through the
rexglue recompiler with a **Direct3D 12** presenter (dossier: `D3D12Presenter`, `D3D12CommandProcessor::IssueSwap`), and
geo-11 only hooks Direct3D 11. Our own XR bridge already does what DS3-VR's dll does (two eyes to OpenXR, head
tracking) and more (motion-controlled hands). **What is worth copying:** the HUD on a floating panel the player can
size and place; the in-headset menu with tabs (the same ask as Village's Visceral Emporium); per-eye DLSS/DLAA for the
frame rate (our 20 fps row); the frame cap at the headset rate; "both eyes from one frame" as a cheap stereo mode.
Helifax's geo-11 fix pages are where the stereo maths for a D3D11 game would be read, if a D3D11 game of ours ever needs it.

Starred and followed scubasteve2365 (github-thanks.md).
