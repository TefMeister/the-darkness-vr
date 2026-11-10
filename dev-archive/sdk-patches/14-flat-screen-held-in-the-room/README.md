# SDK patch 14: the flat pictures held in the room (2026-11-10)

`xr_output.cpp` here is the WHOLE file as it stands after patches 09-13 plus this one (the file is ours from patch 09 on).
`apply.py` is the edit that made patch 14 from the patch-13 state (set the two paths at its top first).

What it does: the game program exports `dk_xr_flat()`; while it returns 1 (out of a level: menus, films, loading) the
headset layer is submitted with the eye poses of the moment the flat stretch began, turned level (yaw only), and with
the game's tangents scaled by `DK_XR_FLAT_SCALE` (default 0.75). The runtime then keeps the picture where it was while
the head turns: a screen in the room. In a level nothing changes.
