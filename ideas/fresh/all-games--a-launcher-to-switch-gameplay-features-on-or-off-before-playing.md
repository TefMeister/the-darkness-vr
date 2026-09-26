# A launcher to switch gameplay features on or off before playing

Order: 9003
From: the ideas repo, `games/all-games.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/all-games.md>), copied 2026-09-26
Shared: yes

`[raw]` · `[not judged]` — *nothing checked against any of our mods*

> "if possible, create a launcher that lets the player choose different gameplay altering fetures
> to be included in the modded game or not before stsrtimg the game"

Verbatim record: [`inbox/2026-09-26-ab3000-lived-in-world-and-mod-launcher.md`](../inbox/2026-09-26-ab3000-lived-in-world-and-mod-launcher.md)

A small window that opens before the game does, listing the features that change how the game
plays, each with an on/off switch. The player picks, presses start, and the game launches with only
those features in.

**What it'd take:** most of our mods already read a settings file when the game starts, so a
launcher could be a front end that writes that file and then starts the game. How much each game
can switch on or off without a restart differs per mod, and has not been checked.

---
