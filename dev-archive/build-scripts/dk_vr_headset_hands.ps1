# The Darkness VR, headset run WITH motion-controlled hands. Same as dk_vr_headset.ps1, plus:
# DK_HANDS=1: each controller moves its hand (position, through the game's own hand IK), the PLAYER's only (2026-10-29);
# DK_HANDS_ROT=1: the hands also turn with the controllers, by the pose in dk_hand_pose.txt (the hand poser).
# THE HAND POSER (2026-10-29), all on the controllers: hold B and Y together for 1 second = pose mode (the hands show
# the game's own angle, the game gets no controller input); hold a controller the way its game hand looks and pull
# that trigger = lined up; that hand's stick turns it a little (grip held: the stick rolls it); B + Y for 1 second
# again = pose mode off, saved. With no dk_hand_pose.txt yet the hands keep the game's own angle until lined up.
$env:DK_SBS = "1"
$env:DK_SBS_HOLD = "1"
$env:DK_STEREO_PERIOD = "1"
$env:DK_CAM_EYE = "1.25"
# 2026-10-29: the freeze off for the speed test (Tefa: "Jackie still moves at super speeds"); dk_vr_headset_hands_freeze.ps1 keeps it on
$env:DK_FREEZE = "0"
# 2026-10-29: the screen effects off (Tefa: "motion blur is so blurry")
$env:DK_VR_SCREEN_FX_OFF = "1"
$env:DK_EYE_BY_PACKET = "1"
$env:DK_XR = "1"
$env:DK_XR_HEAD = "1"
$env:DK_XR_HEAD_MIRROR = "0"
$env:DK_HANDS = "1"
$env:DK_HANDS_ROT = "1"
Remove-Item Env:XR_RUNTIME_JSON -ErrorAction SilentlyContinue
$p = Start-Process -FilePath (Join-Path $PSScriptRoot "darknessrecomp.exe") -WorkingDirectory $PSScriptRoot -PassThru
"pid=$($p.Id)"
