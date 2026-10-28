# The menus and the opening logos on a screen in front, not glued to the head

Order: 7001
From: the ideas repo, `games/the-darkness.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/the-darkness.md>), copied 2026-10-29

`[raw]` · `[looks doable]` — *not checked against the port; the head-locked view is ours to place*

> "get main menu and first game logos to show up on a screen in front of me, not follow my head"

Right now everything the game draws flat (the first logos, the main menu) rides with the head, because the whole
picture is placed where the headset looks. The ask: while the game shows a menu or a logo screen, hang that picture
on a still screen in front of the player instead, so it stays put when they look around, the way a cinema screen
does. In play the world goes back to following the head.

**What it'd take:** the port already knows when the game is in a menu (no game camera); in those moments the stereo
layer places the picture as a quad in the room space, at a fixed spot in front, instead of in the head space.
⚠️ Not checked: whether the OpenXR output in the SDK patch can switch spaces per frame without a stall.

Verbatim record: [`inbox/2026-10-29-the-darkness-menus-and-logos-on-a-screen-in-front.md`](../inbox/2026-10-29-the-darkness-menus-and-logos-on-a-screen-in-front.md)

---
