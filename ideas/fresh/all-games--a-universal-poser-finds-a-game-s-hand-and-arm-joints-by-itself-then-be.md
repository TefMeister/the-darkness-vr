# A universal poser: finds a game's hand and arm joints by itself, then bends them with sliders

Order: 9003
From: the ideas repo, `games/all-games.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/all-games.md>), copied 2026-10-14
Shared: yes

`[raw]` · `[hard]` — *judged from what each engine is like, not checked against any game*

> "or yeah, better yet, file the idea to start working on a universal poser system, that knows what to look for in a
> game to bend hand and arm joints"

Verbatim record: [`inbox/2026-10-13-all-games-universal-poser-system.md`](../inbox/2026-10-13-all-games-universal-poser-system.md)

Tefa's widening of the entry below, the same day: not one poser per engine, but ONE system that knows what to look
for. **What it'd take:** a recipe of signs that work in most games: skeletons are lists of joints, each a small block
of position and turn numbers, usually with readable names (`hand`, `wrist`, `finger`, `l_`/`r_`) and a parent chain
running shoulder, elbow, wrist, fingers; moving a real hand in front of the camera shows which block moves. Our VR
ports already find these joints by hand for each game, so the first step is writing that search down as one tool,
then a shared slider menu (like the RE2 hand tuning menu) on top. The hard part is games whose skeleton is rebuilt
every frame by the animation, where a change has to be written at the right moment each frame.
