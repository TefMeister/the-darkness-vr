# The Darkness VR, headset run WITH motion-controlled hands (2026-10-28 build). Same as dk_vr_headset.ps1, plus:
# DK_HANDS=1: each controller moves its hand (position, through the game's own hand IK);
# DK_HANDS_ROT=1: the hands also turn with the controllers. The rotation is lined up once, at the first moment the
# hands are used: whatever way the controllers are held then counts as the game's own hand pose. To line it up again
# (hold both controllers the way the hands look in the game, then), a session sends ik.calib=1 through the bridge.
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
