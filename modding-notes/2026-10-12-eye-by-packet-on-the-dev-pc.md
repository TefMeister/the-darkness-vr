# Eye by packet, on the dev PC (2026-10-12)

`/lm` on the dev PC, Claude driving on Opus (Tefa asked for it in place of Fable), one background reader.

## In one paragraph

The dev PC's build setup had broken in the E: → D: move, so it was rebuilt fresh, with the side-by-side
patch (07) that only the home PC had. The reader wrote the board's top design: each frame packet picks its
eye from its serial, and each swap looks up the eye of the packet it drew (probe + SDK patch 08), plus a
"hold the left until its right arrives" compose (08b), a screen-effects switch and a music mute. **Live, the
bookkeeping is exact**: every swap carries the right eye, never the same eye twice, and every camera build
lands inside a packet. **But the two halves of a pair still show two different moments** (the hands in
different poses), so the freeze does not hold the simulation between a pair's packets in this mode. That is
the next problem.

## What was measured

- Eye per swap, menu and car scene: `SWAPS L=R same_eye_twice=0 gpu_lookup_miss=0` `[verified-live 2026-10-12, n=3 runs]`.
- Camera builds: `cam_in_packet=491 cam_outside=0` in the car scene; none at all on the menu `[verified-live 2026-10-12, n=1]`.
  So the menu cannot test the eye offset, and the first two A/B runs (menu) and the scripted car runs (which
  never got past the menu) measured nothing.
- Hold compose: `pairs_shown=1200`, no orphans `[verified-live 2026-10-12, n=1]`.
- By eye: halves show different animation poses, with and without the hold `[verified-live 2026-10-12, n=2, by eye]`.
  `max_hold` reached 15.5 s once.

## Driving

- A timed A on CONTINUE is often swallowed while the menu fades in. Press, check the picture changed, retry.
- The game pauses when the virtual pad goes away (its process exits). Keep the pad alive for the whole run.
- Tefa's real Xbox One pad was plugged in; the virtual pad still drove the menus.

## Installed on the dev PC

Probe with eye-by-packet + screen-effects switch, SDK with 04/05/06/07/08/08b (Condemned 01 kept, 02 reverted);
all off unless their `DK_*` switch is set. Old files in `build-local/bak-2026-10-12-pre-eye-by-packet/`.
Music set to 0 in the save profile (backup beside it).

## Not established

- Why the freeze does not hold a pair together in packet mode (the reader is on it).
- Whether the screen-effects switch removes visible blur or flares (built, not yet run).
- Whether the music is actually silent (not heard).
