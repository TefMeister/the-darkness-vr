# The Darkness VR on the OpenXR Simulator (pretend headset) WITH the hands, for measuring with nobody in a headset (2026-11-04).
# The same as dk_vr_headset_hands.ps1 except XR_RUNTIME_JSON. Same as dk_vr_headset.ps1, plus:
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
# 2026-11-10 (b018): the full headset view. The game builds its own world view this wide (engine degrees, aspect); 0 = its own 86 x 55 degree strip. Live: bridge vars view.fov and view.aspect
$env:DK_VIEW_AUTO = "1"   # fit the view to the headset by itself
# 2026-11-10 (b020): in a level the right stick does not tilt the view (the head does), and the left stick walks where the head looks; 0 = off. Live: ctl.no_pitch / ctl.head_walk
$env:DK_VR_NO_PITCH = "1"
$env:DK_VR_HEAD_WALK = "1"
# 2026-11-10 (b021): menus and films are a screen held in the room, smaller (0.75 of its old size); DK_XR_FLAT_WORLD 0 = stuck to the face as before
$env:DK_XR_FLAT_WORLD = "1"
$env:DK_XR_FLAT_SCALE = "0.75"
$env:DK_VR_EXPOSURE = "0"   # 2026-11-06: the game's auto exposure off; with it on the picture blew out to 83 % white and pumped (simulator)
$env:DK_VR_NO_AA = "1"   # 2026-11-10 (b023): the console's anti-aliasing off, so every frame is drawn once instead of in three strips; 0 = as before
$env:DK_XR_EYE_LIGHT = "0"   # 2026-11-10: the per-eye brightness log off; it copied both eye pictures off the card every pair (alley 73 -> 81 game frames a second, simulator); 1 = on for a brightness check
$env:DK_XR_HEAD = "1"
$env:DK_XR_HEAD_MIRROR = "0"
$env:DK_HANDS = "1"
$env:DK_HANDS_ROT = "1"
# 2026-11-12 (b028): the main menu's MULTIPLAYER line is QUIT (the game's own quit question; vr_menu_quit.cpp). 0 = off.
$env:DK_MENU_QUIT = "1"
# 2026-11-12 (Tefa: "we do need to build a quit button into the main menu"): on the MAIN MENU the game's X (the Quest's B)
# opens the game's own question "Are you sure you want to quit?" NO / YES; YES ends the game cleanly. (The page is in the
# game's menu data; the console menu never linked to it.) The pause menu's X does nothing. A test that sets its own
# DK_DEVMENU_MAIN_CMD (a level jump) keeps it.
if (-not $env:DK_DEVMENU_MAIN_CMD) {
    $env:DK_DEVMENU = "1"
    $env:DK_DEVMENU_MAIN_CMD = "cg_submenu('quit_ask')"
    $env:DK_DEVMENU_GAME_CMD = "noop()"
}
$env:XR_RUNTIME_JSON = "D:/claude video game stuff/tools/OpenXR-Simulator/openxr_simulator.json"   # the pretend headset (2026-11-04); the headset scripts remove this
$p = Start-Process -FilePath (Join-Path $PSScriptRoot "darknessrecomp.exe") -WorkingDirectory $PSScriptRoot -PassThru
"pid=$($p.Id)"
