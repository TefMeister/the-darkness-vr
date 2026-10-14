# A hand and body poser like EMV Engine's, for our other third-person games

Order: 9004
From: the ideas repo, `games/all-games.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/all-games.md>), copied 2026-10-14
Shared: yes

`[raw]` · `[hard]` — *judged from what EMV Engine is built on, not checked against any of the other engines*

> "what we need to do at some point, is an add-on for the EMVengine, that adds new games to the list, so we can use
> the EMV poser on other 3rd person games like Alan wake, tew and so on - just an idea for now"

Verbatim record: [`inbox/2026-10-13-all-games-emv-poser-for-other-engines.md`](../inbox/2026-10-13-all-games-emv-poser-for-other-engines.md)

The goal: the same live posing of fingers, hands and joints that EMV Engine gives us in RE2, in Alan Wake, The Evil
Within and our other third-person ports.

**What it'd take:** EMV Engine (alphaZomega) is a Lua script that runs inside REFramework and leans on the RE Engine's
own catalogue of every game object and joint, so its list can only ever hold RE Engine games. Alan Wake (Northlight)
and The Evil Within (id Tech 5) have no such catalogue and no REFramework. So it would not be an add-on to EMV Engine
but a small poser of our own per engine: find the skeleton and its joints in memory (the same recon each VR port
already does for the hands), then a menu with sliders like the RE2 hand tuning menu. Once one engine's skeleton is
found, the menu part is reusable.

---
