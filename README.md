# The Darkness (2007) — working toward VR

Notes and tooling for getting **The Darkness** (Starbreeze, 2007) playable on PC and then into a
VR headset.

The Darkness never had a PC release — it shipped on Xbox 360 and PlayStation 3 and stayed there.
A community **static recompilation** to native PC is in progress elsewhere (a working build has been
shown publicly, source not yet released at the time of writing). This repository is not that port.
It is our own preparation and research around it, with VR as the goal.

**This is the project we most want in VR.** Everything here is aimed at that.

## What is here

| Folder | What it holds |
| --- | --- |
| [`engine-research/`](engine-research/) | [`ENGINE-DOSSIER.md`](engine-research/ENGINE-DOSSIER.md) — what is actually known about the game's files and engine, with confidence tags |
| [`dev-archive/`](dev-archive/) | Working source, scripts and reverse-engineering evidence |
| [`modding-notes/`](modding-notes/) | Dated field notes from each working session, including what did **not** work |

## Status

**Preparation done, waiting on someone else's release.**

A verified 1:1 copy of the Xbox 360 disc has been made and the game files extracted, so the moment a
PC recompilation is published we can use it immediately — those projects ship no game content and
require you to supply files from your own disc.

No VR work has begun, and it cannot begin until there is a PC build to attach it to.

## Why VR is realistic here, eventually

The route runs through the recompilation, not around it. A static recompilation turns the original
Xbox 360 code into **readable, buildable C++**, which means stereo rendering can be added *at the
source* rather than injected into a sealed binary from outside.

We have already done exactly that groundwork on a closely-related project —
[**condemned-2-vr**](https://github.com/TefMeister/condemned-2-vr), another 2007-08 Xbox 360 title
now running on PC from a self-built recompilation. The build knowledge, the fixes and the renderer
notes there apply directly here.

⚠️ **This depends on the Darkness port releasing its source.** If it ships as binaries only, the
cheap source-level route closes and this becomes a much harder problem.

## What this is not

- **Not the PC port**, not a fork of it, and not affiliated with whoever is building it.
- **No game content.** No assets, executables or disc images — nothing from the game. You need your
  own legitimate copy.
- **Unfinished.** Nothing here is a supported product.

## Caution

Experimental and unfinished. Any VR build that comes out of this may cause **severe motion sickness
and discomfort**. That warning will only be removed for a build once it has been played through and
confirmed comfortable.

## Credits

- **Starbreeze Studios**, who made the game, and **2K Games**, who published it.
- **Whoever is building the PC recompilation** — the hard part is theirs. Credit will be named here
  properly once the project is public.
- **The [ReXGlue](https://github.com/rexglue/rexglue-sdk) project**, for the Xbox 360 recompilation
  toolkit this class of port is built on.
- **[Redump](http://redump.org/)**, whose drive and firmware documentation made reading the disc
  possible, and the **Kreon** firmware authors.
- **[redumper](https://github.com/superg/redumper)** (superg) and **[xdvdfs](https://github.com/antangelo/xdvdfs)**
  (antangelo), the tools used to copy the disc and read it.

If you should be credited here and are not, or want something changed or removed, please get in
touch and it will be put right as fast as we can.

## Legal

A non-commercial fan project. It requires you to own a legitimate copy of the game and redistributes
no original assets.

## Licence

**BSD 3-Clause** — see [`LICENSE`](LICENSE). Deliberately the same licence as the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) and the recompilation projects it supports, so
that fixes can move freely in either direction without a licensing conversation.

It covers the code and notes in this repository only. It grants **no rights in any game or asset** —
none are included here, and you need your own legitimate copy of the game.
